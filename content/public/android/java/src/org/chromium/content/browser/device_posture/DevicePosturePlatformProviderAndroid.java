// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.device_posture;

import android.graphics.Rect;
import android.os.Build;

import androidx.annotation.Nullable;
import androidx.window.extensions.layout.DisplayFeature;
import androidx.window.extensions.layout.FoldingFeature;
import androidx.window.extensions.layout.WindowLayoutInfo;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.content.browser.WindowEventObserver;
import org.chromium.content.browser.WindowEventObserverManager;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.window.WindowApiCheck;

/**
 * Java implementation of DevicePosturePlatformProviderAndroid. WindowLayoutInfoManager will call
 * into this class which will inform about the new foldable specific characteristics and then call
 * into the native class to relay them to blink.
 */
@JNINamespace("content")
public class DevicePosturePlatformProviderAndroid implements WindowEventObserver {
    private long mNativeDevicePosturePlatformProvider;
    private final WebContentsImpl mWebContents;
    private WindowLayoutInfoListener mWindowLayoutInfoListener;
    private boolean mListening;

    @CalledByNative
    private static DevicePosturePlatformProviderAndroid create(
            long nativeDevicePosturePlatformProvider, WebContentsImpl webContents) {
        return new DevicePosturePlatformProviderAndroid(
                nativeDevicePosturePlatformProvider, webContents);
    }

    private DevicePosturePlatformProviderAndroid(
            long nativeDevicePosturePlatformProvider, WebContentsImpl webContents) {
        assert nativeDevicePosturePlatformProvider != 0;
        assert webContents != null;
        mNativeDevicePosturePlatformProvider = nativeDevicePosturePlatformProvider;
        mWebContents = webContents;
        WindowEventObserverManager manager = WindowEventObserverManager.from(mWebContents);
        if (manager != null) {
            manager.addObserver(this);
        }
    }

    @CalledByNative
    private void startListening() {
        if (ContentFeatureMap.isEnabled(BlinkFeatures.DEVICE_POSTURE)
                || ContentFeatureMap.isEnabled(BlinkFeatures.VIEWPORT_SEGMENTS)) {
            mListening = true;
            observeWindowLayoutListener(mWebContents.getTopLevelNativeWindow());
        }
    }

    private void observeWindowLayoutListener(@Nullable WindowAndroid window) {
        if (window == null
                || Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU
                || !ContentFeatureMap.isEnabled(BlinkFeatures.DEVICE_POSTURE)
                || !WindowApiCheck.isAvailable()) {
            return;
        }

        assert mWindowLayoutInfoListener == null;
        mWindowLayoutInfoListener =
                WindowLayoutInfoListener.getWindowLayoutListenerForWindow(window);
        if (mWindowLayoutInfoListener != null) {
            mWindowLayoutInfoListener.addObserver(this);
        }
    }

    @CalledByNative
    private void stopListening() {
        mListening = false;
        unObserveWindowLayoutListener();
    }

    @CalledByNative
    private void destroy() {
        stopListening();
        mNativeDevicePosturePlatformProvider = 0;
    }

    private void unObserveWindowLayoutListener() {
        if (mWindowLayoutInfoListener != null) {
            mWindowLayoutInfoListener.removeObserver(this);
            mWindowLayoutInfoListener = null;
        }
    }

    @Override
    public void onWindowAndroidChanged(WindowAndroid newWindowAndroid) {
        unObserveWindowLayoutListener();
        // We were listening before the change, we should listen on the new window.
        if (mListening) {
            observeWindowLayoutListener(newWindowAndroid);
        }
    }

    // At this time Chrome only supports one display feature so let's return the first one.
    private @Nullable FoldingFeature getFirstFoldingFeature(WindowLayoutInfo windowLayoutInfo) {
        if (windowLayoutInfo.getDisplayFeatures().isEmpty()) {
            return null;
        }

        for (DisplayFeature feature : windowLayoutInfo.getDisplayFeatures()) {
            if (feature instanceof FoldingFeature) {
                return (FoldingFeature) feature;
            }
        }
        return null;
    }

    public void onWindowLayoutInfoChanged(WindowLayoutInfo windowLayoutInfo) {
        if (mNativeDevicePosturePlatformProvider != 0) {
            // The display feature works as follow on Android:
            // - If the application is running on a single physical, but not foldable screen (for
            // e.g. the cover screen on a foldable device) the display feature list will be empty.
            // - If the application is running on one of the physical screen of a dual screen (not
            // spanned) the display feature list will be empty.
            // - If the application is running across the two physical screens of a dual screen
            // device (spanned) the display feature will contain its bounds and the posture.
            // - If the application is running side by side on a foldable screen, the display
            // feature list will be empty.
            // - If the application is running spanned on a foldable screen the display feature will
            // *always* have bounds set but the posture will be updated accordingly.
            FoldingFeature feature = getFirstFoldingFeature(windowLayoutInfo);
            Rect displayFeatureBounds = new Rect();
            // The display feature may have been removed so we need to notify the clients.
            if (feature == null) {
                notifyNativePlatformProvider(false, displayFeatureBounds);
                return;
            }

            boolean isFolded = feature.getState() == FoldingFeature.STATE_HALF_OPENED;
            // If the device is a dual screen and it's spanning we always need to send the bounds
            // since content could be occluded.
            // If the device is a foldable device we only need to send the bounds if the posture is
            // folded.
            if (feature.getType() == FoldingFeature.TYPE_HINGE || isFolded) {
                displayFeatureBounds = feature.getBounds();
            }

            notifyNativePlatformProvider(isFolded, displayFeatureBounds);
        }
    }

    // We need to split the JNI call into a separate method to make sure
    // R8 can inline DevicePosturePlatformProviderAndroidJni correctly. If we use
    // WindowLayoutInfo, R8 will not be able to inline the code because it's not available on all
    // devices.
    private void notifyNativePlatformProvider(boolean isFolded, Rect displayFeatureBounds) {
        assert mNativeDevicePosturePlatformProvider != 0;
        DevicePosturePlatformProviderAndroidJni.get()
                .updateDisplayFeature(
                        mNativeDevicePosturePlatformProvider,
                        isFolded,
                        displayFeatureBounds.left,
                        displayFeatureBounds.top,
                        displayFeatureBounds.right,
                        displayFeatureBounds.bottom);
    }

    @NativeMethods
    interface Natives {
        void updateDisplayFeature(
                long nativeDevicePosturePlatformProviderAndroid,
                boolean isFolded,
                int displayFeatureBoundsLeft,
                int displayFeatureBoundsTop,
                int displayFeatureBoundsRight,
                int displayFeatureBoundsBottom);
    }
}
