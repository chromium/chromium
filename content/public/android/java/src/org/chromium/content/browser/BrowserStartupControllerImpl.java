// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.StrictMode;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.content.app.ContentMain;
import org.chromium.content.browser.ServicificationStartupUma.ServicificationStartup;
import org.chromium.content_public.browser.BrowserStartupController;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Implementation of {@link BrowserStartupController}.
 * This is a singleton, and stores a reference to the application context.
 */
@JNINamespace("content")
public class BrowserStartupControllerImpl implements BrowserStartupController {
    private static final String TAG = "BrowserStartup";

    // Helper constants for {@link #executeEnqueuedCallbacks(int, boolean)}.
    @VisibleForTesting static final int STARTUP_SUCCESS = -1;
    @VisibleForTesting static final int STARTUP_FAILURE = 1;

    @IntDef({BrowserStartType.FULL_BROWSER, BrowserStartType.MINIMAL_BROWSER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface BrowserStartType {
        int FULL_BROWSER = 0;
        int MINIMAL_BROWSER = 1;
    }

    private static BrowserStartupControllerImpl sInstance;

    private static boolean sShouldStartGpuProcessOnBrowserStartup;

    @VisibleForTesting
    @CalledByNative
    static void browserStartupComplete(int result) {
        if (sInstance != null) {
            sInstance.executeEnqueuedCallbacks(result);
        }
    }

    @CalledByNative
    static void minimalBrowserStartupComplete() {
        if (sInstance != null) {
            sInstance.minimalBrowserStarted();
        }
    }

    @CalledByNative
    static boolean shouldStartGpuProcessOnBrowserStartup() {
        return sShouldStartGpuProcessOnBrowserStartup;
    }

    // A list of callbacks that should be called when the async startup of the browser process is
    // complete.
    private final List<StartupCallback> mAsyncStartupCallbacks;

    // A list of callbacks that should be called after a minimal browser environment is initialized.
    // These callbacks will be called once all the ongoing requests to start a minimal or full
    // browser process are completed. For example, if there is no outstanding request to start full
    // browser process, the callbacks will be executed once the minimal browser starts. On the other
    // hand, the callbacks will be defered until full browser starts.
    private final List<StartupCallback> mMinimalBrowserStartedCallbacks;

    // Whether the async startup of the browser process has started.
    private boolean mHasStartedInitializingBrowserProcess;

    // Ensures prepareToStartBrowserProcess() logic happens only once.
    private boolean mPrepareToStartCompleted;

    private boolean mHasCalledContentStart;

    // Whether the async startup of the browser process is complete.
    private boolean mFullBrowserStartupDone;

    // This field is set after startup has been completed based on whether the startup was a success
    // or not. It is used when later requests to startup come in that happen after the initial set
    // of enqueued callbacks have been executed.
    private boolean mStartupSuccess;

    // Tests may inject a method to be run instead of calling ContentMain() in order for them to
    // initialize the C++ system via another means.
    private Runnable mContentMainCallbackForTests;

    // Browser start up type. If the type is |BROWSER_START_TYPE_MINIMAL|, start up
    // will be paused after the minimal environment is setup. Additional request to launch the full
    // browser process is needed to fully complete the startup process. Callbacks will executed
    // once the browser is fully started, or when the minimal environment is setup and there are no
    // outstanding requests to start the full browser.
    @BrowserStartType private int mCurrentBrowserStartType = BrowserStartType.FULL_BROWSER;

    // If the app is only started with a minimal browser, whether it needs to launch full browser
    // funcionalities now.
    private boolean mLaunchFullBrowserAfterMinimalBrowserStart;

    // Whether the minimal browser environment is set up.
    private boolean mMinimalBrowserStarted;

    private TracingControllerAndroidImpl mTracingController;

    BrowserStartupControllerImpl() {
        mAsyncStartupCallbacks = new ArrayList<>();
        mMinimalBrowserStartedCallbacks = new ArrayList<>();
        if (BuildInfo.isDebugAndroid() && !ContextUtils.isSdkSandboxProcess()) {
            // Only set up the tracing broadcast receiver on debug builds of the OS and
            // non-SdkSandbox process. Normal tracing should use the DevTools API.
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    new Runnable() {
                        @Override
                        public void run() {
                            addStartupCompletedObserver(
                                    new StartupCallback() {
                                        @Override
                                        public void onSuccess() {
                                            assert mTracingController == null;
                                            Context context = ContextUtils.getApplicationContext();
                                            mTracingController =
                                                    new TracingControllerAndroidImpl(context);
                                            mTracingController.registerReceiver(context);
                                        }

                                        @Override
                                        public void onFailure() {
                                            // Startup failed.
                                        }
                                    });
                        }
                    });
        }
    }

    /**
     * Get BrowserStartupController instance, create a new one if no existing.
     *
     * @return BrowserStartupController instance.
     */
    public static BrowserStartupController getInstance() {
        assert ThreadUtils.runningOnUiThread() : "Tried to start the browser on the wrong thread.";
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new BrowserStartupControllerImpl();
        }
        return sInstance;
    }

    @VisibleForTesting
    public static void overrideInstanceForTest(BrowserStartupControllerImpl controller) {
        var oldValue = sInstance;
        sInstance = controller;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    @Override
    public void startBrowserProcessesAsync(
            @LibraryProcessType int libraryProcessType,
            boolean startGpuProcess,
            boolean startMinimalBrowser,
            final StartupCallback callback) {
        assert !LibraryLoader.isBrowserProcessStartupBlockedForTesting();
        assertProcessTypeSupported(libraryProcessType);
        assert ThreadUtils.runningOnUiThread() : "Tried to start the browser on the wrong thread.";
        ServicificationStartupUma.getInstance()
                .record(
                        ServicificationStartupUma.getStartupMode(
                                mFullBrowserStartupDone,
                                mMinimalBrowserStarted,
                                startMinimalBrowser));

        if (mFullBrowserStartupDone || (startMinimalBrowser && mMinimalBrowserStarted)) {
            // Browser process initialization has already been completed, so we can immediately post
            // the callback.
            postStartupCompleted(callback);
            return;
        }

        // Browser process has not been fully started yet, so we defer executing the callback.
        if (startMinimalBrowser) {
            mMinimalBrowserStartedCallbacks.add(callback);
        } else {
            mAsyncStartupCallbacks.add(callback);
        }
        // If a minimal browser process is launched, we need to relaunch the full process in
        // minimalBrowserStarted() if such a request was received.
        mLaunchFullBrowserAfterMinimalBrowserStart |=
                (mCurrentBrowserStartType == BrowserStartType.MINIMAL_BROWSER)
                        && !startMinimalBrowser;
        if (!mHasStartedInitializingBrowserProcess) {
            // This is the first time we have been asked to start the browser process. We set the
            // flag that indicates that we have kicked off starting the browser process.
            mHasStartedInitializingBrowserProcess = true;
            sShouldStartGpuProcessOnBrowserStartup |= startGpuProcess;

            // Start-up at this point occurs before the first frame of the app is drawn. Although
            // contentStart() can be called eagerly, deferring it would allow a frame to be drawn,
            // so that Android reports Chrome to start before our SurfaceView has rendered. Our
            // metrics have also adapted to this. Therefore we wrap contentStart() into Runnable,
            // and let prepareToStartBrowserProcess() decide whether to defer it by a frame (in
            // production) or not (overridden in tests). http://b/181151614#comment6
            prepareToStartBrowserProcess(
                    false,
                    new Runnable() {
                        @Override
                        public void run() {
                            ThreadUtils.assertOnUiThread();
                            if (mHasCalledContentStart) return;
                            mCurrentBrowserStartType =
                                    startMinimalBrowser
                                            ? BrowserStartType.MINIMAL_BROWSER
                                            : BrowserStartType.FULL_BROWSER;
                            if (contentStart() > 0) {
                                // Failed. The callbacks may not have run, so run them.
                                enqueueCallbackExecutionOnStartupFailure();
                            }
                        }
                    });

        } else if (mMinimalBrowserStarted && mLaunchFullBrowserAfterMinimalBrowserStart) {
            // If we missed the minimalBrowserStarted() call, launch the full browser now if needed.
            // Otherwise, minimalBrowserStarted() will handle the full browser launch.
            mCurrentBrowserStartType = BrowserStartType.FULL_BROWSER;
            if (contentStart() > 0) enqueueCallbackExecutionOnStartupFailure();
        }
    }

    @Override
    public void startBrowserProcessesSync(
            @LibraryProcessType int libraryProcessType,
            boolean singleProcess,
            boolean startGpuProcess) {
        assert !LibraryLoader.isBrowserProcessStartupBlockedForTesting();
        assertProcessTypeSupported(libraryProcessType);

        sShouldStartGpuProcessOnBrowserStartup |= startGpuProcess;

        ServicificationStartupUma.getInstance()
                .record(
                        ServicificationStartupUma.getStartupMode(
                                mFullBrowserStartupDone,
                                mMinimalBrowserStarted,
                                /* startMinimalBrowser= */ false));

        // If already started skip to checking the result
        if (!mFullBrowserStartupDone) {
            // contentStart() need not be deferred, so passing null.
            prepareToStartBrowserProcess(singleProcess, /* deferrableTask= */ null);

            boolean startedSuccessfully = true;
            if (!mHasCalledContentStart
                    || mCurrentBrowserStartType == BrowserStartType.MINIMAL_BROWSER) {
                mCurrentBrowserStartType = BrowserStartType.FULL_BROWSER;
                if (contentStart() > 0) {
                    // Failed. The callbacks may not have run, so run them.
                    enqueueCallbackExecutionOnStartupFailure();
                    startedSuccessfully = false;
                }
            }
            if (startedSuccessfully) {
                flushStartupTasks();
            }
        }

        // Startup should now be complete
        assert mFullBrowserStartupDone;
        if (!mStartupSuccess) {
            throw new ProcessInitException(LoaderErrors.NATIVE_STARTUP_FAILED);
        }
    }

    /** Start the browser process by calling ContentMain.start(). */
    int contentStart() {
        int result = 0;
        if (mContentMainCallbackForTests == null) {
            boolean startMinimalBrowser =
                    mCurrentBrowserStartType == BrowserStartType.MINIMAL_BROWSER;
            result = contentMainStart(startMinimalBrowser);
            // No need to launch the full browser again if we are launching full browser now.
            if (!startMinimalBrowser) mLaunchFullBrowserAfterMinimalBrowserStart = false;
        } else {
            assert mCurrentBrowserStartType == BrowserStartType.FULL_BROWSER;
            // Run the injected Runnable instead of ContentMain().
            mContentMainCallbackForTests.run();
            mLaunchFullBrowserAfterMinimalBrowserStart = false;
        }
        mHasCalledContentStart = true;
        return result;
    }

    @Override
    public void setContentMainCallbackForTests(Runnable r) {
        assert !mHasCalledContentStart;
        mContentMainCallbackForTests = r;
    }

    /** Wrap ContentMain.start() for testing. */
    @VisibleForTesting
    int contentMainStart(boolean startMinimalBrowser) {
        return ContentMain.start(startMinimalBrowser);
    }

    @VisibleForTesting
    void flushStartupTasks() {
        try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped("flushStartupTasks")) {
            BrowserStartupControllerImplJni.get().flushStartupTasks();
        }
    }

    @Override
    public boolean isFullBrowserStarted() {
        ThreadUtils.assertOnUiThread();
        return mFullBrowserStartupDone && mStartupSuccess;
    }

    @Override
    public boolean isRunningInMinimalBrowserMode() {
        ThreadUtils.assertOnUiThread();
        return mMinimalBrowserStarted && !mFullBrowserStartupDone && mStartupSuccess;
    }

    @Override
    public boolean isNativeStarted() {
        ThreadUtils.assertOnUiThread();
        return (mMinimalBrowserStarted || mFullBrowserStartupDone) && mStartupSuccess;
    }

    @Override
    public void addStartupCompletedObserver(StartupCallback callback) {
        ThreadUtils.assertOnUiThread();
        if (mFullBrowserStartupDone) {
            postStartupCompleted(callback);
        } else {
            mAsyncStartupCallbacks.add(callback);
        }
    }

    @Override
    public @ServicificationStartup int getStartupMode(boolean startMinimalBrowser) {
        return ServicificationStartupUma.getStartupMode(
                mFullBrowserStartupDone, mMinimalBrowserStarted, startMinimalBrowser);
    }

    /**
     * Asserts that library process type is one of the supported types.
     * @param libraryProcessType the type of process the shared library is loaded. It must be
     *                           LibraryProcessType.PROCESS_BROWSER or
     *                           LibraryProcessType.PROCESS_WEBVIEW.
     */
    private void assertProcessTypeSupported(@LibraryProcessType int libraryProcessType) {
        assert LibraryProcessType.PROCESS_BROWSER == libraryProcessType
                || LibraryProcessType.PROCESS_WEBVIEW == libraryProcessType;
        LibraryLoader.getInstance().assertCompatibleProcessType(libraryProcessType);
    }

    /** Called when the minimal browser environment is done initializing. */
    private void minimalBrowserStarted() {
        mMinimalBrowserStarted = true;
        if (mLaunchFullBrowserAfterMinimalBrowserStart) {
            // If startFullBrowser() fails, execute the callbacks right away. Otherwise,
            // callbacks will be deferred until browser startup completes.
            mCurrentBrowserStartType = BrowserStartType.FULL_BROWSER;
            if (contentStart() > 0) enqueueCallbackExecutionOnStartupFailure();
            return;
        }

        if (mCurrentBrowserStartType == BrowserStartType.MINIMAL_BROWSER) {
            executeMinimalBrowserStartupCallbacks(STARTUP_SUCCESS);
        }
        recordStartupUma();
    }

    private void executeEnqueuedCallbacks(int startupResult) {
        assert ThreadUtils.runningOnUiThread() : "Callback from browser startup from wrong thread.";
        mFullBrowserStartupDone = true;
        mStartupSuccess = (startupResult <= 0);
        for (StartupCallback asyncStartupCallback : mAsyncStartupCallbacks) {
            if (mStartupSuccess) {
                asyncStartupCallback.onSuccess();
            } else {
                asyncStartupCallback.onFailure();
            }
        }
        // We don't want to hold on to any objects after we do not need them anymore.
        mAsyncStartupCallbacks.clear();

        executeMinimalBrowserStartupCallbacks(startupResult);
        recordStartupUma();
    }

    private void executeMinimalBrowserStartupCallbacks(int startupResult) {
        mStartupSuccess = (startupResult <= 0);
        for (StartupCallback callback : mMinimalBrowserStartedCallbacks) {
            if (mStartupSuccess) {
                callback.onSuccess();
            } else {
                callback.onFailure();
            }
        }
        mMinimalBrowserStartedCallbacks.clear();
    }

    // Post a task to tell the callbacks that startup failed. Since the execution clears the
    // callback lists, it is safe to call this more than once.
    private void enqueueCallbackExecutionOnStartupFailure() {
        PostTask.postTask(TaskTraits.UI_DEFAULT, () -> executeEnqueuedCallbacks(STARTUP_FAILURE));
    }

    private void postStartupCompleted(final StartupCallback callback) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                new Runnable() {
                    @Override
                    public void run() {
                        if (mStartupSuccess) {
                            callback.onSuccess();
                        } else {
                            callback.onFailure();
                        }
                    }
                });
    }

    @VisibleForTesting
    void prepareToStartBrowserProcess(final boolean singleProcess, final Runnable deferrableTask) {
        if (mPrepareToStartCompleted) {
            return;
        }
        Log.d(TAG, "Initializing chromium process, singleProcess=%b", singleProcess);
        mPrepareToStartCompleted = true;
        try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped("prepareToStartBrowserProcess")) {
            // This strictmode exception is to cover the case where the browser process is being
            // started asynchronously but not in the main browser flow.  The main browser flow
            // will trigger library loading earlier and this will be a no-op, but in the other
            // cases this will need to block on loading libraries. This applies to tests and
            // ManageSpaceActivity, which can be launched from Settings.
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            try {
                // Normally Main.java will have already loaded the library asynchronously, we
                // only need to load it here if we arrived via another flow, e.g. bookmark
                // access & sync setup.
                LibraryLoader.getInstance().ensureInitialized();
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }

            // TODO(yfriedman): Remove dependency on a command line flag for this.
            DeviceUtilsImpl.addDeviceSpecificUserAgentSwitch();
            BrowserStartupControllerImplJni.get().setCommandLineFlags(singleProcess);
        }

        if (deferrableTask != null) {
            PostTask.postTask(TaskTraits.UI_USER_BLOCKING, deferrableTask);
        }
    }

    /** Can be overridden by testing. */
    @VisibleForTesting
    void recordStartupUma() {
        ServicificationStartupUma.getInstance().commit();
    }

    @NativeMethods
    interface Natives {
        void setCommandLineFlags(boolean singleProcess);

        void flushStartupTasks();
    }
}
