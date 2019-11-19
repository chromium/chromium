// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.assist.AssistStructure.ViewNode;
import android.content.ContentResolver;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.text.SpannableString;
import android.text.style.URLSpan;
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

import androidx.annotation.VisibleForTesting;

import org.chromium.base.UserData;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.WindowEventObserver;
import org.chromium.content.browser.WindowEventObserverManager;
import org.chromium.content.browser.accessibility.captioning.CaptioningController;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl.UserDataFactory;
import org.chromium.content_public.browser.AccessibilitySnapshotCallback;
import org.chromium.content_public.browser.AccessibilitySnapshotNode;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Implementation of {@link WebContentsAccessibility} interface.
 * Native accessibility for a {@link WebContents}. Actual native instance is
 * created lazily upon the first request from Android framework on
 *{@link AccessibilityNodeProvider}, and shares the lifetime with {@link WebContents}.
 */
@JNINamespace("content")
public class WebContentsAccessibilityImpl extends AccessibilityNodeProvider
        implements AccessibilityStateChangeListener, WebContentsAccessibility, WindowEventObserver,
                   UserData {
    // Constants from AccessibilityNodeInfo defined in the K SDK.
    private static final int ACTION_COLLAPSE = 0x00080000;
    private static final int ACTION_EXPAND = 0x00040000;

    // Constants from AccessibilityNodeInfo defined in the L SDK.
    private static final int ACTION_SET_TEXT = 0x200000;
    private static final String ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE =
            "ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE";
    private static final int WINDOW_CONTENT_CHANGED_DELAY_MS = 500;
    private static final int SCROLLED_TO_ANCHOR_DELAY_MS = 500;

    // Constants from AccessibilityNodeInfo defined in the M SDK.
    // Source: https://developer.android.com/reference/android/R.id.html
    protected static final int ACTION_CONTEXT_CLICK = 0x0102003c;
    protected static final int ACTION_SHOW_ON_SCREEN = 0x01020036;
    protected static final int ACTION_SCROLL_UP = 0x01020038;
    protected static final int ACTION_SCROLL_DOWN = 0x0102003a;
    protected static final int ACTION_SCROLL_LEFT = 0x01020039;
    protected static final int ACTION_SCROLL_RIGHT = 0x0102003b;

    // Constant for no granularity selected.
    private static final int NO_GRANULARITY_SELECTED = 0;

    private final WebContentsImpl mWebContents;
    protected AccessibilityManager mAccessibilityManager;
    protected final Context mContext;
    private String mProductVersion;
    protected long mNativeObj;
    private Rect mAccessibilityFocusRect;
    private boolean mIsHovering;
    private int mLastHoverId = View.NO_ID;
    protected int mCurrentRootId;
    private int[] mTempLocation = new int[2];
    protected ViewGroup mView;
    private boolean mUserHasTouchExplored;
    private boolean mPendingScrollToMakeNodeVisible;
    private boolean mNotifyFrameInfoInitializedCalled;
    private boolean mAccessibilityEnabledForTesting;
    private int mSelectionGranularity;
    private int mSelectionStartIndex;
    private int mSelectionEndIndex;
    protected int mAccessibilityFocusId;
    protected int mSelectionNodeId;
    private Runnable mSendWindowContentChangedRunnable;
    private Runnable mScrolledToAnchorRunnable;
    private View mAutofillPopupView;
    private CaptioningController mCaptioningController;

    // Whether native accessibility is allowed.
    private boolean mNativeAccessibilityAllowed;

    // Whether accessibility focus should be set to the page when it finishes loading.
    // This only applies if an accessibility service like TalkBack is running.
    // This is desirable behavior for a browser window, but not for an embedded
    // WebView.
    private boolean mShouldFocusOnPageLoad;

    // If true, the web contents are obscured by another view and we shouldn't
    // return an AccessibilityNodeProvider or process touch exploration events.
    private boolean mIsObscuredByAnotherView;

    // Accessibility touch exploration state.
    private boolean mTouchExplorationEnabled;

    /**
     * Create a WebContentsAccessibilityImpl object.
     */
    private static class Factory implements UserDataFactory<WebContentsAccessibilityImpl> {
        @Override
        public WebContentsAccessibilityImpl create(WebContents webContents) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                return new OWebContentsAccessibility(webContents);
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                return new LollipopWebContentsAccessibility(webContents);
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
                return new KitKatWebContentsAccessibility(webContents);
            } else {
                return new WebContentsAccessibilityImpl(webContents);
            }
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

    protected WebContentsAccessibilityImpl(WebContents webContents) {
        mWebContents = (WebContentsImpl) webContents;
        mView = mWebContents.getViewAndroidDelegate().getContainerView();
        mContext = mView.getContext();
        mProductVersion = mWebContents.getProductVersion();
        mAccessibilityManager =
                (AccessibilityManager) mContext.getSystemService(Context.ACCESSIBILITY_SERVICE);
        mCaptioningController = new CaptioningController(mWebContents);
        WindowEventObserverManager.from(mWebContents).addObserver(this);

        // Native is initialized lazily, when node provider is actually requested.
    }

    /**
     * Called after the native a11y part is initialized. Overridable by subclasses
     * to do initialization that is not required until the native is set up.
     */
    protected void onNativeInit() {
        mAccessibilityFocusId = View.NO_ID;
        mSelectionNodeId = View.NO_ID;
        mIsHovering = false;
        mCurrentRootId = View.NO_ID;
    }

    @CalledByNative
    protected void onNativeObjectDestroyed() {
        mNativeObj = 0;
    }

    protected boolean isNativeInitialized() {
        return mNativeObj != 0;
    }

    private boolean isEnabled() {
        return isNativeInitialized() ? WebContentsAccessibilityImplJni.get().isEnabled(
                       mNativeObj, WebContentsAccessibilityImpl.this)
                                     : false;
    }

    @VisibleForTesting
    @Override
    public void setAccessibilityEnabledForTesting() {
        mAccessibilityEnabledForTesting = true;
    }

    @VisibleForTesting
    @Override
    public void addSpellingErrorForTesting(int virtualViewId, int startOffset, int endOffset) {
        WebContentsAccessibilityImplJni.get().addSpellingErrorForTesting(mNativeObj,
                WebContentsAccessibilityImpl.this, virtualViewId, startOffset, endOffset);
    }

    // WindowEventObserver

    @Override
    public void onDetachedFromWindow() {
        mAccessibilityManager.removeAccessibilityStateChangeListener(this);
        mCaptioningController.stopListening();
    }

    @Override
    public void onAttachedToWindow() {
        mAccessibilityManager.addAccessibilityStateChangeListener(this);
        refreshState();
        mCaptioningController.startListening();
    }

    /**
     * Refresh a11y state with that of {@link AccessibilityManager}.
     */
    public void refreshState() {
        setState(mAccessibilityManager.isEnabled());
    }

    // AccessibilityNodeProvider

    @Override
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        if (mIsObscuredByAnotherView) return null;

        if (!isNativeInitialized()) {
            if (!mNativeAccessibilityAllowed) return null;
            mNativeObj = WebContentsAccessibilityImplJni.get().init(
                    WebContentsAccessibilityImpl.this, mWebContents);
            onNativeInit();
        }
        if (!isEnabled()) {
            WebContentsAccessibilityImplJni.get().enable(
                    mNativeObj, WebContentsAccessibilityImpl.this);
            return null;
        }
        return this;
    }

    @Override
    public AccessibilityNodeInfo createAccessibilityNodeInfo(int virtualViewId) {
        if (!isAccessibilityEnabled()) {
            return null;
        }
        int rootId = WebContentsAccessibilityImplJni.get().getRootId(
                mNativeObj, WebContentsAccessibilityImpl.this);

        if (virtualViewId == View.NO_ID) {
            return createNodeForHost(rootId);
        }

        if (!isFrameInfoInitialized()) {
            return null;
        }

        final AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain(mView);
        info.setPackageName(mContext.getPackageName());
        info.setSource(mView, virtualViewId);

        if (virtualViewId == rootId) {
            info.setParent(mView);
        }

        if (WebContentsAccessibilityImplJni.get().populateAccessibilityNodeInfo(
                    mNativeObj, WebContentsAccessibilityImpl.this, info, virtualViewId)) {
            return info;
        } else {
            info.recycle();
            return null;
        }
    }

    @Override
    public List<AccessibilityNodeInfo> findAccessibilityNodeInfosByText(
            String text, int virtualViewId) {
        return new ArrayList<AccessibilityNodeInfo>();
    }

    private static boolean isValidMovementGranularity(int granularity) {
        switch (granularity) {
            case AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER:
            case AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD:
            case AccessibilityNodeInfo.MOVEMENT_GRANULARITY_LINE:
                return true;
        }
        return false;
    }

    // AccessibilityStateChangeListener

    @Override
    public void onAccessibilityStateChanged(boolean enabled) {
        setState(enabled);
    }

    // WebContentsAccessibility

    @Override
    public void setObscuredByAnotherView(boolean isObscured) {
        if (isObscured != mIsObscuredByAnotherView) {
            mIsObscuredByAnotherView = isObscured;
            mView.sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
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
    public boolean supportsAction(int action) {
        // TODO(dmazzoni): implement this.
        return false;
    }

    @Override
    public boolean performAction(int action, Bundle arguments) {
        // TODO(dmazzoni): implement this.
        return false;
    }

    @TargetApi(Build.VERSION_CODES.M)
    @Override
    public void onProvideVirtualStructure(
            final ViewStructure structure, final boolean ignoreScrollOffset) {
        // Do not collect accessibility tree in incognito mode
        if (mWebContents.isIncognito()) {
            structure.setChildCount(0);
            return;
        }
        structure.setChildCount(1);
        final ViewStructure viewRoot = structure.asyncNewChild(0);
        mWebContents.requestAccessibilitySnapshot(new AccessibilitySnapshotCallback() {
            @Override
            public void onAccessibilitySnapshot(AccessibilitySnapshotNode root) {
                viewRoot.setClassName("");
                viewRoot.setHint(mProductVersion);
                if (root == null) {
                    viewRoot.asyncCommit();
                    return;
                }
                createVirtualStructure(viewRoot, root, ignoreScrollOffset);
            }
        });
    }

    // When creating the View structure, the left and top are relative to the parent node.
    @TargetApi(Build.VERSION_CODES.M)
    private void createVirtualStructure(ViewStructure viewNode, AccessibilitySnapshotNode node,
            final boolean ignoreScrollOffset) {
        viewNode.setClassName(node.className);
        if (node.hasSelection) {
            viewNode.setText(node.text, node.startSelection, node.endSelection);
        } else {
            viewNode.setText(node.text);
        }
        RenderCoordinatesImpl renderCoordinates = mWebContents.getRenderCoordinates();
        int left = (int) renderCoordinates.fromLocalCssToPix(node.x);
        int top = (int) renderCoordinates.fromLocalCssToPix(node.y);
        int width = (int) renderCoordinates.fromLocalCssToPix(node.width);
        int height = (int) renderCoordinates.fromLocalCssToPix(node.height);

        Rect boundsInParent = new Rect(left, top, left + width, top + height);
        if (node.isRootNode) {
            // Offset of the web content relative to the View.
            boundsInParent.offset(0, (int) renderCoordinates.getContentOffsetYPix());
            if (!ignoreScrollOffset) {
                boundsInParent.offset(-(int) renderCoordinates.getScrollXPix(),
                        -(int) renderCoordinates.getScrollYPix());
            }
        }

        viewNode.setDimens(boundsInParent.left, boundsInParent.top, 0, 0, width, height);
        viewNode.setChildCount(node.children.size());
        if (node.hasStyle) {
            // The text size should be in physical pixels, not CSS pixels.
            float textSize = renderCoordinates.fromLocalCssToPix(node.textSize);

            int style = (node.bold ? ViewNode.TEXT_STYLE_BOLD : 0)
                    | (node.italic ? ViewNode.TEXT_STYLE_ITALIC : 0)
                    | (node.underline ? ViewNode.TEXT_STYLE_UNDERLINE : 0)
                    | (node.lineThrough ? ViewNode.TEXT_STYLE_STRIKE_THRU : 0);
            viewNode.setTextStyle(textSize, node.color, node.bgcolor, style);
        }
        for (int i = 0; i < node.children.size(); i++) {
            createVirtualStructure(viewNode.asyncNewChild(i), node.children.get(i), true);
        }
        viewNode.asyncCommit();
    }

    @Override
    public boolean performAction(int virtualViewId, int action, Bundle arguments) {
        // We don't support any actions on the host view or nodes
        // that are not (any longer) in the tree.
        if (!isAccessibilityEnabled()
                || !WebContentsAccessibilityImplJni.get().isNodeValid(
                        mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId)) {
            return false;
        }

        switch (action) {
            case AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS:
                if (!moveAccessibilityFocusToId(virtualViewId)) return true;
                if (!mIsHovering) {
                    WebContentsAccessibilityImplJni.get().scrollToMakeNodeVisible(
                            mNativeObj, WebContentsAccessibilityImpl.this, mAccessibilityFocusId);
                } else {
                    mPendingScrollToMakeNodeVisible = true;
                }
                return true;
            case AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS:
                // ALWAYS respond with TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED whether we thought
                // it had focus or not, so that the Android framework cache is correct.
                sendAccessibilityEvent(
                        virtualViewId, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED);
                if (mAccessibilityFocusId == virtualViewId) {
                    WebContentsAccessibilityImplJni.get().moveAccessibilityFocus(mNativeObj,
                            WebContentsAccessibilityImpl.this, mAccessibilityFocusId, View.NO_ID);
                    mAccessibilityFocusId = View.NO_ID;
                    mAccessibilityFocusRect = null;
                }
                if (mLastHoverId == virtualViewId) {
                    sendAccessibilityEvent(mLastHoverId, AccessibilityEvent.TYPE_VIEW_HOVER_EXIT);
                    mLastHoverId = View.NO_ID;
                }
                return true;
            case AccessibilityNodeInfo.ACTION_CLICK:
                if (!mView.hasFocus()) mView.requestFocus();
                WebContentsAccessibilityImplJni.get().click(
                        mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId);
                return true;
            case AccessibilityNodeInfo.ACTION_FOCUS:
                if (!mView.hasFocus()) mView.requestFocus();
                WebContentsAccessibilityImplJni.get().focus(
                        mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId);
                return true;
            case AccessibilityNodeInfo.ACTION_CLEAR_FOCUS:
                WebContentsAccessibilityImplJni.get().blur(
                        mNativeObj, WebContentsAccessibilityImpl.this);
                return true;
            case AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT: {
                if (arguments == null) return false;
                String elementType = arguments.getString(
                        AccessibilityNodeInfo.ACTION_ARGUMENT_HTML_ELEMENT_STRING);
                if (elementType == null) return false;
                elementType = elementType.toUpperCase(Locale.US);
                return jumpToElementType(virtualViewId, elementType, true);
            }
            case AccessibilityNodeInfo.ACTION_PREVIOUS_HTML_ELEMENT: {
                if (arguments == null) return false;
                String elementType = arguments.getString(
                        AccessibilityNodeInfo.ACTION_ARGUMENT_HTML_ELEMENT_STRING);
                if (elementType == null) return false;
                elementType = elementType.toUpperCase(Locale.US);
                return jumpToElementType(virtualViewId, elementType, false);
            }
            case ACTION_SET_TEXT: {
                if (!WebContentsAccessibilityImplJni.get().isEditableText(
                            mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId)) {
                    return false;
                }
                if (arguments == null) return false;
                String newText = arguments.getString(ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE);
                if (newText == null) return false;
                WebContentsAccessibilityImplJni.get().setTextFieldValue(
                        mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId, newText);
                // Match Android framework and set the cursor to the end of the text field.
                WebContentsAccessibilityImplJni.get().setSelection(mNativeObj,
                        WebContentsAccessibilityImpl.this, virtualViewId, newText.length(),
                        newText.length());
                return true;
            }
            case AccessibilityNodeInfo.ACTION_SET_SELECTION: {
                if (!WebContentsAccessibilityImplJni.get().isEditableText(
                            mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId)) {
                    return false;
                }
                int selectionStart = 0;
                int selectionEnd = 0;
                if (arguments != null) {
                    selectionStart = arguments.getInt(
                            AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT);
                    selectionEnd = arguments.getInt(
                            AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT);
                }
                WebContentsAccessibilityImplJni.get().setSelection(mNativeObj,
                        WebContentsAccessibilityImpl.this, virtualViewId, selectionStart,
                        selectionEnd);
                return true;
            }
            case AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY: {
                if (arguments == null) return false;
                int granularity = arguments.getInt(
                        AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT);
                boolean extend = arguments.getBoolean(
                        AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN);
                if (!isValidMovementGranularity(granularity)) {
                    return false;
                }
                return nextAtGranularity(granularity, extend, virtualViewId);
            }
            case AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY: {
                if (arguments == null) return false;
                int granularity = arguments.getInt(
                        AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT);
                boolean extend = arguments.getBoolean(
                        AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN);
                if (!isValidMovementGranularity(granularity)) {
                    return false;
                }
                return previousAtGranularity(granularity, extend, virtualViewId);
            }
            case AccessibilityNodeInfo.ACTION_SCROLL_FORWARD:
                return scrollForward(virtualViewId);
            case AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD:
                return scrollBackward(virtualViewId);
            case AccessibilityNodeInfo.ACTION_CUT:
                if (mWebContents != null) {
                    mWebContents.cut();
                    return true;
                }
                return false;
            case AccessibilityNodeInfo.ACTION_COPY:
                if (mWebContents != null) {
                    mWebContents.copy();
                    return true;
                }
                return false;
            case AccessibilityNodeInfo.ACTION_PASTE:
                if (mWebContents != null) {
                    mWebContents.paste();
                    return true;
                }
                return false;
            case AccessibilityNodeInfo.ACTION_COLLAPSE:
            case AccessibilityNodeInfo.ACTION_EXPAND:
                // If something is collapsible or expandable, just activate it to toggle.
                WebContentsAccessibilityImplJni.get().click(
                        mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId);
                return true;
            case ACTION_SHOW_ON_SCREEN:
                WebContentsAccessibilityImplJni.get().scrollToMakeNodeVisible(
                        mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId);
                return true;
            case ACTION_CONTEXT_CLICK:
                WebContentsAccessibilityImplJni.get().showContextMenu(
                        mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId);
                return true;
            case ACTION_SCROLL_UP:
                return WebContentsAccessibilityImplJni.get().scroll(mNativeObj,
                        WebContentsAccessibilityImpl.this, virtualViewId, ScrollDirection.UP);
            case ACTION_SCROLL_DOWN:
                return WebContentsAccessibilityImplJni.get().scroll(mNativeObj,
                        WebContentsAccessibilityImpl.this, virtualViewId, ScrollDirection.DOWN);
            case ACTION_SCROLL_LEFT:
                return WebContentsAccessibilityImplJni.get().scroll(mNativeObj,
                        WebContentsAccessibilityImpl.this, virtualViewId, ScrollDirection.LEFT);
            case ACTION_SCROLL_RIGHT:
                return WebContentsAccessibilityImplJni.get().scroll(mNativeObj,
                        WebContentsAccessibilityImpl.this, virtualViewId, ScrollDirection.RIGHT);
            default:
                break;
        }
        return false;
    }

    @Override
    public void onAutofillPopupDisplayed(View autofillPopupView) {
        if (isAccessibilityEnabled()) {
            mAutofillPopupView = autofillPopupView;
            WebContentsAccessibilityImplJni.get().onAutofillPopupDisplayed(
                    mNativeObj, WebContentsAccessibilityImpl.this);
        }
    }

    @Override
    public void onAutofillPopupDismissed() {
        if (isAccessibilityEnabled()) {
            WebContentsAccessibilityImplJni.get().onAutofillPopupDismissed(
                    mNativeObj, WebContentsAccessibilityImpl.this);
            mAutofillPopupView = null;
        }
    }

    @Override
    public void onAutofillPopupAccessibilityFocusCleared() {
        if (isAccessibilityEnabled()) {
            int id = WebContentsAccessibilityImplJni.get()
                             .getIdForElementAfterElementHostingAutofillPopup(
                                     mNativeObj, WebContentsAccessibilityImpl.this);
            if (id == 0) return;

            moveAccessibilityFocusToId(id);
            WebContentsAccessibilityImplJni.get().scrollToMakeNodeVisible(
                    mNativeObj, WebContentsAccessibilityImpl.this, mAccessibilityFocusId);
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
            if (mLastHoverId != View.NO_ID) {
                sendAccessibilityEvent(mLastHoverId, AccessibilityEvent.TYPE_VIEW_HOVER_EXIT);
                mLastHoverId = View.NO_ID;
            }
            if (mPendingScrollToMakeNodeVisible) {
                WebContentsAccessibilityImplJni.get().scrollToMakeNodeVisible(
                        mNativeObj, WebContentsAccessibilityImpl.this, mAccessibilityFocusId);
            }
            mPendingScrollToMakeNodeVisible = false;
            return true;
        }

        mIsHovering = true;
        mUserHasTouchExplored = true;
        return true;
    }

    /**
     * Notify us when the frame info is initialized,
     * the first time, since until that point, we can't use RenderCoordinates to transform
     * web coordinates to screen coordinates.
     */
    @CalledByNative
    private void notifyFrameInfoInitialized() {
        if (mNotifyFrameInfoInitializedCalled) return;

        mNotifyFrameInfoInitializedCalled = true;

        // Invalidate the container view, since the chrome accessibility tree is now
        // ready and listed as the child of the container view.
        sendWindowContentChangedOnView();

        // (Re-) focus focused element, since we weren't able to create an
        // AccessibilityNodeInfo for this element before.
        if (!mShouldFocusOnPageLoad) return;
        if (mAccessibilityFocusId != View.NO_ID) {
            moveAccessibilityFocusToIdAndRefocusIfNeeded(mAccessibilityFocusId);
        }
    }

    private boolean jumpToElementType(int virtualViewId, String elementType, boolean forwards) {
        int id = WebContentsAccessibilityImplJni.get().findElementType(mNativeObj,
                WebContentsAccessibilityImpl.this, virtualViewId, elementType, forwards);
        if (id == 0) return false;

        moveAccessibilityFocusToId(id);
        WebContentsAccessibilityImplJni.get().scrollToMakeNodeVisible(
                mNativeObj, WebContentsAccessibilityImpl.this, mAccessibilityFocusId);
        return true;
    }

    private void setGranularityAndUpdateSelection(int granularity) {
        mSelectionGranularity = granularity;
        if (mSelectionGranularity == NO_GRANULARITY_SELECTED) {
            mSelectionStartIndex = -1;
            mSelectionEndIndex = -1;
        }
        if (WebContentsAccessibilityImplJni.get().isEditableText(
                    mNativeObj, WebContentsAccessibilityImpl.this, mAccessibilityFocusId)
                && WebContentsAccessibilityImplJni.get().isFocused(
                        mNativeObj, WebContentsAccessibilityImpl.this, mAccessibilityFocusId)) {
            mSelectionStartIndex =
                    WebContentsAccessibilityImplJni.get().getEditableTextSelectionStart(
                            mNativeObj, WebContentsAccessibilityImpl.this, mAccessibilityFocusId);
            mSelectionEndIndex = WebContentsAccessibilityImplJni.get().getEditableTextSelectionEnd(
                    mNativeObj, WebContentsAccessibilityImpl.this, mAccessibilityFocusId);
        }
    }

    private boolean nextAtGranularity(int granularity, boolean extendSelection, int virtualViewId) {
        if (virtualViewId != mSelectionNodeId) return false;
        setGranularityAndUpdateSelection(granularity);

        // This calls finishGranularityMove when it's done.
        return WebContentsAccessibilityImplJni.get().nextAtGranularity(mNativeObj,
                WebContentsAccessibilityImpl.this, mSelectionGranularity, extendSelection,
                virtualViewId, mSelectionStartIndex);
    }

    private boolean previousAtGranularity(
            int granularity, boolean extendSelection, int virtualViewId) {
        if (virtualViewId != mSelectionNodeId) return false;
        setGranularityAndUpdateSelection(granularity);

        // This calls finishGranularityMove when it's done.
        return WebContentsAccessibilityImplJni.get().previousAtGranularity(mNativeObj,
                WebContentsAccessibilityImpl.this, mSelectionGranularity, extendSelection,
                virtualViewId, mSelectionEndIndex);
    }

    @CalledByNative
    private void finishGranularityMove(String text, boolean extendSelection, int itemStartIndex,
            int itemEndIndex, boolean forwards) {
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

        // Update the cursor or selection based on the traversal. If it's an editable
        // text node, set the real editing cursor too.
        if (forwards) {
            mSelectionEndIndex = itemEndIndex;
        } else {
            mSelectionEndIndex = itemStartIndex;
        }
        if (!extendSelection) {
            mSelectionStartIndex = mSelectionEndIndex;
        }
        if (WebContentsAccessibilityImplJni.get().isEditableText(
                    mNativeObj, WebContentsAccessibilityImpl.this, mSelectionNodeId)
                && WebContentsAccessibilityImplJni.get().isFocused(
                        mNativeObj, WebContentsAccessibilityImpl.this, mSelectionNodeId)) {
            WebContentsAccessibilityImplJni.get().setSelection(mNativeObj,
                    WebContentsAccessibilityImpl.this, mSelectionNodeId, mSelectionStartIndex,
                    mSelectionEndIndex);
        }

        // The selection event's "from" and "to" indices are just a cursor at the focus
        // end of the movement, or a selection if extendSelection is true.
        selectionEvent.setFromIndex(mSelectionStartIndex);
        selectionEvent.setToIndex(mSelectionStartIndex);
        selectionEvent.setItemCount(text.length());

        // The traverse event's "from" and "to" indices surround the item (e.g. the word,
        // etc.) with no whitespace.
        if (forwards
                && WebContentsAccessibilityImplJni.get().isEditableText(
                        mNativeObj, WebContentsAccessibilityImpl.this, mSelectionNodeId)) {
            traverseEvent.setFromIndex(itemStartIndex - 1);
            traverseEvent.setToIndex(itemEndIndex - 1);
        } else {
            traverseEvent.setFromIndex(itemStartIndex);
            traverseEvent.setToIndex(itemEndIndex);
        }
        traverseEvent.setItemCount(text.length());
        traverseEvent.setMovementGranularity(mSelectionGranularity);
        traverseEvent.setContentDescription(text);

        // The traverse event needs to set its associated action that triggered it.
        if (forwards) {
            traverseEvent.setAction(AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY);
        } else {
            traverseEvent.setAction(AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY);
        }

        mView.requestSendAccessibilityEvent(mView, selectionEvent);
        mView.requestSendAccessibilityEvent(mView, traverseEvent);
    }

    private boolean scrollForward(int virtualViewId) {
        if (WebContentsAccessibilityImplJni.get().isSlider(
                    mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId)) {
            return WebContentsAccessibilityImplJni.get().adjustSlider(
                    mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId, true);
        } else {
            return WebContentsAccessibilityImplJni.get().scroll(mNativeObj,
                    WebContentsAccessibilityImpl.this, virtualViewId, ScrollDirection.FORWARD);
        }
    }

    private boolean scrollBackward(int virtualViewId) {
        if (WebContentsAccessibilityImplJni.get().isSlider(
                    mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId)) {
            return WebContentsAccessibilityImplJni.get().adjustSlider(
                    mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId, false);
        } else {
            return WebContentsAccessibilityImplJni.get().scroll(mNativeObj,
                    WebContentsAccessibilityImpl.this, virtualViewId, ScrollDirection.BACKWARD);
        }
    }

    private boolean moveAccessibilityFocusToId(int newAccessibilityFocusId) {
        if (newAccessibilityFocusId == mAccessibilityFocusId) return false;

        WebContentsAccessibilityImplJni.get().moveAccessibilityFocus(mNativeObj,
                WebContentsAccessibilityImpl.this, mAccessibilityFocusId, newAccessibilityFocusId);

        mAccessibilityFocusId = newAccessibilityFocusId;
        mAccessibilityFocusRect = null;
        // Used to store the node (edit text field) that has input focus but not a11y focus.
        // Usually while the user is typing in an edit text field, a11y is on the IME and input
        // focus is on the edit field. Granularity move needs to know where the input focus is.
        mSelectionNodeId = mAccessibilityFocusId;
        mSelectionGranularity = NO_GRANULARITY_SELECTED;
        mSelectionStartIndex = -1;
        mSelectionEndIndex = WebContentsAccessibilityImplJni.get().getTextLength(
                mNativeObj, WebContentsAccessibilityImpl.this, newAccessibilityFocusId);

        if (WebContentsAccessibilityImplJni.get().isAutofillPopupNode(
                    mNativeObj, WebContentsAccessibilityImpl.this, mAccessibilityFocusId)) {
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
        if (mSendWindowContentChangedRunnable != null) return;

        mSendWindowContentChangedRunnable = new Runnable() {
            @Override
            public void run() {
                sendWindowContentChangedOnView();
            }
        };

        mView.postDelayed(mSendWindowContentChangedRunnable, WINDOW_CONTENT_CHANGED_DELAY_MS);
    }

    private void sendWindowContentChangedOnView() {
        if (mSendWindowContentChangedRunnable != null) {
            mView.removeCallbacks(mSendWindowContentChangedRunnable);
            mSendWindowContentChangedRunnable = null;
        }
        mView.sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
    }

    private void sendWindowContentChangedOnVirtualView(int virtualViewId) {
        sendAccessibilityEvent(virtualViewId, AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
    }

    private void sendAccessibilityEvent(int virtualViewId, int eventType) {
        // The container view is indicated by a virtualViewId of NO_ID; post these events directly
        // since there's no web-specific information to attach.
        if (virtualViewId == View.NO_ID) {
            mView.sendAccessibilityEvent(eventType);
            return;
        }

        AccessibilityEvent event = buildAccessibilityEvent(virtualViewId, eventType);
        if (event != null) {
            mView.requestSendAccessibilityEvent(mView, event);
        }
    }

    private AccessibilityEvent buildAccessibilityEvent(int virtualViewId, int eventType) {
        // If we don't have any frame info, then the virtual hierarchy
        // doesn't exist in the view of the Android framework, so should
        // never send any events.
        if (!isAccessibilityEnabled() || !isFrameInfoInitialized()) {
            return null;
        }

        // This is currently needed if we want Android to visually highlight
        // the item that has accessibility focus. In practice, this doesn't seem to slow
        // things down, because it's only called when the accessibility focus moves.
        // TODO(dmazzoni): remove this if/when Android framework fixes bug.
        mView.postInvalidate();

        final AccessibilityEvent event = AccessibilityEvent.obtain(eventType);
        event.setPackageName(mContext.getPackageName());
        event.setSource(mView, virtualViewId);
        if (!WebContentsAccessibilityImplJni.get().populateAccessibilityEvent(mNativeObj,
                    WebContentsAccessibilityImpl.this, event, virtualViewId, eventType)) {
            event.recycle();
            return null;
        }
        return event;
    }

    private Bundle getOrCreateBundleForAccessibilityEvent(AccessibilityEvent event) {
        Bundle bundle = (Bundle) event.getParcelableData();
        if (bundle == null) {
            bundle = new Bundle();
            event.setParcelableData(bundle);
        }
        return bundle;
    }

    @Override
    public boolean isAccessibilityEnabled() {
        return isNativeInitialized()
                && (mAccessibilityEnabledForTesting || mAccessibilityManager.isEnabled());
    }

    private AccessibilityNodeInfo createNodeForHost(int rootId) {
        // Since we don't want the parent to be focusable, but we can't remove
        // actions from a node, copy over the necessary fields.
        final AccessibilityNodeInfo result = AccessibilityNodeInfo.obtain(mView);
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
     * notifyFrameInfoInitialized might not be called at all if RenderCoordinates
     * gets initialized first.
     */
    private boolean isFrameInfoInitialized() {
        if (mWebContents == null) {
            // We already got frame info since WebContents finished its lifecycle.
            return true;
        }
        RenderCoordinatesImpl rc = mWebContents.getRenderCoordinates();
        return rc.getContentWidthCss() != 0.0 || rc.getContentHeightCss() != 0.0;
    }

    @CalledByNative
    private void handlePageLoaded(int id) {
        if (!mShouldFocusOnPageLoad) return;
        if (mUserHasTouchExplored) return;
        moveAccessibilityFocusToIdAndRefocusIfNeeded(id);
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
        sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_CLICKED);
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
        int rootId = WebContentsAccessibilityImplJni.get().getRootId(
                mNativeObj, WebContentsAccessibilityImpl.this);
        if (rootId != mCurrentRootId) {
            mCurrentRootId = rootId;
            sendWindowContentChangedOnView();
        } else {
            sendWindowContentChangedOnVirtualView(id);
        }
    }

    @CalledByNative
    private void handleNavigate() {
        mAccessibilityFocusId = View.NO_ID;
        mAccessibilityFocusRect = null;
        mUserHasTouchExplored = false;
        // Invalidate the host, since its child is now gone.
        sendWindowContentChangedOnView();
    }

    @CalledByNative
    private void handleScrollPositionChanged(int id) {
        sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_SCROLLED);
    }

    @CalledByNative
    private void handleScrolledToAnchor(final int id) {
        // "Scrolled to anchor" means that the user followed a same-page link;
        // the id here is of the element that was scrolled into view, and that
        // should be where accessibility focus lands.
        //
        // However, in practice there's a race condition because following a
        // same-page link often results in a focus change from the same-page link
        // that was focused previously.
        //
        // As a result, it works better to move accessibility focus to the new
        // location after a short delay, after the focus change.
        if (mScrolledToAnchorRunnable != null) return;
        mScrolledToAnchorRunnable = new Runnable() {
            @Override
            public void run() {
                moveAccessibilityFocusToId(id);
                mScrolledToAnchorRunnable = null;
            }
        };

        mView.postDelayed(mScrolledToAnchorRunnable, SCROLLED_TO_ANCHOR_DELAY_MS);
    }

    @CalledByNative
    private void handleHover(int id) {
        if (mLastHoverId == id) return;
        if (!mIsHovering) return;

        // Always send the ENTER and then the EXIT event, to match a standard Android View.
        sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_HOVER_ENTER);
        if (mLastHoverId != View.NO_ID) {
            sendAccessibilityEvent(mLastHoverId, AccessibilityEvent.TYPE_VIEW_HOVER_EXIT);
        }
        mLastHoverId = id;
    }

    @CalledByNative
    private void announceLiveRegionText(String text) {
        mView.announceForAccessibility(text);
    }

    @CalledByNative
    private void setAccessibilityNodeInfoParent(AccessibilityNodeInfo node, int parentId) {
        node.setParent(mView, parentId);
    }

    @CalledByNative
    private void addAccessibilityNodeInfoChild(AccessibilityNodeInfo node, int childId) {
        node.addChild(mView, childId);
    }

    @CalledByNative
    private void setAccessibilityNodeInfoBooleanAttributes(AccessibilityNodeInfo node,
            int virtualViewId, boolean checkable, boolean checked, boolean clickable,
            boolean enabled, boolean focusable, boolean focused, boolean password,
            boolean scrollable, boolean selected, boolean visibleToUser) {
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

        node.setMovementGranularities(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER
                | AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD
                | AccessibilityNodeInfo.MOVEMENT_GRANULARITY_LINE);

        if (mAccessibilityFocusId == virtualViewId) {
            node.setAccessibilityFocused(true);
        } else {
            node.setAccessibilityFocused(false);
        }
    }

    // For anything lower than API level 21 (Lollipop), calls AccessibilityNodeInfo.addAction(int)
    // if it's a supported action, and does nothing otherwise.  For 21 and higher, this is
    // overridden in LollipopWebContentsAccessibility using the new non-deprecated API.
    @SuppressWarnings("deprecation")
    protected void addAction(AccessibilityNodeInfo node, int actionId) {
        // Before API level 21, it's not possible to expose actions other than the "legacy standard"
        // ones.
        if (actionId > AccessibilityNodeInfo.ACTION_SET_TEXT) return;

        node.addAction(actionId);
    }

    @CalledByNative
    private void addAccessibilityNodeInfoActions(AccessibilityNodeInfo node, int virtualViewId,
            boolean canScrollForward, boolean canScrollBackward, boolean canScrollUp,
            boolean canScrollDown, boolean canScrollLeft, boolean canScrollRight, boolean clickable,
            boolean editableText, boolean enabled, boolean focusable, boolean focused,
            boolean isCollapsed, boolean isExpanded, boolean hasNonEmptyValue,
            boolean hasNonEmptyInnerText) {
        addAction(node, AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT);
        addAction(node, AccessibilityNodeInfo.ACTION_PREVIOUS_HTML_ELEMENT);
        addAction(node, ACTION_SHOW_ON_SCREEN);
        addAction(node, ACTION_CONTEXT_CLICK);

        if (hasNonEmptyInnerText) {
            addAction(node, AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY);
            addAction(node, AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY);
        }

        if (editableText && enabled) {
            // TODO: don't support actions that modify it if it's read-only (but
            // SET_SELECTION and COPY are okay).
            addAction(node, ACTION_SET_TEXT);
            addAction(node, AccessibilityNodeInfo.ACTION_PASTE);

            if (hasNonEmptyValue) {
                addAction(node, AccessibilityNodeInfo.ACTION_SET_SELECTION);
                addAction(node, AccessibilityNodeInfo.ACTION_CUT);
                addAction(node, AccessibilityNodeInfo.ACTION_COPY);
            }
        }

        if (canScrollForward) {
            addAction(node, AccessibilityNodeInfo.ACTION_SCROLL_FORWARD);
        }

        if (canScrollBackward) {
            addAction(node, AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD);
        }

        if (canScrollUp) {
            addAction(node, ACTION_SCROLL_UP);
        }

        if (canScrollDown) {
            addAction(node, ACTION_SCROLL_DOWN);
        }

        if (canScrollLeft) {
            addAction(node, ACTION_SCROLL_LEFT);
        }

        if (canScrollRight) {
            addAction(node, ACTION_SCROLL_RIGHT);
        }

        if (focusable) {
            if (focused) {
                addAction(node, AccessibilityNodeInfo.ACTION_CLEAR_FOCUS);
            } else {
                addAction(node, AccessibilityNodeInfo.ACTION_FOCUS);
            }
        }

        if (mAccessibilityFocusId == virtualViewId) {
            addAction(node, AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS);
        } else {
            addAction(node, AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS);
        }

        if (clickable) {
            addAction(node, AccessibilityNodeInfo.ACTION_CLICK);
        }

        if (isCollapsed) {
            addAction(node, ACTION_EXPAND);
        }

        if (isExpanded) {
            addAction(node, ACTION_COLLAPSE);
        }
    }

    @CalledByNative
    private void setAccessibilityNodeInfoClassName(AccessibilityNodeInfo node, String className) {
        node.setClassName(className);
    }

    @SuppressLint("NewApi")
    @CalledByNative
    private void setAccessibilityNodeInfoText(AccessibilityNodeInfo node, String text,
            boolean annotateAsLink, boolean isEditableText, String language, int[] suggestionStarts,
            int[] suggestionEnds, String[] suggestions) {
        CharSequence computedText = computeText(
                text, isEditableText, language, suggestionStarts, suggestionEnds, suggestions);
        node.setText(computedText);
    }

    protected CharSequence computeText(String text, boolean annotateAsLink, String language,
            int[] suggestionStarts, int[] suggestionEnds, String[] suggestions) {
        if (annotateAsLink) {
            SpannableString spannable = new SpannableString(text);
            spannable.setSpan(new URLSpan(""), 0, spannable.length(), 0);
            return spannable;
        }
        return text;
    }

    protected void convertWebRectToAndroidCoordinates(Rect rect) {
        // Offset by the scroll position.
        RenderCoordinatesImpl rc = mWebContents.getRenderCoordinates();
        rect.offset(-(int) rc.getScrollX(), -(int) rc.getScrollY());

        // Convert CSS (web) pixels to Android View pixels
        rect.left = (int) rc.fromLocalCssToPix(rect.left);
        rect.top = (int) rc.fromLocalCssToPix(rect.top);
        rect.bottom = (int) rc.fromLocalCssToPix(rect.bottom);
        rect.right = (int) rc.fromLocalCssToPix(rect.right);

        // Offset by the location of the web content within the view.
        rect.offset(0, (int) rc.getContentOffsetYPix());

        // Finally offset by the location of the view within the screen.
        final int[] viewLocation = new int[2];
        mView.getLocationOnScreen(viewLocation);
        rect.offset(viewLocation[0], viewLocation[1]);

        // Clip to the viewport bounds.
        int viewportRectTop = viewLocation[1] + (int) rc.getContentOffsetYPix();
        int viewportRectBottom = viewportRectTop + mView.getHeight();
        if (rect.top < viewportRectTop) rect.top = viewportRectTop;
        if (rect.bottom > viewportRectBottom) rect.bottom = viewportRectBottom;
    }

    @CalledByNative
    private void setAccessibilityNodeInfoLocation(AccessibilityNodeInfo node,
            final int virtualViewId, int absoluteLeft, int absoluteTop, int parentRelativeLeft,
            int parentRelativeTop, int width, int height, boolean isRootNode) {
        // First set the bounds in parent.
        Rect boundsInParent = new Rect(parentRelativeLeft, parentRelativeTop,
                parentRelativeLeft + width, parentRelativeTop + height);
        if (isRootNode) {
            // Offset of the web content relative to the View.
            RenderCoordinatesImpl rc = mWebContents.getRenderCoordinates();
            boundsInParent.offset(0, (int) rc.getContentOffsetYPix());
        }
        node.setBoundsInParent(boundsInParent);

        Rect rect = new Rect(absoluteLeft, absoluteTop, absoluteLeft + width, absoluteTop + height);
        convertWebRectToAndroidCoordinates(rect);

        node.setBoundsInScreen(rect);

        // Work around a bug in the Android framework where if the object with accessibility
        // focus moves, the accessibility focus rect is not updated - both the visual highlight,
        // and the location on the screen that's clicked if you double-tap. To work around this,
        // when we know the object with accessibility focus moved, move focus away and then
        // move focus right back to it, which tricks Android into updating its bounds.
        if (virtualViewId == mAccessibilityFocusId && virtualViewId != mCurrentRootId) {
            if (mAccessibilityFocusRect == null) {
                mAccessibilityFocusRect = rect;
            } else if (!mAccessibilityFocusRect.equals(rect)) {
                mAccessibilityFocusRect = rect;
                moveAccessibilityFocusToIdAndRefocusIfNeeded(virtualViewId);
            }
        }
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoKitKatAttributes(AccessibilityNodeInfo node,
            boolean isRoot, boolean isEditableText, String role, String roleDescription,
            String hint, int selectionStartIndex, int selectionEndIndex, boolean hasImage,
            boolean contentInvalid, String targetUrl) {
        // Requires KitKat or higher.
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoLollipopAttributes(AccessibilityNodeInfo node,
            boolean canOpenPopup, boolean contentInvalid, boolean dismissable, boolean multiLine,
            int inputType, int liveRegion, String errorMessage) {
        // Requires Lollipop or higher.
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoCollectionInfo(
            AccessibilityNodeInfo node, int rowCount, int columnCount, boolean hierarchical) {
        // Requires Lollipop or higher.
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoCollectionItemInfo(AccessibilityNodeInfo node,
            int rowIndex, int rowSpan, int columnIndex, int columnSpan, boolean heading) {
        // Requires Lollipop or higher.
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoRangeInfo(
            AccessibilityNodeInfo node, int rangeType, float min, float max, float current) {
        // Requires Lollipop or higher.
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoViewIdResourceName(
            AccessibilityNodeInfo node, String viewIdResourceName) {
        // Requires Lollipop or higher.
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoOAttributes(
            AccessibilityNodeInfo node, boolean hasCharacterLocations) {
        // Requires O or higher.
    }

    @CalledByNative
    private void setAccessibilityEventBooleanAttributes(AccessibilityEvent event, boolean checked,
            boolean enabled, boolean password, boolean scrollable) {
        event.setChecked(checked);
        event.setEnabled(enabled);
        event.setPassword(password);
        event.setScrollable(scrollable);
    }

    @CalledByNative
    private void setAccessibilityEventClassName(AccessibilityEvent event, String className) {
        event.setClassName(className);
    }

    @CalledByNative
    private void setAccessibilityEventListAttributes(
            AccessibilityEvent event, int currentItemIndex, int itemCount) {
        event.setCurrentItemIndex(currentItemIndex);
        event.setItemCount(itemCount);
    }

    @CalledByNative
    private void setAccessibilityEventScrollAttributes(
            AccessibilityEvent event, int scrollX, int scrollY, int maxScrollX, int maxScrollY) {
        event.setScrollX(scrollX);
        event.setScrollY(scrollY);
        event.setMaxScrollX(maxScrollX);
        event.setMaxScrollY(maxScrollY);
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

    @CalledByNative
    protected void setAccessibilityEventLollipopAttributes(AccessibilityEvent event,
            boolean canOpenPopup, boolean contentInvalid, boolean dismissable, boolean multiLine,
            int inputType, int liveRegion) {
        // Backwards compatibility for Lollipop AccessibilityNodeInfo fields.
        Bundle bundle = getOrCreateBundleForAccessibilityEvent(event);
        bundle.putBoolean("AccessibilityNodeInfo.canOpenPopup", canOpenPopup);
        bundle.putBoolean("AccessibilityNodeInfo.contentInvalid", contentInvalid);
        bundle.putBoolean("AccessibilityNodeInfo.dismissable", dismissable);
        bundle.putBoolean("AccessibilityNodeInfo.multiLine", multiLine);
        bundle.putInt("AccessibilityNodeInfo.inputType", inputType);
        bundle.putInt("AccessibilityNodeInfo.liveRegion", liveRegion);
    }

    @CalledByNative
    protected void setAccessibilityEventCollectionInfo(
            AccessibilityEvent event, int rowCount, int columnCount, boolean hierarchical) {
        // Backwards compatibility for Lollipop AccessibilityNodeInfo fields.
        Bundle bundle = getOrCreateBundleForAccessibilityEvent(event);
        bundle.putInt("AccessibilityNodeInfo.CollectionInfo.rowCount", rowCount);
        bundle.putInt("AccessibilityNodeInfo.CollectionInfo.columnCount", columnCount);
        bundle.putBoolean("AccessibilityNodeInfo.CollectionInfo.hierarchical", hierarchical);
    }

    @CalledByNative
    protected void setAccessibilityEventHeadingFlag(AccessibilityEvent event, boolean heading) {
        // Backwards compatibility for Lollipop AccessibilityNodeInfo fields.
        Bundle bundle = getOrCreateBundleForAccessibilityEvent(event);
        bundle.putBoolean("AccessibilityNodeInfo.CollectionItemInfo.heading", heading);
    }

    @CalledByNative
    protected void setAccessibilityEventCollectionItemInfo(
            AccessibilityEvent event, int rowIndex, int rowSpan, int columnIndex, int columnSpan) {
        // Backwards compatibility for Lollipop AccessibilityNodeInfo fields.
        Bundle bundle = getOrCreateBundleForAccessibilityEvent(event);
        bundle.putInt("AccessibilityNodeInfo.CollectionItemInfo.rowIndex", rowIndex);
        bundle.putInt("AccessibilityNodeInfo.CollectionItemInfo.rowSpan", rowSpan);
        bundle.putInt("AccessibilityNodeInfo.CollectionItemInfo.columnIndex", columnIndex);
        bundle.putInt("AccessibilityNodeInfo.CollectionItemInfo.columnSpan", columnSpan);
    }

    @CalledByNative
    protected void setAccessibilityEventRangeInfo(
            AccessibilityEvent event, int rangeType, float min, float max, float current) {
        // Backwards compatibility for Lollipop AccessibilityNodeInfo fields.
        Bundle bundle = getOrCreateBundleForAccessibilityEvent(event);
        bundle.putInt("AccessibilityNodeInfo.RangeInfo.type", rangeType);
        bundle.putFloat("AccessibilityNodeInfo.RangeInfo.min", min);
        bundle.putFloat("AccessibilityNodeInfo.RangeInfo.max", max);
        bundle.putFloat("AccessibilityNodeInfo.RangeInfo.current", current);
    }

    /**
     * On Android O and higher, we should respect whatever is displayed
     * in a password box and report that via accessibility APIs, whether
     * that's the unobscured password, or all dots.
     *
     * Previous to O, shouldExposePasswordText() returns a system setting
     * that determines whether we should return the unobscured password or all
     * dots, independent of what was displayed visually.
     */
    @CalledByNative
    boolean shouldRespectDisplayedPasswordText() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
    }

    /**
     * Only relevant prior to Android O, see shouldRespectDisplayedPasswordText.
     */
    @CalledByNative
    boolean shouldExposePasswordText() {
        ContentResolver contentResolver = mContext.getContentResolver();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            return (Settings.System.getInt(contentResolver, Settings.System.TEXT_SHOW_PASSWORD, 1)
                    == 1);
        }

        return (Settings.Secure.getInt(
                        contentResolver, Settings.Secure.ACCESSIBILITY_SPEAK_PASSWORD, 0)
                == 1);
    }

    /**
     * Iterate over all enabled accessibility services and return a bitmask containing the union
     * of all event types that they listen to.
     * @return
     */
    @CalledByNative
    private int getAccessibilityServiceEventTypeMask() {
        int eventTypeMask = 0;
        for (AccessibilityServiceInfo service :
                mAccessibilityManager.getEnabledAccessibilityServiceList(
                        AccessibilityServiceInfo.FEEDBACK_ALL_MASK)) {
            eventTypeMask |= service.eventTypes;
        }
        return eventTypeMask;
    }

    /**
     * Iterate over all enabled accessibility services and return a bitmask containing the union
     * of all feedback types that they provide.
     * @return
     */
    @CalledByNative
    private int getAccessibilityServiceFeedbackTypeMask() {
        int feedbackTypeMask = 0;
        for (AccessibilityServiceInfo service :
                mAccessibilityManager.getEnabledAccessibilityServiceList(
                        AccessibilityServiceInfo.FEEDBACK_ALL_MASK)) {
            feedbackTypeMask |= service.feedbackType;
        }
        return feedbackTypeMask;
    }

    /**
     * Iterate over all enabled accessibility services and return a bitmask containing the union
     * of all accessibility service flags from any of them.
     * @return
     */
    @CalledByNative
    private int getAccessibilityServiceFlagsMask() {
        int flagsMask = 0;
        for (AccessibilityServiceInfo service :
                mAccessibilityManager.getEnabledAccessibilityServiceList(
                        AccessibilityServiceInfo.FEEDBACK_ALL_MASK)) {
            flagsMask |= service.flags;
        }
        return flagsMask;
    }

    /**
     * Iterate over all enabled accessibility services and return a bitmask containing the union
     * of all service capabilities.
     * @return
     */
    @CalledByNative
    protected int getAccessibilityServiceCapabilitiesMask() {
        // Implemented in KitKatWebContentsAccessibility.
        return 0;
    }

    @NativeMethods
    interface Natives {
        long init(WebContentsAccessibilityImpl caller, WebContents webContents);
        void onAutofillPopupDisplayed(
                long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller);
        void onAutofillPopupDismissed(
                long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller);
        int getIdForElementAfterElementHostingAutofillPopup(
                long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller);
        int getRootId(
                long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller);
        boolean isNodeValid(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        boolean isAutofillPopupNode(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        boolean isEditableText(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        boolean isFocused(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        int getEditableTextSelectionStart(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        int getEditableTextSelectionEnd(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        boolean populateAccessibilityNodeInfo(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, AccessibilityNodeInfo info, int id);
        boolean populateAccessibilityEvent(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, AccessibilityEvent event, int id,
                int eventType);
        void click(long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller,
                int id);
        void focus(long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller,
                int id);
        void blur(long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller);
        void scrollToMakeNodeVisible(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        int findElementType(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int startId, String elementType,
                boolean forwards);
        void setTextFieldValue(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id, String newValue);
        void setSelection(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id, int start, int end);
        boolean nextAtGranularity(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int selectionGranularity,
                boolean extendSelection, int id, int cursorIndex);
        boolean previousAtGranularity(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int selectionGranularity,
                boolean extendSelection, int id, int cursorIndex);
        boolean adjustSlider(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id, boolean increment);
        void moveAccessibilityFocus(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int oldId, int newId);
        boolean isSlider(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        boolean scroll(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id, int direction);
        String getSupportedHtmlElementTypes(
                long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller);
        void showContextMenu(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        boolean isEnabled(
                long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller);
        void enable(
                long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller);
        boolean areInlineTextBoxesLoaded(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        void loadInlineTextBoxes(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        int[] getCharacterBoundingBoxes(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id, int start, int len);
        int getTextLength(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        void addSpellingErrorForTesting(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id, int startOffset, int endOffset);
    }
}
