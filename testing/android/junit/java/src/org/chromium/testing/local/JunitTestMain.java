// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import org.junit.runner.Computer;
import org.junit.runner.JUnitCore;
import org.junit.runner.Request;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.regex.Pattern;

/**
 *  Runs tests based on JUnit from the classpath on the host JVM based on the
 *  provided filter configurations.
 */
public final class JunitTestMain {
    /** Enforced by {@link org.chromium.tools.errorprone.plugin.TestClassNameCheck}. */
    private static final String TEST_CLASS_FILE_END = "Test.class";
    private static final String CLASS_FILE_EXT = ".class";
    private static final Pattern COLON = Pattern.compile(":");
    private static final Pattern FORWARD_SLASH = Pattern.compile("/");

    private JunitTestMain() {
    }

    /**
     *  Finds all test classes on the class path annotated with RunWith.
     */
    public static Class[] findClassesFromClasspath() {
        String[] jarPaths = COLON.split(System.getProperty("java.class.path"));
        List<Class> classes = new ArrayList<Class>();
        for (String jp : jarPaths) {
            try {
                JarFile jf = new JarFile(jp);
                for (Enumeration<JarEntry> eje = jf.entries(); eje.hasMoreElements();) {
                    JarEntry je = eje.nextElement();
                    String cn = je.getName();
                    if (!cn.endsWith(TEST_CLASS_FILE_END) || cn.indexOf('$') != -1) {
                        continue;
                    }
                    cn = cn.substring(0, cn.length() - CLASS_FILE_EXT.length());
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
                    className, /*initialize*/ false, JunitTestMain.class.getClassLoader());
        } catch (ClassNotFoundException e) {
            System.err.println("Class not found: " + className);
        } catch (NoClassDefFoundError e) {
            System.err.println("Class definition not found: " + className);
        } catch (Exception e) {
            System.err.println("Other exception while reading class: " + className);
        }
        return null;
    }

    public static void main(String[] args) {
        JunitTestArgParser parser = JunitTestArgParser.parse(args);

        JUnitCore core = new JUnitCore();
        Class[] classes = findClassesFromClasspath();

        Computer computer;
        if (parser.isListTests()) {
            // Causes test names to have the sdk version as a [suffix].
            System.setProperty("robolectric.alwaysIncludeVariantMarkersInTestName", "true");
            computer = new TestListComputer(System.out);
        } else {
            GtestLogger gtestLogger = new GtestLogger(System.out);
            core.addListener(new GtestListener(gtestLogger));
            JsonLogger jsonLogger = new JsonLogger(parser.getJsonOutputFile());
            core.addListener(new JsonListener(jsonLogger));
            computer = new GtestComputer(gtestLogger);
        }

        Request testRequest = Request.classes(computer, classes);
        for (String packageFilter : parser.getPackageFilters()) {
            testRequest = testRequest.filterWith(new PackageFilter(packageFilter));
        }
        for (Class<?> runnerFilter : parser.getRunnerFilters()) {
            testRequest = testRequest.filterWith(new RunnerFilter(runnerFilter));
        }
        for (String gtestFilter : parser.getGtestFilters()) {
            testRequest = testRequest.filterWith(new GtestFilter(gtestFilter));
        }
        System.exit(core.run(testRequest).wasSuccessful() ? 0 : 1);
    }
}
