// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.mock;

import android.annotation.SuppressLint;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Parcel;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.blink_public.input.SelectionGranularity;
import org.chromium.cc.input.BrowserControlsOffsetTagsInfo;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.ImageDownloadCallback;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.StylusWritingHandler;
import org.chromium.content_public.browser.StylusWritingImeCallback;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.back_forward_transition.AnimationStage;
import org.chromium.ui.OverscrollRefreshHandler;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.mojom.VirtualKeyboardMode;
import org.chromium.url.GURL;

/** Mock class for {@link WebContents}. */
@SuppressLint("ParcelCreator")
public class MockWebContents implements WebContents {
    public RenderFrameHost renderFrameHost;
    private GURL mLastCommittedUrl;

    @Override
    public void setDelegates(
            String productVersion,
            ViewAndroidDelegate viewDelegate,
            ViewEventSink.InternalAccessDelegate accessDelegate,
            WindowAndroid windowAndroid,
            WebContents.InternalsHolder internalsHolder) {}

    @Override
    public void clearJavaWebContentsObservers() {}

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {}

    @Override
    public WindowAndroid getTopLevelNativeWindow() {
        return null;
    }

    @Override
    public ViewAndroidDelegate getViewAndroidDelegate() {
        return null;
    }

    @Override
    public void setTopLevelNativeWindow(WindowAndroid windowAndroid) {}

    @Override
    public void destroy() {}

    @Override
    public boolean isDestroyed() {
        return false;
    }

    @Override
    public void clearNativeReference() {}

    @Override
    public NavigationController getNavigationController() {
        return null;
    }

    @Override
    public RenderFrameHost getMainFrame() {
        return renderFrameHost;
    }

    @Override
    public RenderFrameHost getFocusedFrame() {
        return null;
    }

    @Override
    public boolean isFocusedElementEditable() {
        return false;
    }

    @Override
    public RenderFrameHost getRenderFrameHostFromId(GlobalRenderFrameHostId id) {
        return null;
    }

    @Override
    @Nullable
    public RenderWidgetHostView getRenderWidgetHostView() {
        return null;
    }

    @Override
    public @Visibility int getVisibility() {
        return Visibility.VISIBLE;
    }

    @Override
    public void updateWebContentsVisibility(@Visibility int visibility) {}

    @Override
    public String getTitle() {
        return null;
    }

    @Override
    public GURL getVisibleUrl() {
        return GURL.emptyGURL();
    }

    @Override
    @VirtualKeyboardMode.EnumType
    public int getVirtualKeyboardMode() {
        return VirtualKeyboardMode.UNSET;
    }

    @Override
    public String getEncoding() {
        return null;
    }

    @Override
    public boolean isLoading() {
        return false;
    }

    @Override
    public boolean shouldShowLoadingUI() {
        return false;
    }

    @Override
    public boolean hasUncommittedNavigationInPrimaryMainFrame() {
        return false;
    }

    @Override
    public void dispatchBeforeUnload(boolean autoCancel) {}

    @Override
    public void stop() {}

    @Override
    public void setImportance(int importance) {}

    @Override
    public void suspendAllMediaPlayers() {}

    @Override
    public void setAudioMuted(boolean mute) {}

    @Override
    public boolean isAudioMuted() { return false; }

    @Override
    public boolean focusLocationBarByDefault() {
        return false;
    }

    @Override
    public void setFocus(boolean hasFocus) {}

    @Override
    public boolean isFullscreenForCurrentTab() {
        return false;
    }

    @Override
    public void exitFullscreen() {}

    @Override
    public void scrollFocusedEditableNodeIntoView() {}

    @Override
    public void selectAroundCaret(
            @SelectionGranularity int granularity,
            boolean shouldShowHandle,
            boolean shouldShowContextMenu,
            int startOffset,
            int endOffset,
            int surroundingTextLength) {}

    @Override
    public void adjustSelectionByCharacterOffset(
            int startAdjust, int endAdjust, boolean showSelectionMenu) {}

    @Override
    public GURL getLastCommittedUrl() {
        return mLastCommittedUrl;
    }

    public void setLastCommittedUrl(GURL url) {
        mLastCommittedUrl = url;
    }

    @Override
    public boolean isIncognito() {
        return false;
    }

    @Override
    public void resumeLoadingCreatedWebContents() {}

    @Override
    public void evaluateJavaScript(String script, JavaScriptCallback callback) {}

    @Override
    public void evaluateJavaScriptForTests(String script, JavaScriptCallback callback) {}

    @Override
    public void addMessageToDevToolsConsole(int level, String message) {}

    @Override
    public void postMessageToMainFrame(
            MessagePayload messagePayload,
            String sourceOrigin,
            String targetOrigin,
            MessagePort[] ports) {}

    @Override
    public MessagePort[] createMessageChannel() {
        return null;
    }

    @Override
    public boolean hasAccessedInitialDocument() {
        return false;
    }

    @Override
    public boolean hasViewTransitionOptIn() {
        return false;
    }

    @Override
    public int getThemeColor() {
        return 0;
    }

    @Override
    public int getBackgroundColor() {
        return 0;
    }

    @Override
    public float getLoadProgress() {
        return 0;
    }

    @Override
    public void requestSmartClipExtract(int x, int y, int width, int height) {}

    @Override
    public void setSmartClipResultHandler(Handler smartClipHandler) {}

    @Override
    public void setStylusWritingHandler(StylusWritingHandler stylusWritingHandler) {}

    @Override
    public StylusWritingImeCallback getStylusWritingImeCallback() {
        return null;
    }

    @Override
    public EventForwarder getEventForwarder() {
        return null;
    }

    @Override
    public void addObserver(WebContentsObserver observer) {}

    @Override
    public void removeObserver(WebContentsObserver observer) {}

    @Override
    public void setOverscrollRefreshHandler(OverscrollRefreshHandler handler) {}

    @Override
    public void setSpatialNavigationDisabled(boolean disabled) {}

    @Override
    public int downloadImage(
            GURL url,
            boolean isFavicon,
            int maxBitmapSize,
            boolean bypassCache,
            ImageDownloadCallback callback) {
        return 0;
    }

    @Override
    public boolean hasActiveEffectivelyFullscreenVideo() {
        return false;
    }

    @Override
    public boolean isPictureInPictureAllowedForFullscreenVideo() {
        return false;
    }

    @Override
    public Rect getFullscreenVideoSize() {
        return null;
    }

    @Override
    public void setHasPersistentVideo(boolean value) {}

    @Override
    public void setSize(int width, int height) {}

    @Override
    public int getWidth() {
        return 0;
    }

    @Override
    public int getHeight() {
        return 0;
    }

    @Override
    public void setDisplayCutoutSafeArea(Rect insets) {}

    @Override
    public void notifyRendererPreferenceUpdate() {}

    @Override
    public void notifyBrowserControlsHeightChanged() {}

    @Override
    public void tearDownDialogOverlays() {}

    @Override
    public boolean needToFireBeforeUnloadOrUnloadEvents() {
        return false;
    }

    @Override
    public void onContentForNavigationEntryShown() {}

    @Override
    public int getCurrentBackForwardTransitionStage() {
        return AnimationStage.NONE;
    }

    @Override
    public void captureContentAsBitmapForTesting(Callback<Bitmap> callback) {}

    @Override
    public void setLongPressLinkSelectText(boolean enabled) {}

    @Override
    public void notifyControlsConstraintsChanged(
            BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
            BrowserControlsOffsetTagsInfo offsetTagsInfo) {}
}
