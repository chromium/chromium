// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static androidx.core.view.accessibility.AccessibilityEventCompat.CONTENT_CHANGE_TYPE_PANE_APPEARED;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.ACTION_ARGUMENT_HTML_ELEMENT_STRING;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.ACTION_ARGUMENT_PROGRESS_VALUE;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.ACTION_ARGUMENT_SELECTION_END_INT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.ACTION_ARGUMENT_SELECTION_START_INT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_ACCESSIBILITY_FOCUS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CLEAR_FOCUS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CLICK;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_COLLAPSE;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CONTEXT_CLICK;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_COPY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CUT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_EXPAND;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_FOCUS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_IME_ENTER;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_LONG_CLICK;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_NEXT_AT_MOVEMENT_GRANULARITY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_NEXT_HTML_ELEMENT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_DOWN;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_LEFT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_RIGHT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_UP;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PASTE;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PREVIOUS_HTML_ELEMENT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_BACKWARD;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_DOWN;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_FORWARD;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_LEFT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_RIGHT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_UP;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SET_PROGRESS;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SET_SELECTION;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SET_TEXT;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SHOW_ON_SCREEN;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_CHARACTER;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_LINE;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_PARAGRAPH;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_WORD;

import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_DATA_REQUEST_IMAGE_DATA_KEY;
import static org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.EXTRAS_KEY_URL;
import static org.chromium.content_public.browser.ContentFeatureList.ACCESSIBILITY_MANAGE_BROADCAST_RECEIVER_ON_BACKGROUND;

import android.annotation.SuppressLint;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ReceiverCallNotAllowedException;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.autofill.AutofillManager;
import android.view.inputmethod.EditorInfo;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.core.view.accessibility.AccessibilityNodeProviderCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.StrictModeContext;
import org.chromium.base.TraceEvent;
import org.chromium.base.UserData;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.BuildConfig;
import org.chromium.content.browser.WindowEventObserver;
import org.chromium.content.browser.WindowEventObserverManager;
import org.chromium.content.browser.accessibility.AccessibilityDelegate.AccessibilityCoordinates;
import org.chromium.content.browser.accessibility.AccessibilityNodeInfoBuilder.BuilderDelegate;
import org.chromium.content.browser.accessibility.AutoDisableAccessibilityHandler.Client;
import org.chromium.content.browser.accessibility.captioning.CaptioningController;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl.UserDataFactory;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.accessibility.AccessibilityFeatures;
import org.chromium.ui.accessibility.AccessibilityFeaturesMap;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Implementation of {@link WebContentsAccessibility} interface. Native accessibility for a {@link
 * WebContents}. Actual native instance is created lazily upon the first request from Android
 * framework on {@link AccessibilityNodeProvider}, and shares the lifetime with {@link WebContents}.
 * Internally this class uses the {@link AccessibilityNodeProviderCompat} interface, and uses the
 * {@link AccessibilityNodeInfoCompat} object for the virtual tree, but will unwrap and surface the
 * non-Compat versions of these for any clients.
 */
@JNINamespace("content")
public class WebContentsAccessibilityImpl extends AccessibilityNodeProviderCompat
        implements WebContentsAccessibility,
                WindowEventObserver,
                UserData,
                AccessibilityState.Listener,
                ViewAndroidDelegate.ContainerViewObserver {
    private static final String TAG = "A11yImpl";

    // Constant for paragraph predicate key from web_contents_accessibility_android.cc
    private static final String PARAGRAPH_ELEMENT_TYPE = "PARAGRAPH";

    // Constant for no granularity selected.
    private static final int NO_GRANULARITY_SELECTED = 0;

    // Delay times for throttling of successive AccessibilityEvents in milliseconds.
    private static final int ACCESSIBILITY_EVENT_DELAY_DEFAULT = 100;
    private static final int ACCESSIBILITY_EVENT_DELAY_HOVER = 50;

    // Delay time for disabling renderer accessibility when no services are enabled. Used to prevent
    // churn if an accessibility service is quickly disabled then re-enabled.
    private static final int NO_ACCESSIBILITY_SERVICES_ENABLED_DELAY_MS = 5 * 1000;

    // Maximum number of times that the auto-disable feature can affect |this|.
    private static final int AUTO_DISABLE_SINGLE_INSTANCE_TOGGLE_LIMIT = 3;

    private final AccessibilityDelegate mDelegate;
    protected AccessibilityManager mAccessibilityManager;
    protected Context mContext;
    private final String mProductVersion;
    protected long mNativeObj;
    protected long mNativeAssistDataObj;
    private boolean mIsHovering;
    private int mLastHoverId = View.NO_ID;
    private int mCurrentRootId;
    protected View mView;
    private boolean mPendingScrollToMakeNodeVisible;
    private boolean mNotifyFrameInfoInitializedCalled;
    private boolean mAccessibilityEnabledOverride;
    private int mSelectionGranularity;
    private int mAccessibilityFocusId;
    private int mLastAccessibilityFocusId = View.NO_ID;
    private int mSelectionNodeId;
    private View mAutofillPopupView;
    private CaptioningController mCaptioningController;
    private boolean mIsCurrentlyExtendingSelection;
    private int mSelectionStart;
    private int mCursorIndex;
    private String mSupportedHtmlElementTypes;
    private final AccessibilityNodeInfoBuilder mAccessibilityNodeInfoBuilder;
    private boolean mHasFinishedLatestAccessibilitySnapshot;
    private boolean mPendingSetSequentialFocus;

    // Observer for WebContents, used to update state when |this| is shown/hidden.
    private WebContentsObserver mWebContentsObserver;

    // Tracker for all actions performed and events sent by this instance, used for testing.
    private AccessibilityActionAndEventTracker mTracker;

    // Helper object to track and record values relevant to histograms.
    private final AccessibilityHistogramRecorder mHistogramRecorder;

    // Whether or not the next selection event should be fired. We only want to sent one traverse
    // and one selection event per granularity move, this ensures no double events while still
    // sending events when the user is using other assistive technology (e.g. external keyboard)
    private boolean mSuppressNextSelectionEvent;

    // Whether accessibility focus should be set to the page when it finishes loading.
    // This only applies if an accessibility service like TalkBack is running.
    // This is desirable behavior for a browser window, but not for an embedded
    // WebView.
    private boolean mShouldFocusOnPageLoad;

    // True if this instance is a candidate to have the image descriptions feature enabled. The
    // feature is dependent on embedder behavior and screen reader state. Default false.
    private boolean mIsImageDescriptionsCandidate;

    // If true, the web contents are obscured by another view and we will return a null
    // AccessibilityNodeProvider, and will not process touch exploration events or calls to
    // performAction. If false, all accessibility requests will be honored. When null, treat the
    // value as false, this is to differentiate between an initial value and a value set by a
    // client, since we assert the value is changed with each call to the setter. (Default: null).
    private Boolean mIsObscuredByAnotherView;

    // This array maps a given virtualViewId to an |AccessibilityNodeInfoCompat| for that view. We
    // use this to update a node quickly rather than building from one scratch each time.
    private final SparseArray<AccessibilityNodeInfoCompat> mNodeInfoCache = new SparseArray<>();

    // This handles the dispatching of accessibility events. It acts as an intermediary where we can
    // apply throttling rules, delay event construction, etc.
    private final AccessibilityEventDispatcher mEventDispatcher;
    private volatile String mSystemLanguageTag;
    private BroadcastReceiver mBroadcastReceiver;
    // Only un-register the broadcast receiver if this is true, otherwise it would result in a
    // crash.
    private volatile boolean mIsBroadcastReceiverRegistered;

    // Set of all nodes that have received a request to populate image data. The request only needs
    // to be run once per node, and it completes asynchronously. We track which nodes have already
    // started the async request so that if downstream apps request the same node multiple times
    // we can avoid doing the extra work.
    private final Set<Integer> mImageDataRequestedNodes = new HashSet<Integer>();

    // Handler for the "Auto Disable" accessibility feature and related state variables.
    private final AutoDisableAccessibilityHandler mAutoDisableAccessibilityHandler;
    private boolean mIsCurrentlyAutoDisabled;
    private int mAutoDisableUsageCounter;
    private boolean mIsAutoDisableAccessibilityCandidate;

    // To avoid any potential synchronization issues we post all broadcast receiver registration
    // actions to the same sequence to be run serially.
    private static final TaskRunner sSequencedTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.BEST_EFFORT_MAY_BLOCK);

    /** Create a WebContentsAccessibilityImpl object. */
    private static class Factory implements UserDataFactory<WebContentsAccessibilityImpl> {
        @Override
        public WebContentsAccessibilityImpl create(WebContents webContents) {
            return createForDelegate(new WebContentsAccessibilityDelegate(webContents));
        }
    }

    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<WebContentsAccessibilityImpl> INSTANCE = new Factory();
    }

    public static WebContentsAccessibilityImpl fromWebContents(WebContents webContents) {
        return ((WebContentsImpl) webContents)
                .getOrSetUserData(
                        WebContentsAccessibilityImpl.class, UserDataFactoryLazyHolder.INSTANCE);
    }

    public static WebContentsAccessibilityImpl fromDelegate(AccessibilityDelegate delegate) {
        // If WebContents exists, {@link #fromWebContents} should be used.
        assert delegate.getWebContents() == null;
        return createForDelegate(delegate);
    }

    private static WebContentsAccessibilityImpl createForDelegate(AccessibilityDelegate delegate) {
        return new WebContentsAccessibilityImpl(delegate);
    }

    protected WebContentsAccessibilityImpl(AccessibilityDelegate delegate) {
        TraceEvent.begin("WebContentsAccessibilityImpl.ctor");
        mDelegate = delegate;
        mView = mDelegate.getContainerView();
        mContext = mView.getContext();
        mProductVersion = mDelegate.getProductVersion();
        mAccessibilityManager =
                (AccessibilityManager) mContext.getSystemService(Context.ACCESSIBILITY_SERVICE);

        // Need to be initialized before AXTreeUpdate initialization because updateMaxNodesInCache
        // gets called then. Also needs to be initialized before the WindowEventObserver is added,
        // which may call #onAttachedToWindow (or detached) if that is the current state.
        mHistogramRecorder = new AccessibilityHistogramRecorder();

        WebContents webContents = mDelegate.getWebContents();
        if (webContents != null) {
            mCaptioningController = new CaptioningController(webContents);
            WindowEventObserverManager.from(webContents).addObserver(this);
            webContents.getViewAndroidDelegate().addObserver(this);
        }
        mDelegate.setOnScrollPositionChangedCallback(
                () -> {
                    handleScrollPositionChanged(mAccessibilityFocusId);
                    moveAccessibilityFocusToId(mAccessibilityFocusId);
                });

        AccessibilityState.addListener(this);

        mAccessibilityNodeInfoBuilder =
                new AccessibilityNodeInfoBuilder(
                        new BuilderDelegate() {
                            @Override
                            public View getView() {
                                return mView;
                            }

                            @Override
                            public Context getContext() {
                                return mContext;
                            }

                            @Override
                            public int currentRootId() {
                                return mCurrentRootId;
                            }

                            @Override
                            public int currentAccessibilityFocusId() {
                                return mAccessibilityFocusId;
                            }

                            @Override
                            public String getLanguageTag() {
                                return mSystemLanguageTag;
                            }

                            @Override
                            public String getSupportedHtmlTags() {
                                return mSupportedHtmlElementTypes;
                            }

                            @Override
                            public AccessibilityCoordinates getAccessibilityCoordinates() {
                                return mDelegate.getAccessibilityCoordinates();
                            }
                        });

        mAutoDisableAccessibilityHandler =
                new AutoDisableAccessibilityHandler(
                        new Client() {
                            @Override
                            public View getView() {
                                return mView;
                            }

                            @Override
                            public void onDisabled() {
                                assert mNativeObj != 0
                                        : "Native code is not initialized, but disable was called.";
                                TraceEvent.begin(
                                        "WebContentsAccessibilityImpl.AutoDisableAccessibilityHandler.onDisabled");
                                mHistogramRecorder.onDisableCalled(mAutoDisableUsageCounter == 0);
                                // If the Auto-disable timer has expired, begin disabling the
                                // renderer, and clearing the Java-side caches. Changing AXModes
                                // must be done on the main thread.
                                WebContentsAccessibilityImplJni.get()
                                        .disableRendererAccessibility(mNativeObj);
                                mEventDispatcher.clearQueue();
                                mNodeInfoCache.clear();
                                mIsCurrentlyAutoDisabled = true;
                                TraceEvent.end(
                                        "WebContentsAccessibilityImpl.AutoDisableAccessibilityHandler.onDisabled");
                            }
                        });

        // Define our delays on a per event type basis.
        Map<Integer, Integer> eventThrottleDelays = new HashMap<Integer, Integer>();
        eventThrottleDelays.put(
                AccessibilityEvent.TYPE_VIEW_SCROLLED, ACCESSIBILITY_EVENT_DELAY_DEFAULT);
        eventThrottleDelays.put(
                AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED, ACCESSIBILITY_EVENT_DELAY_DEFAULT);
        eventThrottleDelays.put(
                AccessibilityEvent.TYPE_VIEW_HOVER_ENTER, ACCESSIBILITY_EVENT_DELAY_HOVER);

        // Define events to throttle without regard for |virtualViewId|.
        Set<Integer> viewIndependentEvents = new HashSet<Integer>();
        viewIndependentEvents.add(AccessibilityEvent.TYPE_VIEW_HOVER_ENTER);

        AccessibilityEventDispatcher.Client client =
                new AccessibilityEventDispatcher.Client() {
                    @Override
                    public void postRunnable(Runnable toPost, long delayInMilliseconds) {
                        mView.postDelayed(toPost, delayInMilliseconds);
                    }

                    @Override
                    public void removeRunnable(Runnable toRemove) {
                        mView.removeCallbacks(toRemove);
                    }

                    @Override
                    public boolean dispatchEvent(int virtualViewId, int eventType) {
                        AccessibilityEvent event =
                                buildAccessibilityEvent(virtualViewId, eventType);
                        if (event == null) return false;

                        requestSendAccessibilityEvent(event);

                        // Always send the ENTER and then the EXIT event, to match a
                        // standard Android View.
                        if (eventType == AccessibilityEvent.TYPE_VIEW_HOVER_ENTER) {
                            AccessibilityEvent exitEvent =
                                    buildAccessibilityEvent(
                                            mLastHoverId, AccessibilityEvent.TYPE_VIEW_HOVER_EXIT);
                            if (exitEvent != null) {
                                requestSendAccessibilityEvent(exitEvent);
                                mLastHoverId = virtualViewId;
                            } else if (virtualViewId != View.NO_ID
                                    && mLastHoverId != virtualViewId) {
                                // If IDs become mismatched, or on first hover, this will
                                // sync the values again so all further hovers have
                                // correct event pairing.
                                mLastHoverId = virtualViewId;
                            }
                        }

                        return true;
                    }
                };
        mEventDispatcher =
                new AccessibilityEventDispatcher(
                        client, eventThrottleDelays, viewIndependentEvents, new HashSet<Integer>());

        if (mDelegate.getNativeAXTree() != 0) {
            initializeNativeWithAXTreeUpdate(mDelegate.getNativeAXTree());
        }
        // If the AXTree is not provided, native is initialized lazily, when node provider is
        // actually requested.

        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O
                && !BuildConfig.IS_FOR_TEST) {
            // The system service call for AutofillManager can timeout and throws an Exception.
            // This is treated differently in each version of Android, so we must catch a
            // generic Exception. (refer to crbug.com/1186406 or AutofillManagerWrapper ctor).
            try {
                AutofillManager autofillManager = mContext.getSystemService(AutofillManager.class);
                if (autofillManager != null
                        && autofillManager.isEnabled()
                        && autofillManager.hasEnabledAutofillServices()) {
                    // Native accessibility is usually initialized when getAccessibilityNodeProvider
                    // is called, but the Autofill compatibility bridge only calls that method after
                    // it has received the first accessibility events. To solve the chicken-and-egg
                    // problem, always initialize the native parts when the user has an Autofill
                    // service enabled.
                    getAccessibilityNodeProvider();
                }
            } catch (Exception e) {
                Log.e(TAG, "AutofillManager did not resolve before time limit.");
            }
        }

        TraceEvent.end("WebContentsAccessibilityImpl.ctor");
    }

    /**
     * Called after the native a11y part is initialized. Overridable by subclasses
     * to do initialization that is not required until the native is set up.
     */
    protected void onNativeInit() {
        TraceEvent.begin("WebContentsAccessibilityImpl.onNativeInit");
        mHistogramRecorder.updateTimeOfNativeInitialization();
        mAccessibilityFocusId = View.NO_ID;
        mLastAccessibilityFocusId = View.NO_ID;
        mSelectionNodeId = View.NO_ID;
        mIsHovering = false;
        mCurrentRootId = View.NO_ID;

        mSupportedHtmlElementTypes =
                WebContentsAccessibilityImplJni.get().getSupportedHtmlElementTypes(mNativeObj);
        mBroadcastReceiver =
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        mSystemLanguageTag = Locale.getDefault().toLanguageTag();
                    }
                };

        // Register a broadcast receiver for locale change.
        if (mView.isAttachedToWindow()) {
            if (ContentFeatureMap.isEnabled(
                    ACCESSIBILITY_MANAGE_BROADCAST_RECEIVER_ON_BACKGROUND)) {
                // To prevent having empty languageTag until this background task runs.
                mSystemLanguageTag = Locale.getDefault().toLanguageTag();
                sSequencedTaskRunner.execute(this::registerLocaleChangeReceiver);
            } else {
                registerLocaleChangeReceiver();
            }
        }

        // Define a set of relevant AccessibilityEvents.
        Runnable serviceMaskRunnable =
                () -> {
                    mEventDispatcher.updateRelevantEventTypes(
                            AccessibilityState.relevantEventTypesForCurrentServices());
                };
        mView.post(serviceMaskRunnable);

        // Send state values set by embedders to native-side objects.
        refreshNativeState();

        TraceEvent.end("WebContentsAccessibilityImpl.onNativeInit");
    }

    @CalledByNative
    protected void onNativeObjectDestroyed() {
        mNativeObj = 0;
    }

    @Override
    public boolean isNativeInitialized() {
        return mNativeObj != 0;
    }

    private boolean isRootManagerConnected() {
        return isNativeInitialized()
                && WebContentsAccessibilityImplJni.get().isRootManagerConnected(mNativeObj);
    }

    public boolean isAccessibilityEnabled() {
        return isNativeInitialized()
                && (mAccessibilityEnabledOverride
                        || mAccessibilityManager.isEnabled()
                        || AccessibilityState.isAnyAccessibilityServiceEnabled());
    }

    public void addSpellingErrorForTesting(int virtualViewId, int startOffset, int endOffset) {
        WebContentsAccessibilityImplJni.get()
                .addSpellingErrorForTesting(mNativeObj, virtualViewId, startOffset, endOffset);
    }

    public void setMaxContentChangedEventsToFireForTesting(int maxEvents) {
        WebContentsAccessibilityImplJni.get()
                .setMaxContentChangedEventsToFireForTesting(mNativeObj, maxEvents);
    }

    public int getMaxContentChangedEventsToFireForTesting() {
        return WebContentsAccessibilityImplJni.get()
                .getMaxContentChangedEventsToFireForTesting(mNativeObj);
    }

    public void forceAutoDisableAccessibilityForTesting() {
        mAutoDisableAccessibilityHandler.notifyDisable();
    }

    public void setAccessibilityTrackerForTesting(AccessibilityActionAndEventTracker tracker) {
        mHistogramRecorder.updateTimeOfFirstShown();
        var oldValue = mTracker;
        mTracker = tracker;
        ResettersForTesting.register(() -> mTracker = oldValue);
    }

    public void setIsAutoDisableAccessibilityCandidateForTesting(
            boolean isAutoDisableAccessibilityCandidate) {
        mIsAutoDisableAccessibilityCandidate = isAutoDisableAccessibilityCandidate;
    }

    public boolean hasAnyPendingTimersForTesting() {
        return mAutoDisableAccessibilityHandler.hasPendingTimer();
    }

    public void signalEndOfTestForTesting() {
        WebContentsAccessibilityImplJni.get().signalEndOfTestForTesting(mNativeObj);
    }

    public void forceRecordUMAHistogramsForTesting() {
        mHistogramRecorder.recordEventsHistograms();
    }

    public void forceRecordCacheUMAHistogramsForTesting() {
        mHistogramRecorder.recordCacheHistograms();
    }

    public void forceRecordUsageUMAHistogramsForTesting() {
        mHistogramRecorder.recordAccessibilityUsageHistograms();
    }

    public boolean hasFinishedLatestAccessibilitySnapshotForTesting() {
        return mHasFinishedLatestAccessibilitySnapshot;
    }

    @CalledByNative
    public void handleEndOfTestSignal() {
        // We have received a signal that we have reached the end of a unit test. If we have a
        // tracker listening, set the test is complete.
        if (mTracker != null) {
            mTracker.signalEndOfTest();
        }
    }

    // WebContentsObserver

    private void registerWebContentsObserver(WebContents webContents) {
        if (mWebContentsObserver != null) return;

        mWebContentsObserver =
                new WebContentsObserver(webContents) {
                    @Override
                    public void onVisibilityChanged(@Visibility int visibility) {
                        if (visibility == Visibility.VISIBLE) {
                            // The Tab holding |this| instance was shown, e.g. the user brings
                            // Chrome back to the foreground, switches to this Tab, etc.
                            mHistogramRecorder.updateTimeOfFirstShown();

                            // Accessibility state may have changed while |this| was not shown, so
                            // refresh.
                            refreshNativeState();
                            if (isNativeInitialized()) {
                                // When we are in an initialized state, accessibility may be
                                // disabled. In that case, we should not update the time of native
                                // initialization, and instead only update the time of the last
                                // disabled call so we don't count any time while this instance was
                                // hidden/backgrounded.
                                if (mIsCurrentlyAutoDisabled) {
                                    mHistogramRecorder.showAutoDisabledInstance();
                                } else {
                                    mHistogramRecorder.updateTimeOfNativeInitialization();
                                }
                            }
                        } else {
                            // The Tab holding |this| instance was hidden or occluded, e.g. a new
                            // Tab was opened, user has backgrounded Chrome, opened Settings, etc.
                            // Record usage times and reset state.
                            mHistogramRecorder.recordAccessibilityUsageHistograms();

                            // When the native code was initialized, also record performance
                            // metrics.
                            if (isNativeInitialized()) {
                                mHistogramRecorder.recordAccessibilityPerformanceHistograms();
                                // When we are in an initialized state, accessibility may be
                                // disabled. In that case, we should keep an on-going sum of the
                                // time spent disabled (without counting time while
                                // hidden/backgrounded).
                                if (mIsCurrentlyAutoDisabled) {
                                    mHistogramRecorder.hideAutoDisabledInstance();
                                }
                                mAutoDisableAccessibilityHandler.cancelDisableTimer();
                            }
                        }
                    }
                };
    }

    // WindowEventObserver

    @Override
    public void onDetachedFromWindow() {
        try (TraceEvent te =
                TraceEvent.scoped("WebContentsAccessibilityImpl.onDetachedFromWindow")) {
            mCaptioningController.stopListening();

            // Destroy the WebContentsObserver if |this| is no longer attached to a Window, but
            // first record whatever data we have collected since
            // onVisibilityChanged(Visibility.HIDDEN) may not have been called, for example when
            // opening the Tab Switcher. Timers will restart during the next onAttach.
            if (mWebContentsObserver != null) {
                mHistogramRecorder.recordAccessibilityUsageHistograms();
                mWebContentsObserver.destroy();
                mWebContentsObserver = null;
            }

            // When the native code was initialized, also record performance metrics unregister
            // our broadcast receiver.
            if (isNativeInitialized()) {
                if (mIsBroadcastReceiverRegistered) {
                    if (ContentFeatureMap.isEnabled(
                            ACCESSIBILITY_MANAGE_BROADCAST_RECEIVER_ON_BACKGROUND)) {
                        sSequencedTaskRunner.execute(
                                () ->
                                        ContextUtils.getApplicationContext()
                                                .unregisterReceiver(mBroadcastReceiver));
                    } else {
                        ContextUtils.getApplicationContext().unregisterReceiver(mBroadcastReceiver);
                    }
                    mIsBroadcastReceiverRegistered = false;
                }
                mHistogramRecorder.recordAccessibilityPerformanceHistograms();
                // When we are in an initialized state, accessibility may be disabled. In that
                // case, we should keep an on-going sum of the time spent disabled (without
                // counting time while hidden/backgrounded).
                if (mIsCurrentlyAutoDisabled) {
                    mHistogramRecorder.hideAutoDisabledInstance();
                }
                mAutoDisableAccessibilityHandler.cancelDisableTimer();
            }
        }
    }

    @Override
    public void onAttachedToWindow() {
        TraceEvent.begin("WebContentsAccessibilityImpl.onAttachedToWindow");

        // When webContents is non-null (e.g. not a Paint Preview), we will track usage stats.
        if (mDelegate.getWebContents() != null) {
            registerWebContentsObserver(mDelegate.getWebContents());
            mWebContentsObserver.onVisibilityChanged(Visibility.VISIBLE);
        }

        refreshNativeState();

        // Some devices (e.g. OnePlus) are enforcing a Strict Mode Violation in code outside Chrome,
        // which can result a crash when the listener starts.
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            mCaptioningController.startListening();
        }

        if (isNativeInitialized()) {
            if (ContentFeatureMap.isEnabled(
                    ACCESSIBILITY_MANAGE_BROADCAST_RECEIVER_ON_BACKGROUND)) {
                // To prevent having empty languageTag until this background task runs.
                mSystemLanguageTag = Locale.getDefault().toLanguageTag();
                sSequencedTaskRunner.execute(this::registerLocaleChangeReceiver);
            } else {
                registerLocaleChangeReceiver();
            }
        }
        TraceEvent.end("WebContentsAccessibilityImpl.onAttachedToWindow");
    }

    private void registerLocaleChangeReceiver() {
        try {
            IntentFilter filter = new IntentFilter(Intent.ACTION_LOCALE_CHANGED);
            ContextUtils.registerProtectedBroadcastReceiver(
                    ContextUtils.getApplicationContext(), mBroadcastReceiver, filter);
            mIsBroadcastReceiverRegistered = true;
        } catch (ReceiverCallNotAllowedException e) {
            // WebView may be running inside a BroadcastReceiver, in which case registerReceiver is
            // not allowed.
        }
        mSystemLanguageTag = Locale.getDefault().toLanguageTag();
    }

    @Override
    public void onWindowAndroidChanged(WindowAndroid windowAndroid) {
        TraceEvent.begin("WebContentsAccessibilityImpl.onWindowAndroidChanged");
        // When the WindowAndroid changes, we must update our Context reference to the new value.
        // We also need to remove all references to the previous context, which in this case would
        // be the reference in any existing SuggestionSpans. To remove these, clear our cache to
        // recycle all nodes. Any other AccessibilityNodeInfo objects that were created would have
        // been passed to the Framework, which can handle clean-up on its end. We do not want to
        // delete |this| because the object is (largely) not WindowAndroid dependent.
        mNodeInfoCache.clear();
        if (windowAndroid != null && windowAndroid.getContext().get() != null) {
            mContext = windowAndroid.getContext().get();
        }

        TraceEvent.end("WebContentsAccessibilityImpl.onWindowAndroidChanged");
    }

    @Override
    public void onUpdateContainerView(ViewGroup view) {
        // When the ContainerView is updated, we must update the |mView| variable and remove all
        // previous references to it. We clear the AccessibilityEventDispatcher queue, which may
        // have posted Runnable(s) to the old view. We also clear the AccessibilityNodeInfo cache
        // since some objects may still be referencing the old view as their parent or source. We
        // do not want to delete |this| because the object is (largely) not ContainerView dependent.
        mEventDispatcher.clearQueue();
        mNodeInfoCache.clear();
        mView = view;
    }

    @Override
    public void destroy() {
        TraceEvent.begin("WebContentsAccessibilityImpl.destroy");
        mNodeInfoCache.clear();
        mEventDispatcher.clearQueue();
        mAutoDisableAccessibilityHandler.cancelDisableTimer();
        if (mDelegate.getWebContents() == null) {
            deleteEarly();
        } else {
            if (mWebContentsObserver != null) mWebContentsObserver.destroy();
            WindowEventObserverManager.from(mDelegate.getWebContents()).removeObserver(this);
            ((WebContentsImpl) mDelegate.getWebContents())
                    .removeUserData(WebContentsAccessibilityImpl.class);
        }
        TraceEvent.end("WebContentsAccessibilityImpl.destroy");
    }

    protected void deleteEarly() {
        if (mNativeObj != 0) {
            TraceEvent.begin("WebContentsAccessibilityImpl.deleteEarly");
            WebContentsAccessibilityImplJni.get().deleteEarly(mNativeObj);
            assert mNativeObj == 0;
            TraceEvent.end("WebContentsAccessibilityImpl.deleteEarly");
        }
    }

    private void refreshNativeState() {
        try (TraceEvent te = TraceEvent.scoped("WebContentsAccessibilityImpl.refreshNativeState")) {
            if (!isNativeInitialized()) return;

            // Update the browser-level AXMode based on running applications.
            WebContentsAccessibilityImplJni.get()
                    .setBrowserAXMode(
                            WebContentsAccessibilityImpl.this,
                            AccessibilityState.isScreenReaderEnabled(),
                            AccessibilityState.isOnlyPasswordManagersEnabled(),
                            /* isAccessibilityEnabled= */ true);

            // Update the state of enabling/disabling the image descriptions feature. To enable the
            // feature, this instance must be a candidate and a screen reader must be enabled.
            WebContentsAccessibilityImplJni.get()
                    .setAllowImageDescriptions(
                            mNativeObj,
                            mIsImageDescriptionsCandidate
                                    && AccessibilityState.isScreenReaderEnabled());

            // Update the list of events we dispatch to enabled services.
            mEventDispatcher.updateRelevantEventTypes(
                    AccessibilityState.relevantEventTypesForCurrentServices());

            // When no accessibility services are running, disable renderer accessibility and tear
            // down objects. If we have disabled then re-enabled the renderer accessibility multiple
            // times for this instance, return early and keep enabled to prevent further churn.
            if (mAutoDisableUsageCounter >= AUTO_DISABLE_SINGLE_INSTANCE_TOGGLE_LIMIT
                    || !mIsAutoDisableAccessibilityCandidate) {
                mAutoDisableAccessibilityHandler.cancelDisableTimer();
                return;
            }

            // The C++ and Java instances are not fully connected until the root manager has
            // been connected, which will happen asynchronously. Accessibility cannot be auto
            // disabled and re-enabled when there is no root manager. See note in
            // {@link web_contents_accessibility_android.h}.
            if (!isRootManagerConnected()) return;

            // If accessibility was auto-disabled, then we do not want to restart a new timer.
            if (mIsCurrentlyAutoDisabled) return;

            if (!AccessibilityState.isAnyAccessibilityServiceEnabled()) {
                mAutoDisableAccessibilityHandler.cancelDisableTimer();
                mAutoDisableAccessibilityHandler.startDisableTimer(
                        NO_ACCESSIBILITY_SERVICES_ENABLED_DELAY_MS);
            } else {
                mAutoDisableAccessibilityHandler.cancelDisableTimer();
            }
        }
    }

    // AccessibilityNodeProvider

    @Override
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        // The |WebContentsAccessibilityImpl| class will rely on the Compat library, but we will
        // not require other parts of Chrome to do the same for simplicity, so unwrap the
        // |AccessibilityNodeProvider| object before returning.
        AccessibilityNodeProviderCompat anpc = getAccessibilityNodeProviderCompat();
        if (anpc == null) return null;

        return (AccessibilityNodeProvider) anpc.getProvider();
    }

    /**
     * Allows clients to get an |AccessibilityNodeProviderCompat| instance if they do not want
     * the unwrapped version that is available with getAccessibilityNodeProvider above.
     *
     * @return AccessibilityNodeProviderCompat (this)
     */
    public AccessibilityNodeProviderCompat getAccessibilityNodeProviderCompat() {
        if (shouldPreventNativeEngineUse()) return null;

        // If the Auto-Disable feature is on, and accessibility has been disabled, when the
        // Android Framework calls this method, it is a signal to re-enable renderer accessibility.
        // This must be done before we try to verify/reconnect the root manager, since doing so
        // requires a reference to the webContents.
        if (mIsCurrentlyAutoDisabled) {
            TraceEvent.begin("WebContentsAccessibilityImpl.reEnableRendererAccessibility");
            mHistogramRecorder.onReEnableCalled(mAutoDisableUsageCounter == 0);
            WebContentsAccessibilityImplJni.get()
                    .reEnableRendererAccessibility(mNativeObj, mDelegate.getWebContents());
            mIsCurrentlyAutoDisabled = false;
            mAutoDisableUsageCounter++;
            TraceEvent.end("WebContentsAccessibilityImpl.reEnableRendererAccessibility");
        }

        if (!isNativeInitialized()) {
            assert mDelegate.getWebContents() != null
                    : "WebContentsAccessibility with no webContents should not be initialized, or"
                            + " it should be initialized during constructor with an AXTreeUpdate.";

            mNativeObj =
                    WebContentsAccessibilityImplJni.get()
                            .init(
                                    WebContentsAccessibilityImpl.this,
                                    mDelegate.getWebContents(),
                                    mAccessibilityNodeInfoBuilder);
            onNativeInit();
        }

        if (!isRootManagerConnected()) {
            WebContentsAccessibilityImplJni.get().connectInstanceToRootManager(mNativeObj);
            return null;
        }

        return this;
    }

    protected void initializeNativeWithAXTreeUpdate(long nativeAxTree) {
        assert !isNativeInitialized();

        mNativeObj =
                WebContentsAccessibilityImplJni.get()
                        .initWithAXTree(
                                WebContentsAccessibilityImpl.this,
                                nativeAxTree,
                                mAccessibilityNodeInfoBuilder);
        onNativeInit();
    }

    @CalledByNative
    public String generateAccessibilityNodeInfoString(int virtualViewId) {
        // If accessibility isn't enabled, all the AccessibilityNodeInfoCompat objects will be null,
        // so temporarily set the |mAccessibilityEnabledOverride| flag to true, then disable it.
        mAccessibilityEnabledOverride = true;
        String returnString =
                AccessibilityNodeInfoUtils.toString(
                        createAccessibilityNodeInfo(virtualViewId), true);
        mAccessibilityEnabledOverride = false;
        return returnString;
    }

    @CalledByNative
    public void updateMaxNodesInCache() {
        mHistogramRecorder.updateMaxNodesInCache(mNodeInfoCache.size());
    }

    @CalledByNative
    public void clearNodeInfoCacheForGivenId(int virtualViewId) {
        // Recycle and remove the element in our cache for this |virtualViewId|.
        if (mNodeInfoCache.get(virtualViewId) != null) {
            mNodeInfoCache.get(virtualViewId).recycle();
            mNodeInfoCache.remove(virtualViewId);
        }
        // Remove this node from requested image data nodes in case data changed with update.
        mImageDataRequestedNodes.remove(virtualViewId);
    }

    @Override
    public AccessibilityNodeInfoCompat createAccessibilityNodeInfo(int virtualViewId) {
        if (!isAccessibilityEnabled()) {
            return null;
        }
        if (mCurrentRootId == View.NO_ID) {
            mCurrentRootId = WebContentsAccessibilityImplJni.get().getRootId(mNativeObj);
        }

        if (virtualViewId == View.NO_ID) {
            return createNodeForHost(mCurrentRootId);
        }

        if (!isFrameInfoInitialized()) {
            return null;
        }

        // We need to create an |AccessibilityNodeInfoCompat| object for this |virtualViewId|. If we
        // have one in our cache, then communicate this so web_contents_accessibility_android.cc
        // will update a fraction of the object and for the rest leverage what is already there.
        if (mNodeInfoCache.get(virtualViewId) != null) {
            AccessibilityNodeInfoCompat cachedNode =
                    AccessibilityNodeInfoCompat.obtain(mNodeInfoCache.get(virtualViewId));

            // Always update the source node id to prevent potential infinite loop in framework.
            cachedNode.setSource(mView, virtualViewId);

            if (WebContentsAccessibilityImplJni.get()
                    .updateCachedAccessibilityNodeInfo(mNativeObj, cachedNode, virtualViewId)) {
                // After successfully re-populating this cached node, update the accessibility
                // focus since this would not be included in the update call, and set the
                // available actions accordingly, then return result.
                cachedNode.setAccessibilityFocused(mAccessibilityFocusId == virtualViewId);

                if (mAccessibilityFocusId == virtualViewId) {
                    cachedNode.addAction(ACTION_CLEAR_ACCESSIBILITY_FOCUS);
                    cachedNode.removeAction(ACTION_ACCESSIBILITY_FOCUS);
                } else {
                    cachedNode.removeAction(ACTION_CLEAR_ACCESSIBILITY_FOCUS);
                    cachedNode.addAction(ACTION_ACCESSIBILITY_FOCUS);
                }

                mHistogramRecorder.incrementNodeWasReturnedFromCache();
                return cachedNode;
            } else {
                // If the node is no longer valid, wipe it from the cache and return null
                mNodeInfoCache.get(virtualViewId).recycle();
                mNodeInfoCache.remove(virtualViewId);
                return null;
            }

        } else {
            // If we have no copy of this node in our cache, build a new one from scratch.
            final AccessibilityNodeInfoCompat info = AccessibilityNodeInfoCompat.obtain(mView);
            info.setPackageName(mContext.getPackageName());
            info.setSource(mView, virtualViewId);

            if (virtualViewId == mCurrentRootId) {
                info.setParent(mView);
            }

            if (WebContentsAccessibilityImplJni.get()
                    .populateAccessibilityNodeInfo(mNativeObj, info, virtualViewId)) {
                // After successfully populating this node, add it to our cache then return.
                mNodeInfoCache.put(virtualViewId, AccessibilityNodeInfoCompat.obtain(info));
                mHistogramRecorder.incrementNodeWasCreatedFromScratch();
                return info;
            } else {
                info.recycle();
                return null;
            }
        }
    }

    @Override
    public List<AccessibilityNodeInfoCompat> findAccessibilityNodeInfosByText(
            String text, int virtualViewId) {
        return new ArrayList<AccessibilityNodeInfoCompat>();
    }

    private static boolean isValidMovementGranularity(int granularity) {
        switch (granularity) {
            case MOVEMENT_GRANULARITY_CHARACTER:
            case MOVEMENT_GRANULARITY_WORD:
            case MOVEMENT_GRANULARITY_LINE:
            case MOVEMENT_GRANULARITY_PARAGRAPH:
                return true;
        }
        return false;
    }

    // BrowserAccessibilityStateListener

    @Override
    public void onAccessibilityStateChanged(
            AccessibilityState.State oldAccessibilityState,
            AccessibilityState.State newAccessibilityState) {
        refreshNativeState();
    }

    // WebContentsAccessibility

    @Override
    public void setObscuredByAnotherView(boolean isObscured) {
        assert mIsObscuredByAnotherView == null || isObscured != mIsObscuredByAnotherView
                : "Two clients are both trying to obscure web contents accessibility. These are "
                        + "duplicate requests, or prone to error.";
        mIsObscuredByAnotherView = isObscured;
        sendAccessibilityEvent(View.NO_ID, AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
    }

    private boolean shouldPreventNativeEngineUse() {
        return mIsObscuredByAnotherView != null && mIsObscuredByAnotherView;
    }

    @Override
    public void setShouldFocusOnPageLoad(boolean on) {
        mShouldFocusOnPageLoad = on;
    }

    @Override
    public void setIsImageDescriptionsCandidate(boolean isImageDescriptionsCandidate) {
        mIsImageDescriptionsCandidate = isImageDescriptionsCandidate;
    }

    @Override
    public void setIsAutoDisableAccessibilityCandidate(
            boolean isAutoDisableAccessibilityCandidate) {
        mIsAutoDisableAccessibilityCandidate = isAutoDisableAccessibilityCandidate;
    }

    @Override
    public void onProvideVirtualStructure(
            final ViewStructure structure, final boolean ignoreScrollOffset) {
        // Do not collect accessibility tree in incognito mode
        if (mDelegate.isIncognito()) {
            structure.setChildCount(0);
            return;
        }
        structure.setChildCount(1);
        final ViewStructure viewRoot = structure.asyncNewChild(0);
        viewRoot.setClassName("");
        viewRoot.setHint(mProductVersion);

        WebContents webContents = mDelegate.getWebContents();
        if (webContents != null && !webContents.isDestroyed()) {
            Bundle extras = viewRoot.getExtras();
            extras.putCharSequence(EXTRAS_KEY_URL, webContents.getVisibleUrl().getSpec());
        }

        mHasFinishedLatestAccessibilitySnapshot = false;
        long beforeSnapshotTimeMs = SystemClock.elapsedRealtime();

        if (ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_UNIFIED_SNAPSHOTS)) {
            mNativeAssistDataObj =
                    WebContentsAccessibilityImplJni.get()
                            .initForAssistData(
                                    WebContentsAccessibilityImpl.this,
                                    webContents,
                                    new AssistDataBuilder());

            WebContentsAccessibilityImplJni.get()
                    .requestAccessibilityTreeSnapshot(
                            mNativeAssistDataObj,
                            viewRoot,
                            mDelegate.getAccessibilityCoordinates(),
                            mView,
                            () -> onSnapshotDoneCallback(viewRoot, beforeSnapshotTimeMs));
        } else {
            mDelegate.requestAccessibilitySnapshot(
                    viewRoot, () -> onSnapshotDoneCallback(viewRoot, beforeSnapshotTimeMs));
        }
    }

    private void onSnapshotDoneCallback(ViewStructure viewRoot, long beforeSnapshotTimeMs) {
        viewRoot.asyncCommit();
        mHasFinishedLatestAccessibilitySnapshot = true;

        if (AccessibilityFeaturesMap.isEnabled(
                AccessibilityFeatures.ACCESSIBILITY_SNAPSHOT_STRESS_TESTS)) {
            long snapshotRuntimeMs = SystemClock.elapsedRealtime() - beforeSnapshotTimeMs;
            RecordHistogram.recordLinearCountHistogram(
                    "Accessibility.AXTreeSnapshotter.Snapshot.EndToEndRuntime",
                    (int) snapshotRuntimeMs,
                    1,
                    5 * 1000,
                    100);
        }

        if (ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_UNIFIED_SNAPSHOTS)) {
            // In some cases (e.g. testing) the full engine may also be running, so don't delete.
            if (!isNativeInitialized()) {
                WebContentsAccessibilityImplJni.get().deleteEarly(mNativeAssistDataObj);
                mNativeAssistDataObj = 0;
            }
        }
    }

    @Override
    public boolean performAction(int virtualViewId, int action, Bundle arguments) {
        // We don't support any actions on the host view or nodes
        // that are not (any longer) in the tree.
        if (!isAccessibilityEnabled()
                || shouldPreventNativeEngineUse()
                || !WebContentsAccessibilityImplJni.get().isNodeValid(mNativeObj, virtualViewId)) {
            return false;
        }

        if (mTracker != null) mTracker.addAction(action, arguments);

        // Constant expressions are required for switches. To avoid duplicating aspects of the
        // framework, or adding an enum or IntDef to the codebase, we opt for an if/else-if
        // approach. The benefits of using the Compat library makes up for the messier code.
        if (action == ACTION_ACCESSIBILITY_FOCUS.getId()) {
            if (!moveAccessibilityFocusToId(virtualViewId)) return true;
            if (!mIsHovering) {
                scrollToMakeNodeVisible(mAccessibilityFocusId);
            } else {
                mPendingScrollToMakeNodeVisible = true;
            }
            return true;
        } else if (action == ACTION_CLEAR_ACCESSIBILITY_FOCUS.getId()) {
            // ALWAYS respond with TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED whether we thought
            // it had focus or not, so that the Android framework cache is correct.
            sendAccessibilityEvent(
                    virtualViewId, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED);
            if (mAccessibilityFocusId == virtualViewId) {
                WebContentsAccessibilityImplJni.get()
                        .moveAccessibilityFocus(mNativeObj, mAccessibilityFocusId, View.NO_ID);
                mAccessibilityFocusId = View.NO_ID;
            }
            if (mLastHoverId == virtualViewId) {
                sendAccessibilityEvent(mLastHoverId, AccessibilityEvent.TYPE_VIEW_HOVER_EXIT);
                mLastHoverId = View.NO_ID;
            }
            return true;
        } else if (action == ACTION_CLICK.getId()) {
            if (!mView.hasFocus()) mView.requestFocus();
            performClick(virtualViewId);
            return true;
        } else if (action == ACTION_FOCUS.getId()) {
            if (!mView.hasFocus()) mView.requestFocus();
            WebContentsAccessibilityImplJni.get().focus(mNativeObj, virtualViewId);
            return true;
        } else if (action == ACTION_CLEAR_FOCUS.getId()) {
            WebContentsAccessibilityImplJni.get().blur(mNativeObj);
            return true;
        } else if (action == ACTION_NEXT_HTML_ELEMENT.getId()) {
            if (arguments == null) return false;
            String elementType = arguments.getString(ACTION_ARGUMENT_HTML_ELEMENT_STRING);
            if (elementType == null) return false;
            elementType = elementType.toUpperCase(Locale.US);
            return jumpToElementType(
                    virtualViewId,
                    elementType,
                    /* forwards= */ true,
                    /* canWrap= */ false,
                    /* setSequentialFocus= */ true);
        } else if (action == ACTION_PREVIOUS_HTML_ELEMENT.getId()) {
            if (arguments == null) return false;
            String elementType = arguments.getString(ACTION_ARGUMENT_HTML_ELEMENT_STRING);
            if (elementType == null) return false;
            elementType = elementType.toUpperCase(Locale.US);
            return jumpToElementType(
                    virtualViewId,
                    elementType,
                    /* forwards= */ false,
                    /* canWrap= */ virtualViewId == mCurrentRootId,
                    /* setSequentialFocus= */ true);
        } else if (action == ACTION_SET_TEXT.getId()) {
            if (!WebContentsAccessibilityImplJni.get().isEditableText(mNativeObj, virtualViewId)) {
                return false;
            }
            if (arguments == null) return false;
            CharSequence bundleText =
                    arguments.getCharSequence(ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE);
            if (bundleText == null) return false;
            String newText = bundleText.toString();
            WebContentsAccessibilityImplJni.get()
                    .setTextFieldValue(mNativeObj, virtualViewId, newText);
            // Match Android framework and set the cursor to the end of the text field.
            WebContentsAccessibilityImplJni.get()
                    .setSelection(mNativeObj, virtualViewId, newText.length(), newText.length());
            return true;
        } else if (action == ACTION_SET_SELECTION.getId()) {
            if (!WebContentsAccessibilityImplJni.get().isEditableText(mNativeObj, virtualViewId)) {
                return false;
            }
            int selectionStart = 0;
            int selectionEnd = 0;
            if (arguments != null) {
                selectionStart = arguments.getInt(ACTION_ARGUMENT_SELECTION_START_INT);
                selectionEnd = arguments.getInt(ACTION_ARGUMENT_SELECTION_END_INT);
            }
            WebContentsAccessibilityImplJni.get()
                    .setSelection(mNativeObj, virtualViewId, selectionStart, selectionEnd);
            return true;
        } else if (action == ACTION_NEXT_AT_MOVEMENT_GRANULARITY.getId()) {
            if (arguments == null) return false;
            int granularity = arguments.getInt(ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT);
            boolean extend = arguments.getBoolean(ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN);
            if (!isValidMovementGranularity(granularity)) {
                return false;
                // ATs view paragraphs as a granularity rather than an element type to jump between.
                // As a stopgap until we implement an actual paragraph granularity, we can send
                // these movements to jumpToElementType instead to allow AT users to at least
                // navigate backward and forward by paragraph
                // TODO(jacklynch): Implement paragraph granularity and remove this block
            } else if (granularity == MOVEMENT_GRANULARITY_PARAGRAPH) {
                return jumpToElementType(
                        virtualViewId,
                        PARAGRAPH_ELEMENT_TYPE,
                        /* forwards= */ true,
                        /* canWrap= */ false,
                        /* setSequentialFocus= */ false);
            }
            return nextAtGranularity(granularity, extend, virtualViewId);
        } else if (action == ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY.getId()) {
            if (arguments == null) return false;
            int granularity = arguments.getInt(ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT);
            boolean extend = arguments.getBoolean(ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN);
            if (!isValidMovementGranularity(granularity)) {
                return false;
                // ATs view paragraphs as a granularity rather than an element type to jump between.
                // As a stopgap until we implement an actual paragraph granularity, we can send
                // these movements to jumpToElementType instead to allow AT users to at least
                // navigate backward and forward by paragraph
                // TODO(jacklynch): Implement paragraph granularity and remove this block
            } else if (granularity == MOVEMENT_GRANULARITY_PARAGRAPH) {
                return jumpToElementType(
                        virtualViewId,
                        PARAGRAPH_ELEMENT_TYPE,
                        /* forwards= */ false,
                        /* canWrap= */ virtualViewId == mCurrentRootId,
                        /* setSequentialFocus= */ false);
            }
            return previousAtGranularity(granularity, extend, virtualViewId);
        } else if (action == ACTION_SCROLL_FORWARD.getId()) {
            return scrollForward(virtualViewId);
        } else if (action == ACTION_SCROLL_BACKWARD.getId()) {
            return scrollBackward(virtualViewId);
        } else if (action == ACTION_CUT.getId()) {
            if (mDelegate.getWebContents() != null) {
                ((WebContentsImpl) mDelegate.getWebContents()).cut();
                return true;
            }
            return false;
        } else if (action == ACTION_COPY.getId()) {
            if (mDelegate.getWebContents() != null) {
                ((WebContentsImpl) mDelegate.getWebContents()).copy();
                return true;
            }
            return false;
        } else if (action == ACTION_PASTE.getId()) {
            if (mDelegate.getWebContents() != null) {
                ((WebContentsImpl) mDelegate.getWebContents()).paste();
                return true;
            }
            return false;
        } else if (action == ACTION_COLLAPSE.getId() || action == ACTION_EXPAND.getId()) {
            // If something is collapsible or expandable, just activate it to toggle.
            performClick(virtualViewId);
            return true;
        } else if (action == ACTION_SHOW_ON_SCREEN.getId()) {
            scrollToMakeNodeVisible(virtualViewId);
            return true;
        } else if (action == ACTION_CONTEXT_CLICK.getId() || action == ACTION_LONG_CLICK.getId()) {
            WebContentsAccessibilityImplJni.get().showContextMenu(mNativeObj, virtualViewId);
            return true;
        } else if (action == ACTION_SCROLL_UP.getId() || action == ACTION_PAGE_UP.getId()) {
            return WebContentsAccessibilityImplJni.get()
                    .scroll(
                            mNativeObj,
                            virtualViewId,
                            ScrollDirection.UP,
                            action == ACTION_PAGE_UP.getId());
        } else if (action == ACTION_SCROLL_DOWN.getId() || action == ACTION_PAGE_DOWN.getId()) {
            return WebContentsAccessibilityImplJni.get()
                    .scroll(
                            mNativeObj,
                            virtualViewId,
                            ScrollDirection.DOWN,
                            action == ACTION_PAGE_DOWN.getId());
        } else if (action == ACTION_SCROLL_LEFT.getId() || action == ACTION_PAGE_LEFT.getId()) {
            return WebContentsAccessibilityImplJni.get()
                    .scroll(
                            mNativeObj,
                            virtualViewId,
                            ScrollDirection.LEFT,
                            action == ACTION_PAGE_LEFT.getId());
        } else if (action == ACTION_SCROLL_RIGHT.getId() || action == ACTION_PAGE_RIGHT.getId()) {
            return WebContentsAccessibilityImplJni.get()
                    .scroll(
                            mNativeObj,
                            virtualViewId,
                            ScrollDirection.RIGHT,
                            action == ACTION_PAGE_RIGHT.getId());
        } else if (action == ACTION_SET_PROGRESS.getId()) {
            if (arguments == null) return false;
            if (!arguments.containsKey(ACTION_ARGUMENT_PROGRESS_VALUE)) return false;
            return WebContentsAccessibilityImplJni.get()
                    .setRangeValue(
                            mNativeObj,
                            virtualViewId,
                            arguments.getFloat(ACTION_ARGUMENT_PROGRESS_VALUE));
        } else if (action == ACTION_IME_ENTER.getId()) {
            if (mDelegate.getWebContents() != null) {
                if (ImeAdapterImpl.fromWebContents(mDelegate.getWebContents()) != null) {
                    // We send an unspecified action to ensure Enter key is hit
                    return ImeAdapterImpl.fromWebContents(mDelegate.getWebContents())
                            .performEditorAction(EditorInfo.IME_ACTION_UNSPECIFIED);
                }
            }
            return false;
        } else {
            // This should never be hit, so do the equivalent of NOTREACHED;
            assert false : "AccessibilityNodeProvider called performAction with unexpected action.";
        }

        return false;
    }

    @Override
    public void onAutofillPopupDisplayed(View autofillPopupView) {
        if (isAccessibilityEnabled()) {
            mAutofillPopupView = autofillPopupView;
            WebContentsAccessibilityImplJni.get().onAutofillPopupDisplayed(mNativeObj);
        }
    }

    @Override
    public void onAutofillPopupDismissed() {
        if (isAccessibilityEnabled()) {
            WebContentsAccessibilityImplJni.get().onAutofillPopupDismissed(mNativeObj);
            mAutofillPopupView = null;
        }
    }

    @Override
    public void onAutofillPopupAccessibilityFocusCleared() {
        if (isAccessibilityEnabled()) {
            int id =
                    WebContentsAccessibilityImplJni.get()
                            .getIdForElementAfterElementHostingAutofillPopup(mNativeObj);
            if (id == 0) return;

            moveAccessibilityFocusToId(id);
            scrollToMakeNodeVisible(mAccessibilityFocusId);
        }
    }

    // Returns true if the hover event is to be consumed by accessibility feature.
    @CalledByNative
    private boolean onHoverEvent(int action) {
        if (!isAccessibilityEnabled()) {
            return false;
        }

        if (action == MotionEvent.ACTION_HOVER_EXIT) {
            mIsHovering = false;
            return true;
        }

        mIsHovering = true;
        return true;
    }

    @Override
    public boolean onHoverEventNoRenderer(MotionEvent event) {
        if (!onHoverEvent(event.getAction())) return false;

        float x = event.getX() + mDelegate.getAccessibilityCoordinates().getScrollX();
        float y = event.getY() + mDelegate.getAccessibilityCoordinates().getScrollY();
        return WebContentsAccessibilityImplJni.get().onHoverEventNoRenderer(mNativeObj, x, y);
    }

    @Override
    public void resetFocus() {
        if (mNativeObj == 0) return;

        // Reset accessibility focus.
        WebContentsAccessibilityImplJni.get()
                .moveAccessibilityFocus(mNativeObj, mAccessibilityFocusId, View.NO_ID);
        mAccessibilityFocusId = View.NO_ID;

        sendAccessibilityEvent(mLastHoverId, AccessibilityEvent.TYPE_VIEW_HOVER_EXIT);
        mLastHoverId = View.NO_ID;
    }

    @Override
    public void restoreFocus() {
        if (isAccessibilityEnabled() && mLastAccessibilityFocusId != View.NO_ID) {
            moveAccessibilityFocusToId(mLastAccessibilityFocusId);
            scrollToMakeNodeVisible(mLastAccessibilityFocusId);
        }
    }

    /**
     * Notify us when the frame info is initialized,
     * the first time, since until that point, we can't use AccessibilityCoordinates to transform
     * web coordinates to screen coordinates.
     */
    @CalledByNative
    private void notifyFrameInfoInitialized() {
        if (mNotifyFrameInfoInitializedCalled) return;

        mNotifyFrameInfoInitializedCalled = true;

        // Invalidate the container view, since the chrome accessibility tree is now
        // ready and listed as the child of the container view.
        sendAccessibilityEvent(View.NO_ID, AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);

        // (Re-) focus focused element, since we weren't able to create an
        // AccessibilityNodeInfoCompat for this element before.
        if (!mShouldFocusOnPageLoad) return;
        if (mAccessibilityFocusId != View.NO_ID) {
            moveAccessibilityFocusToId(mAccessibilityFocusId);
        }
    }

    private boolean jumpToElementType(
            int virtualViewId,
            String elementType,
            boolean forwards,
            boolean canWrap,
            boolean setSequentialFocus) {
        int id =
                WebContentsAccessibilityImplJni.get()
                        .findElementType(
                                mNativeObj,
                                virtualViewId,
                                elementType,
                                forwards,
                                canWrap,
                                elementType.isEmpty());
        if (id == 0) return false;

        if (setSequentialFocus) {
            mPendingSetSequentialFocus = true;
            WebContentsAccessibilityImplJni.get().setSequentialFocusStartingPoint(mNativeObj, id);
        }

        moveAccessibilityFocusToId(id);
        scrollToMakeNodeVisible(mAccessibilityFocusId);
        return true;
    }

    private void setGranularityAndUpdateSelection(int granularity) {
        mSelectionGranularity = granularity;

        if (WebContentsAccessibilityImplJni.get().isEditableText(mNativeObj, mAccessibilityFocusId)
                && WebContentsAccessibilityImplJni.get()
                        .isFocused(mNativeObj, mAccessibilityFocusId)) {
            // If selection/cursor are "unassigned" (e.g. first user swipe), then assign as needed
            if (mSelectionStart == -1) {
                mSelectionStart =
                        WebContentsAccessibilityImplJni.get()
                                .getEditableTextSelectionStart(mNativeObj, mAccessibilityFocusId);
            }
            if (mCursorIndex == -1) {
                mCursorIndex =
                        WebContentsAccessibilityImplJni.get()
                                .getEditableTextSelectionEnd(mNativeObj, mAccessibilityFocusId);
            }
        }
    }

    private boolean nextAtGranularity(int granularity, boolean extendSelection, int virtualViewId) {
        if (virtualViewId != mSelectionNodeId) return false;
        setGranularityAndUpdateSelection(granularity);

        // This calls finishGranularityMoveNext when it's done.
        // If we are extending or starting a selection, pass the current cursor index, otherwise
        // default to selection start, which will be the position at the end of the last move
        if (extendSelection && mIsCurrentlyExtendingSelection) {
            return WebContentsAccessibilityImplJni.get()
                    .nextAtGranularity(
                            mNativeObj,
                            mSelectionGranularity,
                            extendSelection,
                            virtualViewId,
                            mCursorIndex);
        } else {
            return WebContentsAccessibilityImplJni.get()
                    .nextAtGranularity(
                            mNativeObj,
                            mSelectionGranularity,
                            extendSelection,
                            virtualViewId,
                            mSelectionStart);
        }
    }

    private boolean previousAtGranularity(
            int granularity, boolean extendSelection, int virtualViewId) {
        if (virtualViewId != mSelectionNodeId) return false;
        setGranularityAndUpdateSelection(granularity);

        // This calls finishGranularityMovePrevious when it's done.
        return WebContentsAccessibilityImplJni.get()
                .previousAtGranularity(
                        mNativeObj,
                        mSelectionGranularity,
                        extendSelection,
                        virtualViewId,
                        mCursorIndex);
    }

    @CalledByNative
    private void finishGranularityMoveNext(
            String text, boolean extendSelection, int itemStartIndex, int itemEndIndex) {
        // Prepare to send both a selection and a traversal event in sequence.
        AccessibilityEvent selectionEvent =
                buildAccessibilityEvent(
                        mSelectionNodeId, AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED);
        if (selectionEvent == null) return;

        AccessibilityEvent traverseEvent =
                buildAccessibilityEvent(
                        mSelectionNodeId,
                        AccessibilityEvent.TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY);
        if (traverseEvent == null) {
            selectionEvent.recycle();
            return;
        }

        // Build selection event dependent on whether user is extending selection or not
        if (extendSelection) {
            // User started selecting, set the selection start point (only set once per selection)
            if (!mIsCurrentlyExtendingSelection) {
                mIsCurrentlyExtendingSelection = true;
                mSelectionStart = itemStartIndex;
            }

            selectionEvent.setFromIndex(mSelectionStart);
            selectionEvent.setToIndex(itemEndIndex);

        } else {
            // User is no longer selecting, or wasn't originally, reset values
            mIsCurrentlyExtendingSelection = false;
            mSelectionStart = itemEndIndex;

            // Set selection to/from indices to new cursor position, itemEndIndex with forwards nav
            selectionEvent.setFromIndex(itemEndIndex);
            selectionEvent.setToIndex(itemEndIndex);
        }

        // Moving forwards, cursor is now at end of granularity move (itemEndIndex)
        mCursorIndex = itemEndIndex;
        selectionEvent.setItemCount(text.length());

        // Call back to native code to update selection
        setSelection(selectionEvent);

        // Build traverse event, set appropriate action
        traverseEvent.setFromIndex(itemStartIndex);
        traverseEvent.setToIndex(itemEndIndex);
        traverseEvent.setItemCount(text.length());
        traverseEvent.setMovementGranularity(mSelectionGranularity);
        traverseEvent.setContentDescription(text);
        traverseEvent.setAction(ACTION_NEXT_AT_MOVEMENT_GRANULARITY.getId());

        requestSendAccessibilityEvent(selectionEvent);
        requestSendAccessibilityEvent(traverseEvent);

        // Suppress the next event since we have already sent traverse and selection for this move
        mSuppressNextSelectionEvent = true;
    }

    @CalledByNative
    private void finishGranularityMovePrevious(
            String text, boolean extendSelection, int itemStartIndex, int itemEndIndex) {
        // Prepare to send both a selection and a traversal event in sequence.
        AccessibilityEvent selectionEvent =
                buildAccessibilityEvent(
                        mSelectionNodeId, AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED);
        if (selectionEvent == null) return;

        AccessibilityEvent traverseEvent =
                buildAccessibilityEvent(
                        mSelectionNodeId,
                        AccessibilityEvent.TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY);
        if (traverseEvent == null) {
            selectionEvent.recycle();
            return;
        }

        // Build selection event dependent on whether user is extending selection or not
        if (extendSelection) {
            // User started selecting, set the selection start point (only set once per selection)
            if (!mIsCurrentlyExtendingSelection) {
                mIsCurrentlyExtendingSelection = true;
                mSelectionStart = itemEndIndex;
            }

            selectionEvent.setFromIndex(mSelectionStart);
            selectionEvent.setToIndex(itemStartIndex);

        } else {
            // User is no longer selecting, or wasn't originally, reset values
            mIsCurrentlyExtendingSelection = false;
            mSelectionStart = itemStartIndex;

            // Set selection to/from indices to new cursor position, itemStartIndex with back nav
            selectionEvent.setFromIndex(itemStartIndex);
            selectionEvent.setToIndex(itemStartIndex);
        }

        // Moving backwards, cursor is now at the start of the granularity move (itemStartIndex)
        mCursorIndex = itemStartIndex;
        selectionEvent.setItemCount(text.length());

        // Call back to native code to update selection
        setSelection(selectionEvent);

        // Build traverse event, set appropriate action
        traverseEvent.setFromIndex(itemStartIndex);
        traverseEvent.setToIndex(itemEndIndex);
        traverseEvent.setItemCount(text.length());
        traverseEvent.setMovementGranularity(mSelectionGranularity);
        traverseEvent.setContentDescription(text);
        traverseEvent.setAction(ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY.getId());

        requestSendAccessibilityEvent(selectionEvent);
        requestSendAccessibilityEvent(traverseEvent);

        // Suppress the next event since we have already sent traverse and selection for this move
        mSuppressNextSelectionEvent = true;
    }

    private void scrollToMakeNodeVisible(int virtualViewId) {
        if (mDelegate.getNativeAXTree() != 0) {
            mDelegate.scrollToMakeNodeVisible(getAbsolutePositionForNode(virtualViewId));
        } else {
            mPendingScrollToMakeNodeVisible = true;
            WebContentsAccessibilityImplJni.get()
                    .scrollToMakeNodeVisible(mNativeObj, virtualViewId);
        }
    }

    private void performClick(int virtualViewId) {
        if (mDelegate.getNativeAXTree() != 0) {
            mDelegate.performClick(getAbsolutePositionForNode(virtualViewId));
        } else {
            WebContentsAccessibilityImplJni.get().click(mNativeObj, virtualViewId);
        }
    }

    private void setSelection(AccessibilityEvent selectionEvent) {
        if (WebContentsAccessibilityImplJni.get().isEditableText(mNativeObj, mSelectionNodeId)
                && WebContentsAccessibilityImplJni.get().isFocused(mNativeObj, mSelectionNodeId)) {
            WebContentsAccessibilityImplJni.get()
                    .setSelection(
                            mNativeObj,
                            mSelectionNodeId,
                            selectionEvent.getFromIndex(),
                            selectionEvent.getToIndex());
        }
    }

    private boolean scrollForward(int virtualViewId) {
        if (WebContentsAccessibilityImplJni.get().isSlider(mNativeObj, virtualViewId)) {
            return WebContentsAccessibilityImplJni.get()
                    .adjustSlider(mNativeObj, virtualViewId, true);
        } else {
            return WebContentsAccessibilityImplJni.get()
                    .scroll(mNativeObj, virtualViewId, ScrollDirection.FORWARD, false);
        }
    }

    private boolean scrollBackward(int virtualViewId) {
        if (WebContentsAccessibilityImplJni.get().isSlider(mNativeObj, virtualViewId)) {
            return WebContentsAccessibilityImplJni.get()
                    .adjustSlider(mNativeObj, virtualViewId, false);
        } else {
            return WebContentsAccessibilityImplJni.get()
                    .scroll(mNativeObj, virtualViewId, ScrollDirection.BACKWARD, false);
        }
    }

    private boolean moveAccessibilityFocusToId(int newAccessibilityFocusId) {
        if (newAccessibilityFocusId == mAccessibilityFocusId) return false;

        if (newAccessibilityFocusId != View.NO_ID) {
            mLastAccessibilityFocusId = newAccessibilityFocusId;
        }

        WebContentsAccessibilityImplJni.get()
                .moveAccessibilityFocus(mNativeObj, mAccessibilityFocusId, newAccessibilityFocusId);

        mAccessibilityFocusId = newAccessibilityFocusId;
        // Used to store the node (edit text field) that has input focus but not a11y focus.
        // Usually while the user is typing in an edit text field, a11y is on the IME and input
        // focus is on the edit field. Granularity move needs to know where the input focus is.
        mSelectionNodeId = mAccessibilityFocusId;
        mSelectionGranularity = NO_GRANULARITY_SELECTED;
        mIsCurrentlyExtendingSelection = false;
        mSelectionStart = -1;
        mCursorIndex =
                WebContentsAccessibilityImplJni.get()
                        .getTextLength(mNativeObj, mAccessibilityFocusId);
        mSuppressNextSelectionEvent = false;

        if (WebContentsAccessibilityImplJni.get()
                .isAutofillPopupNode(mNativeObj, mAccessibilityFocusId)) {
            mAutofillPopupView.requestFocus();
        }

        // Android has a bug that can lead to the a11y focus not being rendered: b/264356970
        // The reason is that this event alone is not enough to rerender, this line works it
        // around by adding the rerender trigger via the underlying view.
        // TODO(b/264356970): Remove when all supported platforms have this bug fixed.
        mView.invalidate();

        sendAccessibilityEvent(
                mAccessibilityFocusId, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED);
        return true;
    }

    /**
     * Send a WINDOW_CONTENT_CHANGED event after a short delay. This helps throttle such
     * events from firing too quickly during animations, for example.
     */
    @CalledByNative
    private void sendDelayedWindowContentChangedEvent() {
        sendAccessibilityEvent(View.NO_ID, AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
    }

    private void sendAccessibilityEvent(int virtualViewId, int eventType) {
        // The container view is indicated by a virtualViewId of NO_ID; post these events directly
        // since there's no web-specific information to attach.
        if (virtualViewId == View.NO_ID) {
            mView.sendAccessibilityEvent(eventType);
            return;
        }

        // Do not send an event when we want to suppress this event, update flag for next event
        if (mSuppressNextSelectionEvent
                && eventType == AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED) {
            mSuppressNextSelectionEvent = false;
            return;
        }

        mHistogramRecorder.incrementEnqueuedEvents();
        mEventDispatcher.enqueueEvent(virtualViewId, eventType);
    }

    private AccessibilityEvent buildAccessibilityEvent(int virtualViewId, int eventType) {
        // If accessibility is disabled, node is invalid, or we don't have any frame info,
        // then the virtual hierarchy doesn't exist in the view of the Android framework,
        // so should never send any events.
        if (!isAccessibilityEnabled()
                || !isFrameInfoInitialized()
                || !WebContentsAccessibilityImplJni.get().isNodeValid(mNativeObj, virtualViewId)) {
            return null;
        }

        final AccessibilityEvent event = AccessibilityEvent.obtain(eventType);
        event.setPackageName(mContext.getPackageName());
        event.setSource(mView, virtualViewId);
        if (eventType == AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED) {
            event.setContentChangeTypes(AccessibilityEvent.CONTENT_CHANGE_TYPE_SUBTREE);
        }
        if (!WebContentsAccessibilityImplJni.get()
                .populateAccessibilityEvent(mNativeObj, event, virtualViewId, eventType)) {
            event.recycle();
            return null;
        }
        return event;
    }

    private AccessibilityNodeInfoCompat createNodeForHost(int rootId) {
        // Since we don't want the parent to be focusable, but we can't remove
        // actions from a node, copy over the necessary fields.
        final AccessibilityNodeInfoCompat result = AccessibilityNodeInfoCompat.obtain(mView);
        // mView requires an |AccessibilityNodeInfo| object here, so we keep the |source| as the
        // non-Compat type rather than unwrapping an |AccessibilityNodeInfoCompat| object.
        final AccessibilityNodeInfo source = AccessibilityNodeInfo.obtain(mView);
        mView.onInitializeAccessibilityNodeInfo(source);

        // Copy over parent and screen bounds.
        Rect rect = new Rect();
        source.getBoundsInParent(rect);
        result.setBoundsInParent(rect);
        source.getBoundsInScreen(rect);
        result.setBoundsInScreen(rect);

        // Set up the parent view, if applicable.
        final ViewParent parent = mView.getParentForAccessibility();
        if (parent instanceof View) {
            result.setParent((View) parent);
        }

        // Populate the minimum required fields.
        result.setVisibleToUser(source.isVisibleToUser());
        result.setEnabled(source.isEnabled());
        result.setPackageName(source.getPackageName());
        result.setClassName(source.getClassName());

        // Add the Chrome root node.
        if (isFrameInfoInitialized()) {
            result.addChild(mView, rootId);
        }

        return result;
    }

    /**
     * Returns whether or not the frame info is initialized, meaning we can safely
     * convert web coordinates to screen coordinates. When this is first initialized,
     * notifyFrameInfoInitialized is called - but we shouldn't check whether or not
     * that method was called as a way to determine if frame info is valid because
     * notifyFrameInfoInitialized might not be called at all if AccessibilityCoordinates
     * gets initialized first.
     */
    private boolean isFrameInfoInitialized() {
        if (mDelegate.getWebContents() == null && mNativeObj == 0) {
            // We already got frame info since WebContents finished its lifecycle.
            return true;
        }

        AccessibilityCoordinates ac = mDelegate.getAccessibilityCoordinates();
        return ac.getContentWidthCss() != 0.0 || ac.getContentHeightCss() != 0.0;
    }

    @CalledByNative
    private void handleFocusChanged(int id) {
        // If |mShouldFocusOnPageLoad| is false, that means this is a WebView and
        // we should avoid moving accessibility focus when the page loads, but more
        // generally we should avoid moving accessibility focus whenever it's not
        // already within this WebView.
        if (!mShouldFocusOnPageLoad && mAccessibilityFocusId == View.NO_ID) return;

        if (mPendingSetSequentialFocus) {
            mPendingSetSequentialFocus = false;

            // Ignore focuses on the root. Setting sequential focus can
            // result in a blur aka focus on the root. This interferes
            // with some accessibility services that move accessibility
            // focus along with input focus.
            if (mCurrentRootId == id) {
                return;
            }
        }

        sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_FOCUSED);
        moveAccessibilityFocusToId(id);
    }

    @CalledByNative
    private void handleCheckStateChanged(int id) {
        // If the node has accessibility focus, fire TYPE_VIEW_CLICKED event. This check ensures
        // only necessary announcements are made (e.g. changing a radio group selection
        // would erroneously announce "checked not checked" without this check)
        if (mAccessibilityFocusId == id) {
            sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_CLICKED);
        }
    }

    @CalledByNative
    private void handleStateDescriptionChanged(int id) {
        if (isAccessibilityEnabled()) {
            AccessibilityEvent event =
                    AccessibilityEvent.obtain(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
            if (event == null) return;

            event.setContentChangeTypes(AccessibilityEvent.CONTENT_CHANGE_TYPE_STATE_DESCRIPTION);
            event.setSource(mView, id);
            requestSendAccessibilityEvent(event);
        }
    }

    @CalledByNative
    private void handleClicked(int id) {
        sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_CLICKED);
    }

    @CalledByNative
    private void handleTextSelectionChanged(int id) {
        sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED);
    }

    @CalledByNative
    private void handleTextContentChanged(int id) {
        AccessibilityEvent event =
                buildAccessibilityEvent(id, AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
        if (event != null) {
            event.setContentChangeTypes(AccessibilityEvent.CONTENT_CHANGE_TYPE_TEXT);
            requestSendAccessibilityEvent(event);
        }
    }

    @CalledByNative
    private void handleEditableTextChanged(int id) {
        sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED);
    }

    @CalledByNative
    private void handleSliderChanged(int id) {
        // If the node has accessibility focus, fire TYPE_VIEW_SELECTED, which triggers
        // TalkBack to announce the change. If not, fire TYPE_VIEW_SCROLLED, which
        // does not trigger an immediate announcement but still ensures some event is fired.
        if (mAccessibilityFocusId == id) {
            sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_SELECTED);
        } else {
            sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_SCROLLED);
        }
    }

    @CalledByNative
    private void handleContentChanged(int id) {
        sendAccessibilityEvent(id, AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
    }

    @CalledByNative
    private void handleNavigate(int newRootId) {
        mAccessibilityFocusId = View.NO_ID;
        mLastAccessibilityFocusId = View.NO_ID;
        mCurrentRootId = newRootId;
        // Invalidate the host, since its child is now gone.
        sendAccessibilityEvent(View.NO_ID, AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
    }

    @CalledByNative
    protected void handleScrollPositionChanged(int id) {
        sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_SCROLLED);
        if (mPendingScrollToMakeNodeVisible) {
            sendAccessibilityEvent(id, AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
            mPendingScrollToMakeNodeVisible = false;
        }
    }

    @CalledByNative
    private void handleScrolledToAnchor(int id) {
        moveAccessibilityFocusToId(id);
    }

    @CalledByNative
    private void handleHover(int id) {
        if (mLastHoverId == id) return;
        if (!mIsHovering) return;

        sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_HOVER_ENTER);
        // The above call doesn't work reliably for nodes that weren't in the viewport when
        // using an AXTree that was cached.
        if (mDelegate.getNativeAXTree() != 0) {
            // As a workaround force the node into focus when a paint preview is showing.
            moveAccessibilityFocusToId(id);
        }
    }

    @CalledByNative
    @SuppressLint("WrongConstant")
    protected void handleDialogModalOpened(int virtualViewId) {
        if (isAccessibilityEnabled()) {
            AccessibilityEvent event =
                    AccessibilityEvent.obtain(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
            if (event == null) return;

            event.setContentChangeTypes(CONTENT_CHANGE_TYPE_PANE_APPEARED);
            event.setSource(mView, virtualViewId);
            requestSendAccessibilityEvent(event);
        }
    }

    @CalledByNative
    private void announceLiveRegionText(String text) {
        if (isAccessibilityEnabled()) {
            AccessibilityEvent event =
                    AccessibilityEvent.obtain(AccessibilityEvent.TYPE_ANNOUNCEMENT);
            if (event == null) return;

            event.getText().add(text);
            event.setContentDescription(null);
            requestSendAccessibilityEvent(event);
        }
    }

    protected boolean areInlineTextBoxesLoaded(int virtualViewId) {
        return WebContentsAccessibilityImplJni.get()
                .areInlineTextBoxesLoaded(mNativeObj, virtualViewId);
    }

    protected void loadInlineTextBoxes(int virtualViewId) {
        WebContentsAccessibilityImplJni.get().loadInlineTextBoxes(mNativeObj, virtualViewId);
    }

    protected int[] getCharacterBoundingBoxes(
            int virtualViewId, int positionInfoStartIndex, int positionInfoLength) {
        return WebContentsAccessibilityImplJni.get()
                .getCharacterBoundingBoxes(
                        mNativeObj, virtualViewId, positionInfoStartIndex, positionInfoLength);
    }

    protected void requestSendAccessibilityEvent(AccessibilityEvent event) {
        // If there is no parent, then the event can be ignored. In general the parent is only
        // transiently null (such as during teardown, switching tabs...). Also ensure that
        // accessibility is still enabled, throttling may result in events sent late.
        if (mView.getParent() != null && isAccessibilityEnabled()) {
            mHistogramRecorder.incrementDispatchedEvents();
            if (mTracker != null) mTracker.addEvent(event);
            try {
                mView.getParent().requestSendAccessibilityEvent(mView, event);
            } catch (IllegalStateException ignored) {
                // During boot-up of some content shell tests, events will erroneously be sent even
                // though the AccessibilityManager is not enabled, resulting in a crash.
                // TODO(mschillaci): Address flakiness to remove this try/catch, crbug.com/1186376.
            }
        }
    }

    private Rect getAbsolutePositionForNode(int virtualViewId) {
        int[] coords =
                WebContentsAccessibilityImplJni.get()
                        .getAbsolutePositionForNode(mNativeObj, virtualViewId);
        if (coords == null) return null;

        return new Rect(coords[0], coords[1], coords[2], coords[3]);
    }

    @CalledByNative
    private void setAccessibilityEventBaseAttributes(
            AccessibilityEvent event,
            boolean checked,
            boolean enabled,
            boolean password,
            boolean scrollable,
            int currentItemIndex,
            int itemCount,
            int scrollX,
            int scrollY,
            int maxScrollX,
            int maxScrollY,
            String className) {
        event.setChecked(checked);
        event.setEnabled(enabled);
        event.setPassword(password);
        event.setScrollable(scrollable);
        event.setCurrentItemIndex(currentItemIndex);
        event.setItemCount(itemCount);
        event.setScrollX(scrollX);
        event.setScrollY(scrollY);
        event.setMaxScrollX(maxScrollX);
        event.setMaxScrollY(maxScrollY);
        event.setClassName(className);
    }

    @CalledByNative
    private void setAccessibilityEventTextChangedAttrs(
            AccessibilityEvent event,
            int fromIndex,
            int addedCount,
            int removedCount,
            String beforeText,
            String text) {
        event.setFromIndex(fromIndex);
        event.setAddedCount(addedCount);
        event.setRemovedCount(removedCount);
        event.setBeforeText(beforeText);
        event.getText().add(text);
    }

    @CalledByNative
    private void setAccessibilityEventSelectionAttrs(
            AccessibilityEvent event, int fromIndex, int toIndex, int itemCount, String text) {
        event.setFromIndex(fromIndex);
        event.setToIndex(toIndex);
        event.setItemCount(itemCount);
        event.getText().add(text);
    }

    @Override
    public void addExtraDataToAccessibilityNodeInfo(
            int virtualViewId,
            AccessibilityNodeInfoCompat info,
            String extraDataKey,
            Bundle arguments) {
        switch (extraDataKey) {
            case EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY:
                getExtraDataTextCharacterLocations(virtualViewId, info, arguments);
                break;
            case EXTRAS_DATA_REQUEST_IMAGE_DATA_KEY:
                getImageData(virtualViewId, info);
                break;
        }
    }

    private void getExtraDataTextCharacterLocations(
            int virtualViewId, AccessibilityNodeInfoCompat info, Bundle arguments) {
        // Arguments must be provided, but some debug tools may not so guard against this.
        if (arguments == null) return;

        if (!areInlineTextBoxesLoaded(virtualViewId)) {
            loadInlineTextBoxes(virtualViewId);
        }

        int positionInfoStartIndex =
                arguments.getInt(EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX, -1);
        int positionInfoLength =
                arguments.getInt(EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH, -1);
        if (positionInfoLength <= 0 || positionInfoStartIndex < 0) return;

        int[] coords =
                getCharacterBoundingBoxes(
                        virtualViewId, positionInfoStartIndex, positionInfoLength);
        if (coords == null) return;
        assert coords.length == positionInfoLength * 4;

        RectF[] boundingRects = new RectF[positionInfoLength];
        for (int i = 0; i < positionInfoLength; i++) {
            Rect rect =
                    new Rect(
                            coords[4 * i + 0],
                            coords[4 * i + 1],
                            coords[4 * i + 2],
                            coords[4 * i + 3]);
            AccessibilityNodeInfoBuilder.convertWebRectToAndroidCoordinates(
                    rect, info.getExtras(), mDelegate.getAccessibilityCoordinates(), mView);
            boundingRects[i] = new RectF(rect);
        }

        info.getExtras().putParcelableArray(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY, boundingRects);
    }

    private void getImageData(int virtualViewId, AccessibilityNodeInfoCompat info) {
        boolean hasSentPreviousRequest = mImageDataRequestedNodes.contains(virtualViewId);
        // If the below call returns true, then image data has been set on the node.
        if (!WebContentsAccessibilityImplJni.get()
                .getImageData(mNativeObj, info, virtualViewId, hasSentPreviousRequest)) {
            // If the above call returns false, then the data was missing. The native-side code
            // will have started the asynchronous process to populate the image data if no previous
            // request has been sent. Add this |virtualViewId| to the list of requested nodes.
            mImageDataRequestedNodes.add(virtualViewId);
        }
    }

    @NativeMethods
    interface Natives {
        long init(
                WebContentsAccessibilityImpl caller,
                WebContents webContents,
                AccessibilityNodeInfoBuilder builder);

        long initWithAXTree(
                WebContentsAccessibilityImpl caller,
                long axTreePtr,
                AccessibilityNodeInfoBuilder builder);

        // These two methods are only used for one-off accessibility tree snapshots.
        long initForAssistData(
                WebContentsAccessibilityImpl caller,
                WebContents webContents,
                AssistDataBuilder builder);

        void requestAccessibilityTreeSnapshot(
                long nativeWebContentsAccessibilityAndroid,
                ViewStructure viewRoot,
                AccessibilityDelegate.AccessibilityCoordinates accessibilityCoordinates,
                View view,
                Runnable onDoneCallback);

        void connectInstanceToRootManager(long nativeWebContentsAccessibilityAndroid);

        void setBrowserAXMode(
                WebContentsAccessibilityImpl caller,
                boolean screenReaderMode,
                boolean formControlsMode,
                boolean isAccessibilityEnabled);

        void disableRendererAccessibility(long nativeWebContentsAccessibilityAndroid);

        void reEnableRendererAccessibility(
                long nativeWebContentsAccessibilityAndroid, WebContents webContents);

        void deleteEarly(long nativeWebContentsAccessibilityAndroid);

        void onAutofillPopupDisplayed(long nativeWebContentsAccessibilityAndroid);

        void onAutofillPopupDismissed(long nativeWebContentsAccessibilityAndroid);

        int getIdForElementAfterElementHostingAutofillPopup(
                long nativeWebContentsAccessibilityAndroid);

        int getRootId(long nativeWebContentsAccessibilityAndroid);

        boolean isNodeValid(long nativeWebContentsAccessibilityAndroid, int id);

        boolean isAutofillPopupNode(long nativeWebContentsAccessibilityAndroid, int id);

        boolean isEditableText(long nativeWebContentsAccessibilityAndroid, int id);

        boolean isFocused(long nativeWebContentsAccessibilityAndroid, int id);

        int getEditableTextSelectionStart(long nativeWebContentsAccessibilityAndroid, int id);

        int getEditableTextSelectionEnd(long nativeWebContentsAccessibilityAndroid, int id);

        int[] getAbsolutePositionForNode(long nativeWebContentsAccessibilityAndroid, int id);

        boolean updateCachedAccessibilityNodeInfo(
                long nativeWebContentsAccessibilityAndroid,
                AccessibilityNodeInfoCompat info,
                int id);

        boolean populateAccessibilityNodeInfo(
                long nativeWebContentsAccessibilityAndroid,
                AccessibilityNodeInfoCompat info,
                int id);

        boolean populateAccessibilityEvent(
                long nativeWebContentsAccessibilityAndroid,
                AccessibilityEvent event,
                int id,
                int eventType);

        void click(long nativeWebContentsAccessibilityAndroid, int id);

        void focus(long nativeWebContentsAccessibilityAndroid, int id);

        void blur(long nativeWebContentsAccessibilityAndroid);

        void scrollToMakeNodeVisible(long nativeWebContentsAccessibilityAndroid, int id);

        int findElementType(
                long nativeWebContentsAccessibilityAndroid,
                int startId,
                String elementType,
                boolean forwards,
                boolean canWrapToLastElement,
                boolean useDefaultPredicate);

        void setTextFieldValue(long nativeWebContentsAccessibilityAndroid, int id, String newValue);

        void setSelection(long nativeWebContentsAccessibilityAndroid, int id, int start, int end);

        boolean nextAtGranularity(
                long nativeWebContentsAccessibilityAndroid,
                int selectionGranularity,
                boolean extendSelection,
                int id,
                int cursorIndex);

        boolean previousAtGranularity(
                long nativeWebContentsAccessibilityAndroid,
                int selectionGranularity,
                boolean extendSelection,
                int id,
                int cursorIndex);

        boolean adjustSlider(long nativeWebContentsAccessibilityAndroid, int id, boolean increment);

        void moveAccessibilityFocus(
                long nativeWebContentsAccessibilityAndroid, int oldId, int newId);

        void setSequentialFocusStartingPoint(long nativeWebContentsAccessibilityAndroid, int id);

        boolean isSlider(long nativeWebContentsAccessibilityAndroid, int id);

        boolean scroll(
                long nativeWebContentsAccessibilityAndroid,
                int id,
                int direction,
                boolean pageScroll);

        boolean setRangeValue(long nativeWebContentsAccessibilityAndroid, int id, float value);

        String getSupportedHtmlElementTypes(long nativeWebContentsAccessibilityAndroid);

        void showContextMenu(long nativeWebContentsAccessibilityAndroid, int id);

        boolean isRootManagerConnected(long nativeWebContentsAccessibilityAndroid);

        boolean areInlineTextBoxesLoaded(long nativeWebContentsAccessibilityAndroid, int id);

        void loadInlineTextBoxes(long nativeWebContentsAccessibilityAndroid, int id);

        int[] getCharacterBoundingBoxes(
                long nativeWebContentsAccessibilityAndroid, int id, int start, int len);

        int getTextLength(long nativeWebContentsAccessibilityAndroid, int id);

        void addSpellingErrorForTesting(
                long nativeWebContentsAccessibilityAndroid, int id, int startOffset, int endOffset);

        void setMaxContentChangedEventsToFireForTesting(
                long nativeWebContentsAccessibilityAndroid, int maxEvents);

        int getMaxContentChangedEventsToFireForTesting(long nativeWebContentsAccessibilityAndroid);

        void signalEndOfTestForTesting(long nativeWebContentsAccessibilityAndroid);

        void setAllowImageDescriptions(
                long nativeWebContentsAccessibilityAndroid, boolean allowImageDescriptions);

        boolean onHoverEventNoRenderer(
                long nativeWebContentsAccessibilityAndroid, float x, float y);

        boolean getImageData(
                long nativeWebContentsAccessibilityAndroid,
                AccessibilityNodeInfoCompat info,
                int id,
                boolean hasSentPreviousRequest);
    }
}
