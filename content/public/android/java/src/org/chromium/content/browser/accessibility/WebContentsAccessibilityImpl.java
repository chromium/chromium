// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

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
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_CHARACTER;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_LINE;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_PARAGRAPH;
import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.MOVEMENT_GRANULARITY_WORD;

import android.annotation.SuppressLint;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ReceiverCallNotAllowedException;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.style.LocaleSpan;
import android.text.style.SuggestionSpan;
import android.text.style.URLSpan;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityManager.AccessibilityStateChangeListener;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.autofill.AutofillManager;
import android.view.inputmethod.EditorInfo;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.core.view.accessibility.AccessibilityNodeProviderCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.UserData;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content.browser.WindowEventObserver;
import org.chromium.content.browser.WindowEventObserverManager;
import org.chromium.content.browser.accessibility.AccessibilityDelegate.AccessibilityCoordinates;
import org.chromium.content.browser.accessibility.captioning.CaptioningController;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl.UserDataFactory;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Implementation of {@link WebContentsAccessibility} interface.
 * Native accessibility for a {@link WebContents}. Actual native instance is
 * created lazily upon the first request from Android framework on
 * {@link AccessibilityNodeProvider}, and shares the lifetime with {@link WebContents}.
 * Internally this class uses the {@link AccessibilityNodeProviderCompat} interface, and uses
 * the {@link AccessibilityNodeInfoCompat} object for the virtual tree, but will unwrap and surface
 * the non-Compat versions of these for any clients.
 */
@JNINamespace("content")
public class WebContentsAccessibilityImpl extends AccessibilityNodeProviderCompat
        implements AccessibilityStateChangeListener, WebContentsAccessibility, WindowEventObserver,
                   UserData, AccessibilityState.Listener,
                   ViewAndroidDelegate.ContainerViewObserver {
    private static final String TAG = "A11yImpl";
    // The following constants have been hard coded so we can support actions newer than our
    // minimum SDK without having to break methods into a series of subclasses.
    // TODO(mschillaci): Remove these once they are added to the AccessibilityNodeInfoCompat class.
    public static final String EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY =
            "android.view.accessibility.extra.DATA_TEXT_CHARACTER_LOCATION_KEY";
    public static final String EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX =
            "android.view.accessibility.extra.DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX";
    public static final String EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH =
            "android.view.accessibility.extra.DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH";
    public static final int CONTENT_CHANGE_TYPE_PANE_APPEARED = 0x00000010;

    // Constants defined for AccessibilityNodeInfo Bundle extras keys.
    public static final String EXTRAS_KEY_BRAILLE_LABEL = "AccessibilityNodeInfo.brailleLabel";
    public static final String EXTRAS_KEY_BRAILLE_ROLE_DESCRIPTION =
            "AccessibilityNodeInfo.brailleRoleDescription";
    public static final String EXTRAS_KEY_CHROME_ROLE = "AccessibilityNodeInfo.chromeRole";
    public static final String EXTRAS_KEY_CLICKABLE_SCORE = "AccessibilityNodeInfo.clickableScore";
    public static final String EXTRAS_KEY_CSS_DISPLAY = "AccessibilityNodeInfo.cssDisplay";
    public static final String EXTRAS_KEY_HAS_IMAGE = "AccessibilityNodeInfo.hasImage";
    public static final String EXTRAS_KEY_HINT = "AccessibilityNodeInfo.hint";
    public static final String EXTRAS_KEY_IMAGE_DATA = "AccessibilityNodeInfo.imageData";
    public static final String EXTRAS_KEY_OFFSCREEN = "AccessibilityNodeInfo.offscreen";
    public static final String EXTRAS_KEY_ROLE_DESCRIPTION =
            "AccessibilityNodeInfo.roleDescription";
    public static final String EXTRAS_KEY_SUPPORTED_ELEMENTS =
            "ACTION_ARGUMENT_HTML_ELEMENT_STRING_VALUES";
    public static final String EXTRAS_KEY_TARGET_URL = "AccessibilityNodeInfo.targetUrl";
    public static final String EXTRAS_KEY_UNCLIPPED_TOP = "AccessibilityNodeInfo.unclippedTop";
    public static final String EXTRAS_KEY_UNCLIPPED_BOTTOM =
            "AccessibilityNodeInfo.unclippedBottom";
    public static final String EXTRAS_KEY_URL = "url";

    // Constants defined for requests to add data to AccessibilityNodeInfo Bundle extras.
    public static final String EXTRAS_DATA_REQUEST_IMAGE_DATA_KEY =
            "AccessibilityNodeInfo.requestImageData";

    // Constant for paragraph predicate key from web_contents_accessibility_android.cc
    private static final String PARAGRAPH_ELEMENT_TYPE = "PARAGRAPH";

    // Constant for no granularity selected.
    private static final int NO_GRANULARITY_SELECTED = 0;

    // Delay times for throttling of successive AccessibilityEvents in milliseconds.
    private static final int ACCESSIBILITY_EVENT_DELAY_DEFAULT = 100;
    private static final int ACCESSIBILITY_EVENT_DELAY_HOVER = 50;

    // Throttle time for content invalid utterances. Content invalid will only be announced at most
    // once per this time interval in milliseconds for a given focused node.
    private static final int CONTENT_INVALID_THROTTLE_DELAY = 4500;

    // Static instances of the two types of extra data keys that can be added to nodes.
    private static final List<String> sTextCharacterLocation =
            Collections.singletonList(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY);

    private static final List<String> sRequestImageData =
            Collections.singletonList(EXTRAS_DATA_REQUEST_IMAGE_DATA_KEY);

    private final AccessibilityDelegate mDelegate;
    protected AccessibilityManager mAccessibilityManager;
    protected Context mContext;
    private String mProductVersion;
    protected long mNativeObj;
    private Rect mAccessibilityFocusRect;
    private boolean mIsHovering;
    private int mLastHoverId = View.NO_ID;
    private int mCurrentRootId;
    protected View mView;
    private boolean mUserHasTouchExplored;
    private boolean mPendingScrollToMakeNodeVisible;
    private boolean mNotifyFrameInfoInitializedCalled;
    private boolean mAccessibilityEnabledOverride;
    private int mSelectionGranularity;
    private int mAccessibilityFocusId;
    private int mSelectionNodeId;
    private View mAutofillPopupView;
    private CaptioningController mCaptioningController;
    private boolean mIsCurrentlyExtendingSelection;
    private int mSelectionStart;
    private int mCursorIndex;
    private String mSupportedHtmlElementTypes;

    // Tracker for all actions performed and events sent by this instance, used for testing.
    private AccessibilityActionAndEventTracker mTracker;

    private AccessibilityHistogramRecorder mHistogramRecorder;

    // Whether or not the next selection event should be fired. We only want to sent one traverse
    // and one selection event per granularity move, this ensures no double events while still
    // sending events when the user is using other assistive technology (e.g. external keyboard)
    private boolean mSuppressNextSelectionEvent;

    // Whether native accessibility is allowed.
    private boolean mNativeAccessibilityAllowed;

    // Whether accessibility focus should be set to the page when it finishes loading.
    // This only applies if an accessibility service like TalkBack is running.
    // This is desirable behavior for a browser window, but not for an embedded
    // WebView.
    private boolean mShouldFocusOnPageLoad;

    // True if this instance is a candidate to have the image descriptions feature enabled. The
    // feature is dependent on embedder behavior and screen reader state. Default false.
    private boolean mIsImageDescriptionsCandidate;

    // If true, the web contents are obscured by another view and we shouldn't
    // return an AccessibilityNodeProvider or process touch exploration events.
    private boolean mIsObscuredByAnotherView;

    // Accessibility touch exploration state.
    private boolean mTouchExplorationEnabled;

    // This array maps a given virtualViewId to an |AccessibilityNodeInfoCompat| for that view. We
    // use this to update a node quickly rather than building from one scratch each time.
    private SparseArray<AccessibilityNodeInfoCompat> mNodeInfoCache = new SparseArray<>();

    // This handles the dispatching of accessibility events. It acts as an intermediary where we can
    // apply throttling rules, delay event construction, etc.
    private AccessibilityEventDispatcher mEventDispatcher;
    private String mSystemLanguageTag;
    private BroadcastReceiver mBroadcastReceiver;

    // These track the last focused content invalid view id and the last time we reported content
    // invalid for that node. Used to ensure we report content invalid on a node once per interval.
    private int mLastContentInvalidViewId;
    private long mLastContentInvalidUtteranceTime;

    // Set of all nodes that have received a request to populate image data. The request only needs
    // to be run once per node, and it completes asynchronously. We track which nodes have already
    // started the async request so that if downstream apps request the same node multiple times
    // we can avoid doing the extra work.
    private final Set<Integer> mImageDataRequestedNodes = new HashSet<Integer>();

    /**
     * Create a WebContentsAccessibilityImpl object.
     */
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

        WebContents webContents = mDelegate.getWebContents();
        if (webContents != null) {
            mCaptioningController = new CaptioningController(webContents);
            WindowEventObserverManager.from(webContents).addObserver(this);
            webContents.getViewAndroidDelegate().addObserver(this);
        } else {
            refreshState();
        }
        mDelegate.setOnScrollPositionChangedCallback(() -> {
            handleScrollPositionChanged(mAccessibilityFocusId);
            moveAccessibilityFocusToIdAndRefocusIfNeeded(mAccessibilityFocusId);
        });

        AccessibilityState.addListener(this);

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

        mEventDispatcher =
                new AccessibilityEventDispatcher(new AccessibilityEventDispatcher.Client() {
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

                        // Always send the ENTER and then the EXIT event, to match a standard
                        // Android View.
                        if (eventType == AccessibilityEvent.TYPE_VIEW_HOVER_ENTER) {
                            AccessibilityEvent exitEvent = buildAccessibilityEvent(
                                    mLastHoverId, AccessibilityEvent.TYPE_VIEW_HOVER_EXIT);
                            if (exitEvent != null) {
                                requestSendAccessibilityEvent(exitEvent);
                                mLastHoverId = virtualViewId;
                            } else if (virtualViewId != View.NO_ID
                                    && mLastHoverId != virtualViewId) {
                                // If IDs become mismatched, or on first hover, this will sync the
                                // values again so all further hovers have correct event pairing.
                                mLastHoverId = virtualViewId;
                            }
                        }

                        return true;
                    }
                }, eventThrottleDelays, viewIndependentEvents, new HashSet<Integer>(), false);

        // Need to be initialized before AXTreeUpdate initialization because updateMaxNodesInCache
        // gets called then
        mHistogramRecorder = new AccessibilityHistogramRecorder();

        if (mDelegate.getNativeAXTree() != 0) {
            initializeNativeWithAXTreeUpdate(mDelegate.getNativeAXTree());
        }
        // If the AXTree is not provided, native is initialized lazily, when node provider is
        // actually requested.

        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
            // The system service call for AutofillManager can timeout and throws an Exception.
            // This is treated differently in each version of Android, so we must catch a
            // generic Exception. (refer to crbug.com/1186406 or AutofillManagerWrapper ctor).
            try {
                AutofillManager autofillManager = mContext.getSystemService(AutofillManager.class);
                if (autofillManager != null && autofillManager.isEnabled()) {
                    // Native accessibility is usually initialized when getAccessibilityNodeProvider
                    // is called, but the Autofill compatibility bridge only calls that method after
                    // it has received the first accessibility events. To solve the chicken-and-egg
                    // problem, always initialize the native parts when the user has an Autofill
                    // service enabled.
                    refreshState();
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
        mAccessibilityFocusId = View.NO_ID;
        mSelectionNodeId = View.NO_ID;
        mIsHovering = false;
        mCurrentRootId = View.NO_ID;

        mSupportedHtmlElementTypes =
                WebContentsAccessibilityImplJni.get().getSupportedHtmlElementTypes(mNativeObj);
        mBroadcastReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                mSystemLanguageTag = Locale.getDefault().toLanguageTag();
            }
        };

        // Register a broadcast receiver for locale change.
        if (mView.isAttachedToWindow()) registerLocaleChangeReceiver();

        // TODO(mschillaci,jacklynch): Move into {refreshNativeState} or similar method once
        //                            {BrowserAccessibilityState.Listener} has more granularity.
        // Define a set of relevant AccessibilityEvents if the OnDemand feature is enabled.
        if (ContentFeatureList.isEnabled(ContentFeatureList.ON_DEMAND_ACCESSIBILITY_EVENTS)) {
            Runnable serviceMaskRunnable = () -> {
                int serviceEventMask = AccessibilityState.getAccessibilityServiceEventTypeMask();
                mEventDispatcher.updateRelevantEventTypes(
                        convertMaskToEventTypes(serviceEventMask));
                mEventDispatcher.setOnDemandEnabled(true);
            };
            mView.post(serviceMaskRunnable);
        }

        // Send state values set by embedders to native-side objects.
        refreshNativeState();

        TraceEvent.end("WebContentsAccessibilityImpl.onNativeInit");
    }

    @CalledByNative
    protected void onNativeObjectDestroyed() {
        mNativeObj = 0;
    }

    protected boolean isNativeInitialized() {
        return mNativeObj != 0;
    }

    private boolean isEnabled() {
        return isNativeInitialized() ? WebContentsAccessibilityImplJni.get().isEnabled(mNativeObj)
                                     : false;
    }

    @VisibleForTesting
    @Override
    public void setAccessibilityEnabledForTesting() {
        mAccessibilityEnabledOverride = true;
    }

    @VisibleForTesting
    @Override
    public void setBrowserAccessibilityStateForTesting() {
        AccessibilityState.setEventTypeMaskForTesting();
    }

    @VisibleForTesting
    @Override
    public void addSpellingErrorForTesting(int virtualViewId, int startOffset, int endOffset) {
        WebContentsAccessibilityImplJni.get().addSpellingErrorForTesting(
                mNativeObj, virtualViewId, startOffset, endOffset);
    }

    @VisibleForTesting
    public void setMaxContentChangedEventsToFireForTesting(int maxEvents) {
        WebContentsAccessibilityImplJni.get().setMaxContentChangedEventsToFireForTesting(
                mNativeObj, maxEvents);
    }

    @VisibleForTesting
    public int getMaxContentChangedEventsToFireForTesting() {
        return WebContentsAccessibilityImplJni.get().getMaxContentChangedEventsToFireForTesting(
                mNativeObj);
    }

    @VisibleForTesting
    public void setAccessibilityTrackerForTesting(AccessibilityActionAndEventTracker tracker) {
        mTracker = tracker;
    }

    @VisibleForTesting
    public void signalEndOfTestForTesting() {
        WebContentsAccessibilityImplJni.get().signalEndOfTestForTesting(mNativeObj);
    }

    @VisibleForTesting
    public void forceRecordUMAHistogramsForTesting() {
        mHistogramRecorder.recordEventsHistograms();
    }

    @VisibleForTesting
    public void forceRecordCacheUMAHistogramsForTesting() {
        mHistogramRecorder.recordCacheHistograms();
    }

    @VisibleForTesting
    public void setEventTypeMaskEmptyForTesting() {
        AccessibilityState.setEventTypeMaskEmptyForTesting();
    }

    @VisibleForTesting
    public void setScreenReaderModeForTesting(boolean enabled) {
        AccessibilityState.setScreenReaderModeForTesting(enabled);
    }

    @CalledByNative
    public void handleEndOfTestSignal() {
        // We have received a signal that we have reached the end of a unit test. If we have a
        // tracker listening, set the test is complete.
        if (mTracker != null) {
            mTracker.signalEndOfTest();
        }
    }

    // WindowEventObserver

    @Override
    public void onDetachedFromWindow() {
        mAccessibilityManager.removeAccessibilityStateChangeListener(this);
        mCaptioningController.stopListening();
        if (!isNativeInitialized()) return;
        ContextUtils.getApplicationContext().unregisterReceiver(mBroadcastReceiver);
        mHistogramRecorder.recordHistograms();
    }

    @Override
    public void onAttachedToWindow() {
        TraceEvent.begin("WebContentsAccessibilityImpl.onAttachedToWindow");
        mAccessibilityManager.addAccessibilityStateChangeListener(this);
        refreshState();
        refreshNativeState();
        mCaptioningController.startListening();
        registerLocaleChangeReceiver();
        TraceEvent.end("WebContentsAccessibilityImpl.onAttachedToWindow");
    }

    private void registerLocaleChangeReceiver() {
        if (!isNativeInitialized()) return;
        try {
            IntentFilter filter = new IntentFilter(Intent.ACTION_LOCALE_CHANGED);
            ContextUtils.registerProtectedBroadcastReceiver(
                    ContextUtils.getApplicationContext(), mBroadcastReceiver, filter);
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
        if (mDelegate.getWebContents() == null) {
            deleteEarly();
        } else {
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

    /**
     * Refresh a11y state with that of {@link AccessibilityManager}.
     */
    public void refreshState() {
        setState(mAccessibilityManager.isEnabled());
    }

    private void refreshNativeState() {
        try (TraceEvent te = TraceEvent.scoped("WebContentsAccessibilityImpl.refreshNativeState")) {
            if (!isNativeInitialized()) return;

            // Update the AXMode based on screen reader status.
            WebContentsAccessibilityImplJni.get().setAXMode(mNativeObj,
                    AccessibilityState.screenReaderMode(),
                    /* isAccessibilityEnabled= */ true);

            // Update the state of how passwords are exposed based on user settings.
            WebContentsAccessibilityImplJni.get().setPasswordRules(mNativeObj,
                    AccessibilityAutofillHelper.shouldRespectDisplayedPasswordText(),
                    AccessibilityAutofillHelper.shouldExposePasswordText());

            // Update the state of enabling/disabling the image descriptions feature. To enable the
            // feature, this instance must be a candidate and a screen reader must be enabled.
            WebContentsAccessibilityImplJni.get().setAllowImageDescriptions(mNativeObj,
                    mIsImageDescriptionsCandidate && AccessibilityState.screenReaderMode());
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
        if (mIsObscuredByAnotherView) return null;

        if (!isNativeInitialized()) {
            if (!mNativeAccessibilityAllowed) return null;
            if (mDelegate.getWebContents() != null) {
                mNativeObj = WebContentsAccessibilityImplJni.get().init(
                        WebContentsAccessibilityImpl.this, mDelegate.getWebContents());
            } else {
                return null;
            }
            onNativeInit();
        }
        if (!isEnabled()) {
            boolean screenReaderMode = AccessibilityState.screenReaderMode();
            WebContentsAccessibilityImplJni.get().enable(mNativeObj, screenReaderMode);
            return null;
        }

        return this;
    }

    protected void initializeNativeWithAXTreeUpdate(long nativeAxTree) {
        assert !isNativeInitialized();

        mNativeObj = WebContentsAccessibilityImplJni.get().initWithAXTree(
                WebContentsAccessibilityImpl.this, nativeAxTree);
        onNativeInit();
    }

    @CalledByNative
    public String generateAccessibilityNodeInfoString(int virtualViewId) {
        // If accessibility isn't enabled, all the AccessibilityNodeInfoCompat objects will be null,
        // so temporarily set the |mAccessibilityEnabledOverride| flag to true, then disable it.
        mAccessibilityEnabledOverride = true;
        String returnString =
                AccessibilityNodeInfoUtils.toString(createAccessibilityNodeInfo(virtualViewId));
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

            if (WebContentsAccessibilityImplJni.get().updateCachedAccessibilityNodeInfo(
                        mNativeObj, cachedNode, virtualViewId)) {
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

            if (WebContentsAccessibilityImplJni.get().populateAccessibilityNodeInfo(
                        mNativeObj, info, virtualViewId)) {
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

    // AccessibilityStateChangeListener
    // TODO(dmazzoni): have BrowserAccessibilityState monitor this and merge
    // into BrowserAccessibilityStateListener.

    @Override
    public void onAccessibilityStateChanged(boolean enabled) {
        setState(enabled);
    }

    // BrowserAccessibilityStateListener

    @Override
    public void onBrowserAccessibilityStateChanged(boolean newScreenReaderEnabledState) {
        refreshNativeState();

        // TODO(mschillaci,jacklynch): Move into {refreshNativeState} or similar method once
        //                            {BrowserAccessibilityState.Listener} has more granularity.
        // Update the list of events we dispatch to enabled services.
        if (isNativeInitialized()
                && ContentFeatureList.isEnabled(
                        ContentFeatureList.ON_DEMAND_ACCESSIBILITY_EVENTS)) {
            int serviceEventMask = AccessibilityState.getAccessibilityServiceEventTypeMask();
            mEventDispatcher.updateRelevantEventTypes(convertMaskToEventTypes(serviceEventMask));
        }
    }

    public Set<Integer> convertMaskToEventTypes(int serviceEventTypes) {
        Set<Integer> relevantEventTypes = new HashSet<Integer>();
        int eventTypeBit;

        while (serviceEventTypes != 0) {
            eventTypeBit = (1 << Integer.numberOfTrailingZeros(serviceEventTypes));
            relevantEventTypes.add(eventTypeBit);
            serviceEventTypes &= ~eventTypeBit;
        }

        return relevantEventTypes;
    }

    // WebContentsAccessibility

    @Override
    public void setObscuredByAnotherView(boolean isObscured) {
        if (isObscured != mIsObscuredByAnotherView) {
            mIsObscuredByAnotherView = isObscured;
            sendAccessibilityEvent(View.NO_ID, AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
        }
    }

    @Override
    public boolean isTouchExplorationEnabled() {
        return mTouchExplorationEnabled;
    }

    @Override
    public void setState(boolean state) {
        if (!state) {
            mNativeAccessibilityAllowed = false;
            mTouchExplorationEnabled = false;
        } else {
            mNativeAccessibilityAllowed = true;
            mTouchExplorationEnabled = mAccessibilityManager.isTouchExplorationEnabled();
        }
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
    public boolean supportsAction(int action) {
        // TODO(dmazzoni): implement this.
        return false;
    }

    @Override
    public boolean performAction(int action, Bundle arguments) {
        // TODO(dmazzoni): implement this.
        return false;
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

        mDelegate.requestAccessibilitySnapshot(viewRoot, new Runnable() {
            @Override
            public void run() {
                viewRoot.asyncCommit();
            }
        });
    }

    @Override
    public boolean performAction(int virtualViewId, int action, Bundle arguments) {
        // We don't support any actions on the host view or nodes
        // that are not (any longer) in the tree.
        if (!isAccessibilityEnabled()
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
                WebContentsAccessibilityImplJni.get().moveAccessibilityFocus(
                        mNativeObj, mAccessibilityFocusId, View.NO_ID);
                mAccessibilityFocusId = View.NO_ID;
                mAccessibilityFocusRect = null;
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
                    virtualViewId, elementType, /*forwards*/ true, /*canWrap*/ false);
        } else if (action == ACTION_PREVIOUS_HTML_ELEMENT.getId()) {
            if (arguments == null) return false;
            String elementType = arguments.getString(ACTION_ARGUMENT_HTML_ELEMENT_STRING);
            if (elementType == null) return false;
            elementType = elementType.toUpperCase(Locale.US);
            return jumpToElementType(virtualViewId, elementType, /*forwards*/ false,
                    /*canWrap*/ virtualViewId == mCurrentRootId);
        } else if (action == ACTION_SET_TEXT.getId()) {
            if (!WebContentsAccessibilityImplJni.get().isEditableText(mNativeObj, virtualViewId)) {
                return false;
            }
            if (arguments == null) return false;
            CharSequence bundleText =
                    arguments.getCharSequence(ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE);
            if (bundleText == null) return false;
            String newText = bundleText.toString();
            WebContentsAccessibilityImplJni.get().setTextFieldValue(
                    mNativeObj, virtualViewId, newText);
            // Match Android framework and set the cursor to the end of the text field.
            WebContentsAccessibilityImplJni.get().setSelection(
                    mNativeObj, virtualViewId, newText.length(), newText.length());
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
            WebContentsAccessibilityImplJni.get().setSelection(
                    mNativeObj, virtualViewId, selectionStart, selectionEnd);
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
                return jumpToElementType(virtualViewId, PARAGRAPH_ELEMENT_TYPE, /*forwards*/ true,
                        /*canWrap*/ false);
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
                return jumpToElementType(virtualViewId, PARAGRAPH_ELEMENT_TYPE, /*forwards*/ false,
                        /*canWrap*/ virtualViewId == mCurrentRootId);
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
            return WebContentsAccessibilityImplJni.get().scroll(mNativeObj, virtualViewId,
                    ScrollDirection.UP, action == ACTION_PAGE_UP.getId());
        } else if (action == ACTION_SCROLL_DOWN.getId() || action == ACTION_PAGE_DOWN.getId()) {
            return WebContentsAccessibilityImplJni.get().scroll(mNativeObj, virtualViewId,
                    ScrollDirection.DOWN, action == ACTION_PAGE_DOWN.getId());
        } else if (action == ACTION_SCROLL_LEFT.getId() || action == ACTION_PAGE_LEFT.getId()) {
            return WebContentsAccessibilityImplJni.get().scroll(mNativeObj, virtualViewId,
                    ScrollDirection.LEFT, action == ACTION_PAGE_LEFT.getId());
        } else if (action == ACTION_SCROLL_RIGHT.getId() || action == ACTION_PAGE_RIGHT.getId()) {
            return WebContentsAccessibilityImplJni.get().scroll(mNativeObj, virtualViewId,
                    ScrollDirection.RIGHT, action == ACTION_PAGE_RIGHT.getId());
        } else if (action == ACTION_SET_PROGRESS.getId()) {
            if (arguments == null) return false;
            if (!arguments.containsKey(ACTION_ARGUMENT_PROGRESS_VALUE)) return false;
            return WebContentsAccessibilityImplJni.get().setRangeValue(
                    mNativeObj, virtualViewId, arguments.getFloat(ACTION_ARGUMENT_PROGRESS_VALUE));
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
            int id = WebContentsAccessibilityImplJni.get()
                             .getIdForElementAfterElementHostingAutofillPopup(mNativeObj);
            if (id == 0) return;

            moveAccessibilityFocusToId(id);
            scrollToMakeNodeVisible(mAccessibilityFocusId);
        }
    }

    // TODO(mschillaci,jacklynch): Move into {refreshNativeState} once {BrowserAccessibilityState.
    //                             Listener} provides more granularity.
    public void updateAXModeFromNativeAccessibilityState() {
        if (!isNativeInitialized()) return;
        // Update the AXMode based on screen reader status.
        WebContentsAccessibilityImplJni.get().setAXMode(
                mNativeObj, AccessibilityState.screenReaderMode(), isAccessibilityEnabled());
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
        mUserHasTouchExplored = true;
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
        WebContentsAccessibilityImplJni.get().moveAccessibilityFocus(
                mNativeObj, mAccessibilityFocusId, View.NO_ID);
        mAccessibilityFocusId = View.NO_ID;
        mAccessibilityFocusRect = null;

        sendAccessibilityEvent(mLastHoverId, AccessibilityEvent.TYPE_VIEW_HOVER_EXIT);
        mLastHoverId = View.NO_ID;
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
            moveAccessibilityFocusToIdAndRefocusIfNeeded(mAccessibilityFocusId);
        }
    }

    private boolean jumpToElementType(
            int virtualViewId, String elementType, boolean forwards, boolean canWrap) {
        int id = WebContentsAccessibilityImplJni.get().findElementType(
                mNativeObj, virtualViewId, elementType, forwards, canWrap, elementType.isEmpty());
        if (id == 0) return false;

        moveAccessibilityFocusToId(id);
        scrollToMakeNodeVisible(mAccessibilityFocusId);
        return true;
    }

    private void setGranularityAndUpdateSelection(int granularity) {
        mSelectionGranularity = granularity;

        if (WebContentsAccessibilityImplJni.get().isEditableText(mNativeObj, mAccessibilityFocusId)
                && WebContentsAccessibilityImplJni.get().isFocused(
                        mNativeObj, mAccessibilityFocusId)) {
            // If selection/cursor are "unassigned" (e.g. first user swipe), then assign as needed
            if (mSelectionStart == -1) {
                mSelectionStart =
                        WebContentsAccessibilityImplJni.get().getEditableTextSelectionStart(
                                mNativeObj, mAccessibilityFocusId);
            }
            if (mCursorIndex == -1) {
                mCursorIndex = WebContentsAccessibilityImplJni.get().getEditableTextSelectionEnd(
                        mNativeObj, mAccessibilityFocusId);
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
            return WebContentsAccessibilityImplJni.get().nextAtGranularity(mNativeObj,
                    mSelectionGranularity, extendSelection, virtualViewId, mCursorIndex);
        } else {
            return WebContentsAccessibilityImplJni.get().nextAtGranularity(mNativeObj,
                    mSelectionGranularity, extendSelection, virtualViewId, mSelectionStart);
        }
    }

    private boolean previousAtGranularity(
            int granularity, boolean extendSelection, int virtualViewId) {
        if (virtualViewId != mSelectionNodeId) return false;
        setGranularityAndUpdateSelection(granularity);

        // This calls finishGranularityMovePrevious when it's done.
        return WebContentsAccessibilityImplJni.get().previousAtGranularity(
                mNativeObj, mSelectionGranularity, extendSelection, virtualViewId, mCursorIndex);
    }

    @CalledByNative
    private void finishGranularityMoveNext(
            String text, boolean extendSelection, int itemStartIndex, int itemEndIndex) {
        // Prepare to send both a selection and a traversal event in sequence.
        AccessibilityEvent selectionEvent = buildAccessibilityEvent(
                mSelectionNodeId, AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED);
        if (selectionEvent == null) return;

        AccessibilityEvent traverseEvent = buildAccessibilityEvent(mSelectionNodeId,
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
        AccessibilityEvent selectionEvent = buildAccessibilityEvent(
                mSelectionNodeId, AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED);
        if (selectionEvent == null) return;

        AccessibilityEvent traverseEvent = buildAccessibilityEvent(mSelectionNodeId,
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
        if (mDelegate.scrollToMakeNodeVisible(getAbsolutePositionForNode(virtualViewId))) return;

        mPendingScrollToMakeNodeVisible = true;
        WebContentsAccessibilityImplJni.get().scrollToMakeNodeVisible(mNativeObj, virtualViewId);
    }

    private void performClick(int virtualViewId) {
        if (mDelegate.performClick(getAbsolutePositionForNode(virtualViewId))) return;

        WebContentsAccessibilityImplJni.get().click(mNativeObj, virtualViewId);
    }

    private void setSelection(AccessibilityEvent selectionEvent) {
        if (WebContentsAccessibilityImplJni.get().isEditableText(mNativeObj, mSelectionNodeId)
                && WebContentsAccessibilityImplJni.get().isFocused(mNativeObj, mSelectionNodeId)) {
            WebContentsAccessibilityImplJni.get().setSelection(mNativeObj, mSelectionNodeId,
                    selectionEvent.getFromIndex(), selectionEvent.getToIndex());
        }
    }

    private boolean scrollForward(int virtualViewId) {
        if (WebContentsAccessibilityImplJni.get().isSlider(mNativeObj, virtualViewId)) {
            return WebContentsAccessibilityImplJni.get().adjustSlider(
                    mNativeObj, virtualViewId, true);
        } else {
            return WebContentsAccessibilityImplJni.get().scroll(
                    mNativeObj, virtualViewId, ScrollDirection.FORWARD, false);
        }
    }

    private boolean scrollBackward(int virtualViewId) {
        if (WebContentsAccessibilityImplJni.get().isSlider(mNativeObj, virtualViewId)) {
            return WebContentsAccessibilityImplJni.get().adjustSlider(
                    mNativeObj, virtualViewId, false);
        } else {
            return WebContentsAccessibilityImplJni.get().scroll(
                    mNativeObj, virtualViewId, ScrollDirection.BACKWARD, false);
        }
    }

    private boolean moveAccessibilityFocusToId(int newAccessibilityFocusId) {
        if (newAccessibilityFocusId == mAccessibilityFocusId) return false;

        WebContentsAccessibilityImplJni.get().moveAccessibilityFocus(
                mNativeObj, mAccessibilityFocusId, newAccessibilityFocusId);

        mAccessibilityFocusId = newAccessibilityFocusId;
        mAccessibilityFocusRect = null;
        // Used to store the node (edit text field) that has input focus but not a11y focus.
        // Usually while the user is typing in an edit text field, a11y is on the IME and input
        // focus is on the edit field. Granularity move needs to know where the input focus is.
        mSelectionNodeId = mAccessibilityFocusId;
        mSelectionGranularity = NO_GRANULARITY_SELECTED;
        mIsCurrentlyExtendingSelection = false;
        mSelectionStart = -1;
        mCursorIndex = WebContentsAccessibilityImplJni.get().getTextLength(
                mNativeObj, mAccessibilityFocusId);
        mSuppressNextSelectionEvent = false;

        if (WebContentsAccessibilityImplJni.get().isAutofillPopupNode(
                    mNativeObj, mAccessibilityFocusId)) {
            mAutofillPopupView.requestFocus();
        }

        sendAccessibilityEvent(
                mAccessibilityFocusId, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED);
        return true;
    }

    private void moveAccessibilityFocusToIdAndRefocusIfNeeded(int newAccessibilityFocusId) {
        // Work around a bug in the Android framework where it doesn't fully update the object
        // with accessibility focus even if you send it a WINDOW_CONTENT_CHANGED. To work around
        // this, clear focus and then set focus again.
        if (newAccessibilityFocusId == mAccessibilityFocusId) {
            sendAccessibilityEvent(newAccessibilityFocusId,
                    AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED);
            mAccessibilityFocusId = View.NO_ID;
        }
        moveAccessibilityFocusToId(newAccessibilityFocusId);
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
        if (!isAccessibilityEnabled() || !isFrameInfoInitialized()
                || !WebContentsAccessibilityImplJni.get().isNodeValid(mNativeObj, virtualViewId)) {
            return null;
        }

        final AccessibilityEvent event = AccessibilityEvent.obtain(eventType);
        event.setPackageName(mContext.getPackageName());
        event.setSource(mView, virtualViewId);
        if (eventType == AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED) {
            event.setContentChangeTypes(AccessibilityEvent.CONTENT_CHANGE_TYPE_SUBTREE);
        }
        if (!WebContentsAccessibilityImplJni.get().populateAccessibilityEvent(
                    mNativeObj, event, virtualViewId, eventType)) {
            event.recycle();
            return null;
        }
        return event;
    }

    @Override
    public boolean isAccessibilityEnabled() {
        return isNativeInitialized()
                && (mAccessibilityEnabledOverride || mAccessibilityManager.isEnabled());
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
        mAccessibilityFocusRect = null;
        mUserHasTouchExplored = false;
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
            moveAccessibilityFocusToIdAndRefocusIfNeeded(id);
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

    @CalledByNative
    private void addAccessibilityNodeInfoChildren(
            AccessibilityNodeInfoCompat node, int[] childIds) {
        for (int childId : childIds) {
            node.addChild(mView, childId);
        }
    }

    @CalledByNative
    private void setAccessibilityNodeInfoBooleanAttributes(AccessibilityNodeInfoCompat node,
            int virtualViewId, boolean checkable, boolean checked, boolean clickable,
            boolean contentInvalid, boolean enabled, boolean focusable, boolean focused,
            boolean hasImage, boolean password, boolean scrollable, boolean selected,
            boolean visibleToUser) {
        node.setCheckable(checkable);
        node.setChecked(checked);
        node.setClickable(clickable);
        node.setEnabled(enabled);
        node.setFocusable(focusable);
        node.setFocused(focused);
        node.setPassword(password);
        node.setScrollable(scrollable);
        node.setSelected(selected);
        node.setVisibleToUser(visibleToUser);

        // In the special case that we have invalid content on a focused field, we only want to
        // report that to the user at most once per {@link CONTENT_INVALID_THROTTLE_DELAY} time
        // interval, to be less jarring to the user.
        if (contentInvalid && focused) {
            if (virtualViewId == mLastContentInvalidViewId) {
                // If we are focused on the same node as before, check if it has been longer than
                // our delay since our last utterance, and if so, report invalid content and update
                // our last reported time, otherwise suppress reporting content invalid.
                if (Calendar.getInstance().getTimeInMillis() - mLastContentInvalidUtteranceTime
                        >= CONTENT_INVALID_THROTTLE_DELAY) {
                    mLastContentInvalidUtteranceTime = Calendar.getInstance().getTimeInMillis();
                    node.setContentInvalid(true);
                }
            } else {
                // When we are focused on a new node, report as normal and track new time.
                mLastContentInvalidViewId = virtualViewId;
                mLastContentInvalidUtteranceTime = Calendar.getInstance().getTimeInMillis();
                node.setContentInvalid(true);
            }
        } else {
            // For non-focused fields we want to set contentInvalid as normal.
            node.setContentInvalid(contentInvalid);
        }

        if (hasImage) {
            Bundle bundle = node.getExtras();
            bundle.putCharSequence(EXTRAS_KEY_HAS_IMAGE, "true");
        }

        node.setMovementGranularities(MOVEMENT_GRANULARITY_CHARACTER | MOVEMENT_GRANULARITY_WORD
                | MOVEMENT_GRANULARITY_LINE | MOVEMENT_GRANULARITY_PARAGRAPH);

        node.setAccessibilityFocused(mAccessibilityFocusId == virtualViewId);
    }

    @CalledByNative
    private void addAccessibilityNodeInfoActions(AccessibilityNodeInfoCompat node,
            int virtualViewId, boolean canScrollForward, boolean canScrollBackward,
            boolean canScrollUp, boolean canScrollDown, boolean canScrollLeft,
            boolean canScrollRight, boolean clickable, boolean editableText, boolean enabled,
            boolean focusable, boolean focused, boolean isCollapsed, boolean isExpanded,
            boolean hasNonEmptyValue, boolean hasNonEmptyInnerText, boolean isSeekControl,
            boolean isForm) {
        node.addAction(ACTION_NEXT_HTML_ELEMENT);
        node.addAction(ACTION_PREVIOUS_HTML_ELEMENT);
        node.addAction(ACTION_SHOW_ON_SCREEN);
        node.addAction(ACTION_CONTEXT_CLICK);
        // We choose to not add ACTION_LONG_CLICK to nodes to prevent verbose utterances.

        if (hasNonEmptyInnerText) {
            node.addAction(ACTION_NEXT_AT_MOVEMENT_GRANULARITY);
            node.addAction(ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY);
        }

        if (editableText && enabled) {
            // TODO: don't support actions that modify it if it's read-only (but
            // SET_SELECTION and COPY are okay).
            node.addAction(ACTION_SET_TEXT);
            node.addAction(ACTION_PASTE);
            node.addAction(ACTION_IME_ENTER);

            if (hasNonEmptyValue) {
                node.addAction(ACTION_SET_SELECTION);
                node.addAction(ACTION_CUT);
                node.addAction(ACTION_COPY);
            }
        }

        if (canScrollForward) {
            node.addAction(ACTION_SCROLL_FORWARD);
        }

        if (canScrollBackward) {
            node.addAction(ACTION_SCROLL_BACKWARD);
        }

        if (canScrollUp) {
            node.addAction(ACTION_SCROLL_UP);
            node.addAction(ACTION_PAGE_UP);
        }

        if (canScrollDown) {
            node.addAction(ACTION_SCROLL_DOWN);
            node.addAction(ACTION_PAGE_DOWN);
        }

        if (canScrollLeft) {
            node.addAction(ACTION_SCROLL_LEFT);
            node.addAction(ACTION_PAGE_LEFT);
        }

        if (canScrollRight) {
            node.addAction(ACTION_SCROLL_RIGHT);
            node.addAction(ACTION_PAGE_RIGHT);
        }

        if (focusable) {
            if (focused) {
                node.addAction(ACTION_CLEAR_FOCUS);
            } else {
                node.addAction(ACTION_FOCUS);
            }
        }

        if (mAccessibilityFocusId == virtualViewId) {
            node.addAction(ACTION_CLEAR_ACCESSIBILITY_FOCUS);
        } else {
            node.addAction(ACTION_ACCESSIBILITY_FOCUS);
        }

        if (clickable) {
            node.addAction(ACTION_CLICK);
        }

        if (isCollapsed) {
            node.addAction(ACTION_EXPAND);
        }

        if (isExpanded) {
            node.addAction(ACTION_COLLAPSE);
        }

        if (isSeekControl) {
            node.addAction(ACTION_SET_PROGRESS);
        }
    }

    @CalledByNative
    private void setAccessibilityNodeInfoBaseAttributes(AccessibilityNodeInfoCompat node,
            int virtualViewId, int parentId, String className, String role, String roleDescription,
            String hint, String targetUrl, boolean canOpenPopup, boolean multiLine, int inputType,
            int liveRegion, String errorMessage, int clickableScore, String display,
            String brailleLabel, String brailleRoleDescription) {
        node.setClassName(className);

        Bundle bundle = node.getExtras();
        if (!brailleLabel.isEmpty()) {
            bundle.putCharSequence(EXTRAS_KEY_BRAILLE_LABEL, brailleLabel);
        }
        if (!brailleRoleDescription.isEmpty()) {
            bundle.putCharSequence(EXTRAS_KEY_BRAILLE_ROLE_DESCRIPTION, brailleRoleDescription);
        }
        bundle.putCharSequence(EXTRAS_KEY_CHROME_ROLE, role);
        bundle.putCharSequence(EXTRAS_KEY_ROLE_DESCRIPTION, roleDescription);
        bundle.putCharSequence(EXTRAS_KEY_HINT, hint);
        if (!display.isEmpty()) {
            bundle.putCharSequence(EXTRAS_KEY_CSS_DISPLAY, display);
        }
        if (!targetUrl.isEmpty()) {
            bundle.putCharSequence(EXTRAS_KEY_TARGET_URL, targetUrl);
        }
        if (virtualViewId == mCurrentRootId) {
            bundle.putCharSequence(EXTRAS_KEY_SUPPORTED_ELEMENTS, mSupportedHtmlElementTypes);
        }

        if (parentId != View.NO_ID) {
            node.setParent(mView, parentId);
        }

        node.setCanOpenPopup(canOpenPopup);
        node.setDismissable(false); // No concept of "dismissable" on the web currently.
        node.setMultiLine(multiLine);
        node.setInputType(inputType);

        // Deliberately don't call setLiveRegion because TalkBack speaks
        // the entire region anytime it changes. Instead Chrome will
        // call announceLiveRegionText() only on the nodes that change.
        // node.setLiveRegion(liveRegion);

        // We only apply the |errorMessage| if {@link setAccessibilityNodeInfoBooleanAttributes}
        // set |contentInvalid| to true based on throttle delay.
        if (node.isContentInvalid()) {
            node.setError(errorMessage);
        }

        // For non-zero clickable scores, add to the Bundle extras.
        if (clickableScore > 0) {
            bundle.putInt(EXTRAS_KEY_CLICKABLE_SCORE, clickableScore);
        }
    }

    @SuppressLint("NewApi")
    @CalledByNative
    protected void setAccessibilityNodeInfoText(AccessibilityNodeInfoCompat node, String text,
            boolean annotateAsLink, boolean isEditableText, String language, int[] suggestionStarts,
            int[] suggestionEnds, String[] suggestions, String stateDescription) {
        CharSequence computedText = computeText(
                text, annotateAsLink, language, suggestionStarts, suggestionEnds, suggestions);

        // We add the stateDescription attribute when it is non-null and not empty.
        if (stateDescription != null && !stateDescription.isEmpty()) {
            node.setStateDescription(stateDescription);
        }

        // We expose the nested structure of links, which results in the roles of all nested nodes
        // being read. Use content description in the case of links to prevent verbose TalkBack
        if (annotateAsLink) {
            node.setContentDescription(computedText);
        } else {
            node.setText(computedText);
        }
    }

    protected boolean areInlineTextBoxesLoaded(int virtualViewId) {
        return WebContentsAccessibilityImplJni.get().areInlineTextBoxesLoaded(
                mNativeObj, virtualViewId);
    }

    protected void loadInlineTextBoxes(int virtualViewId) {
        WebContentsAccessibilityImplJni.get().loadInlineTextBoxes(mNativeObj, virtualViewId);
    }

    protected int[] getCharacterBoundingBoxes(
            int virtualViewId, int positionInfoStartIndex, int positionInfoLength) {
        return WebContentsAccessibilityImplJni.get().getCharacterBoundingBoxes(
                mNativeObj, virtualViewId, positionInfoStartIndex, positionInfoLength);
    }

    protected CharSequence computeText(String text, boolean annotateAsLink, String language,
            int[] suggestionStarts, int[] suggestionEnds, String[] suggestions) {
        CharSequence charSequence = text;
        if (annotateAsLink) {
            SpannableString spannable = new SpannableString(text);
            spannable.setSpan(new URLSpan(""), 0, spannable.length(), 0);
            charSequence = spannable;
        }
        if (!language.isEmpty() && !language.equals(mSystemLanguageTag)) {
            SpannableString spannable;
            if (charSequence instanceof SpannableString) {
                spannable = (SpannableString) charSequence;
            } else {
                spannable = new SpannableString(charSequence);
            }
            Locale locale = Locale.forLanguageTag(language);
            spannable.setSpan(new LocaleSpan(locale), 0, spannable.length(), 0);
            charSequence = spannable;
        }

        if (suggestionStarts != null && suggestionStarts.length > 0) {
            assert suggestionEnds != null;
            assert suggestionEnds.length == suggestionStarts.length;
            assert suggestions != null;
            assert suggestions.length == suggestionStarts.length;

            SpannableString spannable;
            if (charSequence instanceof SpannableString) {
                spannable = (SpannableString) charSequence;
            } else {
                spannable = new SpannableString(charSequence);
            }

            int spannableLen = spannable.length();
            for (int i = 0; i < suggestionStarts.length; i++) {
                int start = suggestionStarts[i];
                int end = suggestionEnds[i];
                // Ignore any spans outside the range of the spannable string.
                if (start < 0 || start > spannableLen || end < 0 || end > spannableLen
                        || start > end) {
                    continue;
                }

                String[] suggestionArray = new String[1];
                suggestionArray[0] = suggestions[i];
                int flags = SuggestionSpan.FLAG_MISSPELLED;
                SuggestionSpan suggestionSpan =
                        new SuggestionSpan(mContext, suggestionArray, flags);
                spannable.setSpan(suggestionSpan, start, end, 0);
            }
            charSequence = spannable;
        }

        return charSequence;
    }

    protected void convertWebRectToAndroidCoordinates(Rect rect, Bundle extras) {
        // Offset by the scroll position.
        AccessibilityCoordinates ac = mDelegate.getAccessibilityCoordinates();
        rect.offset(-(int) ac.getScrollX(), -(int) ac.getScrollY());

        // Convert CSS (web) pixels to Android View pixels
        rect.left = (int) ac.fromLocalCssToPix(rect.left);
        rect.top = (int) ac.fromLocalCssToPix(rect.top);
        rect.bottom = (int) ac.fromLocalCssToPix(rect.bottom);
        rect.right = (int) ac.fromLocalCssToPix(rect.right);

        // Offset by the location of the web content within the view.
        rect.offset(0, (int) ac.getContentOffsetYPix());

        // Finally offset by the location of the view within the screen.
        final int[] viewLocation = new int[2];
        mView.getLocationOnScreen(viewLocation);
        rect.offset(viewLocation[0], viewLocation[1]);

        // Clip to the viewport bounds, and add unclipped values to the Bundle.
        int viewportRectTop = viewLocation[1] + (int) ac.getContentOffsetYPix();
        int viewportRectBottom = viewportRectTop + ac.getLastFrameViewportHeightPixInt();
        if (rect.top < viewportRectTop) {
            extras.putInt(EXTRAS_KEY_UNCLIPPED_TOP, rect.top);
            rect.top = viewportRectTop;
        }
        if (rect.bottom > viewportRectBottom) {
            extras.putInt(EXTRAS_KEY_UNCLIPPED_BOTTOM, rect.bottom);
            rect.bottom = viewportRectBottom;
        }
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
        int[] coords = WebContentsAccessibilityImplJni.get().getAbsolutePositionForNode(
                mNativeObj, virtualViewId);
        if (coords == null) return null;

        return new Rect(coords[0], coords[1], coords[2], coords[3]);
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoLocation(AccessibilityNodeInfoCompat node,
            final int virtualViewId, int absoluteLeft, int absoluteTop, int parentRelativeLeft,
            int parentRelativeTop, int width, int height, boolean isOffscreen) {
        // First set the bounds in parent.
        Rect boundsInParent = new Rect(parentRelativeLeft, parentRelativeTop,
                parentRelativeLeft + width, parentRelativeTop + height);
        if (virtualViewId == mCurrentRootId) {
            // Offset of the web content relative to the View.
            AccessibilityCoordinates ac = mDelegate.getAccessibilityCoordinates();
            boundsInParent.offset(0, (int) ac.getContentOffsetYPix());
        }
        node.setBoundsInParent(boundsInParent);

        Rect rect = new Rect(absoluteLeft, absoluteTop, absoluteLeft + width, absoluteTop + height);
        convertWebRectToAndroidCoordinates(rect, node.getExtras());

        node.setBoundsInScreen(rect);

        // For nodes that are considered visible to the user, but are offscreen (because they are
        // scrolled offscreen or obscured from view but not programmatically hidden, e.g. through
        // CSS), add to the extras Bundle to inform interested accessibility services.
        if (isOffscreen) {
            node.getExtras().putBoolean(EXTRAS_KEY_OFFSCREEN, true);
        } else {
            // In case of a cached node, remove the offscreen extra if it is there.
            if (node.getExtras().containsKey(EXTRAS_KEY_OFFSCREEN)) {
                node.getExtras().remove(EXTRAS_KEY_OFFSCREEN);
            }
        }
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoCollectionInfo(
            AccessibilityNodeInfoCompat node, int rowCount, int columnCount, boolean hierarchical) {
        node.setCollectionInfo(AccessibilityNodeInfoCompat.CollectionInfoCompat.obtain(
                rowCount, columnCount, hierarchical));
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoCollectionItemInfo(AccessibilityNodeInfoCompat node,
            int rowIndex, int rowSpan, int columnIndex, int columnSpan, boolean heading) {
        node.setCollectionItemInfo(AccessibilityNodeInfoCompat.CollectionItemInfoCompat.obtain(
                rowIndex, rowSpan, columnIndex, columnSpan, heading));
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoRangeInfo(
            AccessibilityNodeInfoCompat node, int rangeType, float min, float max, float current) {
        node.setRangeInfo(
                AccessibilityNodeInfoCompat.RangeInfoCompat.obtain(rangeType, min, max, current));
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoViewIdResourceName(
            AccessibilityNodeInfoCompat node, String viewIdResourceName) {
        node.setViewIdResourceName(viewIdResourceName);
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoOAttributes(AccessibilityNodeInfoCompat node,
            boolean hasCharacterLocations, boolean hasImage, String hint) {
        node.setHintText(hint);

        // Work-around a gap in the Android API, that |AccessibilityNodeInfoCompat| class does not
        // have the setAvailableExtraData method, so unwrap the node and call it directly.
        // TODO(mschillaci): Remove unwrapping and SDK version req once Android API is updated.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            if (hasCharacterLocations) {
                ((AccessibilityNodeInfo) node.getInfo())
                        .setAvailableExtraData(sTextCharacterLocation);
            } else if (hasImage) {
                ((AccessibilityNodeInfo) node.getInfo()).setAvailableExtraData(sRequestImageData);
            }
        }
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoPaneTitle(
            AccessibilityNodeInfoCompat node, String title) {
        node.setPaneTitle(title);
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoSelectionAttrs(
            AccessibilityNodeInfoCompat node, int startIndex, int endIndex) {
        node.setEditable(true);
        node.setTextSelection(startIndex, endIndex);
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoImageData(
            AccessibilityNodeInfoCompat info, byte[] imageData) {
        info.getExtras().putByteArray(EXTRAS_KEY_IMAGE_DATA, imageData);
    }

    @CalledByNative
    private void setAccessibilityEventBaseAttributes(AccessibilityEvent event, boolean checked,
            boolean enabled, boolean password, boolean scrollable, int currentItemIndex,
            int itemCount, int scrollX, int scrollY, int maxScrollX, int maxScrollY,
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
    private void setAccessibilityEventTextChangedAttrs(AccessibilityEvent event, int fromIndex,
            int addedCount, int removedCount, String beforeText, String text) {
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
    public void addExtraDataToAccessibilityNodeInfo(int virtualViewId,
            AccessibilityNodeInfoCompat info, String extraDataKey, Bundle arguments) {
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

        int[] coords = getCharacterBoundingBoxes(
                virtualViewId, positionInfoStartIndex, positionInfoLength);
        if (coords == null) return;
        assert coords.length == positionInfoLength * 4;

        RectF[] boundingRects = new RectF[positionInfoLength];
        for (int i = 0; i < positionInfoLength; i++) {
            Rect rect = new Rect(
                    coords[4 * i + 0], coords[4 * i + 1], coords[4 * i + 2], coords[4 * i + 3]);
            convertWebRectToAndroidCoordinates(rect, info.getExtras());
            boundingRects[i] = new RectF(rect);
        }

        info.getExtras().putParcelableArray(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY, boundingRects);
    }

    private void getImageData(int virtualViewId, AccessibilityNodeInfoCompat info) {
        boolean hasSentPreviousRequest = mImageDataRequestedNodes.contains(virtualViewId);
        // If the below call returns true, then image data has been set on the node.
        if (!WebContentsAccessibilityImplJni.get().getImageData(
                    mNativeObj, info, virtualViewId, hasSentPreviousRequest)) {
            // If the above call returns false, then the data was missing. The native-side code
            // will have started the asynchronous process to populate the image data if no previous
            // request has been sent. Add this |virtualViewId| to the list of requested nodes.
            mImageDataRequestedNodes.add(virtualViewId);
        }
    }

    @NativeMethods
    interface Natives {
        long init(WebContentsAccessibilityImpl caller, WebContents webContents);
        long initWithAXTree(WebContentsAccessibilityImpl caller, long axTreePtr);
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
        boolean updateCachedAccessibilityNodeInfo(long nativeWebContentsAccessibilityAndroid,
                AccessibilityNodeInfoCompat info, int id);
        boolean populateAccessibilityNodeInfo(long nativeWebContentsAccessibilityAndroid,
                AccessibilityNodeInfoCompat info, int id);
        boolean populateAccessibilityEvent(long nativeWebContentsAccessibilityAndroid,
                AccessibilityEvent event, int id, int eventType);
        void click(long nativeWebContentsAccessibilityAndroid, int id);
        void focus(long nativeWebContentsAccessibilityAndroid, int id);
        void blur(long nativeWebContentsAccessibilityAndroid);
        void scrollToMakeNodeVisible(long nativeWebContentsAccessibilityAndroid, int id);
        int findElementType(long nativeWebContentsAccessibilityAndroid, int startId,
                String elementType, boolean forwards, boolean canWrapToLastElement,
                boolean useDefaultPredicate);
        void setTextFieldValue(long nativeWebContentsAccessibilityAndroid, int id, String newValue);
        void setSelection(long nativeWebContentsAccessibilityAndroid, int id, int start, int end);
        boolean nextAtGranularity(long nativeWebContentsAccessibilityAndroid,
                int selectionGranularity, boolean extendSelection, int id, int cursorIndex);
        boolean previousAtGranularity(long nativeWebContentsAccessibilityAndroid,
                int selectionGranularity, boolean extendSelection, int id, int cursorIndex);
        boolean adjustSlider(long nativeWebContentsAccessibilityAndroid, int id, boolean increment);
        void moveAccessibilityFocus(
                long nativeWebContentsAccessibilityAndroid, int oldId, int newId);
        boolean isSlider(long nativeWebContentsAccessibilityAndroid, int id);
        boolean scroll(long nativeWebContentsAccessibilityAndroid, int id, int direction,
                boolean pageScroll);
        boolean setRangeValue(long nativeWebContentsAccessibilityAndroid, int id, float value);
        String getSupportedHtmlElementTypes(long nativeWebContentsAccessibilityAndroid);
        void showContextMenu(long nativeWebContentsAccessibilityAndroid, int id);
        boolean isEnabled(long nativeWebContentsAccessibilityAndroid);
        void enable(long nativeWebContentsAccessibilityAndroid, boolean screenReaderMode);
        void setAXMode(long nativeWebContentsAccessibilityAndroid, boolean screenReaderMode,
                boolean isAccessibilityEnabled);
        void setPasswordRules(long nativeWebContentsAccessibilityAndroid,
                boolean shouldRespectDisplayedPasswordText, boolean shouldExposePasswordText);
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
        boolean getImageData(long nativeWebContentsAccessibilityAndroid,
                AccessibilityNodeInfoCompat info, int id, boolean hasSentPreviousRequest);
    }
}
