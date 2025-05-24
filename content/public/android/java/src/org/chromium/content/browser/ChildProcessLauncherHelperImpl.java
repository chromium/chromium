// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ChildBindingState;
import org.chromium.base.ContextUtils;
import org.chromium.base.CpuFeatures;
import org.chromium.base.EarlyTraceEvent;
import org.chromium.base.JavaExceptionReporter;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryLoader.MultiProcessMediator;
import org.chromium.base.process_launcher.ChildConnectionAllocator;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.process_launcher.ChildProcessConstants;
import org.chromium.base.process_launcher.ChildProcessLauncher;
import org.chromium.base.process_launcher.IChildProcessArgs;
import org.chromium.base.process_launcher.IFileDescriptorInfo;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content.app.SandboxedProcessService;
import org.chromium.content.common.ContentSwitchUtils;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.content_public.common.ContentSwitches;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.annotation.concurrent.GuardedBy;

/**
 * This is the java counterpart to ChildProcessLauncherHelper. It is owned by native side and has an
 * explicit destroy method. Each public or jni methods should have explicit documentation on what
 * threads they are called.
 */
@JNINamespace("content::internal")
@NullMarked
public final class ChildProcessLauncherHelperImpl {
    private static final String TAG = "ChildProcLH";

    // Manifest values used to specify the service names.
    private static final String NUM_SANDBOXED_SERVICES_KEY =
            "org.chromium.content.browser.NUM_SANDBOXED_SERVICES";
    private static final String NUM_PRIVILEGED_SERVICES_KEY =
            "org.chromium.content.browser.NUM_PRIVILEGED_SERVICES";

    // When decrementing the refcount on bindings, delay the decrement by this amount of time in
    // case a new ref count is added in the mean time. This is a heuristic to avoid temporarily
    // dropping bindings when inputs to calculating importance change independently.
    private static final int REMOVE_BINDING_DELAY_MS = 500;

    // To be conservative, only delay removing binding in the initial second of the process.
    private static final int TIMEOUT_FOR_DELAY_BINDING_REMOVE_MS = 1000;

    // Delay after app is background before reducing process priority.
    private static final int REDUCE_PRIORITY_ON_BACKGROUND_DELAY_MS = 9 * 1000;

    private static final Runnable sDelayedBackgroundTask =
            ChildProcessLauncherHelperImpl::onSentToBackgroundOnLauncherThreadAfterDelay;

    // Flag to check if ServiceGroupImportance should be enabled after native is initialized.
    private static boolean sCheckedServiceGroupImportance;

    // A warmed-up connection to a sandboxed service.
    private static @Nullable SpareChildConnection sSpareSandboxedConnection;

    // Allocator used for sandboxed services.
    private static @Nullable ChildConnectionAllocator sSandboxedChildConnectionAllocator;
    private static @Nullable ChildProcessRanking sSandboxedChildConnectionRanking;

    // Map from PID to ChildProcessLauncherHelper.
    private static final Map<Integer, ChildProcessLauncherHelperImpl> sLauncherByPid =
            new HashMap<>();

    // Allocator used for non-sandboxed services.
    private static @Nullable ChildConnectionAllocator sPrivilegedChildConnectionAllocator;

    // Used by tests to override the default sandboxed service allocator settings.
    private static ChildConnectionAllocator.@Nullable ConnectionFactory
            sSandboxedServiceFactoryForTesting;
    private static int sSandboxedServicesCountForTesting = -1;
    private static @Nullable String sSandboxedServicesNameForTesting;
    private static boolean sSkipDelayForReducePriorityOnBackgroundForTesting;

    private static @Nullable BindingManager sBindingManager;

    // Whether the main application is currently brought to the foreground.
    private static boolean sApplicationInForegroundOnUiThread;

    // Set on UI thread only, but null-checked on launcher thread as well.
    private static ApplicationStatus.@Nullable ApplicationStateListener sAppStateListener;

    // TODO(boliu): These are only set for sandboxed renderer processes. Generalize them for
    // all types of processes.
    private final @Nullable ChildProcessRanking mRanking;
    private final @Nullable BindingManager mBindingManager;

    // Whether the created process should be sandboxed.
    private final boolean mSandboxed;

    // Remove strong binding when app is in background.
    private final boolean mReducePriorityOnBackground;

    // The type of process as determined by the command line.
    private final @Nullable String mProcessType;

    // Whether the process can use warmed up connection.
    private final boolean mCanUseWarmUpConnection;

    // Tracks reporting exception from child process to avoid reporting it more than once.
    private boolean mReportedException;

    // Enables early Java tracing in child process before native is initialized.
    private static final String TRACE_EARLY_JAVA_IN_CHILD_SWITCH =
            "--" + EarlyTraceEvent.TRACE_EARLY_JAVA_IN_CHILD_SWITCH;

    // The first known App Zygote PID. If the app zygote gets restarted, the new bundles from it
    // are not sent further for simplicity. Accessed only on LauncherThread.
    private static int sZygotePid;

    // The bundle with RELRO FD. For sending to child processes, including the ones that did not
    // announce whether they inherit from the app zygote. Declared as volatile to allow sending it
    // from different threads.
    private static volatile @Nullable Bundle sZygoteBundle;

    private static boolean sIgnoreMainFrameVisibilityForImportance;

    private final ChildProcessLauncher.Delegate mLauncherDelegate =
            new ChildProcessLauncher.Delegate() {
                @Override
                public @Nullable ChildProcessConnection getBoundConnection(
                        ChildConnectionAllocator connectionAllocator,
                        ChildProcessConnection.ServiceCallback serviceCallback) {
                    if (!mCanUseWarmUpConnection) return null;
                    SpareChildConnection spareConnection =
                            mSandboxed ? sSpareSandboxedConnection : null;
                    if (spareConnection == null) return null;
                    return spareConnection.getConnection(connectionAllocator, serviceCallback);
                }

                @Override
                public void onBeforeConnectionAllocated(Bundle bundle) {
                    populateServiceBundle(bundle);
                }

                @Override
                public void onBeforeConnectionSetup(IChildProcessArgs childProcessArgs) {
                    // Populate the bundle passed to the service setup call with content specific
                    // parameters.
                    childProcessArgs.cpuCount = CpuFeatures.getCount();
                    childProcessArgs.cpuFeatures = CpuFeatures.getMask();
                    Bundle relros = sZygoteBundle;
                    if (relros == null) {
                        relros = LibraryLoader.getInstance().getMediator().getSharedRelrosBundle();
                    }
                    childProcessArgs.relroBundle = relros;
                }

                @Override
                public void onConnectionEstablished(ChildProcessConnection connection) {
                    assert LauncherThread.runningOnLauncherThread();
                    int pid = connection.getPid();

                    if (pid > 0) {
                        sLauncherByPid.put(pid, ChildProcessLauncherHelperImpl.this);
                        if (mRanking != null) {
                            // TODO(crbug.com/409703175): Set isSpareRenderer once the
                            // spare renderer information is passed when launching the
                            // process.
                            mRanking.addConnection(
                                    connection,
                                    /* visible= */ false,
                                    /* frameDepth= */ 1,
                                    /* intersectsViewport= */ false,
                                    /* isSpareRenderer= */ false,
                                    ChildProcessImportance.MODERATE);
                            if (mBindingManager != null) mBindingManager.rankingChanged();
                        }
                        if (mSandboxed) {
                            ChildProcessConnectionMetrics.getInstance().addConnection(connection);
                        }
                        if (mReducePriorityOnBackground
                                && !ApplicationStatus.hasVisibleActivities()) {
                            reducePriorityOnBackgroundOnLauncherThread();
                        }
                    }

                    // Tell native launch result (whether getPid is 0).
                    if (mNativeChildProcessLauncherHelper != 0) {
                        ChildProcessLauncherHelperImplJni.get()
                                .onChildProcessStarted(
                                        mNativeChildProcessLauncherHelper, connection.getPid());
                    }
                    mNativeChildProcessLauncherHelper = 0;
                }

                @Override
                public void onReceivedZygoteInfo(
                        ChildProcessConnection connection, Bundle relroBundle) {
                    assert LauncherThread.runningOnLauncherThread();
                    distributeZygoteInfo(connection, relroBundle);
                }

                @Override
                public void onConnectionLost(ChildProcessConnection connection) {
                    assert LauncherThread.runningOnLauncherThread();
                    if (connection.getPid() == 0) return;

                    ChildProcessLauncherHelperImpl result =
                            sLauncherByPid.remove(connection.getPid());
                    // Child process might die before onConnectionEstablished.
                    if (result == null) return;

                    if (mBindingManager != null) mBindingManager.removeConnection(connection);
                    if (mRanking != null) {
                        mRanking.removeConnection(connection);
                        if (mBindingManager != null) mBindingManager.rankingChanged();
                    }
                    if (mSandboxed) {
                        ChildProcessConnectionMetrics.getInstance().removeConnection(connection);
                    }
                }
            };

    /**
     * Called for every new child connection. Receives a possibly null bundle inherited from the App
     * Zygote. Sends the bundle to existing processes that did not have usable bundles or sends
     * a previously memoized bundle to the new child.
     *
     * @param connection the connection to the new child
     * @param zygoteBundle the bundle received from the child process, null means that either the
     *                     process did not inherit from the app zygote or the app zygote did not
     *                     produce a usable RELRO region.
     */
    private static void distributeZygoteInfo(
            ChildProcessConnection connection, @Nullable Bundle zygoteBundle) {
        if (LibraryLoader.mainProcessIntendsToProvideRelroFd()) return;

        if (!connection.hasUsableZygoteInfo()) {
            Log.d(TAG, "Connection likely not created from app zygote");
            sendPreviouslySeenZygoteBundle(connection);
            return;
        }

        // If the process was created from the app zygote, but failed to generate the the zygote
        // bundle - ignore it.
        if (zygoteBundle == null) {
            return;
        }

        if (sZygotePid != 0) {
            Log.d(TAG, "Zygote was seen before with a usable RELRO bundle.");
            onObtainedUsableZygoteBundle(connection);
            return;
        }

        Log.d(TAG, "Encountered the first usable RELRO bundle.");
        sZygotePid = connection.getZygotePid();
        sZygoteBundle = zygoteBundle;

        // Use the RELRO FD in the current process. Some nontrivial CPU cycles are consumed because
        // it needs an mmap+memcmp(5 megs)+mmap+munmap. This happens on the process launcher thread,
        // will work correctly on any thread.
        LibraryLoader.getInstance().getMediator().takeSharedRelrosFromBundle(zygoteBundle);

        // Use the RELRO FD for all processes launched up to now. Non-blocking 'oneway' IPCs are
        // used. The CPU time costs in the child process are the same.
        sendPreviouslySeenZygoteBundleToExistingConnections(connection.getPid());
    }

    private static void onObtainedUsableZygoteBundle(ChildProcessConnection connection) {
        if (sZygotePid != connection.getZygotePid()) {
            Log.d(TAG, "Zygote restarted.");
            return;
        }
        // TODO(pasko): To avoid accumulating open file descriptors close the received RELRO FD
        // if it cannot be used.
    }

    private static void sendPreviouslySeenZygoteBundle(ChildProcessConnection connection) {
        if (sZygotePid != 0 && sZygoteBundle != null) {
            connection.consumeZygoteBundle(sZygoteBundle);
        }
    }

    private static void sendPreviouslySeenZygoteBundleToExistingConnections(int pid) {
        assumeNonNull(sZygoteBundle);
        for (var entry : sLauncherByPid.entrySet()) {
            int otherPid = entry.getKey();
            if (pid != otherPid) {
                ChildProcessConnection otherConnection =
                        assumeNonNull(entry.getValue().mLauncher.getConnection());
                if (otherConnection.getZygotePid() == 0) {
                    // The Zygote PID for each connection must be finalized before the launcher
                    // thread starts processing the zygote info. Zygote PID being 0 guarantees that
                    // the zygote did not produce the RELRO region.
                    otherConnection.consumeZygoteBundle(sZygoteBundle);
                }
            }
        }
    }

    private final ChildProcessLauncher mLauncher;

    private long mNativeChildProcessLauncherHelper;

    private long mStartTimeMs;

    // This is the current computed importance from all the inputs from setPriority.
    // The initial value is MODERATE since a newly created connection has visible bindings.
    private @ChildProcessImportance int mEffectiveImportance = ChildProcessImportance.MODERATE;
    private boolean mVisible;

    private boolean mDroppedStrongBingingDueToBackgrounding;

    private final Object mIsSpareRendererLock = new Object();

    @GuardedBy("mIsSpareRendererLock")
    private boolean mIsSpareRenderer;

    @CalledByNative
    private static IFileDescriptorInfo @Nullable [] makeFdInfos(
            @JniType("std::vector<int32_t>") int[] ids,
            @JniType("std::vector<int32_t>") int[] fds,
            @JniType("std::vector<bool>") boolean[] autoCloses,
            @JniType("std::vector<int64_t>") long[] offsets,
            @JniType("std::vector<int64_t>") long[] sizes) {
        assert LauncherThread.runningOnLauncherThread();
        IFileDescriptorInfo[] fileDescriptorInfos = new IFileDescriptorInfo[ids.length];
        for (int i = 0; i < ids.length; i++) {
            ParcelFileDescriptor pFd;
            if (autoCloses[i]) {
                // Adopt the FD, it will be closed when we close the ParcelFileDescriptor.
                pFd = ParcelFileDescriptor.adoptFd(fds[i]);
            } else {
                try {
                    pFd = ParcelFileDescriptor.fromFd(fds[i]);
                } catch (IOException e) {
                    Log.e(
                            TAG,
                            "Invalid FD provided for process connection, id: "
                                    + ids[i]
                                    + " fd: "
                                    + fds[i]);
                    return null;
                }
            }
            IFileDescriptorInfo fileDescriptorInfo = new IFileDescriptorInfo();
            fileDescriptorInfo.id = ids[i];
            fileDescriptorInfo.fd = pFd;
            fileDescriptorInfo.size = sizes[i];
            fileDescriptorInfo.offset = offsets[i];
            fileDescriptorInfos[i] = fileDescriptorInfo;
        }
        return fileDescriptorInfos;
    }

    @CalledByNative
    private static ChildProcessLauncherHelperImpl createAndStart(
            long nativePointer,
            String[] commandLine,
            IFileDescriptorInfo[] filesToBeMapped,
            boolean canUseWarmUpConnection,
            @Nullable IBinder binderBox) {
        assert LauncherThread.runningOnLauncherThread();
        String processType =
                ContentSwitchUtils.getSwitchValue(commandLine, ContentSwitches.SWITCH_PROCESS_TYPE);

        if (TraceEvent.enabled()) {
            commandLine = Arrays.copyOf(commandLine, commandLine.length + 1);
            commandLine[commandLine.length - 1] = TRACE_EARLY_JAVA_IN_CHILD_SWITCH;
        }
        boolean sandboxed = true;
        boolean reducePriorityOnBackground = false;
        if (!ContentSwitches.SWITCH_RENDERER_PROCESS.equals(processType)) {
            if (ContentSwitches.SWITCH_GPU_PROCESS.equals(processType)) {
                sandboxed = false;
                reducePriorityOnBackground =
                        ContentFeatureMap.isEnabled(
                                ContentFeatures.REDUCE_GPU_PRIORITY_ON_BACKGROUND);
            } else {
                // We only support sandboxed utility processes now.
                assert ContentSwitches.SWITCH_UTILITY_PROCESS.equals(processType);

                if (ContentSwitches.NONE_SANDBOX_TYPE.equals(
                        ContentSwitchUtils.getSwitchValue(
                                commandLine, ContentSwitches.SWITCH_SERVICE_SANDBOX_TYPE))) {
                    sandboxed = false;
                }
            }
        }

        IBinder binderCallback =
                ContentSwitches.SWITCH_GPU_PROCESS.equals(processType)
                        ? new GpuProcessCallback()
                        : null;

        ChildProcessLauncherHelperImpl helper =
                new ChildProcessLauncherHelperImpl(
                        nativePointer,
                        commandLine,
                        filesToBeMapped,
                        sandboxed,
                        reducePriorityOnBackground,
                        canUseWarmUpConnection,
                        binderCallback,
                        binderBox);
        helper.start();

        if (sandboxed && !sCheckedServiceGroupImportance) {
            sCheckedServiceGroupImportance = true;
            if (sSandboxedChildConnectionRanking != null
                    && ChildProcessLauncherHelperImplJni.get().serviceGroupImportanceEnabled()) {
                sSandboxedChildConnectionRanking.enableServiceGroupImportance();
            }
        }
        return helper;
    }

    /**
     * @see {@link ChildProcessLauncherHelper#warmUp(Context)}.
     */
    public static void warmUpOnAnyThread(final Context context) {
        LauncherThread.post(
                new Runnable() {
                    @Override
                    public void run() {
                        warmUpOnLauncherThread(context);
                    }
                });
    }

    private static void warmUpOnLauncherThread(Context context) {
        if (sSpareSandboxedConnection != null && !sSpareSandboxedConnection.isEmpty()) {
            return;
        }

        Bundle serviceBundle = populateServiceBundle(new Bundle());
        ChildConnectionAllocator allocator = getConnectionAllocator(context, /* sandboxed= */ true);
        sSpareSandboxedConnection = new SpareChildConnection(context, allocator, serviceBundle);
    }

    /**
     * @see {@link ChildProcessLauncherHelper#startBindingManagement(Context)}.
     */
    public static void startBindingManagement(final Context context) {
        assert ThreadUtils.runningOnUiThread();
        LauncherThread.post(
                new Runnable() {
                    @Override
                    public void run() {
                        assumeNonNull(sSandboxedChildConnectionRanking);
                        ChildConnectionAllocator allocator =
                                getConnectionAllocator(context, /* sandboxed= */ true);
                        if (ChildProcessConnection.supportVariableConnections()) {
                            sBindingManager =
                                    new BindingManager(
                                            context,
                                            BindingManager.NO_MAX_SIZE,
                                            sSandboxedChildConnectionRanking);
                        } else {
                            sBindingManager =
                                    new BindingManager(
                                            context,
                                            allocator.getMaxNumberOfAllocations(),
                                            sSandboxedChildConnectionRanking);
                        }
                        ChildProcessConnectionMetrics.getInstance()
                                .setBindingManager(sBindingManager);
                    }
                });
    }

    private static void onSentToBackground() {
        assert ThreadUtils.runningOnUiThread();
        sApplicationInForegroundOnUiThread = false;
        int delay =
                sSkipDelayForReducePriorityOnBackgroundForTesting
                        ? 0
                        : REDUCE_PRIORITY_ON_BACKGROUND_DELAY_MS;
        LauncherThread.postDelayed(sDelayedBackgroundTask, delay);
        LauncherThread.post(
                () -> {
                    if (sBindingManager != null) sBindingManager.onSentToBackground();
                });
    }

    private static void onSentToBackgroundOnLauncherThreadAfterDelay() {
        assert LauncherThread.runningOnLauncherThread();
        for (ChildProcessLauncherHelperImpl helper : sLauncherByPid.values()) {
            if (!helper.mReducePriorityOnBackground) continue;
            helper.reducePriorityOnBackgroundOnLauncherThread();
        }
    }

    private void reducePriorityOnBackgroundOnLauncherThread() {
        assert LauncherThread.runningOnLauncherThread();
        if (mDroppedStrongBingingDueToBackgrounding) return;
        ChildProcessConnection connection = assumeNonNull(mLauncher.getConnection());
        if (!connection.isConnected()) return;
        if (connection.isStrongBindingBound()) {
            connection.removeStrongBinding();
            mDroppedStrongBingingDueToBackgrounding = true;
        }
    }

    private void raisePriorityOnForegroundOnLauncherThread() {
        assert LauncherThread.runningOnLauncherThread();
        if (!mDroppedStrongBingingDueToBackgrounding) return;
        ChildProcessConnection connection = assumeNonNull(mLauncher.getConnection());
        if (!connection.isConnected()) return;
        connection.addStrongBinding();
        mDroppedStrongBingingDueToBackgrounding = false;
    }

    private static void onBroughtToForeground() {
        assert ThreadUtils.runningOnUiThread();
        sApplicationInForegroundOnUiThread = true;
        LauncherThread.removeCallbacks(sDelayedBackgroundTask);
        LauncherThread.post(
                () -> {
                    for (ChildProcessLauncherHelperImpl helper : sLauncherByPid.values()) {
                        if (!helper.mReducePriorityOnBackground) continue;
                        helper.raisePriorityOnForegroundOnLauncherThread();
                    }
                    if (sBindingManager != null) sBindingManager.onBroughtToForeground();
                });
    }

    public static void setSandboxServicesSettingsForTesting(
            ChildConnectionAllocator.ConnectionFactory factory,
            int serviceCount,
            String serviceName) {
        sSandboxedServiceFactoryForTesting = factory;
        sSandboxedServicesCountForTesting = serviceCount;
        sSandboxedServicesNameForTesting = serviceName;
    }

    public static void setSkipDelayForReducePriorityOnBackgroundForTesting() {
        sSkipDelayForReducePriorityOnBackgroundForTesting = true;
    }

    public static void setIgnoreMainFrameVisibilityForImportance() {
        sIgnoreMainFrameVisibilityForImportance = true;
    }

    @VisibleForTesting
    static ChildConnectionAllocator getConnectionAllocator(Context context, boolean sandboxed) {
        assert LauncherThread.runningOnLauncherThread();
        boolean bindToCaller = ChildProcessCreationParamsImpl.getBindToCallerCheck();
        boolean bindAsExternalService =
                sandboxed && ChildProcessCreationParamsImpl.getIsSandboxedServiceExternal();

        if (!sandboxed) {
            if (sPrivilegedChildConnectionAllocator == null) {
                boolean fallbackToNextSlot =
                        ContentFeatureMap.isEnabled(ContentFeatures.ANDROID_FALLBACK_TO_NEXT_SLOT);
                sPrivilegedChildConnectionAllocator =
                        ChildConnectionAllocator.create(
                                context,
                                LauncherThread.getHandler(),
                                null,
                                ChildProcessCreationParamsImpl.getPackageNameForPrivilegedService(),
                                ChildProcessCreationParamsImpl.getPrivilegedServicesName(),
                                NUM_PRIVILEGED_SERVICES_KEY,
                                bindToCaller,
                                bindAsExternalService,
                                /* useStrongBinding= */ true,
                                fallbackToNextSlot,
                                sandboxed);
            }
            return sPrivilegedChildConnectionAllocator;
        }

        if (sSandboxedChildConnectionAllocator == null) {
            final String packageName =
                    ChildProcessCreationParamsImpl.getPackageNameForSandboxedService();
            Log.d(
                    TAG,
                    "Create a new ChildConnectionAllocator with package name = %s,"
                            + " sandboxed = true",
                    packageName);
            Runnable freeSlotRunnable =
                    () -> {
                        assumeNonNull(sSandboxedChildConnectionRanking);
                        ChildProcessConnection lowestRank =
                                sSandboxedChildConnectionRanking.getLowestRankedConnection();
                        if (lowestRank != null) {
                            lowestRank.kill();
                        }
                    };

            ChildConnectionAllocator connectionAllocator = null;
            if (sSandboxedServicesCountForTesting != -1) {
                // Testing case where allocator settings are overridden.
                String serviceName =
                        !TextUtils.isEmpty(sSandboxedServicesNameForTesting)
                                ? sSandboxedServicesNameForTesting
                                : SandboxedProcessService.class.getName();
                connectionAllocator =
                        ChildConnectionAllocator.createFixedForTesting(
                                freeSlotRunnable,
                                packageName,
                                serviceName,
                                sSandboxedServicesCountForTesting,
                                bindToCaller,
                                bindAsExternalService,
                                /* useStrongBinding= */ false,
                                /* fallbackToNextSlot= */ false,
                                sandboxed);
            } else if (ChildProcessConnection.supportVariableConnections()) {
                connectionAllocator =
                        ChildConnectionAllocator.createVariableSize(
                                context,
                                LauncherThread.getHandler(),
                                freeSlotRunnable,
                                packageName,
                                ChildProcessCreationParamsImpl.getSandboxedServicesName(),
                                bindToCaller,
                                bindAsExternalService,
                                /* useStrongBinding= */ false,
                                sandboxed);
            } else {
                connectionAllocator =
                        ChildConnectionAllocator.create(
                                context,
                                LauncherThread.getHandler(),
                                freeSlotRunnable,
                                packageName,
                                ChildProcessCreationParamsImpl.getSandboxedServicesName(),
                                NUM_SANDBOXED_SERVICES_KEY,
                                bindToCaller,
                                bindAsExternalService,
                                /* useStrongBinding= */ false,
                                /* fallbackToNextSlot= */ false,
                                sandboxed);
            }
            if (sSandboxedServiceFactoryForTesting != null) {
                connectionAllocator.setConnectionFactoryForTesting(
                        sSandboxedServiceFactoryForTesting);
            }
            sSandboxedChildConnectionAllocator = connectionAllocator;
            if (ChildProcessConnection.supportVariableConnections()) {
                sSandboxedChildConnectionRanking = new ChildProcessRanking();
            } else {
                sSandboxedChildConnectionRanking =
                        new ChildProcessRanking(
                                sSandboxedChildConnectionAllocator.getMaxNumberOfAllocations());
            }
        }
        return sSandboxedChildConnectionAllocator;
    }

    private ChildProcessLauncherHelperImpl(
            long nativePointer,
            String[] commandLine,
            IFileDescriptorInfo[] filesToBeMapped,
            boolean sandboxed,
            boolean reducePriorityOnBackground,
            boolean canUseWarmUpConnection,
            @Nullable IBinder binderCallback,
            @Nullable IBinder binderBox) {
        assert LauncherThread.runningOnLauncherThread();

        mNativeChildProcessLauncherHelper = nativePointer;
        mSandboxed = sandboxed;
        mReducePriorityOnBackground = reducePriorityOnBackground;
        mCanUseWarmUpConnection = canUseWarmUpConnection;
        ChildConnectionAllocator connectionAllocator =
                getConnectionAllocator(ContextUtils.getApplicationContext(), sandboxed);

        mLauncher =
                new ChildProcessLauncher(
                        LauncherThread.getHandler(),
                        mLauncherDelegate,
                        commandLine,
                        filesToBeMapped,
                        connectionAllocator,
                        binderCallback == null ? null : Arrays.asList(binderCallback),
                        binderBox);
        mProcessType =
                ContentSwitchUtils.getSwitchValue(commandLine, ContentSwitches.SWITCH_PROCESS_TYPE);

        if (sandboxed) {
            mRanking = sSandboxedChildConnectionRanking;
            mBindingManager = sBindingManager;
        } else {
            mRanking = null;
            mBindingManager = null;
        }

        if (!ApplicationStatus.isInitialized()) return;
        if (sAppStateListener != null) return;
        PostTask.postTask(
                TaskTraits.UI_BEST_EFFORT,
                () -> {
                    if (sAppStateListener != null) return;
                    sApplicationInForegroundOnUiThread = ApplicationStatus.hasVisibleActivities();
                    sAppStateListener =
                            newState -> {
                                switch (newState) {
                                    case ApplicationState.UNKNOWN:
                                        break;
                                    case ApplicationState.HAS_RUNNING_ACTIVITIES:
                                    case ApplicationState.HAS_PAUSED_ACTIVITIES:
                                        if (!sApplicationInForegroundOnUiThread) {
                                            onBroughtToForeground();
                                        }
                                        break;
                                    default:
                                        if (sApplicationInForegroundOnUiThread) {
                                            onSentToBackground();
                                        }
                                        break;
                                }
                            };
                    ApplicationStatus.registerApplicationStateListener(sAppStateListener);
                });
    }

    private void start() {
        mLauncher.start(/* setupConnection= */ true, /* queueIfNoFreeConnection= */ true);
        mStartTimeMs = System.currentTimeMillis();
    }

    /**
     * @return The type of process as specified in the command line at
     * {@link ContentSwitches#SWITCH_PROCESS_TYPE}.
     */
    private String getProcessType() {
        return TextUtils.isEmpty(mProcessType) ? "" : mProcessType;
    }

    private boolean getIsSpareRenderer() {
        synchronized (mIsSpareRendererLock) {
            return mIsSpareRenderer;
        }
    }

    private void setIsSpareRenderer(boolean isSpareRenderer) {
        synchronized (mIsSpareRendererLock) {
            mIsSpareRenderer = isSpareRenderer;
        }
    }

    // Called on client (UI or IO) thread.
    @CalledByNative
    private void getTerminationInfoAndStop(long terminationInfoPtr) {
        ChildProcessConnection connection = mLauncher.getConnection();
        // Here we are accessing the connection from a thread other than the launcher thread, but it
        // does not change once it's been set. So it is safe to test whether it's null here and
        // access it afterwards.
        if (connection == null) return;

        boolean isSpareRenderer;
        synchronized (mIsSpareRendererLock) {
            isSpareRenderer = mIsSpareRenderer;
        }

        // Note there is no guarantee that connection lost has happened. However ChildProcessRanking
        // is not thread safe, so this is the best we can do.
        String exceptionString = connection.getExceptionDuringInit();
        if (exceptionString != null && !mReportedException) {
            mReportedException = true;
            PostTask.postTask(
                    TaskTraits.UI_BEST_EFFORT,
                    () -> JavaExceptionReporter.reportStackTrace(exceptionString));
        }
        ChildProcessLauncherHelperImplJni.get()
                .setTerminationInfo(
                        terminationInfoPtr,
                        connection.bindingStateCurrentOrWhenDied(),
                        connection.isKilledByUs(),
                        connection.hasCleanExit(),
                        exceptionString != null,
                        isSpareRenderer);
        LauncherThread.post(() -> mLauncher.stop());
    }

    @VisibleForTesting
    @CalledByNative
    void setPriority(
            int pid,
            boolean visible,
            boolean hasMediaStream,
            boolean hasImmersiveXrSession,
            boolean hasForegroundServiceWorker,
            long frameDepth,
            boolean intersectsViewport,
            boolean boostForPendingViews,
            boolean boostForLoading,
            boolean isSpareRenderer,
            @ChildProcessImportance int importance) {
        assert LauncherThread.runningOnLauncherThread();
        assert mLauncher.getPid() == pid
                : "The provided pid ("
                        + pid
                        + ") did not match the launcher's pid ("
                        + mLauncher.getPid()
                        + ").";
        if (getByPid(pid) == null) {
            // Child already disconnected. Ignore any trailing calls.
            return;
        }

        ChildProcessConnection connection = assumeNonNull(mLauncher.getConnection());

        if (ChildProcessCreationParamsImpl.getIgnoreVisibilityForImportance()) {
            visible = false;
            boostForPendingViews = false;
        }

        boolean shouldUseMainFrameVisibility = !sIgnoreMainFrameVisibilityForImportance;
        boolean isVisibleMainFrame = visible && frameDepth == 0;
        @ChildProcessImportance int newEffectiveImportance;

        if ((shouldUseMainFrameVisibility && isVisibleMainFrame)
                || importance == ChildProcessImportance.IMPORTANT
                || hasMediaStream
                || hasImmersiveXrSession) {
            newEffectiveImportance = ChildProcessImportance.IMPORTANT;
        } else if ((visible && frameDepth > 0 && intersectsViewport)
                || boostForPendingViews
                || importance == ChildProcessImportance.MODERATE
                || hasForegroundServiceWorker
                || boostForLoading) {
            newEffectiveImportance = ChildProcessImportance.MODERATE;
        } else if (importance == ChildProcessImportance.PERCEPTIBLE
                && ChildProcessConnection.supportNotPerceptibleBinding()) {
            newEffectiveImportance = ChildProcessImportance.PERCEPTIBLE;
        } else {
            newEffectiveImportance = ChildProcessImportance.NORMAL;
        }

        // Add first and remove second.
        if (visible && !mVisible) {
            if (mBindingManager != null) mBindingManager.addConnection(connection);
        }
        mVisible = visible;

        if (mEffectiveImportance != newEffectiveImportance) {
            switch (newEffectiveImportance) {
                case ChildProcessImportance.NORMAL:
                    // Nothing to add.
                    break;
                case ChildProcessImportance.PERCEPTIBLE:
                    // Use not-perceptible binding for protected tabs. A service binding which leads
                    // to PERCEPTIBLE_APP_ADJ (= 200) is ideal for protected tabs, but Android does
                    // not provide the service binding yet.
                    // TODO(crbug.com/400602112): Use Context.BIND_NOT_VISIBLE binding instead.
                    //
                    // This binding is out of control of BindingManager which always unbinds the
                    // lowest ranked process from not-perceptible binding by
                    // ensureLowestRankIsWaived().
                    //
                    // Note that ChildProcessConnection.supportNotPerceptibleBinding() is checked
                    // above on setting ChildProcessImportance.PERCEPTIBLE.
                    connection.addNotPerceptibleBinding();
                    break;
                case ChildProcessImportance.MODERATE:
                    connection.addVisibleBinding();
                    break;
                case ChildProcessImportance.IMPORTANT:
                    connection.addStrongBinding();
                    break;
                default:
                    assert false;
            }
        }

        if (getIsSpareRenderer() != isSpareRenderer
                && ChildProcessConnection.supportNotPerceptibleBinding()
                && ContentFeatureList.sSpareRendererAddNotPerceptibleBinding.getValue()) {
            if (isSpareRenderer) {
                connection.addNotPerceptibleBinding();
            } else {
                connection.removeNotPerceptibleBinding();
            }
        }
        setIsSpareRenderer(isSpareRenderer);

        if (mRanking != null) {
            mRanking.updateConnection(
                    connection,
                    visible,
                    frameDepth,
                    intersectsViewport,
                    isSpareRenderer,
                    importance);
            if (mBindingManager != null) mBindingManager.rankingChanged();
        }

        if (mEffectiveImportance != newEffectiveImportance
                && mEffectiveImportance != ChildProcessImportance.NORMAL) {
            final int existingEffectiveImportance = mEffectiveImportance;
            Runnable removeBindingRunnable =
                    () -> {
                        switch (existingEffectiveImportance) {
                            case ChildProcessImportance.NORMAL:
                                // Nothing to remove.
                                break;
                            case ChildProcessImportance.PERCEPTIBLE:
                                connection.removeNotPerceptibleBinding();
                                break;
                            case ChildProcessImportance.MODERATE:
                                connection.removeVisibleBinding();
                                break;
                            case ChildProcessImportance.IMPORTANT:
                                connection.removeStrongBinding();
                                break;
                            default:
                                assert false;
                        }
                    };
            if (System.currentTimeMillis() - mStartTimeMs < TIMEOUT_FOR_DELAY_BINDING_REMOVE_MS) {
                LauncherThread.postDelayed(removeBindingRunnable, REMOVE_BINDING_DELAY_MS);
            } else {
                removeBindingRunnable.run();
            }
        }

        mEffectiveImportance = newEffectiveImportance;
    }

    @CalledByNative
    static void stop(int pid) {
        assert LauncherThread.runningOnLauncherThread();
        Log.d(TAG, "stopping child connection: pid=%d", pid);
        ChildProcessLauncherHelperImpl launcher = getByPid(pid);
        // launcher can be null for single process.
        if (launcher != null) {
            // Can happen for single process.
            launcher.mLauncher.stop();
        }
    }

    // Called on client (UI or IO) thread.
    @CalledByNative
    private @ChildBindingState int getEffectiveChildBindingState() {
        ChildProcessConnection connection = mLauncher.getConnection();
        // Here we are accessing the connection from a thread other than the launcher thread, but it
        // does not change once it's been set. So it is safe to test whether it's null here and
        // access it afterwards.
        if (connection == null) return ChildBindingState.UNBOUND;

        return connection.bindingStateCurrent();
    }

    /**
     * Dumps the stack of the child process with |pid| without crashing it.
     *
     * @param pid Process id of the child process.
     */
    @CalledByNative
    private void dumpProcessStack(int pid) {
        assert LauncherThread.runningOnLauncherThread();
        ChildProcessLauncherHelperImpl launcher = getByPid(pid);
        if (launcher != null) {
            ChildProcessConnection connection = assumeNonNull(launcher.mLauncher.getConnection());
            connection.dumpProcessStack();
        }
    }

    private static Bundle populateServiceBundle(Bundle bundle) {
        ChildProcessCreationParamsImpl.addIntentExtras(bundle);
        bundle.putBoolean(
                ChildProcessConstants.EXTRA_BIND_TO_CALLER,
                ChildProcessCreationParamsImpl.getBindToCallerCheck());
        MultiProcessMediator m = LibraryLoader.getInstance().getMediator();
        m.ensureInitializedInMainProcess();
        m.putLoadAddressToBundle(bundle);
        return bundle;
    }

    private static @Nullable ChildProcessLauncherHelperImpl getByPid(int pid) {
        return sLauncherByPid.get(pid);
    }

    /**
     * Groups all currently tracked processes by type and returns a map of type -> list of PIDs.
     *
     * @param callback The callback to notify with the process information.  {@code callback} will
     *                 run on the same thread this method is called on.  That thread must support a
     *                 {@link android.os.Looper}.
     */
    public static void getProcessIdsByType(Callback<Map<String, List<Integer>>> callback) {
        final Handler responseHandler = new Handler();
        LauncherThread.post(
                () -> {
                    Map<String, List<Integer>> map = new HashMap<>();
                    for (var entry : sLauncherByPid.entrySet()) {
                        String type = entry.getValue().getProcessType();
                        List<Integer> pids = map.get(type);
                        if (pids == null) {
                            pids = new ArrayList<>();
                            map.put(type, pids);
                        }
                        pids.add(entry.getKey());
                    }

                    responseHandler.post(callback.bind(map));
                });
    }

    // Testing only related methods.

    int getPidForTesting() {
        assert LauncherThread.runningOnLauncherThread();
        return mLauncher.getPid();
    }

    public static Map<Integer, ChildProcessLauncherHelperImpl> getAllProcessesForTesting() {
        return sLauncherByPid;
    }

    public static ChildProcessLauncherHelperImpl createAndStartForTesting(
            String[] commandLine,
            IFileDescriptorInfo[] filesToBeMapped,
            boolean sandboxed,
            boolean reducePriorityOnBackground,
            boolean canUseWarmUpConnection,
            IBinder binderCallback,
            boolean doSetupConnection) {
        ChildProcessLauncherHelperImpl launcherHelper =
                new ChildProcessLauncherHelperImpl(
                        0L,
                        commandLine,
                        filesToBeMapped,
                        sandboxed,
                        reducePriorityOnBackground,
                        canUseWarmUpConnection,
                        binderCallback,
                        null);
        launcherHelper.mLauncher.start(doSetupConnection, /* queueIfNoFreeConnection= */ true);
        return launcherHelper;
    }

    /** @return the count of services set-up and working. */
    static int getConnectedServicesCountForTesting() {
        int count =
                sPrivilegedChildConnectionAllocator == null
                        ? 0
                        : sPrivilegedChildConnectionAllocator.allocatedConnectionsCountForTesting();
        return count + getConnectedSandboxedServicesCountForTesting();
    }

    public static int getConnectedSandboxedServicesCountForTesting() {
        return sSandboxedChildConnectionAllocator == null
                ? 0
                : sSandboxedChildConnectionAllocator.allocatedConnectionsCountForTesting();
    }

    @VisibleForTesting
    public @Nullable ChildProcessConnection getChildProcessConnection() {
        return mLauncher.getConnection();
    }

    public ChildConnectionAllocator getChildConnectionAllocatorForTesting() {
        return mLauncher.getConnectionAllocator();
    }

    public static @Nullable ChildProcessConnection getWarmUpConnectionForTesting() {
        return sSpareSandboxedConnection == null ? null : sSpareSandboxedConnection.getConnection();
    }

    @NativeMethods
    interface Natives {
        // Can be called on a number of threads, including launcher, and binder.
        void onChildProcessStarted(long nativeChildProcessLauncherHelper, int pid);

        void setTerminationInfo(
                long termiantionInfoPtr,
                @ChildBindingState int bindingState,
                boolean killedByUs,
                boolean cleanExit,
                boolean exceptionDuringInit,
                boolean isSpareRenderer);

        boolean serviceGroupImportanceEnabled();
    }
}
