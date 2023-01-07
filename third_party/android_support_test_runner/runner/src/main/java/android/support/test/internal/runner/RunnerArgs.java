package android.support.test.internal.runner;

import android.app.Instrumentation;
import android.content.pm.InstrumentationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.test.runner.lifecycle.ApplicationLifecycleCallback;
import android.util.Log;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import org.junit.runner.notification.RunListener;

/**
 * Contains input arguments passed to the instrumentation test runner.
 */
public class RunnerArgs {
    private static final String LOG_TAG = "RunnerArgs";

    // constants for supported instrumentation arguments
    static final String ARGUMENT_TEST_CLASS = "class";
    static final String ARGUMENT_NOT_TEST_CLASS = "notClass";
    static final String ARGUMENT_TEST_SIZE = "size";
    static final String ARGUMENT_LOG_ONLY = "log";
    static final String ARGUMENT_ANNOTATION = "annotation";
    static final String ARGUMENT_NOT_ANNOTATION = "notAnnotation";
    static final String ARGUMENT_NUM_SHARDS = "numShards";
    static final String ARGUMENT_SHARD_INDEX = "shardIndex";
    static final String ARGUMENT_DELAY_IN_MILLIS = "delay_msec";
    static final String ARGUMENT_COVERAGE = "coverage";
    static final String ARGUMENT_COVERAGE_PATH = "coverageFile";
    static final String ARGUMENT_SUITE_ASSIGNMENT = "suiteAssignment";
    static final String ARGUMENT_DEBUG = "debug";
    static final String ARGUMENT_LISTENER = "listener";
    static final String ARGUMENT_TEST_PACKAGE = "package";
    static final String ARGUMENT_NOT_TEST_PACKAGE = "notPackage";
    static final String ARGUMENT_TIMEOUT = "timeout_msec";
    static final String ARGUMENT_TEST_FILE = "testFile";
    static final String ARGUMENT_DISABLE_ANALYTICS = "disableAnalytics";
    static final String ARGUMENT_APP_LISTENER = "appListener";
    // TODO: consider supporting 'count' from InstrumentationTestRunner

    // used to separate multiple fully-qualified test case class names
    private static final char CLASS_SEPARATOR = ',';
    // used to separate fully-qualified test case class name, and one of its methods
    private static final char METHOD_SEPARATOR = '#';

    public final boolean debug;
    public final boolean suiteAssignment;
    public final boolean codeCoverage;
    public final String codeCoveragePath;
    public final int delayInMillis;
    public final boolean logOnly;
    public final List<String> testPackages;
    public final List<String> notTestPackages;
    public final String testSize;
    public final String annotation;
    public final List<String> notAnnotations;
    public final long testTimeout;
    public final List<RunListener> listeners;
    public final List<TestArg> tests;
    public final List<TestArg> notTests;
    public final int numShards;
    public final int shardIndex;
    public final boolean disableAnalytics;
    public final List<ApplicationLifecycleCallback> appListeners;

    /**
     * Encapsulates a test class and optional method
     */
    public static class TestArg {
        public final String testClassName;
        public final String methodName;

        TestArg(String className, String methodName) {
            this.testClassName = className;
            this.methodName = methodName;
        }

        TestArg(String className) {
            this(className, null);
        }
    }

    private RunnerArgs(Builder builder) {
        this.debug = builder.debug;
        this.suiteAssignment = builder.suiteAssignment;
        this.codeCoverage = builder.codeCoverage;
        this.codeCoveragePath = builder.codeCoveragePath;
        this.delayInMillis = builder.delayInMillis;
        this.logOnly = builder.logOnly;
        this.testPackages = builder.testPackages;
        this.notTestPackages = builder.notTestPackages;
        this.testSize = builder.testSize;
        this.annotation = builder.annotation;
        this.notAnnotations = Collections.unmodifiableList(builder.notAnnotations);
        this.testTimeout = builder.testTimeout;
        this.listeners = Collections.unmodifiableList(builder.listeners);
        this.tests = Collections.unmodifiableList(builder.tests);
        this.notTests = Collections.unmodifiableList(builder.notTests);
        this.numShards = builder.numShards;
        this.shardIndex = builder.shardIndex;
        this.disableAnalytics = builder.disableAnalytics;
        this.appListeners = Collections.unmodifiableList(builder.appListeners);
    }

    public static class Builder {
        private boolean debug = false;
        private boolean suiteAssignment = false;
        private boolean codeCoverage = false;
        private String codeCoveragePath = null;
        private int delayInMillis = -1;
        private boolean logOnly = false;
        private List<String> testPackages = new ArrayList<>();
        private List<String> notTestPackages = new ArrayList<>();
        private String testSize = null;
        private String annotation = null;
        private List<String> notAnnotations = new ArrayList<String>();
        private long testTimeout = -1;
        private List<RunListener> listeners = new ArrayList<RunListener>();
        private List<TestArg> tests = new ArrayList<>();
        private List<TestArg> notTests = new ArrayList<>();
        private int numShards = 0;
        private int shardIndex = 0;
        private boolean disableAnalytics = false;
        private List<ApplicationLifecycleCallback> appListeners =
                new ArrayList<ApplicationLifecycleCallback>();

        /**
         * Populate the arg data from given Bundle
         */
        public Builder fromBundle(Bundle bundle) {
            this.debug = parseBoolean(bundle.getString(ARGUMENT_DEBUG));
            this.delayInMillis =
                    parseUnsignedInt(bundle.get(ARGUMENT_DELAY_IN_MILLIS), ARGUMENT_DELAY_IN_MILLIS);
            this.tests.addAll(parseTestClasses(bundle.getString(ARGUMENT_TEST_CLASS)));
            this.tests.addAll(parseTestClassesFromFile(bundle.getString(ARGUMENT_TEST_FILE)));
            this.notTests.addAll(parseTestClasses(bundle.getString(ARGUMENT_NOT_TEST_CLASS)));
            this.listeners.addAll(parseAndLoadClasses(bundle.getString(ARGUMENT_LISTENER),
                    RunListener.class));
            this.testPackages.addAll(parseTestPackages(bundle.getString(ARGUMENT_TEST_PACKAGE)));
            this.notTestPackages.addAll(parseTestPackages(bundle.getString(ARGUMENT_NOT_TEST_PACKAGE)));
            this.testSize = bundle.getString(ARGUMENT_TEST_SIZE);
            this.annotation = bundle.getString(ARGUMENT_ANNOTATION);
            this.notAnnotations.addAll(parseStrings(bundle.getString(ARGUMENT_NOT_ANNOTATION)));
            this.testTimeout = parseUnsignedLong(bundle.getString(ARGUMENT_TIMEOUT), ARGUMENT_TIMEOUT);
            this.numShards = parseUnsignedInt(bundle.get(ARGUMENT_NUM_SHARDS), ARGUMENT_NUM_SHARDS);
            this.shardIndex = parseUnsignedInt(bundle.get(ARGUMENT_SHARD_INDEX), ARGUMENT_SHARD_INDEX);
            this.logOnly = parseBoolean(bundle.getString(ARGUMENT_LOG_ONLY));
            this.disableAnalytics = parseBoolean(bundle.getString(ARGUMENT_DEBUG));
            this.appListeners.addAll(parseAndLoadClasses(bundle.getString(ARGUMENT_APP_LISTENER),
                    ApplicationLifecycleCallback.class));
            this.codeCoverage = parseBoolean(bundle.getString(ARGUMENT_COVERAGE));
            this.codeCoveragePath = bundle.getString(ARGUMENT_COVERAGE_PATH);
            this.suiteAssignment = parseBoolean(bundle.getString(ARGUMENT_SUITE_ASSIGNMENT));
            return this;
        }

        /**
         * Populate the arg data from the instrumentation:metadata attribute in Manifest
         */
        public Builder fromManifest(Instrumentation instr) {
            PackageManager pm = instr.getContext().getPackageManager();
            try {
                InstrumentationInfo instrInfo = pm.getInstrumentationInfo(instr.getComponentName(),
                        PackageManager.GET_META_DATA);
                Bundle b = instrInfo.metaData;
                if (b == null) {
                    // metadata not present - skip
                    return this;
                }
                // parse the metadata using same key names
                return fromBundle(b);
            } catch (PackageManager.NameNotFoundException e) {
                // should never happen
                Log.wtf(LOG_TAG, String.format("Could not find component %s",
                        instr.getComponentName()));
            }
            return this;
        }


        /**
         * Utility method to split String element data in CSV format into a List
         *
         * @return empty list if null input, otherwise list of strings
         */
        private List<String> parseStrings(String value) {
            if (value == null) {
                return Collections.emptyList();
            }
            return Arrays.asList(value.split(","));
        }

        /**
         * Parse boolean value from a String
         *
         * @return the boolean value, false on null input
         */
        private boolean parseBoolean(String booleanValue) {
            return booleanValue != null && Boolean.parseBoolean(booleanValue);
        }

        /**
         * Parse int from given value - except either int or string
         *
         * @return the value, -1 if not found
         * @throws NumberFormatException if value is negative or not a number
         */
        private int parseUnsignedInt(Object value, String name) {
            if (value != null) {
                int intValue = Integer.parseInt(value.toString());
                if (intValue < 0) {
                    throw new NumberFormatException(name + " can not be negative");
                }

                return intValue;
            }
            return -1;
        }

        /**
         * Parse long from given value - except either Long or String
         *
         * @return the value, -1 if not found
         * @throws NumberFormatException if value is negative or not a number
         */
        private long parseUnsignedLong(Object value, String name) {
            if (value != null) {
                long longValue = Long.parseLong(value.toString());
                if (longValue < 0) {
                    throw new NumberFormatException(name + " can not be negative");
                }
                return longValue;
            }
            return -1;
        }

        /**
         * Parse test package data from given CSV data in following format
         * com.android.foo,com.android.bar,...
         *
         * @return list of package names, empty list if input is null
         */
        private List<String> parseTestPackages(String packagesArg) {
            List<String> packages = new ArrayList<>();
            if (packagesArg != null) {
                for (String packageName : packagesArg.split(String.valueOf(CLASS_SEPARATOR))) {
                    packages.add(packageName);
                }
            }
            return packages;
        }

        /**
         * Parse test class and method data from given CSV data in following format
         * com.TestClass1#method1,com.TestClass2,...
         *
         * @return list of TestArg data, empty list if input is null
         */
        private List<TestArg> parseTestClasses(String classesArg) {
            List<TestArg> tests = new ArrayList<TestArg>();
            if (classesArg != null) {
                for (String className : classesArg.split(String.valueOf(CLASS_SEPARATOR))) {
                    tests.add(parseTestClass(className));
                }
            }
            return tests;
        }

        /**
         * Parse an individual test class and optionally method from given string
         * <p/>
         * Expected format: com.TestClass1[#method1]
         */
        private TestArg parseTestClass(String testClassName) {
            int methodSeparatorIndex = testClassName.indexOf(METHOD_SEPARATOR);
            if (methodSeparatorIndex > 0) {
                String testMethodName = testClassName.substring(methodSeparatorIndex + 1);
                testClassName = testClassName.substring(0, methodSeparatorIndex);
                return new TestArg(testClassName, testMethodName);
            } else {
                return new TestArg(testClassName);
            }
        }

        /**
         * Parse and load the content of a test file
         *
         * @param filePath path to test file containing full package names of test classes and
         *                 optionally methods to add.
         */
        private List<TestArg> parseTestClassesFromFile(String filePath) {
            List<TestArg> tests = new ArrayList<TestArg>();
            if (filePath == null) {
                return tests;
            }
            BufferedReader br = null;
            String line;
            try {
                br = new BufferedReader(new FileReader(new File(filePath)));
                while ((line = br.readLine()) != null) {
                    tests.add(parseTestClass(line));
                }
            } catch (FileNotFoundException e) {
                throw new IllegalArgumentException("testfile not found: " + filePath);
            } catch (IOException e) {
                throw new IllegalArgumentException("Could not read testfile " + filePath, e);
            } finally {
                if (br != null) {
                    try {
                        br.close();
                    } catch (IOException e) { /* ignore */ }
                }
            }
            return tests;
        }

        /**
         * Create a set of objects given a CSV string of full class names and type
         *
         * @return the List of RunListeners or empty list on null input
         */
        private <T> List<T> parseAndLoadClasses(String classString, Class<T> type) {
            List<T> objects = new ArrayList<T>();
            if (classString != null) {
                for (String className : classString.split(",")) {
                    addByClassName(objects, className, type);
                }
            }
            return objects;
        }

        /**
         * Load and add object given class string.
         * <p/>
         * No effect if input is null or empty.
         * <p/>
         *
         * @param objects the List to add to
         * @param className the fully qualified class name\
         *
         * @throws IllegalArgumentException if listener cannot be loaded
         */
        private <T> void addByClassName(List<T> objects,
                                    String className, Class<T> type) {
            if (className == null || className.length() == 0) {
                return;
            }
            try {
                final Class<?> klass = Class.forName(className);
                klass.getConstructor().setAccessible(true);
                @SuppressWarnings("unchecked")
                final T l =  (T) klass.newInstance();
                objects.add(l);
            } catch (ClassNotFoundException e) {
                throw new IllegalArgumentException("Could not find extra class "
                        + className);
            } catch (NoSuchMethodException e) {
                throw new IllegalArgumentException("Must have no argument constructor for class "
                        + className);
            } catch (ClassCastException e) {
                throw new IllegalArgumentException(className + " does not extend " + type.getName()
                        );
            } catch (InstantiationException e) {
                throw new IllegalArgumentException("Failed to create: " + className, e);
            } catch (IllegalAccessException e) {
                throw new IllegalArgumentException("Failed to create listener: " + className, e);
            }
        }

        public RunnerArgs build() {
            return new RunnerArgs(this);
        }
    }
}
