/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.support.test.runner;

import static android.support.test.internal.util.Checks.checkMainThread;
import static android.support.test.internal.util.Checks.checkNotMainThread;

import android.app.Activity;
import android.app.Application;
import android.app.Fragment;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.MessageQueue.IdleHandler;
import android.support.test.InstrumentationRegistry;
import android.support.test.internal.runner.hidden.ExposedInstrumentationApi;
import android.support.test.internal.runner.intent.IntentMonitorImpl;
import android.support.test.internal.runner.intercepting.DefaultInterceptingActivityFactory;
import android.support.test.internal.runner.lifecycle.ActivityLifecycleMonitorImpl;
import android.support.test.internal.runner.lifecycle.ApplicationLifecycleMonitorImpl;
import android.support.test.internal.util.Checks;
import android.support.test.runner.intent.IntentMonitorRegistry;
import android.support.test.runner.intent.IntentStubberRegistry;
import android.support.test.runner.intercepting.InterceptingActivityFactory;
import android.support.test.runner.lifecycle.ActivityLifecycleMonitorRegistry;
import android.support.test.runner.lifecycle.ApplicationLifecycleMonitorRegistry;
import android.support.test.runner.lifecycle.ApplicationStage;
import android.support.test.runner.lifecycle.Stage;
import android.util.Log;
import java.io.File;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import java.util.concurrent.FutureTask;
import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;

/**
 * An instrumentation that enables several advanced features and makes some hard guarantees about
 * the state of the application under instrumentation.
 * <p/>
 * A short list of these capabilities:
 * <ul>
 * <li>Forces Application.onCreate() to happen before Instrumentation.onStart() runs (ensuring your
 * code always runs in a sane state).</li>
 * <li>Logs application death due to exceptions.</li>
 * <li>Allows tracking of activity lifecycle states.</li>
 * <li>Registers instrumentation arguments in an easy to access place.</li>
 * <li>Ensures your activities are creating themselves in reasonable amounts of time.</li>
 * <li>Provides facilities to dump current app threads to test outputs.</li>
 * <li>Ensures all activities finish before instrumentation exits.</li>
 * </ul>
 *
 * This Instrumentation is *NOT* a test instrumentation (some of its subclasses are). It makes no
 * assumptions about what the subclass wants to do.
 */
public class MonitoringInstrumentation extends ExposedInstrumentationApi {

    private static final long MILLIS_TO_WAIT_FOR_ACTIVITY_TO_STOP = TimeUnit.SECONDS.toMillis(2);
    private static final long MILLIS_TO_POLL_FOR_ACTIVITY_STOP =
            MILLIS_TO_WAIT_FOR_ACTIVITY_TO_STOP / 40;

    private static final String LOG_TAG = "MonitoringInstrumentation";

    private static final int START_ACTIVITY_TIMEOUT_SECONDS = 45;
    private ActivityLifecycleMonitorImpl mLifecycleMonitor = new ActivityLifecycleMonitorImpl();
    private ApplicationLifecycleMonitorImpl mApplicationMonitor =
            new ApplicationLifecycleMonitorImpl();
    private IntentMonitorImpl mIntentMonitor = new IntentMonitorImpl();
    private ExecutorService mExecutorService;
    private Handler mHandlerForMainLooper;
    private AtomicBoolean mAnActivityHasBeenLaunched = new AtomicBoolean(false);
    private Thread mMainThread;
    private AtomicLong mLastIdleTime = new AtomicLong(0);
    private AtomicInteger mStartedActivityCounter = new AtomicInteger(0);

    private IdleHandler mIdleHandler = new IdleHandler() {
        @Override
        public boolean queueIdle() {
            mLastIdleTime.set(System.currentTimeMillis());
            return true;
        }
    };

    private volatile boolean mFinished = false;
    private volatile InterceptingActivityFactory mInterceptingActivityFactory;

    /**
     * Sets up lifecycle monitoring, and argument registry.
     * <p>
     * Subclasses must call up to onCreate(). This onCreate method does not call start()
     * it is the subclasses responsibility to call start if it desires.
     * </p>
     */
    @Override
    public void onCreate(Bundle arguments) {
        Log.i(LOG_TAG, "Instrumentation Started!");
        logUncaughtExceptions();

        installMultidex();

        InstrumentationRegistry.registerInstance(this, arguments);
        ActivityLifecycleMonitorRegistry.registerInstance(mLifecycleMonitor);
        ApplicationLifecycleMonitorRegistry.registerInstance(mApplicationMonitor);
        IntentMonitorRegistry.registerInstance(mIntentMonitor);

        mHandlerForMainLooper = new Handler(Looper.getMainLooper());
        mMainThread = Thread.currentThread();
        final int corePoolSize = 0;
        final long keepAliveTime = 0L;
        mExecutorService = new ThreadPoolExecutor(corePoolSize, Integer.MAX_VALUE, keepAliveTime,
                TimeUnit.SECONDS, new SynchronousQueue<Runnable>());
        Looper.myQueue().addIdleHandler(mIdleHandler);
        super.onCreate(arguments);
        specifyDexMakerCacheProperty();
        setupDexmakerClassloader();
        useDefaultInterceptingActivityFactory();
    }

    private final void installMultidex() {
        // Typically multidex is installed by inserting call at Application#attachBaseContext
        // However in  ICS and presumably below, instrumentation#onCreate is called before
        // attachBaseContext. Thus need to install it here, if its on classpath, to prevent
        // potential class loading issues when using multidex
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.ICE_CREAM_SANDWICH_MR1) {
            try {
                Class<?> multidex = Class.forName(
                        "android.support.multidex.MultiDex");
                Method install = multidex.getDeclaredMethod("install", Context.class);
                install.invoke(null, getTargetContext());
            } catch (ClassNotFoundException ignored){
                Log.i(LOG_TAG, "No multidex.");
            } catch (NoSuchMethodException nsme){
                Log.i(LOG_TAG, "No multidex.");
            } catch (InvocationTargetException ite){
                throw new RuntimeException(
                        "multidex is available at runtime, but calling it failed.", ite);
            } catch (IllegalAccessException iae){
                throw new RuntimeException(
                        "multidex is available at runtime, but calling it failed.", iae);
            }
        }
    }

    private final void specifyDexMakerCacheProperty() {
        // DexMaker uses heuristics to figure out where to store its temporary dex files
        // these heuristics may break (eg - they no longer work on JB MR2). So we create
        // our own cache dir to be used if the app doesnt specify a cache dir, rather then
        // relying on heuristics.
        //
        File dexCache = getTargetContext().getDir("dxmaker_cache", Context.MODE_PRIVATE);
        System.getProperties().put("dexmaker.dexcache", dexCache.getAbsolutePath());
    }

    private void setupDexmakerClassloader() {
        ClassLoader originalClassLoader = Thread.currentThread().getContextClassLoader();
        // must set the context classloader for apps that use a shared uid, see
        // frameworks/base/core/java/android/app/LoadedApk.java
        ClassLoader newClassLoader = this.getClass().getClassLoader();
        Log.i(LOG_TAG, String.format("Setting context classloader to '%s', Original: '%s'",
                newClassLoader.toString(), originalClassLoader.toString()));
        Thread.currentThread().setContextClassLoader(newClassLoader);
    }

    private void logUncaughtExceptions() {
        final Thread.UncaughtExceptionHandler standardHandler =
                Thread.currentThread().getUncaughtExceptionHandler();
        Thread.currentThread().setUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread t, Throwable e) {
                onException(t, e);
                if (null != standardHandler) {
                    standardHandler.uncaughtException(t, e);
                }
            }
        });
    }

    /**
     * This implementation of onStart() will guarantee that the Application's onCreate method
     * has completed when it returns.
     * <p>
     * Subclasses should call super.onStart() before executing any code that touches the application
     * and it's state.
     * </p>
     */
    @Override
    public void onStart() {
        super.onStart();

        runOnMainSync(new Runnable() {
            @Override
            public void run() {
                tryLoadingJsBridge();
            }});

        // Due to the way Android initializes instrumentation - all instrumentations have the
        // possibility of seeing the Application and its classes in an inconsistent state.
        // Specifically ActivityThread creates Instrumentation first, initializes it, and calls
        // instrumentation.onCreate(). After it does that, it calls
        // instrumentation.callApplicationOnCreate() which ends up calling the application's
        // onCreateMethod.
        //
        // So, Android's InstrumentationTestRunner's onCreate method() spawns a separate thread to
        // execute tests. This causes tests to start accessing the application and its classes while
        // the ActivityThread is calling callApplicationOnCreate() in its own thread.
        //
        // This makes it possible for tests to see the application in a state that is normally never
        // visible: pre-application.onCreate() and during application.onCreate()).
        //
        // *phew* that sucks! Here we waitForOnIdleSync() to ensure onCreate has completed before we
        // start executing tests.
        waitForIdleSync();
    }

    /**
     * Ensures all activities launched in this instrumentation are finished before the
     * instrumentation exits.
     * <p>
     * Subclasses who override this method should do their finish processing and then call
     * super.finish to invoke this logic. Not waiting for all activities to finish() before exiting
     * can cause device wide instability.
     * </p>
     */
    @Override
    public void finish(int resultCode, Bundle results) {
        if (mFinished) {
            Log.w(LOG_TAG, "finish called 2x!");
            return;
        } else {
            mFinished = true;
        }

        mHandlerForMainLooper.post(new ActivityFinisher());

        long startTime = System.currentTimeMillis();
        waitForActivitiesToComplete();
        long endTime = System.currentTimeMillis();
        Log.i(LOG_TAG, String.format("waitForActivitiesToComplete() took: %sms", endTime - startTime));
        ActivityLifecycleMonitorRegistry.registerInstance(null);
        super.finish(resultCode, results);
    }

    /**
     * Ensures we've onStopped() all activities which were onStarted().
     * <p>
     * According to Activity's contract, the process is not killable between onStart and onStop.
     * Breaking this contract (which finish() will if you let it) can cause bad behaviour (including
     * a full restart of system_server).
     * </p>
     * <p>
     * We give the app 2 seconds to stop all its activities, then we proceed.
     * </p>
     */
    protected void waitForActivitiesToComplete() {
        long endTime = System.currentTimeMillis() + MILLIS_TO_WAIT_FOR_ACTIVITY_TO_STOP;
        int currentActivityCount = mStartedActivityCounter.get();

        while (currentActivityCount > 0 && System.currentTimeMillis() < endTime) {
            try {
                Log.i(LOG_TAG, "Unstopped activity count: " + currentActivityCount);
                Thread.sleep(MILLIS_TO_POLL_FOR_ACTIVITY_STOP);
                currentActivityCount = mStartedActivityCounter.get();
            } catch (InterruptedException ie) {
                Log.i(LOG_TAG, "Abandoning activity wait due to interruption.", ie);
                break;
            }
        }

        if (currentActivityCount > 0) {
            dumpThreadStateToOutputs("ThreadState-unstopped.txt");
            Log.w(LOG_TAG, String.format("Still %s activities active after waiting %s ms.",
                    currentActivityCount, MILLIS_TO_WAIT_FOR_ACTIVITY_TO_STOP));
        }
    }

    @Override
    public void onDestroy() {
        Log.i(LOG_TAG, "Instrumentation Finished!");
        Looper.myQueue().removeIdleHandler(mIdleHandler);
        super.onDestroy();
    }

    @Override
    public void callApplicationOnCreate(Application app) {
        mApplicationMonitor.signalLifecycleChange(app, ApplicationStage.PRE_ON_CREATE);
        super.callApplicationOnCreate(app);
        mApplicationMonitor.signalLifecycleChange(app, ApplicationStage.CREATED);
    }

    @Override
    public Activity startActivitySync(final Intent intent) {
        checkNotMainThread();
        long lastIdleTimeBeforeLaunch = mLastIdleTime.get();

        if (mAnActivityHasBeenLaunched.compareAndSet(false, true)) {
            // All activities launched from InstrumentationTestCase.launchActivityWithIntent get
            // started with FLAG_ACTIVITY_NEW_TASK. This includes calls to
            // ActivityInstrumentationTestcase2.getActivity().
            //
            // This gives us a pristine environment - MOST OF THE TIME.
            //
            // However IF we've run a test method previously and that has launched an activity
            // outside of our process our old task is still lingering around. By launching a new
            // activity android will place our activity at the bottom of the stack and bring the
            // previous external activity to the front of the screen.
            //
            // To wipe out the old task and execute within a pristine environment for each test
            // we tell android to CLEAR_TOP the very first activity we see, no matter what.
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        }
        Future<Activity> startedActivity = mExecutorService.submit(new Callable<Activity>() {
            @Override
            public Activity call() {
                return MonitoringInstrumentation.super.startActivitySync(intent);
            }
        });

        try {
            return startedActivity.get(START_ACTIVITY_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        } catch (TimeoutException te) {
            startedActivity.cancel(true);
            dumpThreadStateToOutputs("ThreadState-startActivityTimeout.txt");
            throw new RuntimeException(String.format("Could not launch intent %s within %s seconds."
                    + " Perhaps the main thread has not gone idle within a reasonable amount of "
                    + "time? There could be an animation or something constantly repainting the "
                    + "screen. Or the activity is doing network calls on creation? See the "
                    + "threaddump logs. For your reference the last time the event queue was idle "
                    + "before your activity launch request was %s and now the last time the queue "
                    + "went idle was: %s. If these numbers are the same your activity might be "
                    +"hogging the event queue.",
                    intent, START_ACTIVITY_TIMEOUT_SECONDS, lastIdleTimeBeforeLaunch,
                    mLastIdleTime.get()));
        } catch (ExecutionException ee) {
            throw new RuntimeException("Could not launch activity", ee.getCause());
        } catch (InterruptedException ie) {
            Thread.currentThread().interrupt();
            throw new RuntimeException("interrupted", ie);
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public ActivityResult execStartActivity(
            Context who, IBinder contextThread, IBinder token, Activity target,
            Intent intent, int requestCode) {
        Log.d(LOG_TAG, "execStartActivity(context, ibinder, ibinder, activity, intent, int)");
        mIntentMonitor.signalIntent(intent);
        ActivityResult ar = stubResultFor(intent);
        if (ar != null) {
            Log.i(LOG_TAG, String.format("Stubbing intent %s", intent));
            return ar;
        }
        return super.execStartActivity(who, contextThread, token, target, intent, requestCode);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public ActivityResult execStartActivity(
            Context who, IBinder contextThread, IBinder token, Activity target,
            Intent intent, int requestCode, Bundle options) {
        Log.d(LOG_TAG, "execStartActivity(context, ibinder, ibinder, activity, intent, int, bundle");
        mIntentMonitor.signalIntent(intent);
        ActivityResult ar = stubResultFor(intent);
        if (ar != null) {
            Log.i(LOG_TAG, String.format("Stubbing intent %s", intent));
            return ar;
        }
        return super.execStartActivity(who, contextThread, token, target, intent, requestCode, options);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void execStartActivities(Context who, IBinder contextThread,
            IBinder token, Activity target, Intent[] intents, Bundle options)  {
        // This method is used in HONEYCOMB and higher to create a synthetic back stack for the
        // launched activity. The intent at the end of the array is the top most,
        // user visible activity, and the intents beneath it are launched when the user presses back.
        Log.d(LOG_TAG, "execStartActivities(context, ibinder, ibinder, activity, intent[], bundle)");
        // For requestCode < 0, the caller doesn't expect any result and
        // in this case we are not expecting any result so selecting
        // a value < 0.
        int requestCode = -1;
        for (Intent intent : intents) {
            execStartActivity(who, contextThread, token, target, intent, requestCode, options);
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public ActivityResult execStartActivity(
            Context who, IBinder contextThread, IBinder token, Fragment target,
            Intent intent, int requestCode, Bundle options) {
        Log.d(LOG_TAG, "execStartActivity(context, IBinder, IBinder, Fragment, Intent, int, Bundle)");
        mIntentMonitor.signalIntent(intent);
        ActivityResult ar = stubResultFor(intent);
        if (ar != null) {
            Log.i(LOG_TAG, String.format("Stubbing intent %s", intent));
            return ar;
        }
        return super.execStartActivity(who, contextThread, token, target, intent, requestCode, options);
    }

    private static class StubResultCallable implements Callable<ActivityResult> {
        private final Intent mIntent;

        StubResultCallable(Intent intent) {
            mIntent = intent;
        }

        @Override
        public ActivityResult call() {
            return IntentStubberRegistry.getInstance().getActivityResultForIntent(mIntent);
        }
    }

    private ActivityResult stubResultFor(Intent intent) {
        if (IntentStubberRegistry.isLoaded()) {
            // Activities can be launched from the instrumentation thread, so if that's the case,
            // get on main thread to retrieve the result.
            if (Looper.myLooper() != Looper.getMainLooper()) {
                FutureTask<ActivityResult> task = new FutureTask<ActivityResult>(
                        new StubResultCallable(intent));
                runOnMainSync(task);
                try {
                    return task.get();
                } catch (ExecutionException e) {
                    throw new RuntimeException(
                            String.format("Could not retrieve stub result for intent %s", intent), e);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    throw new RuntimeException(e);
                }
            } else {
                return IntentStubberRegistry.getInstance().getActivityResultForIntent(intent);
            }
        }
        return null;
    }

    @Override
    public boolean onException(Object obj, Throwable e) {
        String error = String.format("Exception encountered by: %s. Dumping thread state to "
                + "outputs and pining for the fjords.", obj);
        Log.e(LOG_TAG, error, e);
        dumpThreadStateToOutputs("ThreadState-onException.txt");
        Log.e(LOG_TAG, "Dying now...");
        return super.onException(obj, e);
    }

    protected void dumpThreadStateToOutputs(String outputFileName) {
        String threadState = getThreadState();
        Log.e("THREAD_STATE", threadState);
    }

    protected String getThreadState() {
        Set<Map.Entry<Thread, StackTraceElement[]>> threads = Thread.getAllStackTraces().entrySet();
        StringBuilder threadState = new StringBuilder();
        for (Map.Entry<Thread, StackTraceElement[]> threadAndStack : threads) {
            StringBuilder threadMessage = new StringBuilder("  ").append(threadAndStack.getKey());
            threadMessage.append("\n");
            for (StackTraceElement ste : threadAndStack.getValue()) {
                threadMessage.append("    ");
                threadMessage.append(ste.toString());
                threadMessage.append("\n");
            }
            threadMessage.append("\n");
            threadState.append(threadMessage.toString());
        }
        return threadState.toString();
    }

    @Override
    public void callActivityOnDestroy(Activity activity) {
        super.callActivityOnDestroy(activity);
        mLifecycleMonitor.signalLifecycleChange(Stage.DESTROYED, activity);
    }

    @Override
    public void callActivityOnRestart(Activity activity) {
        super.callActivityOnRestart(activity);
        mLifecycleMonitor.signalLifecycleChange(Stage.RESTARTED, activity);
    }

    @Override
    public void callActivityOnCreate(Activity activity, Bundle bundle) {
        mLifecycleMonitor.signalLifecycleChange(Stage.PRE_ON_CREATE, activity);
        super.callActivityOnCreate(activity, bundle);
        mLifecycleMonitor.signalLifecycleChange(Stage.CREATED, activity);
    }

    // NOTE: we need to keep a count of activities between the start
    // and stop lifecycle internal to our instrumentation. Exiting the test
    // process with activities in this state can cause crashes/flakiness
    // that would impact a subsequent test run.
    @Override
    public void callActivityOnStart(Activity activity) {
        mStartedActivityCounter.incrementAndGet();
        try {
            super.callActivityOnStart(activity);
            mLifecycleMonitor.signalLifecycleChange(Stage.STARTED, activity);
        } catch (RuntimeException re) {
            mStartedActivityCounter.decrementAndGet();
            throw re;
        }
    }

    @Override
    public void callActivityOnStop(Activity activity) {
        try {
            super.callActivityOnStop(activity);
            mLifecycleMonitor.signalLifecycleChange(Stage.STOPPED, activity);
        } finally {
            mStartedActivityCounter.decrementAndGet();
        }
    }

    @Override
    public void callActivityOnResume(Activity activity) {
        super.callActivityOnResume(activity);
        mLifecycleMonitor.signalLifecycleChange(Stage.RESUMED, activity);
    }

    @Override
    public void callActivityOnPause(Activity activity) {
        super.callActivityOnPause(activity);
        mLifecycleMonitor.signalLifecycleChange(Stage.PAUSED, activity);
    }


    // ActivityUnitTestCase defaults to building the ComponentName via
    // Activity.getClass().getPackage().getName(). This will cause a problem if the Java Package of
    // the Activity is not the Android Package of the application, specifically
    // Activity.getPackageName() will return an incorrect value.
    // @see b/14561718
    @Override
    public Activity newActivity(Class<?> clazz,
                                Context context,
                                IBinder token,
                                Application application,
                                Intent intent,
                                ActivityInfo info,
                                CharSequence title,
                                Activity parent,
                                String id,
                                Object lastNonConfigurationInstance)
            throws InstantiationException, IllegalAccessException {
        String activityClassPackageName = clazz.getPackage().getName();
        String contextPackageName = context.getPackageName();
        ComponentName intentComponentName = intent.getComponent();
        if (!contextPackageName.equals(intentComponentName.getPackageName())) {
            if (activityClassPackageName.equals(intentComponentName.getPackageName())) {
                intent.setComponent(
                        new ComponentName(contextPackageName, intentComponentName.getClassName()));
            }
        }
        return super.newActivity(clazz,
                context,
                token,
                application,
                intent,
                info,
                title,
                parent,
                id,
                lastNonConfigurationInstance);
    }

    @Override
    public Activity newActivity(ClassLoader cl, String className, Intent intent)
        throws InstantiationException, IllegalAccessException, ClassNotFoundException {
        return mInterceptingActivityFactory.shouldIntercept(cl, className, intent)
                ? mInterceptingActivityFactory.create(cl, className, intent)
                : super.newActivity(cl, className, intent);
    }

    /**
     * Use the given InterceptingActivityFactory to create Activity instance in
     * {@link #newActivity(ClassLoader, String, Intent)}. This can be used to override default
     * behavior of activity in tests e.g. mocking startService() method in Activity under test,
     * to avoid starting the real service and instead verifying that a particular service was
     * started.
     *
     * @param interceptingActivityFactory InterceptingActivityFactory to be used for creating
     *                                    activity instance in {@link #newActivity(ClassLoader,
     *                                    String, Intent)}
     */
    public void interceptActivityUsing(InterceptingActivityFactory interceptingActivityFactory) {
        Checks.checkNotNull(interceptingActivityFactory);
        mInterceptingActivityFactory = interceptingActivityFactory;
    }

    /**
     * Use default mechanism of creating activity instance in
     * {@link #newActivity(ClassLoader, String, Intent)}
     */

    public void useDefaultInterceptingActivityFactory() {
        mInterceptingActivityFactory = new DefaultInterceptingActivityFactory();
    }

    /**
     * Loads the JS Bridge for Espresso Web. Only call this method from the main thread!
     */
    private void tryLoadingJsBridge() {
        checkMainThread();
        try {
            Class<?> jsBridge = Class.forName(
                    "android.support.test.espresso.web.bridge.JavaScriptBridge");
            Method install = jsBridge.getDeclaredMethod("installBridge");
            install.invoke(null);
        } catch (ClassNotFoundException ignored) {
            Log.i(LOG_TAG, "Espresso Web not found.");
        } catch (NoSuchMethodException nsme) {
            Log.i(LOG_TAG, "No JSBridge.", nsme);
        } catch (InvocationTargetException ite) {
            throw new RuntimeException(
                    "JSbridge is available at runtime, but calling it failed.", ite);
        } catch (IllegalAccessException iae) {
            throw new RuntimeException(
                    "JSbridge is available at runtime, but calling it failed.", iae);
        }
    }

    /**
     * Loops through all the activities that have not yet finished and explicitly calls finish
     * on them.
     */
    public class ActivityFinisher implements Runnable {
        @Override
        public void run() {
            List<Activity> activities = new ArrayList<Activity>();

            for (Stage s : EnumSet.range(Stage.CREATED, Stage.STOPPED)) {
                activities.addAll(mLifecycleMonitor.getActivitiesInStage(s));
            }

            Log.i(LOG_TAG, "Activities that are still in CREATED to STOPPED: " + activities.size());

            for (Activity activity : activities) {
                if (!activity.isFinishing()) {
                    try {
                        Log.i(LOG_TAG, "Finishing activity: " + activity);
                        activity.finish();
                    } catch (RuntimeException e) {
                        Log.e(LOG_TAG, "Failed to finish activity.", e);
                    }
                }
            }
        }
    };
}
