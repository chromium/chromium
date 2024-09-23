// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.runner.Computer;
import org.junit.runner.Description;
import org.junit.runner.Runner;
import org.junit.runner.manipulation.Filter;
import org.junit.runner.manipulation.Filterable;
import org.junit.runner.manipulation.NoTestsRemainException;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.RunnerBuilder;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.internal.bytecode.SandboxConfig;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

class TestListComputer extends Computer {
    private final List<Description> mDescriptions = new ArrayList<>();
    private final Allowlist mShadowsAllowlist;

    TestListComputer(Allowlist shadowsAllowlist) {
        mShadowsAllowlist = shadowsAllowlist;
    }

    private String computeConfig(
            Description description,
            Set<String> instrumentedPackages,
            Set<String> instrumentedClasses) {
        // Cache key for the ClassLoaders is in SandboxFactory.getSdkEnvironment:
        // https://cs.android.com/android/platform/superproject/main/+/main:external/robolectric-shadows/robolectric/src/main/java/org/robolectric/internal/SandboxFactory.java?q=symbol%3A%5Cborg.robolectric.internal.SandboxFactory.getSdkEnvironment%5Cb%20case%3Ayes
        List<Class<?>> shadows = new ArrayList<>();
        LooperMode.Mode looperMode = Mode.PAUSED;
        ArrayList allAnnotations = new ArrayList();
        allAnnotations.addAll(Arrays.asList(description.getTestClass().getAnnotations()));
        allAnnotations.addAll(description.getAnnotations());
        for (var annotation : allAnnotations) {
            if (annotation instanceof SandboxConfig) {
                shadows.addAll(Arrays.asList(((SandboxConfig) annotation).shadows()));
            } else if (annotation instanceof Config) {
                shadows.addAll(Arrays.asList(((Config) annotation).shadows()));
                instrumentedPackages.addAll(
                        Arrays.asList(((Config) annotation).instrumentedPackages()));
            } else if (annotation instanceof LooperMode) {
                looperMode = ((LooperMode) annotation).value();
            }
        }
        for (var clazz : shadows) {
            Implements annotation = clazz.getAnnotation(Implements.class);
            if (annotation != null) {
                String className = annotation.className();
                if (className.isEmpty()) {
                    className = annotation.value().getName();
                }
                if (!mShadowsAllowlist.allow(className)) {
                    throwShadowException(clazz.getName(), className);
                }
                instrumentedClasses.add(className);
            }
        }
        String methodName = description.getMethodName();
        String sdkSuffix = "";
        // Note: parameterized tests can look like: "FooTest.testFoo[28][6]", where the second [] is
        // the parameterization.
        int startIdx = methodName.indexOf('[');
        if (startIdx != -1) {
            int endIdx = methodName.indexOf(']', startIdx);
            if (endIdx != -1) {
                sdkSuffix = methodName.substring(startIdx, endIdx + 1);
            }
        }
        return looperMode + sdkSuffix;
    }

    private void throwShadowException(String shadowClass, String shadowingClass) {
        String msg =
                """

            Found non-allowlisted Robolectric shadow: %s (shadowing %s).
            Please limit usage of shadows to non-application code by adding explicit test stubbing \
            logic via set*ForTesting() methods.

            See: https://chromium.googlesource.com/chromium/src/+/main/styleguide/java/java.md#testing
            Used allowlist: %s
            """
                        .formatted(shadowClass, shadowingClass, mShadowsAllowlist.getFilename());
        throw new RuntimeException(msg);
    }

    private class TestListRunner extends Runner implements Filterable {
        private final Runner mRunner;

        public TestListRunner(Runner contained) {
            mRunner = contained;
        }

        @Override
        public Description getDescription() {
            return mRunner.getDescription();
        }

        private void collectDescriptions(Description description) {
            if (description.getMethodName() != null) {
                mDescriptions.add(description);
            }
            for (Description child : description.getChildren()) {
                collectDescriptions(child);
            }
        }

        @Override
        public void run(RunNotifier notifier) {
            collectDescriptions(mRunner.getDescription());
        }

        @Override
        public void filter(Filter filter) throws NoTestsRemainException {
            if (mRunner instanceof Filterable) {
                ((Filterable) mRunner).filter(filter);
            }
        }
    }

    @Override
    public Runner getSuite(final RunnerBuilder builder, Class<?>[] classes)
            throws InitializationError {
        return super.getSuite(
                new RunnerBuilder() {
                    @Override
                    public Runner runnerForClass(Class<?> testClass) throws Throwable {
                        return new TestListRunner(builder.runnerForClass(testClass));
                    }
                },
                classes);
    }

    private static JSONObject getOrNewObject(JSONObject parent, String key) throws JSONException {
        JSONObject ret = parent.optJSONObject(key);
        if (ret == null) {
            ret = new JSONObject();
            parent.put(key, ret);
        }
        return ret;
    }

    private static JSONArray getOrNewArray(JSONObject parent, String key) throws JSONException {
        JSONArray ret = parent.optJSONArray(key);
        if (ret == null) {
            ret = new JSONArray();
            parent.put(key, ret);
        }
        return ret;
    }

    public void writeJson(File outputFile) throws FileNotFoundException, JSONException {
        try (PrintStream stream = new PrintStream(new FileOutputStream(outputFile))) {
            stream.print(createJson());
        }
    }

    JSONObject createJson() throws JSONException {
        var instrumentedPackages = new TreeSet<String>();
        var instrumentedClasses = new TreeSet<String>();
        JSONObject root = new JSONObject();

        JSONObject configsObj = new JSONObject();
        root.put("configs", configsObj);
        for (Description d : mDescriptions) {
            String config = computeConfig(d, instrumentedPackages, instrumentedClasses);
            JSONObject configObj = getOrNewObject(configsObj, config);
            JSONArray methodsArr = getOrNewArray(configObj, d.getClassName());
            methodsArr.put(d.getMethodName());
        }

        JSONArray arr = new JSONArray();
        root.put("instrumentedPackages", arr);
        for (String s : instrumentedPackages) {
            arr.put(s);
        }

        arr = new JSONArray();
        root.put("instrumentedClasses", arr);
        for (String s : instrumentedClasses) {
            arr.put(s);
        }
        return root;
    }
}
