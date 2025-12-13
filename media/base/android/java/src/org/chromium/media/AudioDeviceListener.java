// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.Context;
import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;

import java.util.function.Consumer;

/**
 * Listens for changes to the list of audio devices exposed by the OS, invoking the provided
 * callback on the main thread whenever a device is added or removed. The boolean parameter of the
 * callback is @{code true} if the invocation is caused by devices being added, and @{code false} if
 * it is caused by devices being removed.
 */
@NullMarked
class AudioDeviceListener implements Destroyable {
    private final AudioManager mAudioManager;

    private final Consumer<Boolean> mCallback;
    private final AudioDeviceCallback mInternalCallback;

    AudioDeviceListener(Consumer<Boolean> callback) {
        mAudioManager =
                (AudioManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.AUDIO_SERVICE);

        mCallback = callback;
        mInternalCallback =
                new AudioDeviceCallback() {
                    @Override
                    public void onAudioDevicesAdded(AudioDeviceInfo[] addedDevices) {
                        mCallback.accept(true);
                    }

                    @Override
                    public void onAudioDevicesRemoved(AudioDeviceInfo[] removedDevices) {
                        mCallback.accept(false);
                    }
                };
        mAudioManager.registerAudioDeviceCallback(
                mInternalCallback, null); // Use the main thread's Looper.
    }

    @Override
    public void destroy() {
        // No LifetimeAssert here due to browser cleanup not being run consistently on Android.
        mAudioManager.unregisterAudioDeviceCallback(mInternalCallback);
    }
}
