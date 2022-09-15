// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.remoteobjects;

import androidx.annotation.IntDef;

import org.chromium.blink.mojom.RemoteArrayType;
import org.chromium.blink.mojom.RemoteInvocationArgument;
import org.chromium.blink.mojom.RemoteInvocationError;
import org.chromium.blink.mojom.RemoteInvocationResult;
import org.chromium.blink.mojom.RemoteInvocationResultValue;
import org.chromium.blink.mojom.RemoteObject;
import org.chromium.blink.mojom.RemoteTypedArray;
import org.chromium.blink.mojom.SingletonJavaScriptValue;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo_base.BigBufferUtil;
import org.chromium.mojo_base.mojom.String16;

import java.lang.annotation.Annotation;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.lang.reflect.Array;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.DoubleBuffer;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.nio.ShortBuffer;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.SortedMap;
import java.util.TreeMap;
import java.util.regex.Pattern;

/**
 * Exposes limited access to a Java object over a Mojo interface.
 *
 * For LiveConnect references, an archived version is available at:
 * http://web.archive.org/web/20141022204935/http://jdk6.java.net/plugin2/liveconnect/
 */
class RemoteObjectImpl implements RemoteObject {
    /**
     * Receives notification about events for auditing.
     *
     * Separated from this class proper to allow for unit testing in content_junit_tests, where the
     * Android framework is not fully initialized.
     *
     * Implementations should take care not to hold a strong reference to anything that might keep
     * the WebView contents alive due to a GC cycle.
     */
    interface Auditor {
        void onObjectGetClassInvocationAttempt();
    }

    /**
     * Provides numeric identifier for Java objects to be exposed.
     * These identifiers must not collide.
     */
    interface ObjectIdAllocator {
        int getObjectId(Object object, Class<? extends Annotation> safeAnnotationClass);
        Object getObjectById(int id);
        void unrefObjectByObject(Object object);
    }

    /**
     * Method which may not be called.
     */
    private static final Method sGetClassMethod;
    static {
        try {
            sGetClassMethod = Object.class.getMethod("getClass");
        } catch (NoSuchMethodException e) {
            // java.lang.Object#getClass should always exist.
            throw new RuntimeException(e);
        }
    }

    /**
     * The object to which invocations should be directed.
     *
     * The target object cannot be referred to strongly, because it may contain
     * references which form an uncollectable cycle.
     */
    private final WeakReference<Object> mTarget;

    /**
     * Annotation required on all exposed methods.
     * If null, no annotation is required.
     * In practice, this is usually {@link android.webkit.JavascriptInterface}.
     */
    private final Class<? extends Annotation> mSafeAnnotationClass;

    /**
     * Allocates IDs for other Java objects.
     *
     * Cannot be held strongly, because it may (via the objects it holds) contain
     * references which form an uncollectable cycle.
     */
    private final WeakReference<ObjectIdAllocator> mObjectIdAllocator;

    /**
     * Receives notification about events for auditing.
     */
    private final Auditor mAuditor;

    /**
     * Callable methods, indexed by name.
     */
    private final SortedMap<String, List<Method>> mMethods = new TreeMap<>();

    /**
     * If true, allows an object context's inspection when {@link #getMethods} is called.
     */
    private final boolean mAllowInspection;

    private boolean mNotifiedReleasedObject;

    public static final short UNSIGNED_BYTE_MASK = 0xff;
    public static final int UNSIGNED_SHORT_MASK = 0xffff;
    public static final long UNSIGNED_INT_MASK = 0xffffffffL;

    private static final Pattern sDoubleNumberPattern = Pattern.compile(
            "^(-?[0-9]+)(\\.0+)? ( ( (?:\\.[0-9]*[1-9])? )0* ) ((?:e.*)?)$", Pattern.COMMENTS);

    public RemoteObjectImpl(Object target, Class<? extends Annotation> safeAnnotationClass,
            Auditor auditor, ObjectIdAllocator objectIdAllocator, boolean allowInspection) {
        mTarget = new WeakReference<>(target);
        mSafeAnnotationClass = safeAnnotationClass;
        mAuditor = auditor;
        mObjectIdAllocator = new WeakReference<>(objectIdAllocator);
        mAllowInspection = allowInspection;
        mNotifiedReleasedObject = false;

        for (Method method : target.getClass().getMethods()) {
            if (safeAnnotationClass != null && !method.isAnnotationPresent(safeAnnotationClass)) {
                continue;
            }

            String methodName = method.getName();
            List<Method> methodsWithName = mMethods.get(methodName);
            if (methodsWithName == null) {
                methodsWithName = new ArrayList<>(1);
                mMethods.put(methodName, methodsWithName);
            }
            methodsWithName.add(method);
        }
    }

    @Override
    public void hasMethod(String name, HasMethod_Response callback) {
        callback.call(mMethods.containsKey(name));
    }

    @Override
    public void getMethods(GetMethods_Response callback) {
        if (!mAllowInspection) {
            callback.call(new String[0]);
            return;
        }
        Set<String> methodNames = mMethods.keySet();
        callback.call(methodNames.toArray(new String[methodNames.size()]));
    }

    @Override
    public void invokeMethod(
            String name, RemoteInvocationArgument[] arguments, InvokeMethod_Response callback) {
        Object target = mTarget.get();
        ObjectIdAllocator objectIdAllocator = mObjectIdAllocator.get();
        if (target == null || objectIdAllocator == null) {
            // TODO(jbroman): Handle this.
            return;
        }

        int numArguments = arguments.length;
        Method method = findMethod(name, numArguments);
        if (method == null) {
            callback.call(makeErrorResult(RemoteInvocationError.METHOD_NOT_FOUND));
            return;
        }
        if (method.equals(sGetClassMethod)) {
            if (mAuditor != null) {
                mAuditor.onObjectGetClassInvocationAttempt();
            }
            callback.call(makeErrorResult(RemoteInvocationError.OBJECT_GET_CLASS_BLOCKED));
            return;
        }
        if (method.getReturnType().isArray()) {
            // LIVECONNECT_COMPLIANCE: Existing behavior is to not call methods that
            // return arrays. Spec requires calling the method and converting the
            // result to a JavaScript array.
            RemoteInvocationResult result = new RemoteInvocationResult();
            result.value = new RemoteInvocationResultValue();
            result.value.setSingletonValue(SingletonJavaScriptValue.UNDEFINED);
            callback.call(result);
            return;
        }

        Class<?>[] parameterTypes = method.getParameterTypes();
        Object[] args = new Object[numArguments];
        for (int i = 0; i < numArguments; i++) {
            try {
                args[i] = convertArgument(arguments[i], parameterTypes[i],
                        StringCoercionMode.COERCE, objectIdAllocator);
            } catch (IllegalArgumentException e) {
                callback.call(makeErrorResult(RemoteInvocationError.NON_ASSIGNABLE_TYPES));
                return;
            }
        }

        Object result = null;
        try {
            result = method.invoke(target, args);
        } catch (IllegalAccessException | IllegalArgumentException | NullPointerException e) {
            // These should never happen.
            //
            // IllegalAccessException:
            //   java.lang.Class#getMethods returns only public members, so |mMethods| should never
            //   contain any method for which IllegalAccessException would be thrown.
            //
            // IllegalArgumentException:
            //   Argument coercion logic is responsible for creating objects of a suitable Java
            //   type.
            //
            // NullPointerException:
            //   A user of this class is responsible for ensuring that the target is not collected.
            throw new RuntimeException(e);
        } catch (InvocationTargetException e) {
            e.getCause().printStackTrace();
            callback.call(makeErrorResult(RemoteInvocationError.EXCEPTION_THROWN));
            return;
        }

        RemoteInvocationResult mojoResult = convertResult(
                result, method.getReturnType(), objectIdAllocator, mSafeAnnotationClass);
        callback.call(mojoResult);
    }

    @Override
    public void notifyReleasedObject() {
        mNotifiedReleasedObject = true;
    }

    @Override
    public void close() {
        Object target = mTarget.get();
        ObjectIdAllocator objectIdAllocator = mObjectIdAllocator.get();
        // If |mNotifiedReleasedObject| is false, this outlives the RemoteObjectHost and the object
        // is not unreferenced by the RemoteObjectHost. So, we should unreference the object when
        // the mojo pipe is closed.
        if (target != null && objectIdAllocator != null && !mNotifiedReleasedObject) {
            objectIdAllocator.unrefObjectByObject(target);
        }

        mTarget.clear();
    }

    @Override
    public void onConnectionError(MojoException e) {
        close();
    }

    private Method findMethod(String name, int numParameters) {
        List<Method> methods = mMethods.get(name);
        if (methods == null) {
            return null;
        }

        // LIVECONNECT_COMPLIANCE: We just take the first method with the correct
        // number of arguments, while the spec proposes using cost-based algorithm:
        // https://jdk6.java.net/plugin2/liveconnect/#OVERLOADED_METHODS
        for (Method method : methods) {
            if (method.getParameterTypes().length == numParameters) return method;
        }

        return null;
    }

    @IntDef({StringCoercionMode.DO_NOT_COERCE, StringCoercionMode.COERCE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface StringCoercionMode {
        // Do not coerce non-strings to string; instead produce null.
        // Used when coercing arguments inside arrays.
        int DO_NOT_COERCE = 0;

        // Coerce into strings more aggressively. Applied when the parameter type is
        // java.lang.String exactly.
        int COERCE = 1;
    }

    private static Object convertPrimitiveArrayElement(Number number, Class<?> parameterType) {
        assert (parameterType.isPrimitive() && parameterType != boolean.class);
        if (parameterType == byte.class) {
            return number.byteValue();
        } else if (parameterType == char.class) {
            return (char) (number.intValue() & UNSIGNED_SHORT_MASK);
        } else if (parameterType == short.class) {
            return number.shortValue();
        } else if (parameterType == int.class) {
            return number.intValue();
        } else if (parameterType == long.class) {
            return number.longValue();
        } else if (parameterType == float.class) {
            return number.floatValue();
        }

        return number.doubleValue();
    }

    private abstract static class WrapBuffer {
        protected Class<?> mParameterType;
        protected int mLength;

        WrapBuffer(Class<?> parameterType) {
            mParameterType = parameterType;
        }

        public Object copyArray() {
            if (mParameterType == boolean.class) {
                // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to false. Spec
                // requires converting to false for 0 or NaN, true otherwise.
                // The default value of the boolean elements in a boolean array is false.
                return new boolean[mLength];
            } else if (isFloatType() && mParameterType == char.class) {
                // LIVECONNECT_COMPLIANCE: Existing behavior is to convert floating-point types to
                // 0. Spec requires converting doubles similarly to how we convert floating-point
                // types to other numeric types.
                // The default value of the char elements in a char array is 0.
                return new char[mLength];
            } else if (mParameterType == String.class) {
                // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to null for all.
                // The default value of the String elements in a String array is null.
                return new String[mLength];
            }

            Object result = Array.newInstance(mParameterType, mLength);
            for (int i = 0; i < mLength; i++) {
                Array.set(result, i, convertPrimitiveArrayElement(get(i), mParameterType));
            }
            return result;
        }

        protected abstract Number get(int index);
        protected boolean isFloatType() {
            return false;
        }
    }

    private static class WrapByteBuffer extends WrapBuffer {
        ByteBuffer mBuffer;
        boolean mUnsigned;
        WrapByteBuffer(ByteBuffer buffer, Class<?> parameterType, boolean unsigned) {
            super(parameterType);
            mBuffer = buffer;
            mLength = mBuffer.limit();
            mUnsigned = unsigned;
        }

        @Override
        public Object copyArray() {
            if (mParameterType != byte.class) {
                return super.copyArray();
            }
            byte[] result = new byte[mLength];
            mBuffer.get(result);
            return result;
        }

        @Override
        protected Number get(int index) {
            byte number = mBuffer.get(index);
            return (mUnsigned ? (short) (number & UNSIGNED_BYTE_MASK) : number);
        }
    }

    private static class WrapShortBuffer extends WrapBuffer {
        ShortBuffer mBuffer;
        boolean mUnsigned;
        WrapShortBuffer(ShortBuffer buffer, Class<?> parameterType, boolean unsigned) {
            super(parameterType);
            mBuffer = buffer;
            mLength = mBuffer.limit();
            mUnsigned = unsigned;
        }

        @Override
        public Object copyArray() {
            if (mParameterType != short.class) {
                return super.copyArray();
            }
            short[] result = new short[mLength];
            mBuffer.get(result);
            return result;
        }

        @Override
        protected Number get(int index) {
            short number = mBuffer.get(index);
            return (mUnsigned ? (int) (number & UNSIGNED_SHORT_MASK) : number);
        }
    }

    private static class WrapIntBuffer extends WrapBuffer {
        IntBuffer mBuffer;
        boolean mUnsigned;
        WrapIntBuffer(IntBuffer buffer, Class<?> parameterType, boolean unsigned) {
            super(parameterType);
            mBuffer = buffer;
            mLength = mBuffer.limit();
            mUnsigned = unsigned;
        }

        @Override
        public Object copyArray() {
            if (mParameterType != int.class) {
                return super.copyArray();
            }
            int[] result = new int[mLength];
            mBuffer.get(result);
            return result;
        }

        @Override
        protected Number get(int index) {
            int number = mBuffer.get(index);
            return (mUnsigned ? (long) (number & UNSIGNED_INT_MASK) : number);
        }
    }

    private static class WrapFloatBuffer extends WrapBuffer {
        FloatBuffer mBuffer;
        WrapFloatBuffer(FloatBuffer buffer, Class<?> parameterType) {
            super(parameterType);
            mBuffer = buffer;
            mLength = mBuffer.limit();
        }

        @Override
        public Object copyArray() {
            if (mParameterType != float.class) {
                return super.copyArray();
            }
            float[] result = new float[mLength];
            mBuffer.get(result);
            return result;
        }

        @Override
        protected Number get(int index) {
            return mBuffer.get(index);
        }

        @Override
        protected boolean isFloatType() {
            return true;
        }
    }

    private static class WrapDoubleBuffer extends WrapBuffer {
        DoubleBuffer mBuffer;
        WrapDoubleBuffer(DoubleBuffer buffer, Class<?> parameterType) {
            super(parameterType);
            mBuffer = buffer;
            mLength = mBuffer.limit();
        }

        @Override
        public Object copyArray() {
            if (mParameterType != double.class) {
                return super.copyArray();
            }
            double[] result = new double[mLength];
            mBuffer.get(result);
            return result;
        }

        @Override
        protected Number get(int index) {
            return mBuffer.get(index);
        }

        @Override
        protected boolean isFloatType() {
            return true;
        }
    }

    private static Object convertArgument(RemoteInvocationArgument argument, Class<?> parameterType,
            @StringCoercionMode int stringCoercionMode, ObjectIdAllocator objectIdAllocator) {
        switch (argument.which()) {
            case RemoteInvocationArgument.Tag.NumberValue:
                // See http://jdk6.java.net/plugin2/liveconnect/#JS_NUMBER_VALUES.
                // For conversion to numeric types, we need to replicate Java's type
                // conversion rules.
                double numberValue = argument.getNumberValue();
                if (parameterType == byte.class) {
                    return (byte) numberValue;
                } else if (parameterType == char.class) {
                    if (isInt32(numberValue)) {
                        return (char) numberValue;
                    } else {
                        // LIVECONNECT_COMPLIANCE: Existing behavior is to convert double to 0.
                        // Spec requires converting doubles similarly to how we convert doubles to
                        // other numeric types.
                        return (char) 0;
                    }
                } else if (parameterType == short.class) {
                    return (short) numberValue;
                } else if (parameterType == int.class) {
                    return (int) numberValue;
                } else if (parameterType == long.class) {
                    return (long) numberValue;
                } else if (parameterType == float.class) {
                    return (float) numberValue;
                } else if (parameterType == double.class) {
                    return numberValue;
                } else if (parameterType == boolean.class) {
                    // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to false. Spec
                    // requires converting to false for 0 or NaN, true otherwise.
                    return false;
                } else if (parameterType == String.class) {
                    return stringCoercionMode == StringCoercionMode.COERCE
                            ? doubleToString(numberValue)
                            : null;
                } else if (parameterType.isArray()) {
                    // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to null. Spec
                    // requires raising a JavaScript exception.
                    return null;
                } else {
                    // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to null. Spec
                    // requires handling object equivalents of primitive types.
                    assert !parameterType.isPrimitive();
                    return null;
                }
            case RemoteInvocationArgument.Tag.BooleanValue:
                // See http://jdk6.java.net/plugin2/liveconnect/#JS_BOOLEAN_VALUES.
                boolean booleanValue = argument.getBooleanValue();
                if (parameterType == boolean.class) {
                    return booleanValue;
                } else if (parameterType.isPrimitive()) {
                    // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to 0 for all
                    // non-boolean primitive types. Spec requires converting to 0 or 1.
                    return getPrimitiveZero(parameterType);
                } else if (parameterType == String.class) {
                    return stringCoercionMode == StringCoercionMode.COERCE
                            ? Boolean.toString(booleanValue)
                            : null;
                } else if (parameterType.isArray()) {
                    return null;
                } else {
                    // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to NULL. Spec
                    // requires handling java.lang.Boolean and java.lang.Object.
                    assert !parameterType.isPrimitive();
                    return null;
                }
            case RemoteInvocationArgument.Tag.StringValue:
                // See http://jdk6.java.net/plugin2/liveconnect/#JS_STRING_VALUES.
                if (parameterType == String.class) {
                    return mojoStringToJavaString(argument.getStringValue());
                } else if (parameterType.isPrimitive()) {
                    // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to 0. Spec
                    // requires using valueOf() method of corresponding object type, or
                    // converting to boolean based on whether the string is empty.
                    return getPrimitiveZero(parameterType);
                } else if (parameterType.isArray()) {
                    // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to NULL. Spec
                    // requires raising a JavaScript exception.
                    return null;
                } else {
                    // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to NULL. Spec
                    // requires handling java.lang.Object.
                    return null;
                }
            case RemoteInvocationArgument.Tag.SingletonValue:
                int singletonValue = argument.getSingletonValue();
                boolean isUndefined = singletonValue == SingletonJavaScriptValue.UNDEFINED;
                if (parameterType == String.class) {
                    // LIVECONNECT_COMPLIANCE: Existing behavior is to convert undefined to
                    // "undefined". Spec requires converting undefined to NULL.
                    return (argument.getSingletonValue() == SingletonJavaScriptValue.UNDEFINED
                                   && stringCoercionMode == StringCoercionMode.COERCE)
                            ? "undefined"
                            : null;
                } else if (parameterType.isPrimitive()) {
                    return getPrimitiveZero(parameterType);
                } else if (parameterType.isArray()) {
                    // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to NULL. Spec
                    // requires raising a JavaScript exception.
                    return null;
                } else {
                    return null;
                }
            case RemoteInvocationArgument.Tag.ArrayValue:
                RemoteInvocationArgument[] arrayValue = argument.getArrayValue();
                if (parameterType.isArray()) {
                    Class<?> componentType = parameterType.getComponentType();

                    // LIVECONNECT_COMPLIANCE: Existing behavior is to return null for
                    // multi-dimensional and object arrays. Spec requires handling them.
                    if (!componentType.isPrimitive() && componentType != String.class) {
                        return null;
                    }

                    Object result = Array.newInstance(componentType, arrayValue.length);
                    for (int i = 0; i < arrayValue.length; i++) {
                        Object element = convertArgument(arrayValue[i], componentType,
                                StringCoercionMode.DO_NOT_COERCE, objectIdAllocator);
                        Array.set(result, i, element);
                    }
                    return result;
                } else if (parameterType == String.class) {
                    return stringCoercionMode == StringCoercionMode.COERCE ? "undefined" : null;
                } else if (parameterType.isPrimitive()) {
                    return getPrimitiveZero(parameterType);
                } else {
                    // LIVECONNECT_COMPLIANCE: Existing behavior is to pass null. Spec requires
                    // converting if the target type is netscape.javascript.JSObject, otherwise
                    // raising a JavaScript exception.
                    return null;
                }
            case RemoteInvocationArgument.Tag.TypedArrayValue:
                RemoteTypedArray typedArrayValue = argument.getTypedArrayValue();
                if (parameterType.isArray()) {
                    Class<?> componentType = parameterType.getComponentType();

                    // LIVECONNECT_COMPLIANCE: Existing behavior is to return null for
                    // multi-dimensional and object arrays. Spec requires handling them.
                    if (!componentType.isPrimitive() && componentType != String.class) {
                        return null;
                    } else if (componentType.isArray()) {
                        // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to NULL. Spec
                        // requires raising a JavaScript exception.
                        return null;
                    }

                    // TODO(crbug.com/794320): Remove unnecessary copy for the performance.
                    ByteBuffer typedBuffer = ByteBuffer.wrap(
                            BigBufferUtil.getBytesFromBigBuffer(typedArrayValue.buffer));
                    typedBuffer.order(ByteOrder.nativeOrder());

                    if (typedArrayValue.type == RemoteArrayType.INT8_ARRAY) {
                        return new WrapByteBuffer(typedBuffer, componentType, false).copyArray();
                    } else if (typedArrayValue.type == RemoteArrayType.UINT8_ARRAY) {
                        return new WrapByteBuffer(typedBuffer, componentType, true).copyArray();
                    } else if (typedArrayValue.type == RemoteArrayType.INT16_ARRAY) {
                        return new WrapShortBuffer(
                                typedBuffer.asShortBuffer(), componentType, false)
                                .copyArray();
                    } else if (typedArrayValue.type == RemoteArrayType.UINT16_ARRAY) {
                        return new WrapShortBuffer(typedBuffer.asShortBuffer(), componentType, true)
                                .copyArray();
                    } else if (typedArrayValue.type == RemoteArrayType.INT32_ARRAY) {
                        return new WrapIntBuffer(typedBuffer.asIntBuffer(), componentType, false)
                                .copyArray();
                    } else if (typedArrayValue.type == RemoteArrayType.UINT32_ARRAY) {
                        return new WrapIntBuffer(typedBuffer.asIntBuffer(), componentType, true)
                                .copyArray();
                    } else if (typedArrayValue.type == RemoteArrayType.FLOAT32_ARRAY) {
                        return new WrapFloatBuffer(typedBuffer.asFloatBuffer(), componentType)
                                .copyArray();
                    } else if (typedArrayValue.type == RemoteArrayType.FLOAT64_ARRAY) {
                        return new WrapDoubleBuffer(typedBuffer.asDoubleBuffer(), componentType)
                                .copyArray();
                    } else {
                        return null;
                    }
                } else if (parameterType == String.class) {
                    return stringCoercionMode == StringCoercionMode.COERCE ? "undefined" : null;
                } else if (parameterType.isPrimitive()) {
                    return getPrimitiveZero(parameterType);
                } else {
                    // LIVECONNECT_COMPLIANCE: Existing behavior is to pass null. Spec requires
                    // converting if the target type is netscape.javascript.JSObject, otherwise
                    // raising a JavaScript exception.
                    return null;
                }
            case RemoteInvocationArgument.Tag.ObjectIdValue:
                if (parameterType == String.class) {
                    return stringCoercionMode == StringCoercionMode.COERCE ? "undefined" : null;
                } else if (parameterType.isPrimitive()) {
                    return getPrimitiveZero(parameterType);
                } else if (parameterType.isArray()) {
                    // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to null. Spec
                    // requires raising a JavaScript exception.
                    return null;
                }

                Object object = objectIdAllocator.getObjectById(argument.getObjectIdValue());
                if (object == null) {
                    // LIVECONNECT_COMPLIANCE: Existing behavior is to pass null. Spec
                    // requires converting if the target type is
                    // netscape.javascript.JSObject, otherwise raising a JavaScript
                    // exception.
                    return null;
                }
                if (parameterType.isInstance(object)) return object;

                throw new IllegalArgumentException("incompatible argument type with object id");
            default:
                throw new RuntimeException("invalid wire argument type");
        }
    }

    private static RemoteInvocationResult convertResult(Object result, Class<?> returnType,
            ObjectIdAllocator objectIdAllocator, Class<? extends Annotation> safeAnnotationClass) {
        // Methods returning arrays should not be called (for legacy reasons).
        assert !returnType.isArray();

        // LIVECONNECT_COMPLIANCE: The specification suggests that the conversion should happen
        // based on the type of the result value. Existing behavior is to rely on the declared
        // return type of the method. This means, for instance, that a java.lang.String returned
        // from a method declared as returning java.lang.Object will not be converted to a
        // JavaScript string.
        RemoteInvocationResultValue resultValue = new RemoteInvocationResultValue();
        if (returnType == void.class) {
            resultValue.setSingletonValue(SingletonJavaScriptValue.UNDEFINED);
        } else if (returnType == boolean.class) {
            resultValue.setBooleanValue((Boolean) result);
        } else if (returnType == char.class) {
            resultValue.setNumberValue((Character) result);
        } else if (returnType.isPrimitive()) {
            resultValue.setNumberValue(((Number) result).doubleValue());
        } else if (returnType == String.class) {
            if (result == null) {
                // LIVECONNECT_COMPLIANCE: Existing behavior is to return undefined.
                // Spec requires returning a null string.
                resultValue.setSingletonValue(SingletonJavaScriptValue.UNDEFINED);
            } else {
                resultValue.setStringValue(javaStringToMojoString((String) result));
            }
        } else if (result == null) {
            resultValue.setSingletonValue(SingletonJavaScriptValue.NULL);
        } else {
            int objectId = objectIdAllocator.getObjectId(result, safeAnnotationClass);
            resultValue.setObjectId(objectId);
        }
        RemoteInvocationResult mojoResult = new RemoteInvocationResult();
        mojoResult.value = resultValue;
        return mojoResult;
    }

    private static RemoteInvocationResult makeErrorResult(int error) {
        assert error != RemoteInvocationError.OK;
        RemoteInvocationResult result = new RemoteInvocationResult();
        result.error = error;
        return result;
    }

    /**
     * Returns whether the value is an Int32 in the V8 API sense.
     * That is, it has an integer value in [-2^31, 2^31) and is not negative zero.
     */
    private static boolean isInt32(double doubleValue) {
        return doubleValue % 1.0 == 0.0 && doubleValue >= Integer.MIN_VALUE
                && doubleValue <= Integer.MAX_VALUE
                && (doubleValue != 0.0 || (1.0 / doubleValue) > 0.0);
    }

    private static String doubleToString(double doubleValue) {
        // For compatibility, imitate the existing behavior.
        // The previous implementation applied Int64ToString to any integer that fit in 32 bits,
        // except for negative zero, and base::StringPrintf("%.6lg", doubleValue) for all other
        // values.
        if (Double.isNaN(doubleValue)) {
            return "nan";
        }
        if (Double.isInfinite(doubleValue)) {
            return doubleValue > 0 ? "inf" : "-inf";
        }
        // Negative zero is mathematically an integer, but keeps its negative sign.
        if (doubleValue == 0.0 && (1.0 / doubleValue) < 0.0) {
            return "-0";
        }
        // All other 32-bit signed integers are formatted without abbreviation.
        if (doubleValue % 1.0 == 0.0 && doubleValue >= Integer.MIN_VALUE
                && doubleValue <= Integer.MAX_VALUE) {
            return Integer.toString((int) doubleValue);
        }
        // Remove trailing zeroes and, if appropriate, the decimal point.
        // Expression is somewhat complicated, in order to deal with scientific notation. Either
        // group 2 will match (and so the decimal will be stripped along with zeroes), or group 3
        // will match (and the decimal will be left), but not both (since there cannot be more than
        // one decimal point). Group 5 will match an exponential part.
        return sDoubleNumberPattern.matcher(String.format((Locale) null, "%.6g", doubleValue))
                .replaceAll("$1$4$5");
    }

    private static Object getPrimitiveZero(Class<?> parameterType) {
        assert parameterType.isPrimitive();
        if (parameterType == boolean.class) {
            return false;
        } else if (parameterType == byte.class) {
            return (byte) 0;
        } else if (parameterType == char.class) {
            return (char) 0;
        } else if (parameterType == short.class) {
            return (short) 0;
        } else if (parameterType == int.class) {
            return (int) 0;
        } else if (parameterType == long.class) {
            return (long) 0;
        } else if (parameterType == float.class) {
            return (float) 0;
        } else if (parameterType == double.class) {
            return (double) 0;
        } else {
            throw new RuntimeException("unexpected primitive type " + parameterType);
        }
    }

    private static String mojoStringToJavaString(String16 mojoString) {
        short[] data = mojoString.data;
        char[] chars = new char[data.length];
        for (int i = 0; i < chars.length; i++) {
            chars[i] = (char) data[i];
        }
        return String.valueOf(chars);
    }

    private static String16 javaStringToMojoString(String string) {
        short[] data = new short[string.length()];
        for (int i = 0; i < data.length; i++) {
            data[i] = (short) string.charAt(i);
        }
        String16 mojoString = new String16();
        mojoString.data = data;
        return mojoString;
    }
}
