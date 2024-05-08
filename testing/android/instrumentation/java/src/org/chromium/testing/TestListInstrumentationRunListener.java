// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing;

import android.os.Bundle;

import androidx.test.internal.runner.listener.InstrumentationRunListener;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.runner.Description;
import org.junit.runner.Result;
import org.junit.runner.notification.Failure;
import org.junit.runners.model.InitializationError;

import java.io.PrintStream;
import java.lang.annotation.Annotation;
import java.lang.reflect.Array;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

/**
 * A RunListener that captures the list of tests along with annotation information (AndroidX's
 * default log-only listener does not capture annotations).
 */
public class TestListInstrumentationRunListener extends InstrumentationRunListener {
    // Should not conflict with androidx's InstrumentationResultPrinter.java, which uses 0 and 1.
    private static final int STATUS_CODE = 5;
    private static final Set<String> SKIP_METHODS =
            Set.of("toString", "hashCode", "annotationType", "equals");

    private final boolean mRequireBaseRunner;
    private final List<Failure> mFailures = new ArrayList<>();
    private Class<?> mActiveTestClass;
    private final Set<String> mClassesWithWrongRunner = new TreeSet<>();

    public TestListInstrumentationRunListener() {
        this(false);
    }

    public TestListInstrumentationRunListener(boolean requireBaseRunner) {
        mRequireBaseRunner = requireBaseRunner;
    }

    @Override
    public void testFailure(Failure failure) {
        mFailures.add(failure);
        // JUnit's ErrorReportingRunner calls testStart() in the case of InitializationError.
        if (failure.getException() instanceof InitializationError) {
            mClassesWithWrongRunner.remove(failure.getDescription().getClassName());
        }
    }

    @Override
    public void testFinished(Description desc) throws Exception {
        Bundle bundle = new Bundle();

        Class<?> curClass = desc.getTestClass();
        if (!curClass.equals(mActiveTestClass)) {
            bundle.putString("class", curClass.getName());
            bundle.putString(
                    "class_annotations",
                    getAnnotationJSON(Arrays.asList(curClass.getAnnotations())).toString());
            mActiveTestClass = curClass;
        }
        bundle.putString("method", desc.getMethodName());
        bundle.putString("method_annotations", getAnnotationJSON(desc.getAnnotations()).toString());

        sendStatus(STATUS_CODE, bundle);
    }

    /** Store the test method description to a Map at the beginning of a test run. */
    @Override
    public void testStarted(Description desc) throws Exception {
        if (mRequireBaseRunner) {
            // BaseJUnit4ClassRunner only fires testFinished(), so a call to
            // testStarted means a different runner is active, and the test is
            // actually being executed rather than just listed.
            mClassesWithWrongRunner.add(desc.getClassName());
        }
    }

    @Override
    public void instrumentationRunFinished(
            PrintStream streamResult, Bundle resultBundle, Result junitResults) {
        StringBuilder sb = new StringBuilder();
        if (!mFailures.isEmpty()) {
            sb.append("Failure during test listing:\n");
            for (Failure f : mFailures) {
                sb.append(f).append("\n");
            }
        }
        if (!mClassesWithWrongRunner.isEmpty()) {
            if (sb.length() > 0) {
                sb.append("\n");
            }
            sb.append(
                    "Found one or more tests that were not using "
                            + "@RunWith(BaseJUnit4ClassRunner.class) or a subclass thereof:\n");
            for (String name : mClassesWithWrongRunner) {
                sb.append("* ").append(name).append("\n");
            }
        }
        String errorMsg = sb.toString();
        if (!errorMsg.isEmpty()) {
            throw new RuntimeException(errorMsg);
        }
    }

    /**
     * Make a JSONObject dictionary out of annotations, keyed by the Annotation types' simple java
     * names.
     *
     * <p>For example, for the following group of annotations for ExampleClass <code>
     * @A
     * @B(message = "hello", level = 3)
     * public class ExampleClass() {}
     * </code> This method would return a JSONObject as such: <code>
     * {
     *   "A": {},
     *   "B": {
     *     "message": "hello",
     *     "level": "3"
     *   }
     * }
     * </code> The method accomplish this by though through each annotation and reflectively call
     * the annotation's method to get the element value, with exceptions to methods like "equals()"
     * or "hashCode".
     */
    private static JSONObject getAnnotationJSON(Collection<Annotation> annotations)
            throws IllegalAccessException, InvocationTargetException, JSONException {
        JSONObject result = new JSONObject();
        for (Annotation a : annotations) {
            JSONObject aJSON = (JSONObject) asJSON(a);
            String aType = aJSON.keys().next();
            result.put(aType, aJSON.get(aType));
        }
        return result;
    }

    /**
     * Recursively serialize an Annotation or an Annotation field value to a JSON compatible type.
     */
    private static Object asJSON(Object obj)
            throws IllegalAccessException, InvocationTargetException, JSONException {
        // Use instanceof to determine if it is an Annotation.
        // obj.getClass().isAnnotation() doesn't work as expected because
        // obj.getClass() returns a proxy class.
        if (obj instanceof Annotation) {
            Class<? extends Annotation> annotationType = ((Annotation) obj).annotationType();
            JSONObject json = new JSONObject();
            for (Method method : annotationType.getMethods()) {
                if (SKIP_METHODS.contains(method.getName())) {
                    continue;
                }
                json.put(method.getName(), asJSON(method.invoke(obj)));
            }
            JSONObject outerJson = new JSONObject();
            // If proguard is enabled and InnerClasses attribute is not kept,
            // then getCanonicalName() will return Outer$Inner instead of
            // Outer.Inner.  So just use getName().
            outerJson.put(
                    annotationType
                            .getName()
                            .replaceFirst(annotationType.getPackage().getName() + ".", ""),
                    json);
            return outerJson;
        } else {
            Class<?> clazz = obj.getClass();
            if (clazz.isArray()) {
                JSONArray jarr = new JSONArray();
                for (int i = 0; i < Array.getLength(obj); i++) {
                    jarr.put(asJSON(Array.get(obj, i)));
                }
                return jarr;
            } else {
                return obj;
            }
        }
    }
}
