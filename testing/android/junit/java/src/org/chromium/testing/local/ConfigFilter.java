// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.runner.Description;
import org.junit.runner.manipulation.Filter;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

/** Filters tests to only those listed in the JSON config. */
class ConfigFilter extends Filter {
    private final Map<String, Set<String>> mTestsByClass;

    public ConfigFilter(JSONObject configJson) throws JSONException {
        Map<String, Set<String>> testsByClass = new HashMap<>();
        JSONObject configsObj = configJson.getJSONObject("configs");
        JSONObject classesObj = configsObj.getJSONObject(configsObj.keys().next());
        Iterator<String> classNames = classesObj.keys();
        while (classNames.hasNext()) {
            String className = classNames.next();
            JSONArray methodNamesArr = classesObj.getJSONArray(className);
            Set<String> methodsSet = new HashSet<>(methodNamesArr.length());
            for (int i = 0, len = methodNamesArr.length(); i < len; ++i) {
                methodsSet.add(methodNamesArr.getString(i));
            }
            testsByClass.put(className, methodsSet);
        }
        mTestsByClass = testsByClass;
    }

    static Class[] classesFromConfig(JSONObject configJson)
            throws JSONException, ClassNotFoundException {
        JSONObject configsObj = configJson.getJSONObject("configs");
        if (configsObj.length() != 1) {
            throw new IllegalArgumentException(
                    "JSON Config had " + configsObj.length() + " entries");
        }
        JSONObject classesObj = configsObj.getJSONObject(configsObj.keys().next());
        Class[] ret = new Class[classesObj.length()];
        int i = 0;
        Iterator<String> classNames = classesObj.keys();
        ClassLoader classLoader = JunitTestMain.class.getClassLoader();
        while (classNames.hasNext()) {
            ret[i++] = Class.forName(classNames.next(), false, classLoader);
        }
        return ret;
    }

    @Override
    public boolean shouldRun(Description description) {
        if (description.getMethodName() == null) {
            return true;
        }
        Set<String> methodsSet = mTestsByClass.get(description.getClassName());
        if (methodsSet == null) {
            return false;
        }
        return methodsSet.contains(description.getMethodName());
    }

    @Override
    public String describe() {
        return "JSON Config filter";
    }
}
