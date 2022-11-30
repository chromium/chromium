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

import android.app.Instrumentation;
import android.os.Bundle;
import android.support.test.filters.RequiresDevice;
import android.support.test.filters.SdkSuppress;
import android.support.test.internal.runner.ClassPathScanner.ChainedClassNameFilter;
import android.support.test.internal.runner.ClassPathScanner.ExcludePackageNameFilter;
import android.support.test.internal.runner.ClassPathScanner.ExternalClassNameFilter;
import android.support.test.internal.runner.ClassPathScanner.InclusivePackageNameFilter;
import android.support.test.internal.util.AndroidRunnerParams;
import android.support.test.internal.util.Checks;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.test.suitebuilder.annotation.Suppress;
import android.util.Log;
import java.io.IOException;
import java.lang.annotation.Annotation;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Pattern;
import org.junit.runner.Computer;
import org.junit.runner.Description;
import org.junit.runner.Request;
import org.junit.runner.Runner;
import org.junit.runner.manipulation.Filter;
import org.junit.runner.manipulation.NoTestsRemainException;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.model.InitializationError;

/**
 * A builder for {@link TestRequest} that builds up tests to run, filtered on provided set of
 * restrictions.
 */
public class TestRequestBuilder {

    private static final String LOG_TAG = "TestRequestBuilder";

    public static final String LARGE_SIZE = "large";
    public static final String MEDIUM_SIZE = "medium";
    public static final String SMALL_SIZE = "small";

    static final String EMULATOR_HARDWARE = "goldfish";

    // Excluded test packages
    private static final String[] DEFAULT_EXCLUDED_PACKAGES = {
            "junit",
            "org.junit",
            "org.hamcrest",
            "org.mockito",// exclude Mockito for performance and to prevent JVM related errors
            "android.support.test.internal.runner.junit3",// always skip AndroidTestSuite
    };

    static final String MISSING_ARGUMENTS_MSG =
            "Must provide either classes to run, or apks to scan";
    static final String AMBIGUOUS_ARGUMENTS_MSG =
            "Ambiguous arguments: cannot provide both test package and test class(es) to run";

    private List<String> mApkPaths = new ArrayList<String>();
    private Set<String> mIncludedPackages = new HashSet<>();
    private Set<String> mExcludedPackages = new HashSet<>();
    private Set<String> mIncludedClasses = new HashSet<>();
    private Set<String> mExcludedClasses = new HashSet<>();
    private ClassAndMethodFilter mClassMethodFilter = new ClassAndMethodFilter();
    private Filter mFilter = new AnnotationExclusionFilter(Suppress.class)
            .intersect(new SdkSuppressFilter())
            .intersect(new RequiresDeviceFilter())
            .intersect(mClassMethodFilter);
    private boolean mSkipExecution = false;
    private final DeviceBuild mDeviceBuild;
    private long mPerTestTimeout = 0;
    private final Instrumentation mInstr;
    private final Bundle mArgsBundle;
    private ClassLoader mClassLoader;

    /**
     * Instructs the test builder if JUnit3 suite() methods should be executed.
     * <p/>
     * Currently set to false if any method filter is set, for consistency with
     * InstrumentationTestRunner.
      */
    private boolean mIgnoreSuiteMethods = false;

    /**
     * Accessor interface for retrieving device build properties.
     * <p/>
     * Used so unit tests can mock calls
     */
    static interface DeviceBuild {
        /**
         * Returns the SDK API level for current device.
         */
        int getSdkVersionInt();

        /**
         * Returns the hardware type of the current device.
         */
        String getHardware();
    }

    private static class DeviceBuildImpl implements DeviceBuild {
        @Override
        public int getSdkVersionInt() {
            return android.os.Build.VERSION.SDK_INT;
        }

        @Override
        public String getHardware() {
            return android.os.Build.HARDWARE;
        }
    }

    /**
     * Helper parent class for {@link Filter} that allows suites to run if any child matches.
     */
    private abstract static class ParentFilter extends Filter {
        /**
         * {@inheritDoc}
         */
        @Override
        public boolean shouldRun(Description description) {
            if (description.isTest()) {
                return evaluateTest(description);
            }
            // this is a suite, explicitly check if any children should run
            for (Description each : description.getChildren()) {
                if (shouldRun(each)) {
                    return true;
                }
            }
            // no children to run, filter this out
            return false;
        }

        /**
         * Determine if given test description matches filter.
         *
         * @param description the {@link Description} describing the test
         * @return <code>true</code> if matched
         */
        protected abstract boolean evaluateTest(Description description);
    }

    /**
     * Filter that only runs tests whose method or class has been annotated with given filter.
     */
    private static class AnnotationInclusionFilter extends ParentFilter {

        private final Class<? extends Annotation> mAnnotationClass;

        AnnotationInclusionFilter(Class<? extends Annotation> annotation) {
            mAnnotationClass = annotation;
        }

        /**
         * Determine if given test description matches filter.
         *
         * @param description the {@link Description} describing the test
         * @return <code>true</code> if matched
         */
        @Override
        protected boolean evaluateTest(Description description) {
            final Class<?> testClass = description.getTestClass();
            return description.getAnnotation(mAnnotationClass) != null ||
                    (testClass != null && testClass.isAnnotationPresent(mAnnotationClass));
        }

        protected Class<? extends Annotation> getAnnotationClass() {
            return mAnnotationClass;
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public String describe() {
            return String.format("annotation %s", mAnnotationClass.getName());
        }
    }

    /**
     * A filter for test sizes.
     * <p/>
     * Will match if test method has given size annotation, or class does, but only if method does
     * not have any other size annotations. ie method size annotation overrides class size
     * annotation.
     */
    private static class SizeFilter extends AnnotationInclusionFilter {
        @SuppressWarnings("unchecked")
        private static final Set<Class<?>> ALL_SIZES = Collections.unmodifiableSet(new
                HashSet<Class<?>>(Arrays.asList(SmallTest.class, MediumTest.class,
                        LargeTest.class)));

        SizeFilter(Class<? extends Annotation> annotation) {
            super(annotation);
        }

        @Override
        protected boolean evaluateTest(Description description) {
            final Class<?> testClass = description.getTestClass();
            if (description.getAnnotation(getAnnotationClass()) != null) {
                return true;
            } else if (testClass != null && testClass.isAnnotationPresent(getAnnotationClass())) {
                // size annotation matched at class level. Make sure method doesn't have any other
                // size annotations
                for (Annotation a : description.getAnnotations()) {
                    if (ALL_SIZES.contains(a.annotationType())) {
                        return false;
                    }
                }
                return true;
            }
            return false;
        }
    }

    /**
     * Filter out tests whose method or class has been annotated with given filter.
     */
    private static class AnnotationExclusionFilter extends ParentFilter {

        private final Class<? extends Annotation> mAnnotationClass;

        AnnotationExclusionFilter(Class<? extends Annotation> annotation) {
            mAnnotationClass = annotation;
        }

        @Override
        protected boolean evaluateTest(Description description) {
            final Class<?> testClass = description.getTestClass();
            if ((testClass != null && testClass.isAnnotationPresent(mAnnotationClass))
                    || (description.getAnnotation(mAnnotationClass) != null)) {
                return false;
            }
            return true;
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public String describe() {
            return String.format("not annotation %s", mAnnotationClass.getName());
        }
    }

    private class SdkSuppressFilter extends ParentFilter {

        @Override
        protected boolean evaluateTest(Description description) {
            final SdkSuppress s = getAnnotationForTest(description);
            if (s != null && getDeviceSdkInt() < s.minSdkVersion()) {
                return false;
            }
            return true;
        }

        private SdkSuppress getAnnotationForTest(Description description) {
            final SdkSuppress s = description.getAnnotation(SdkSuppress.class);
            if (s != null) {
                return s;
            }
            final Class<?> testClass = description.getTestClass();
            if (testClass != null) {
                return testClass.getAnnotation(SdkSuppress.class);
            }
            return null;
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public String describe() {
            return String.format("skip tests annotated with SdkSuppress if necessary");
        }
    }

    /**
     * Class that filters out tests annotated with {@link RequiresDevice} when running on emulator
     */
    private class RequiresDeviceFilter extends AnnotationExclusionFilter {

        RequiresDeviceFilter() {
            super(RequiresDevice.class);
        }

        @Override
        protected boolean evaluateTest(Description description) {
            if (!super.evaluateTest(description)) {
                // annotation is present - check if device is an emulator
                return !EMULATOR_HARDWARE.equals(getDeviceHardware());
            }
            return true;
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public String describe() {
            return String.format("skip tests annotated with RequiresDevice if necessary");
        }
    }

    private static class ShardingFilter extends Filter {
        private final int mNumShards;
        private final int mShardIndex;

        ShardingFilter(int numShards, int shardIndex) {
            mNumShards = numShards;
            mShardIndex = shardIndex;
        }

        @Override
        public boolean shouldRun(Description description) {
            if (description.isTest()) {
                return (Math.abs(description.hashCode()) % mNumShards) == mShardIndex;
            }
            // this is a suite, explicitly check if any children should run
            for (Description each : description.getChildren()) {
                if (shouldRun(each)) {
                    return true;
                }
            }
            // no children to run, filter this out
            return false;
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public String describe() {
            return String.format("Shard %s of %s shards", mShardIndex, mNumShards);
        }
    }

    /**
     * A {@link Request} that doesn't report an error if all tests are filtered out. Done for
     * consistency with InstrumentationTestRunner.
     */
    private static class LenientFilterRequest extends Request {
        private final Request mRequest;
        private final Filter mFilter;

        public LenientFilterRequest(Request classRequest, Filter filter) {
            mRequest = classRequest;
            mFilter = filter;
        }

        @Override
        public Runner getRunner() {
            try {
                Runner runner = mRequest.getRunner();
                mFilter.apply(runner);
                return runner;
            } catch (NoTestsRemainException e) {
                // don't treat filtering out all tests as an error
                return new BlankRunner();
            }
        }
    }

    /**
     * A {@link Runner} that doesn't do anything
     */
    private static class BlankRunner extends Runner {
        @Override
        public Description getDescription() {
            return Description.createSuiteDescription("no tests found");
        }

        @Override
        public void run(RunNotifier notifier) {
            // do nothing
        }
    }

    /**
     * A {@link Filter} to support the ability to filter out multiple class#method combinations.
     */
    private static class ClassAndMethodFilter extends Filter {

        private Map<String, MethodFilter> mMethodFilters = new HashMap<>();

        @Override
        public boolean shouldRun(Description description) {
            if (mMethodFilters.isEmpty()) {
                return true;
            }
            if (description.isTest()) {
                String className = description.getClassName();
                MethodFilter methodFilter = mMethodFilters.get(className);
                if (methodFilter != null) {
                    return methodFilter.shouldRun(description);
                }
            } else {
                // Check all children, if any
                for (Description child : description.getChildren()) {
                    if (shouldRun(child)) {
                        return true;
                    }
                }
            }
            return false;
        }

        @Override
        public String describe() {
            return "Class and method filter";
        }

        public void addMethod(String className, String methodName) {
            MethodFilter mf = mMethodFilters.get(className);
            if (mf == null) {
                mf = new MethodFilter(className);
                mMethodFilters.put(className, mf);
            }
            mf.add(methodName);
        }

        public void removeMethod(String className, String methodName) {
            MethodFilter mf = mMethodFilters.get(className);
            if (mf == null) {
                mf = new MethodFilter(className);
                mMethodFilters.put(className, mf);
            }
            mf.remove(methodName);
        }
    }

    /**
     * A {@link Filter} used to filter out desired test methods from a given class
     */
    private static class MethodFilter extends Filter {

        private final String mClassName;
        private Set<String> mIncludedMethods = new HashSet<>();
        private Set<String> mExcludedMethods = new HashSet<>();

        /**
         * Constructs a method filter for a given class
         * @param className  name of the class the method belongs to
         */
        public MethodFilter(String className) {
            mClassName = className;
        }

        @Override
        public String describe() {
            return "Method filter for " + mClassName + " class";
        }

        @Override
        public boolean shouldRun(Description description) {
            if (description.isTest()) {
                String methodName = description.getMethodName();
                // Parameterized tests append "[#]" at the end of the method names.
                // For instance, "getFoo" would become "getFoo[0]".
                methodName = stripParameterizedSuffix(methodName);
                if (mExcludedMethods.contains(methodName)) {
                    return false;
                }
                // don't filter out descriptions with method name "initializationError", since
                // Junit will generate such descriptions in error cases, See ErrorReportingRunner
                return mIncludedMethods.isEmpty() || mIncludedMethods.contains(methodName)
                    || methodName.equals("initializationError");
            }
            // At this point, this could only be a description of this filter
            return true;

        }

        // Strips out the parameterized suffix if it exists
        private String stripParameterizedSuffix(String name) {
            Pattern suffixPattern = Pattern.compile(".+(\\[[0-9]+\\])$");
            if (suffixPattern.matcher(name).matches()) {
                name = name.substring(0, name.lastIndexOf('['));
            }
            return name;
        }

        public void add(String methodName) {
            mIncludedMethods.add(methodName);
        }

        public void remove(String methodName) {
            mExcludedMethods.add(methodName);
        }
    }

    /**
     * Creates a TestRequestBuilder
     *
     * @param instr the {@link Instrumentation} to pass to applicable tests
     * @param bundle the {@link Bundle} to pass to applicable tests
     */
    public TestRequestBuilder(Instrumentation instr, Bundle bundle) {
        this(new DeviceBuildImpl(), instr, bundle);
    }

    /**
     * Alternate TestRequestBuilder constructor that accepts a custom DeviceBuild
     */
    // Visible For Testing
    TestRequestBuilder(DeviceBuild deviceBuildAccessor,Instrumentation instr, Bundle bundle) {
        mDeviceBuild = Checks.checkNotNull(deviceBuildAccessor);
        mInstr = Checks.checkNotNull(instr);
        mArgsBundle = Checks.checkNotNull(bundle);
    }

    /**
     * Instruct builder to scan given apk, and add all tests classes found. Cannot be used in
     * conjunction with addTestClass or addTestMethod is used.
     *
     * @param apkPath
     */
    public TestRequestBuilder addApkToScan(String apkPath) {
        mApkPaths.add(apkPath);
        return this;
    }

    /**
     * Set the {@link ClassLoader} to be used to load test cases.
     *
     * @param loader {@link ClassLoader} to load test cases with.
     */
    public TestRequestBuilder setClassLoader(ClassLoader loader) {
        mClassLoader = loader;
        return this;
    }

    /**
     * Add a test class to be executed. All test methods in this class will be executed, unless a
     * test method was explicitly included or excluded.
     *
     * @param className
     */
    public TestRequestBuilder addTestClass(String className) {
        mIncludedClasses.add(className);
        return this;
    }

    /**
     * Excludes a test class. All test methods in this class will be excluded.
     *
     * @param className
     */
    public TestRequestBuilder removeTestClass(String className) {
        mExcludedClasses.add(className);
        return this;
    }

    /**
     * Adds a test method to run.
     */
    public TestRequestBuilder addTestMethod(String testClassName, String testMethodName) {
        mIncludedClasses.add(testClassName);
        mClassMethodFilter.addMethod(testClassName, testMethodName);
        mIgnoreSuiteMethods = true;
        return this;
    }

    /**
     * Excludes a test method from being run.
     */
    public TestRequestBuilder removeTestMethod(String testClassName, String testMethodName) {
        mClassMethodFilter.removeMethod(testClassName, testMethodName);
        mIgnoreSuiteMethods = true;
        return this;
    }

    /**
     * Run only tests within given java package. Cannot be used in conjunction with
     * addTestClass/Method.
     * <p/>
     * At least one addApkPath also must be provided.
     *
     * @param testPackage the fully qualified java package name
     */
    public TestRequestBuilder addTestPackage(String testPackage) {
        mIncludedPackages.add(testPackage);
        return this;
    }

    /**
     * Excludes all tests within given java package. Cannot be used in conjunction with
     * addTestClass/Method.
     * <p/>
     * At least one addApkPath also must be provided.
     *
     * @param testPackage the fully qualified java package name
     */
    public TestRequestBuilder removeTestPackage(String testPackage) {
        mExcludedPackages.add(testPackage);
        return this;
    }

    /**
     * Run only tests with given size
     * @param testSize
     */
    public TestRequestBuilder addTestSizeFilter(String testSize) {
        if (SMALL_SIZE.equals(testSize)) {
            mFilter = mFilter.intersect(new SizeFilter(SmallTest.class));
        } else if (MEDIUM_SIZE.equals(testSize)) {
            mFilter = mFilter.intersect(new SizeFilter(MediumTest.class));
        } else if (LARGE_SIZE.equals(testSize)) {
            mFilter = mFilter.intersect(new SizeFilter(LargeTest.class));
        } else {
            Log.e(LOG_TAG, String.format("Unrecognized test size '%s'", testSize));
        }
        return this;
    }

    /**
     * Only run tests annotated with given annotation class.
     *
     * @param annotation the full class name of annotation
     */
    public TestRequestBuilder addAnnotationInclusionFilter(String annotation) {
        Class<? extends Annotation> annotationClass = loadAnnotationClass(annotation);
        if (annotationClass != null) {
            mFilter = mFilter.intersect(new AnnotationInclusionFilter(annotationClass));
        }
        return this;
    }

    /**
     * Skip tests annotated with given annotation class.
     *
     * @param notAnnotation the full class name of annotation
     */
    public TestRequestBuilder addAnnotationExclusionFilter(String notAnnotation) {
        Class<? extends Annotation> annotationClass = loadAnnotationClass(notAnnotation);
        if (annotationClass != null) {
            mFilter = mFilter.intersect(new AnnotationExclusionFilter(annotationClass));
        }
        return this;
    }

    public TestRequestBuilder addShardingFilter(int numShards, int shardIndex) {
        mFilter = mFilter.intersect(new ShardingFilter(numShards, shardIndex));
        return this;
    }

    /**
     * Build a request that will generate test started and test ended events, but will skip actual
     * test execution.
     */
    public TestRequestBuilder setSkipExecution(boolean b) {
        mSkipExecution = b;
        return this;
    }

    /**
     * Sets milliseconds timeout value applied to each test where 0 means no timeout
     */
    public TestRequestBuilder setPerTestTimeout(long millis) {
        mPerTestTimeout = millis;
        return this;
    }

    /**
     * Convenience method to set builder attributes from {@link RunnerArgs}
     */
    public TestRequestBuilder addFromRunnerArgs(RunnerArgs runnerArgs) {
        for (RunnerArgs.TestArg test : runnerArgs.tests) {
            if (test.methodName == null) {
                addTestClass(test.testClassName);
            } else {
                addTestMethod(test.testClassName, test.methodName);
            }
        }
        for (RunnerArgs.TestArg test : runnerArgs.notTests) {
            if (test.methodName == null) {
                removeTestClass(test.testClassName);
            } else {
                removeTestMethod(test.testClassName, test.methodName);
            }
        }
        for (String pkg : runnerArgs.testPackages) {
            addTestPackage(pkg);
        }
        for (String pkg : runnerArgs.notTestPackages) {
            removeTestPackage(pkg);
        }
        if (runnerArgs.testSize != null) {
            addTestSizeFilter(runnerArgs.testSize);
        }
        if (runnerArgs.annotation != null) {
            addAnnotationInclusionFilter(runnerArgs.annotation);
        }
        for (String notAnnotation : runnerArgs.notAnnotations) {
            addAnnotationExclusionFilter(notAnnotation);
        }
        if (runnerArgs.testTimeout > 0) {
            setPerTestTimeout(runnerArgs.testTimeout);
        }
        if (runnerArgs.numShards > 0 && runnerArgs.shardIndex >= 0 &&
                runnerArgs.shardIndex < runnerArgs.numShards) {
            addShardingFilter(runnerArgs.numShards, runnerArgs.shardIndex);
        }
        if (runnerArgs.logOnly) {
            setSkipExecution(true);
        }
        return this;
    }

    /**
     * Builds the {@link TestRequest} based on provided data.
     *
     * @throws java.lang.IllegalArgumentException if provided set of data is not valid
     */
    public TestRequest build() {
        mIncludedPackages.removeAll(mExcludedPackages);
        mIncludedClasses.removeAll(mExcludedClasses);
        validate(mIncludedClasses);
        TestLoader loader = new TestLoader();
        loader.setClassLoader(mClassLoader);
        if (mIncludedClasses.isEmpty()) {
            // no class restrictions have been specified. Load all classes.
            loadClassesFromClassPath(loader, mExcludedClasses);
        } else {
            loadClasses(mIncludedClasses, loader);
        }

        Request request = classes(
                new AndroidRunnerParams(mInstr, mArgsBundle, mSkipExecution, mPerTestTimeout,
                        mIgnoreSuiteMethods),
                new Computer(),
                loader.getLoadedClasses().toArray(new Class[0]));
        return new TestRequest(loader.getLoadFailures(),
                new LenientFilterRequest(request, mFilter));
    }

    /**
     * Validate that the set of options provided to this builder are valid and not conflicting
     */
    private void validate(Set<String> classNames) {
        if (classNames.isEmpty() && mApkPaths.isEmpty()) {
            throw new IllegalArgumentException(MISSING_ARGUMENTS_MSG);
        }
        // TODO: consider failing if both test classes and apk paths are given.
        // Right now that is allowed though

        if ((!mIncludedPackages.isEmpty() || !mExcludedPackages.isEmpty())
                && !classNames.isEmpty()) {
            throw new IllegalArgumentException(AMBIGUOUS_ARGUMENTS_MSG);
        }
    }

    /**
     * Create a <code>Request</code> that, when processed, will run all the tests
     * in a set of classes.
     *
     * @param runnerParams {@link AndroidRunnerParams} that stores common runner parameters
     * @param computer Helps construct Runners from classes
     * @param classes the classes containing the tests
     * @return a <code>Request</code> that will cause all tests in the classes to be run
     */
    private static Request classes(AndroidRunnerParams runnerParams, Computer computer,
                                   Class<?>... classes) {
        try {
            Runner suite = computer.getSuite(new AndroidRunnerBuilder(runnerParams), classes);
            return Request.runner(suite);
        } catch (InitializationError e) {
            throw new RuntimeException(
                    "Suite constructor, called as above, should always complete");
        }
    }

    private void loadClassesFromClassPath(TestLoader loader, Set<String> excludedClasses) {
        Collection<String> classNames = getClassNamesFromClassPath();
        for (String className : classNames) {
            if (!excludedClasses.contains(className)) {
                loader.loadIfTest(className);
            }
        }
    }

    private void loadClasses(Collection<String> classNames, TestLoader loader) {
        for (String className : classNames) {
            loader.loadClass(className);
        }
    }

    private Collection<String> getClassNamesFromClassPath() {
        if (mApkPaths.isEmpty()) {
            throw new IllegalStateException("neither test class to execute or apk paths were provided");
        }
        Log.i(LOG_TAG, String.format("Scanning classpath to find tests in apks %s",
                mApkPaths));
        ClassPathScanner scanner = createClassPathScanner(mApkPaths);

        ChainedClassNameFilter filter =   new ChainedClassNameFilter();
        // exclude inner classes
        filter.add(new ExternalClassNameFilter());
        for (String pkg : DEFAULT_EXCLUDED_PACKAGES) {
            // Add the test packages to the exclude list unless they were explictly included.
            if (!mIncludedPackages.contains(pkg)) {
                mExcludedPackages.add(pkg);
            }
        }
        for (String pkg : mIncludedPackages) {
            filter.add(new InclusivePackageNameFilter(pkg));
        }
        for (String pkg : mExcludedPackages) {
            filter.add(new ExcludePackageNameFilter(pkg));
        }
        try {
            return scanner.getClassPathEntries(filter);
        } catch (IOException e) {
            Log.e(LOG_TAG, "Failed to scan classes", e);
        }
        return Collections.emptyList();
    }

    /**
     * Factory method for {@link ClassPathScanner}.
     * <p/>
     * Exposed so unit tests can mock.
     */
    ClassPathScanner createClassPathScanner(List<String> apkPaths) {
        return new ClassPathScanner(apkPaths);
    }

    @SuppressWarnings("unchecked")
    private Class<? extends Annotation> loadAnnotationClass(String className) {
        try {
            Class<?> clazz = Class.forName(className);
            return (Class<? extends Annotation>)clazz;
        } catch (ClassNotFoundException e) {
            Log.e(LOG_TAG, String.format("Could not find annotation class: %s", className));
        } catch (ClassCastException e) {
            Log.e(LOG_TAG, String.format("Class %s is not an annotation", className));
        }
        return null;
    }

    private int getDeviceSdkInt() {
        return mDeviceBuild.getSdkVersionInt();
    }

    private String getDeviceHardware() {
        return mDeviceBuild.getHardware();
    }
}
