/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.support.test.rule;

import static android.support.test.internal.util.Checks.checkNotNull;
import static android.support.test.internal.util.Checks.checkState;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.os.Build;
import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.annotation.Beta;
import android.support.test.runner.MonitoringInstrumentation;
import android.support.test.runner.intercepting.SingleActivityFactory;
import android.util.Log;
import java.lang.reflect.Field;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * This rule provides functional testing of a single activity. The activity under test will be
 * launched before each test annotated with
 * <a href="http://junit.org/javadoc/latest/org/junit/Test.html"><code>Test</code></a> and before
 * methods annotated with
 * <a href="http://junit.sourceforge.net/javadoc/org/junit/Before.html"><code>Before</code></a>. It
 * will be terminated after the test is completed and methods annotated with
 * <a href="http://junit.sourceforge.net/javadoc/org/junit/After.html"><code>After</code></a> are
 * finished. During the duration of the test you will be able to manipulate your Activity directly.
 *
 * @param <T> The activity to test
 */
@Beta
public class ActivityTestRule<T extends Activity> extends UiThreadTestRule {

    private static final String TAG = "ActivityInstrumentationRule";
    public static final String FIELD_RESULT_CODE = "mResultCode";
    public static final String FIELD_RESULT_DATA = "mResultData";

    private final Class<T> mActivityClass;

    private Instrumentation mInstrumentation;

    private boolean mInitialTouchMode = false;

    private boolean mLaunchActivity = false;

    private T mActivity;

    private SingleActivityFactory<T> mActivityFactory;

    /**
     * Similar to {@link #ActivityTestRule(Class, boolean, boolean)} but with "touch mode" disabled.
     *
     * @param activityClass    The activity under test. This must be a class in the instrumentation
     *                         targetPackage specified in the AndroidManifest.xml
     * @see ActivityTestRule#ActivityTestRule(Class, boolean, boolean)
     */
    public ActivityTestRule(Class<T> activityClass) {
        this(activityClass, false);
    }

    /**
     * Similar to {@link #ActivityTestRule(Class, boolean, boolean)} but defaults to launch the
     * activity under test once per
     * <a href="http://junit.org/javadoc/latest/org/junit/Test.html"><code>Test</code></a> method.
     * It is launched before the first
     * <a href="http://junit.sourceforge.net/javadoc/org/junit/Before.html"><code>Before</code></a>
     * method, and terminated after the last
     * <a href="http://junit.sourceforge.net/javadoc/org/junit/After.html"><code>After</code></a>
     * method.
     *
     * @param activityClass    The activity under test. This must be a class in the instrumentation
     *                         targetPackage specified in the AndroidManifest.xml
     * @param initialTouchMode true if the Activity should be placed into "touch mode" when started
     * @see ActivityTestRule#ActivityTestRule(Class, boolean, boolean)
     */
    public ActivityTestRule(Class<T> activityClass, boolean initialTouchMode) {
        this(activityClass, initialTouchMode, true);
    }

    /**
     * Creates an {@link ActivityTestRule} for the Activity under test.
     *
     * @param activityClass    The activity under test. This must be a class in the instrumentation
     *                         targetPackage specified in the AndroidManifest.xml
     * @param initialTouchMode true if the Activity should be placed into "touch mode" when started
     * @param launchActivity   true if the Activity should be launched once per
     *                         <a href="http://junit.org/javadoc/latest/org/junit/Test.html">
     *                         <code>Test</code></a> method. It will be launched before the first
     *                         <a href="http://junit.sourceforge.net/javadoc/org/junit/Before.html">
     *                         <code>Before</code></a> method, and terminated after the last
     *                         <a href="http://junit.sourceforge.net/javadoc/org/junit/After.html">
     *                         <code>After</code></a> method.
     */
    public ActivityTestRule(Class<T> activityClass, boolean initialTouchMode,
            boolean launchActivity) {
        mActivityClass = activityClass;
        mInitialTouchMode = initialTouchMode;
        mLaunchActivity = launchActivity;
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
    }

    /**
     * Creates an {@link ActivityTestRule} for the Activity under test.
     *
     * @param activityFactory  factory to be used for creating Activity instance
     * @param initialTouchMode true if the Activity should be placed into "touch mode" when started
     * @param launchActivity   true if the Activity should be launched once per
     *                         <a href="http://junit.org/javadoc/latest/org/junit/Test.html">
     *                         <code>Test</code></a> method. It will be launched before the first
     *                         <a href="http://junit.sourceforge.net/javadoc/org/junit/Before.html">
     *                         <code>Before</code></a> method, and terminated after the last
     *                         <a href="http://junit.sourceforge.net/javadoc/org/junit/After.html">
     *                         <code>After</code></a> method.
     */
    public ActivityTestRule(SingleActivityFactory<T> activityFactory,
                            boolean initialTouchMode, boolean launchActivity) {
        this(activityFactory.getActivityClassToIntercept(), initialTouchMode, launchActivity);
        mActivityFactory = activityFactory;
    }

    /**
     * Override this method to set up Intent as if supplied to
     * {@link android.content.Context#startActivity}.
     * <p>
     * The default Intent (if this method returns null or is not overwritten) is:
     * action = {@link Intent#ACTION_MAIN}
     * flags = {@link Intent#FLAG_ACTIVITY_NEW_TASK}
     * All other intent fields are null or empty.
     *
     * @return The Intent as if supplied to {@link android.content.Context#startActivity}.
     */
    protected Intent getActivityIntent() {
        return new Intent(Intent.ACTION_MAIN);
    }

    /**
     * Override this method to execute any code that should run before your {@link Activity} is
     * created and launched.
     * This method is called before each test method, including any method annotated with
     * <a href="http://junit.sourceforge.net/javadoc/org/junit/Before.html"><code>Before</code></a>.
     */
    protected void beforeActivityLaunched() {
        // empty by default
    }

    /**
     * Override this method to execute any code that should run after your {@link Activity} is
     * launched, but before any test code is run including any method annotated with
     * <a href="http://junit.sourceforge.net/javadoc/org/junit/Before.html"><code>Before</code></a>.
     * <p>
     * Prefer
     * <a href="http://junit.sourceforge.net/javadoc/org/junit/Before.html"><code>Before</code></a>
     * over this method. This method should usually not be overwritten directly in tests and only be
     * used by subclasses of ActivityTestRule to get notified when the activity is created and
     * visible but test runs.
     */
    protected void afterActivityLaunched() {
        // empty by default
    }

    /**
     * Override this method to execute any code that should run after your {@link Activity} is
     * finished.
     * This method is called after each test method, including any method annotated with
     * <a href="http://junit.sourceforge.net/javadoc/org/junit/After.html"><code>After</code></a>.
     */
    protected void afterActivityFinished() {
        // empty by default
    }

    /**
     * @return The activity under test.
     */
    public T getActivity() {
        if (mActivity == null) {
            Log.w(TAG, "Activity wasn't created yet");
        }
        return mActivity;
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        return new ActivityStatement(super.apply(base, description));
    }

    /**
     * Launches the Activity under test.
     * <p>
     * Don't call this method directly, unless you explicitly requested not to launch the Activity
     * manually using the launchActivity flag in
     * {@link ActivityTestRule#ActivityTestRule(Class, boolean, boolean)}.
     * <p>
     * Usage:
     * <pre>
     *    &#064;Test
     *    public void customIntentToStartActivity() {
     *        Intent intent = new Intent(Intent.ACTION_PICK);
     *        mActivity = mActivityRule.launchActivity(intent);
     *    }
     * </pre>
     * @param startIntent The Intent that will be used to start the Activity under test. If
     *                    {@code startIntent} is null, the Intent returned by
     *                    {@link ActivityTestRule#getActivityIntent()} is used.
     * @return the Activity launched by this rule.
     * @see ActivityTestRule#getActivityIntent()
     */
    public T launchActivity(@Nullable Intent startIntent) {
        // set initial touch mode
        mInstrumentation.setInTouchMode(mInitialTouchMode);

        final String targetPackage = mInstrumentation.getTargetContext().getPackageName();
        // inject custom intent, if provided
        if (null == startIntent) {
            startIntent = getActivityIntent();
            if (null == startIntent) {
                Log.w(TAG, "getActivityIntent() returned null using default: " +
                        "Intent(Intent.ACTION_MAIN)");
                startIntent = new Intent(Intent.ACTION_MAIN);
            }
        }
        startIntent.setClassName(targetPackage, mActivityClass.getName());
        startIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Log.d(TAG, String.format("Launching activity %s",
                mActivityClass.getName()));

        beforeActivityLaunched();
        // The following cast is correct because the activity we're creating is of the same type as
        // the one passed in
        mActivity = mActivityClass.cast(mInstrumentation.startActivitySync(startIntent));

        mInstrumentation.waitForIdleSync();

        afterActivityLaunched();
        return mActivity;
    }

    // Visible for testing
    void setInstrumentation(Instrumentation instrumentation) {
        mInstrumentation = checkNotNull(instrumentation, "instrumentation cannot be null!");
    }

    void finishActivity() {
        if (mActivity != null) {
            mActivity.finish();
            mActivity = null;
        }
    }


    /**
     * This method can be used to retrieve the Activity result of an Activity that has called
     * setResult. Usually, the result is handled in {@code }onActivityResult} of parent activity,
     * that has called {@code startActivityForResult}.
     * <p>
     * This method must not be called before Acitivity.finish() was called.
     *
     * @return the ActivityResult that was set most recently
     */
    public ActivityResult getActivityResult() {
        T activity = mActivity;
        checkState(activity.isFinishing());

        try {
            Field resultCodeField = Activity.class.getDeclaredField(FIELD_RESULT_CODE);
            resultCodeField.setAccessible(true);

            Field resultDataField = Activity.class.getDeclaredField(FIELD_RESULT_DATA);
            resultDataField.setAccessible(true);

            return new ActivityResult((int) resultCodeField.get(activity),
                    (Intent) resultDataField.get(activity));

        } catch (NoSuchFieldException e) {
            throw new RuntimeException("Looks like the Android Activity class has changed its" +
                    "private fields for mResultCode or mResultData. " +
                    "Time to update the reflection code.", e);
        } catch (IllegalAccessException e) {
            throw new RuntimeException("Field mResultCode or mResultData is not accessible", e);
        }
    }

    /**
     * <a href="http://junit.org/apidocs/org/junit/runners/model/Statement.html">
     * <code>Statement</code></a> that finishes the activity after the test was executed
     */
    private class ActivityStatement extends Statement {

        private final Statement mBase;

        public ActivityStatement(Statement base) {
            mBase = base;
        }

        @Override
        public void evaluate() throws Throwable {
            MonitoringInstrumentation instrumentation
                    = ActivityTestRule.this.mInstrumentation instanceof MonitoringInstrumentation
                    ? (MonitoringInstrumentation) ActivityTestRule.this.mInstrumentation
                    : null;
            try {
                if (mActivityFactory != null && instrumentation != null) {
                    instrumentation.interceptActivityUsing(mActivityFactory);
                }
                if (mLaunchActivity) {
                    mActivity = launchActivity(getActivityIntent());
                }
                mBase.evaluate();
            } finally {
                if (instrumentation != null) {
                    instrumentation.useDefaultInterceptingActivityFactory();
                }
                finishActivity();
                afterActivityFinished();
            }
        }
    }
}
