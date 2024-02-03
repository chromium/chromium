// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.device_posture;

import androidx.annotation.Nullable;
import androidx.window.extensions.layout.DisplayFeature;
import androidx.window.extensions.layout.FoldingFeature;
import androidx.window.extensions.layout.WindowLayoutInfo;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content.browser.WindowEventObserver;
import org.chromium.content.browser.WindowEventObserverManager;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.WindowAndroid;

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
        if (ContentFeatureMap.isEnabled(ContentFeatures.DEVICE_POSTURE)) {
            mListening = true;
            observeWindowLayoutListener(mWebContents.getTopLevelNativeWindow());
        }
    }

    private void observeWindowLayoutListener(@Nullable WindowAndroid window) {
        if (window == null) {
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

    private boolean isWindowLayoutFolded(WindowLayoutInfo windowLayoutInfo) {
        for (DisplayFeature f : windowLayoutInfo.getDisplayFeatures()) {
            if (f instanceof FoldingFeature) {
                return ((FoldingFeature) f).getState() == FoldingFeature.STATE_HALF_OPENED;
            }
        }
        return false;
    }

    public void onWindowLayoutInfoChanged(WindowLayoutInfo windowLayoutInfo) {
        if (mNativeDevicePosturePlatformProvider != 0) {
            notifyNativePlatformProvider(isWindowLayoutFolded(windowLayoutInfo));
        }
    }

    // We need to split the JNI call into a separate method to make sure
    // R8 can inline DevicePosturePlatformProviderAndroidJni correctly. If we use
    // WindowLayoutInfo, R8 will not be able to inline the code because it's not available on all
    // devices.
    private void notifyNativePlatformProvider(boolean isFolded) {
        assert mNativeDevicePosturePlatformProvider != 0;
        DevicePosturePlatformProviderAndroidJni.get()
                .setDeviceFolded(mNativeDevicePosturePlatformProvider, isFolded);
    }

    @NativeMethods
    interface Natives {
        void setDeviceFolded(long nativeDevicePosturePlatformProviderAndroid, boolean isFolded);
    }
}
