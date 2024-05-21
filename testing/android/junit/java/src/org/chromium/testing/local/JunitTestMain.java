// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.runner.Computer;
import org.junit.runner.JUnitCore;
import org.junit.runner.Request;
import org.junit.runner.Result;
import org.junit.runner.RunWith;
import org.junit.runner.notification.RunListener;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import java.util.ServiceLoader;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.regex.Pattern;

/**
 *  Runs tests based on JUnit from the classpath on the host JVM based on the
 *  provided filter configurations.
 */
public final class JunitTestMain {
    private static final int CLASS_SUFFIX_LEN = ".class".length();
    private static final Pattern COLON = Pattern.compile(":");
    private static final Pattern FORWARD_SLASH = Pattern.compile("/");

    private JunitTestMain() {}

    /** ServiceLoader interface for adding RunListeners. */
    public interface ExtraRunListenerProvider {
        RunListener provideRunListener();
    }

    /** Finds all test classes on the class path annotated with RunWith. */
    public static Class[] findClassesFromClasspath() {
        String[] jarPaths = COLON.split(System.getProperty("java.class.path"));
        List<Class> classes = new ArrayList<Class>();
        for (String jp : jarPaths) {
            // Do not look at android.jar.
            if (jp.contains("third_party/android_sdk")) {
                continue;
            }
            try {
                JarFile jf = new JarFile(jp);
                for (Enumeration<JarEntry> eje = jf.entries(); eje.hasMoreElements(); ) {
                    JarEntry je = eje.nextElement();
                    String cn = je.getName();
                    // Skip classes in common libraries.
                    if (cn.startsWith("androidx.") || cn.startsWith("junit")) {
                        continue;
                    }
                    // Skip nested classes and classes that do not end with "Test".
                    // That tests end with "Test" is enforced by TestClassNameCheck ErrorProne
                    // check.
                    if (cn.contains("$") || !cn.endsWith("Test.class")) {
                        continue;
                    }
                    cn = cn.substring(0, cn.length() - CLASS_SUFFIX_LEN);
                    cn = FORWARD_SLASH.matcher(cn).replaceAll(".");
                    Class<?> c = classOrNull(cn);
                    if (c != null && c.isAnnotationPresent(RunWith.class)) {
                        classes.add(c);
                    }
                }
                jf.close();
            } catch (IOException e) {
                System.err.println("Error while reading classes from " + jp);
            }
        }
        return classes.toArray(new Class[0]);
    }

    private static Class<?> classOrNull(String className) {
        try {
            // Do not initialize classes (clinit) yet, Android methods are all
            // stubs until robolectric loads the real implementations.
            return Class.forName(
                    className, /* initialize= */ false, JunitTestMain.class.getClassLoader());
        } catch (ClassNotFoundException e) {
            System.err.println("Class not found: " + className);
        } catch (NoClassDefFoundError e) {
            System.err.println("Class definition not found: " + className);
        } catch (Exception e) {
            System.err.println("Other exception while reading class: " + className);
        }
        return null;
    }

    private static Result listTestMain(JunitTestArgParser parser)
            throws FileNotFoundException, JSONException {
        JUnitCore core = new JUnitCore();
        TestListComputer computer = new TestListComputer(parser.mShadowsAllowlist);
        Class[] classes = findClassesFromClasspath();
        Request testRequest = Request.classes(computer, classes);
        for (String packageFilter : parser.mPackageFilters) {
            testRequest = testRequest.filterWith(new PackageFilter(packageFilter));
        }
        for (Class<?> runnerFilter : parser.mRunnerFilters) {
            testRequest = testRequest.filterWith(new RunnerFilter(runnerFilter));
        }
        for (String gtestFilter : parser.mGtestFilters) {
            testRequest = testRequest.filterWith(new GtestFilter(gtestFilter));
        }
        Result ret = core.run(testRequest);
        computer.writeJson(new File(parser.mJsonConfig));
        return ret;
    }

    private static Result runTestsMain(JunitTestArgParser parser) throws Exception {
        String data = new String(Files.readAllBytes(Paths.get(parser.mJsonConfig)));
        JSONObject jsonConfig = new JSONObject(data);
        ChromiumAndroidConfigurer.setJsonConfig(jsonConfig);
        Class[] classes = ConfigFilter.classesFromConfig(jsonConfig);

        JUnitCore core = new JUnitCore();
        GtestLogger gtestLogger = new GtestLogger(System.out);
        core.addListener(new GtestListener(gtestLogger));
        JsonLogger jsonLogger = new JsonLogger(new File(parser.mJsonOutput));
        core.addListener(new JsonListener(jsonLogger));
        Computer computer = new GtestComputer(gtestLogger);

        for (ExtraRunListenerProvider listenerProvider :
                ServiceLoader.load(ExtraRunListenerProvider.class)) {
            core.addListener(listenerProvider.provideRunListener());
        }

        Request testRequest =
                Request.classes(computer, classes).filterWith(new ConfigFilter(jsonConfig));
        return core.run(testRequest);
    }

    public static void main(String[] args) throws Exception {
        // Causes test names to have the sdk version as a [suffix].
        // This enables sharding by SDK version.
        System.setProperty("robolectric.alwaysIncludeVariantMarkersInTestName", "true");

        JunitTestArgParser parser = JunitTestArgParser.parse(args);
        Result r = parser.mListTests ? listTestMain(parser) : runTestsMain(parser);
        System.exit(r.wasSuccessful() ? 0 : 1);
    }
}
