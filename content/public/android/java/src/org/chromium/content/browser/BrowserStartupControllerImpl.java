// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.StrictMode;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.task.PostTask;
import org.chromium.content.app.ContentMain;
import org.chromium.content.browser.ServicificationStartupUma.ServicificationStartup;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.resources.ResourceExtractor;

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
    @VisibleForTesting
    static final int STARTUP_SUCCESS = -1;
    @VisibleForTesting
    static final int STARTUP_FAILURE = 1;

    @IntDef({BrowserStartType.FULL_BROWSER, BrowserStartType.SERVICE_MANAGER_ONLY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface BrowserStartType {
        int FULL_BROWSER = 0;
        int SERVICE_MANAGER_ONLY = 1;
    }

    private static BrowserStartupControllerImpl sInstance;

    private static boolean sShouldStartGpuProcessOnBrowserStartup;

    private static void setShouldStartGpuProcessOnBrowserStartup(boolean enable) {
        sShouldStartGpuProcessOnBrowserStartup = enable;
    }

    @VisibleForTesting
    @CalledByNative
    static void browserStartupComplete(int result) {
        if (sInstance != null) {
            sInstance.executeEnqueuedCallbacks(result);
        }
    }

    @CalledByNative
    static void serviceManagerStartupComplete() {
        if (sInstance != null) {
            sInstance.serviceManagerStarted();
        }
    }

    @CalledByNative
    static boolean shouldStartGpuProcessOnBrowserStartup() {
        return sShouldStartGpuProcessOnBrowserStartup;
    }

    // A list of callbacks that should be called when the async startup of the browser process is
    // complete.
    private final List<StartupCallback> mAsyncStartupCallbacks;

    // A list of callbacks that should be called when the ServiceManager is started. These callbacks
    // will be called once all the ongoing requests to start ServiceManager or full browser process
    // are completed. For example, if there is no outstanding request to start full browser process,
    // the callbacks will be executed once ServiceManager starts. On the other hand, the callbacks
    // will be defered until full browser starts.
    private final List<StartupCallback> mServiceManagerCallbacks;

    // Whether the async startup of the browser process has started.
    private boolean mHasStartedInitializingBrowserProcess;

    // Whether tasks that occur after resource extraction have been completed.
    private boolean mPostResourceExtractionTasksCompleted;

    private boolean mHasCalledContentStart;

    // Whether the async startup of the browser process is complete.
    private boolean mFullBrowserStartupDone;

    // This field is set after startup has been completed based on whether the startup was a success
    // or not. It is used when later requests to startup come in that happen after the initial set
    // of enqueued callbacks have been executed.
    private boolean mStartupSuccess;

    private int mLibraryProcessType;

    // Tests may inject a method to be run instead of calling ContentMain() in order for them to
    // initialize the C++ system via another means.
    private Runnable mContentMainCallbackForTests;

    // Browser start up type. If the type is |BROWSER_START_TYPE_SERVICE_MANAGER_ONLY|, start up
    // will be paused after ServiceManager is launched. Additional request to launch the full
    // browser process is needed to fully complete the startup process. Callbacks will executed
    // once the browser is fully started, or when the ServiceManager is started and there is no
    // outstanding requests to start the full browser.
    @BrowserStartType
    private int mCurrentBrowserStartType = BrowserStartType.FULL_BROWSER;

    // If the app is only started with the ServiceManager, whether it needs to launch full browser
    // funcionalities now.
    private boolean mLaunchFullBrowserAfterServiceManagerStart;

    // Whether ServiceManager is started.
    private boolean mServiceManagerStarted;

    private TracingControllerAndroidImpl mTracingController;

    BrowserStartupControllerImpl(int libraryProcessType) {
        mAsyncStartupCallbacks = new ArrayList<>();
        mServiceManagerCallbacks = new ArrayList<>();
        mLibraryProcessType = libraryProcessType;
        if (BuildInfo.isDebugAndroid()) {
            // Only set up the tracing broadcast receiver on debug builds of the OS. Normal tracing
            // should use the DevTools API.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    addStartupCompletedObserver(new StartupCallback() {
                        @Override
                        public void onSuccess() {
                            assert mTracingController == null;
                            Context context = ContextUtils.getApplicationContext();
                            mTracingController = new TracingControllerAndroidImpl(context);
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
     * @param libraryProcessType the type of process the shared library is loaded. it must be
     *                           LibraryProcessType.PROCESS_BROWSER,
     *                           LibraryProcessType.PROCESS_WEBVIEW or
     *                           LibraryProcessType.PROCESS_WEBLAYER.
     * @return BrowserStartupController instance.
     */
    public static BrowserStartupController get(int libraryProcessType) {
        assert ThreadUtils.runningOnUiThread() : "Tried to start the browser on the wrong thread.";
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            assert LibraryProcessType.PROCESS_BROWSER == libraryProcessType
                    || LibraryProcessType.PROCESS_WEBVIEW == libraryProcessType
                    || LibraryProcessType.PROCESS_WEBLAYER == libraryProcessType;
            sInstance = new BrowserStartupControllerImpl(libraryProcessType);
        }
        assert sInstance.mLibraryProcessType == libraryProcessType : "Wrong process type";
        return sInstance;
    }

    @VisibleForTesting
    public static void overrideInstanceForTest(BrowserStartupController controller) {
        sInstance = (BrowserStartupControllerImpl) controller;
    }

    @Override
    public void startBrowserProcessesAsync(boolean startGpuProcess, boolean startServiceManagerOnly,
            final StartupCallback callback) {
        assert ThreadUtils.runningOnUiThread() : "Tried to start the browser on the wrong thread.";
        ServicificationStartupUma.getInstance().record(ServicificationStartupUma.getStartupMode(
                mFullBrowserStartupDone, mServiceManagerStarted, startServiceManagerOnly));

        if (mFullBrowserStartupDone || (startServiceManagerOnly && mServiceManagerStarted)) {
            // Browser process initialization has already been completed, so we can immediately post
            // the callback.
            postStartupCompleted(callback);
            return;
        }

        // Browser process has not been fully started yet, so we defer executing the callback.
        if (startServiceManagerOnly) {
            mServiceManagerCallbacks.add(callback);
        } else {
            mAsyncStartupCallbacks.add(callback);
        }
        // If the browser process is launched with ServiceManager only, we need to relaunch the full
        // process in serviceManagerStarted() if such a request was received.
        mLaunchFullBrowserAfterServiceManagerStart |=
                (mCurrentBrowserStartType == BrowserStartType.SERVICE_MANAGER_ONLY)
                && !startServiceManagerOnly;
        if (!mHasStartedInitializingBrowserProcess) {
            // This is the first time we have been asked to start the browser process. We set the
            // flag that indicates that we have kicked off starting the browser process.
            mHasStartedInitializingBrowserProcess = true;

            setShouldStartGpuProcessOnBrowserStartup(startGpuProcess);

            prepareToStartBrowserProcess(false, new Runnable() {
                @Override
                public void run() {
                    ThreadUtils.assertOnUiThread();
                    if (mHasCalledContentStart) return;
                    mCurrentBrowserStartType = startServiceManagerOnly
                            ? BrowserStartType.SERVICE_MANAGER_ONLY
                            : BrowserStartType.FULL_BROWSER;
                    if (contentStart() > 0) {
                        // Failed. The callbacks may not have run, so run them.
                        enqueueCallbackExecution(STARTUP_FAILURE);
                    }
                }
            });
        } else if (mServiceManagerStarted && mLaunchFullBrowserAfterServiceManagerStart) {
            // If we missed the serviceManagerStarted() call, launch the full browser now if needed.
            // Otherwise, serviceManagerStarted() will handle the full browser launch.
            mCurrentBrowserStartType = BrowserStartType.FULL_BROWSER;
            if (contentStart() > 0) enqueueCallbackExecution(STARTUP_FAILURE);
        }
    }

    @Override
    public void startBrowserProcessesSync(boolean singleProcess) {
        ServicificationStartupUma.getInstance().record(
                ServicificationStartupUma.getStartupMode(mFullBrowserStartupDone,
                        mServiceManagerStarted, false /* startServiceManagerOnly */));

        // If already started skip to checking the result
        if (!mFullBrowserStartupDone) {
            if (!mHasStartedInitializingBrowserProcess || !mPostResourceExtractionTasksCompleted) {
                prepareToStartBrowserProcess(singleProcess, null);
            }

            boolean startedSuccessfully = true;
            if (!mHasCalledContentStart) {
                mCurrentBrowserStartType = BrowserStartType.FULL_BROWSER;
                if (contentStart() > 0) {
                    // Failed. The callbacks may not have run, so run them.
                    enqueueCallbackExecution(STARTUP_FAILURE);
                    startedSuccessfully = false;
                }
            } else if (mCurrentBrowserStartType == BrowserStartType.SERVICE_MANAGER_ONLY) {
                mCurrentBrowserStartType = BrowserStartType.FULL_BROWSER;
                if (contentStart() > 0) {
                    enqueueCallbackExecution(STARTUP_FAILURE);
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

    /**
     * Start the browser process by calling ContentMain.start().
     */
    int contentStart() {
        int result = 0;
        if (mContentMainCallbackForTests == null) {
            boolean startServiceManagerOnly =
                    mCurrentBrowserStartType == BrowserStartType.SERVICE_MANAGER_ONLY;
            result = contentMainStart(startServiceManagerOnly);
            // No need to launch the full browser again if we are launching full browser now.
            if (!startServiceManagerOnly) mLaunchFullBrowserAfterServiceManagerStart = false;
        } else {
            assert mCurrentBrowserStartType == BrowserStartType.FULL_BROWSER;
            // Run the injected Runnable instead of ContentMain().
            mContentMainCallbackForTests.run();
            mLaunchFullBrowserAfterServiceManagerStart = false;
        }
        mHasCalledContentStart = true;
        return result;
    }

    @Override
    public void setContentMainCallbackForTests(Runnable r) {
        assert !mHasCalledContentStart;
        mContentMainCallbackForTests = r;
    }

    /**
     * Wrap ContentMain.start() for testing.
     */
    @VisibleForTesting
    int contentMainStart(boolean startServiceManagerOnly) {
        return ContentMain.start(startServiceManagerOnly);
    }

    @VisibleForTesting
    void flushStartupTasks() {
        BrowserStartupControllerImplJni.get().flushStartupTasks();
    }

    @Override
    public boolean isFullBrowserStarted() {
        ThreadUtils.assertOnUiThread();
        return mFullBrowserStartupDone && mStartupSuccess;
    }

    @Override
    public boolean isRunningInServiceManagerMode() {
        ThreadUtils.assertOnUiThread();
        return mServiceManagerStarted && !mFullBrowserStartupDone && mStartupSuccess;
    }

    @Override
    public boolean isNativeStarted() {
        ThreadUtils.assertOnUiThread();
        return (mServiceManagerStarted || mFullBrowserStartupDone) && mStartupSuccess;
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
    public @ServicificationStartup int getStartupMode(boolean startServiceManagerOnly) {
        return ServicificationStartupUma.getStartupMode(
                mFullBrowserStartupDone, mServiceManagerStarted, startServiceManagerOnly);
    }

    /**
     * Called when ServiceManager is launched.
     */
    private void serviceManagerStarted() {
        mServiceManagerStarted = true;
        if (mLaunchFullBrowserAfterServiceManagerStart) {
            // If startFullBrowser() fails, execute the callbacks right away. Otherwise,
            // callbacks will be deferred until browser startup completes.
            mCurrentBrowserStartType = BrowserStartType.FULL_BROWSER;
            if (contentStart() > 0) enqueueCallbackExecution(STARTUP_FAILURE);
            return;
        }

        if (mCurrentBrowserStartType == BrowserStartType.SERVICE_MANAGER_ONLY) {
            executeServiceManagerCallbacks(STARTUP_SUCCESS);
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

        executeServiceManagerCallbacks(startupResult);
        recordStartupUma();
    }

    private void executeServiceManagerCallbacks(int startupResult) {
        mStartupSuccess = (startupResult <= 0);
        for (StartupCallback serviceMangerCallback : mServiceManagerCallbacks) {
            if (mStartupSuccess) {
                serviceMangerCallback.onSuccess();
            } else {
                serviceMangerCallback.onFailure();
            }
        }
        mServiceManagerCallbacks.clear();
    }

    // Queue the callbacks to run. Since running the callbacks clears the list it is safe to call
    // this more than once.
    private void enqueueCallbackExecution(final int startupFailure) {
        PostTask.postTask(UiThreadTaskTraits.BOOTSTRAP, new Runnable() {
            @Override
            public void run() {
                executeEnqueuedCallbacks(startupFailure);
            }
        });
    }

    private void postStartupCompleted(final StartupCallback callback) {
        PostTask.postTask(UiThreadTaskTraits.BOOTSTRAP, new Runnable() {
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
    void prepareToStartBrowserProcess(
            final boolean singleProcess, final Runnable completionCallback) {
        Log.d(TAG, "Initializing chromium process, singleProcess=%b", singleProcess);

        // This strictmode exception is to cover the case where the browser process is being started
        // asynchronously but not in the main browser flow.  The main browser flow will trigger
        // library loading earlier and this will be a no-op, but in the other cases this will need
        // to block on loading libraries.
        // This applies to tests and ManageSpaceActivity, which can be launched from Settings.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            // Normally Main.java will have already loaded the library asynchronously, we only need
            // to load it here if we arrived via another flow, e.g. bookmark access & sync setup.
            LibraryLoader.getInstance().ensureInitialized(mLibraryProcessType);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        Runnable postResourceExtraction = new Runnable() {
            @Override
            public void run() {
                if (!mPostResourceExtractionTasksCompleted) {
                    // TODO(yfriedman): Remove dependency on a command line flag for this.
                    DeviceUtilsImpl.addDeviceSpecificUserAgentSwitch();
                    BrowserStartupControllerImplJni.get().setCommandLineFlags(singleProcess);
                    mPostResourceExtractionTasksCompleted = true;
                }

                if (completionCallback != null) completionCallback.run();
            }
        };

        ResourceExtractor.get().setResultTraits(UiThreadTaskTraits.BOOTSTRAP);
        if (completionCallback == null) {
            // If no continuation callback is specified, then force the resource extraction
            // to complete.
            ResourceExtractor.get().waitForCompletion();
            postResourceExtraction.run();
        } else {
            ResourceExtractor.get().addCompletionCallback(postResourceExtraction);
        }
    }

    /**
     * Can be overridden by testing.
     */
    @VisibleForTesting
    void recordStartupUma() {
        ServicificationStartupUma.getInstance().commit();
    }

    @NativeMethods
    interface Natives {
        void setCommandLineFlags(boolean singleProcess);
        // Is this an official build of Chrome? Only native code knows for sure. Official build
        // knowledge is needed very early in process startup.
        boolean isOfficialBuild();

        void flushStartupTasks();
    }
}
