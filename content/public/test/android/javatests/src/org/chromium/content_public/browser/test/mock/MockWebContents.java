// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.mock;

import android.annotation.SuppressLint;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Parcel;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.AccessibilitySnapshotCallback;
import org.chromium.content_public.browser.ImageDownloadCallback;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.OverscrollRefreshHandler;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * Mock class for {@link WebContents}.
 */
@SuppressLint("ParcelCreator")
public class MockWebContents implements WebContents {
    public RenderFrameHost renderFrameHost;

    @Override
    public void initialize(String productVersion, ViewAndroidDelegate viewDelegate,
            ViewEventSink.InternalAccessDelegate accessDelegate, WindowAndroid windowAndroid,
            WebContents.InternalsHolder internalsHolder) {}

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
    @Nullable
    public RenderWidgetHostView getRenderWidgetHostView() {
        return null;
    }

    @Override
    public String getTitle() {
        return null;
    }

    @Override
    public String getVisibleUrl() {
        return null;
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
    public boolean isLoadingToDifferentDocument() {
        return false;
    }

    @Override
    public void stop() {}

    @Override
    public void onHide() {}

    @Override
    public void onShow() {}

    @Override
    public void setImportance(int importance) {}

    @Override
    public void suspendAllMediaPlayers() {}

    @Override
    public void setAudioMuted(boolean mute) {}

    @Override
    public boolean isShowingInterstitialPage() {
        return false;
    }

    @Override
    public boolean focusLocationBarByDefault() {
        return false;
    }

    @Override
    public void exitFullscreen() {}

    @Override
    public void scrollFocusedEditableNodeIntoView() {}

    @Override
    public void selectWordAroundCaret() {}

    @Override
    public void adjustSelectionByCharacterOffset(
            int startAdjust, int endAdjust, boolean showSelectionMenu) {}

    @Override
    public String getLastCommittedUrl() {
        return null;
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
            String message, String sourceOrigin, String targetOrigin, MessagePort[] ports) {}

    @Override
    public MessagePort[] createMessageChannel() {
        return null;
    }

    @Override
    public boolean hasAccessedInitialDocument() {
        return false;
    }

    @Override
    public int getThemeColor() {
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
    public void requestAccessibilitySnapshot(AccessibilitySnapshotCallback callback) {}

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
    public int downloadImage(String url, boolean isFavicon, int maxBitmapSize, boolean bypassCache,
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
}
