// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility.captioning;

import static org.chromium.build.NullUtil.assertNonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;

/** Sends notification when platform closed caption settings have changed. */
@JNINamespace("content")
@NullMarked
public class CaptioningController
        implements SystemCaptioningBridge.SystemCaptioningBridgeListener, UserData {
    private static final Class<CaptioningController> USER_DATA_KEY = CaptioningController.class;

    private final SystemCaptioningBridge mSystemCaptioningBridge;
    private long mNativeCaptioningController;

    /** Returns the CaptioningController for the given WebContents. */
    public static CaptioningController fromWebContents(WebContents webContents) {
        return assertNonNull(
                webContents.getOrSetUserData(USER_DATA_KEY, CaptioningController::new));
    }

    @CalledByNative
    private static @Nullable CaptioningController getFromWebContents(
            @JniType("content::WebContents*") WebContents webContents) {
        if (webContents == null) return null;
        return webContents.getOrSetUserData(USER_DATA_KEY, /* userDataFactory= */ null);
    }

    private CaptioningController(WebContents webContents) {
        mSystemCaptioningBridge = CaptioningBridge.getInstance();
        mNativeCaptioningController = CaptioningControllerJni.get().init(webContents);
    }

    @Override
    public void destroy() {
        if (mNativeCaptioningController != 0) {
            CaptioningControllerJni.get().destroy(mNativeCaptioningController);
            mNativeCaptioningController = 0;
        }
    }

    @CalledByNative
    private void onRenderProcessChange() {
        // Immediately sync closed caption settings to the new render process.
        mSystemCaptioningBridge.syncToListener(this);
    }

    @Override
    public void onSystemCaptioningChanged(TextTrackSettings settings) {
        if (mNativeCaptioningController == 0) return;
        CaptioningControllerJni.get()
                .setTextTrackSettings(
                        mNativeCaptioningController,
                        settings.getTextTracksEnabled(),
                        settings.getTextTrackBackgroundColor(),
                        settings.getTextTrackFontFamily(),
                        settings.getTextTrackFontStyle(),
                        settings.getTextTrackFontVariant(),
                        settings.getTextTrackTextColor(),
                        settings.getTextTrackTextShadow(),
                        settings.getTextTrackTextSize());
    }

    public void startListening() {
        mSystemCaptioningBridge.addListener(this);
    }

    public void stopListening() {
        mSystemCaptioningBridge.removeListener(this);
    }

    @NativeMethods
    interface Natives {
        long init(@JniType("content::WebContents*") WebContents webContents);

        void destroy(long nativeCaptioningController);

        void setTextTrackSettings(
                long nativeCaptioningController,
                boolean textTracksEnabled,
                String textTrackBackgroundColor,
                String textTrackFontFamily,
                String textTrackFontStyle,
                String textTrackFontVariant,
                String textTrackTextColor,
                String textTrackTextShadow,
                String textTrackTextSize);
    }
}
