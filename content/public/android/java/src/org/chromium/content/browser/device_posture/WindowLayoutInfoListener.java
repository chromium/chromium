// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.device_posture;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.os.Build;

import androidx.annotation.RequiresApi;
import androidx.window.extensions.core.util.function.Consumer;
import androidx.window.extensions.layout.WindowLayoutInfo;

import org.chromium.base.ObserverList;
import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.window.WindowUtil;

/**
 * WindowLayoutInfoListener This class listen for WindowLayoutInfo changes and inform the device
 * posture service with the values.
 */
@NullMarked
public class WindowLayoutInfoListener implements UnownedUserData {
    private static final UnownedUserDataKey<WindowLayoutInfoListener> KEY =
            new UnownedUserDataKey<>(WindowLayoutInfoListener.class);
    private final Consumer<WindowLayoutInfo> mWindowLayoutInfoChangedCallback;
    private @Nullable WindowAndroid mWindowAndroid;
    private final ObserverList<DevicePosturePlatformProviderAndroid> mObservers =
            new ObserverList<>();
    private @Nullable WindowLayoutInfo mCurrentWindowLayoutInfo;

    private WindowLayoutInfoListener(WindowAndroid window) {
        assert window != null;
        mWindowLayoutInfoChangedCallback = this::onWindowLayoutInfoChanged;
        mWindowAndroid = window;
    }

    private void onWindowLayoutInfoChanged(WindowLayoutInfo windowLayoutInfo) {
        mCurrentWindowLayoutInfo = windowLayoutInfo;
        for (DevicePosturePlatformProviderAndroid observer : mObservers) {
            observer.onWindowLayoutInfoChanged(mCurrentWindowLayoutInfo);
        }
    }

    @Override
    public void onDetachedFromHost(UnownedUserDataHost host) {
        mWindowAndroid = null;
        WindowUtil.removeWindowLayoutInfoListener(mWindowLayoutInfoChangedCallback);
    }

    public void addObserver(DevicePosturePlatformProviderAndroid observer) {
        assumeNonNull(mWindowAndroid);
        assert !mObservers.hasObserver(observer);
        Context context = mWindowAndroid.getContext().get();
        if (mObservers.isEmpty() && context != null) {
            WindowUtil.addWindowLayoutInfoListener(context, mWindowLayoutInfoChangedCallback);
        }

        mObservers.addObserver(observer);
        if (mCurrentWindowLayoutInfo != null) {
            // Notify the new observer right away with the current state.
            observer.onWindowLayoutInfoChanged(mCurrentWindowLayoutInfo);
        }
    }

    public void removeObserver(DevicePosturePlatformProviderAndroid observer) {
        assert mObservers.hasObserver(observer);
        mObservers.removeObserver(observer);
        if (mObservers.isEmpty()) {
            WindowUtil.removeWindowLayoutInfoListener(mWindowLayoutInfoChangedCallback);
            mCurrentWindowLayoutInfo = null;
        }
    }

    /**
     * Attach an instance of WindowLayoutInfoListener to the activity's window user data and return
     * it.
     */
    @RequiresApi(Build.VERSION_CODES.S) // For isUiContext()
    static @Nullable WindowLayoutInfoListener getWindowLayoutListenerForWindow(
            WindowAndroid window) {
        Context context = window.getContext().get();
        if (context == null || !context.isUiContext()) {
            return null;
        }
        WindowLayoutInfoListener listener =
                KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
        if (listener == null) {
            listener = new WindowLayoutInfoListener(window);
            KEY.attachToHost(window.getUnownedUserDataHost(), listener);
        }
        return listener;
    }
}
