/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.support.test.internal.runner;

import android.util.Log;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import org.junit.runner.Description;
import org.junit.runner.notification.Failure;

/**
 * A class for loading JUnit3 and JUnit4 test classes given a set of potential class names.
 */
public final class TestLoader {

    private static final String LOG_TAG = "TestLoader";

    private Map<String, Class<?>> mLoadedClassesMap = new LinkedHashMap<String, Class<?>>();
    private Map<String, Failure> mLoadFailuresMap = new LinkedHashMap<String, Failure>();

    private ClassLoader mClassLoader;

    /**
     * Set the {@link ClassLoader} to be used to load test cases.
     *
     * @param loader {@link ClassLoader} to load test cases with.
     */
    public void setClassLoader(ClassLoader loader) {
        mClassLoader = loader;
    }

    /**
     * Loads the test class from a given class name if its not already loaded.
     * <p/>
     * Will store the result internally. Successfully loaded classes can be retrieved via
     * {@link #getLoadedClasses()}, failures via {@link #getLoadFailures()}.
     *
     * @param className the class name to attempt to load
     * @return the loaded class or null.
     */
    public Class<?> loadClass(String className) {
        Class<?> loadedClass = doLoadClass(className);
        if (loadedClass != null) {
            ClassLoader loader = loadedClass.getClassLoader();
            Method[] methods = loadedClass.getDeclaredMethods();
            mLoadedClassesMap.put(className, loadedClass);
        }
        return loadedClass;
    }

    protected ClassLoader getClassLoader() {
        if (mClassLoader != null) {
            return mClassLoader;
        }

        // TODO: InstrumentationTestRunner uses
        // Class.forName(className, false, getTargetContext().getClassLoader());
        // Evaluate if that is needed. Initial testing indicates
        // getTargetContext().getClassLoader() == this.getClass().getClassLoader()
        return this.getClass().getClassLoader();
    }

    private Class<?> doLoadClass(String className) {
        if (mLoadFailuresMap.containsKey(className)) {
            // Don't load classes that already failed to load
            return null;
        } else if (mLoadedClassesMap.containsKey(className)) {
            // Class with the same name was already loaded, return it
            return mLoadedClassesMap.get(className);
        }

        try {
            ClassLoader myClassLoader = getClassLoader();
            return Class.forName(className, false, myClassLoader);
        } catch (ClassNotFoundException e) {
            String errMsg = String.format("Could not find class: %s", className);
            Log.e(LOG_TAG, errMsg);
            Description description = Description.createSuiteDescription(className);
            Failure failure = new Failure(description, e);
            mLoadFailuresMap.put(className, failure);
        }
        return null;
    }

    /**
     * Loads the test class from the given class name.
     * <p/>
     * Similar to {@link #loadClass(String)}, but will ignore classes that are
     * not tests.
     *
     * @param className the class name to attempt to load
     * @return the loaded class or null.
     */
    public Class<?> loadIfTest(String className) {
        Class<?> loadedClass = doLoadClass(className);
        if (loadedClass != null && isTestClass(loadedClass)) {
            mLoadedClassesMap.put(className, loadedClass);
            return loadedClass;
        }
        return null;
    }

    /**
     * @return whether this {@link TestLoader} contains any loaded classes or load failures.
     */
    public boolean isEmpty() {
        return mLoadedClassesMap.isEmpty() && mLoadFailuresMap.isEmpty();
    }

    /**
     * Get the {@link Collection) of classes successfully loaded via
     * {@link #loadIfTest(String)} calls.
     */
    public Collection<Class<?>> getLoadedClasses() {
        return mLoadedClassesMap.values();
    }

    /**
     * Get the {@link List) of {@link Failure} that occurred during
     * {@link #loadIfTest(String)} calls.
     */
    public Collection<Failure> getLoadFailures() {
        return mLoadFailuresMap.values();
    }

    /**
     * Determines if given class is a valid test class.
     *
     * @param loadedClass
     * @return <code>true</code> if loadedClass is a test
     */
    private boolean isTestClass(Class<?> loadedClass) {
        try {
            if (Modifier.isAbstract(loadedClass.getModifiers())) {
                logDebug(String.format("Skipping abstract class %s: not a test",
                        loadedClass.getName()));
                return false;
            }
            // TODO: try to find upstream junit calls to replace these checks
            if (junit.framework.Test.class.isAssignableFrom(loadedClass)) {
                // ensure that if a TestCase, it has at least one test method otherwise
                // TestSuite will throw error
                if (junit.framework.TestCase.class.isAssignableFrom(loadedClass)) {
                    return hasJUnit3TestMethod(loadedClass);
                }
                return true;
            }
            // TODO: look for a 'suite' method?
            if (loadedClass.isAnnotationPresent(org.junit.runner.RunWith.class)) {
                return true;
            }
            for (Method testMethod : loadedClass.getMethods()) {
                if (testMethod.isAnnotationPresent(org.junit.Test.class)) {
                    return true;
                }
            }
            logDebug(String.format("Skipping class %s: not a test", loadedClass.getName()));
            return false;
        } catch (Exception e) {
            // Defensively catch exceptions - Will throw runtime exception if it cannot load methods.
            // For earlier versions of Android (Pre-ICS), Dalvik might try to initialize a class
            // during getMethods(), fail to do so, hide the error and throw a NoSuchMethodException.
            // Since the java.lang.Class.getMethods does not declare such an exception, resort to a
            // generic catch all.
            // For ICS+, Dalvik will throw a NoClassDefFoundException.
            Log.w(LOG_TAG, String.format("%s in isTestClass for %s", e.toString(),
                    loadedClass.getName()));
            return false;
        } catch (Error e) {
            // defensively catch Errors too
            Log.w(LOG_TAG, String.format("%s in isTestClass for %s", e.toString(),
                    loadedClass.getName()));
            return false;
        }
    }

    private boolean hasJUnit3TestMethod(Class<?> loadedClass) {
        for (Method testMethod : loadedClass.getMethods()) {
            if (isPublicTestMethod(testMethod)) {
                return true;
            }
        }
        return false;
    }

    // copied from junit.framework.TestSuite
    private boolean isPublicTestMethod(Method m) {
        return isTestMethod(m) && Modifier.isPublic(m.getModifiers());
    }

    // copied from junit.framework.TestSuite
    private boolean isTestMethod(Method m) {
        return m.getParameterTypes().length == 0 && m.getName().startsWith("test")
                && m.getReturnType().equals(Void.TYPE);
    }

    /**
     * Utility method for logging debug messages. Only actually logs a message if LOG_TAG is marked
     * as loggable to limit log spam during normal use.
     */
    private void logDebug(String msg) {
        if (Log.isLoggable(LOG_TAG, Log.DEBUG)) {
            Log.d(LOG_TAG, msg);
        }
    }
}
