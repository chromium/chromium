// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;

import java.util.function.Consumer;

/** Listens for changes to the system microphone mute state. */
@NullMarked
class MicrophoneMuteStateListener implements Destroyable {
    private static final IntentFilter INTENT_FILTER =
            new IntentFilter(AudioManager.ACTION_MICROPHONE_MUTE_CHANGED);

    private final AudioManager mAudioManager;
    private final Consumer<Boolean> mCallback;
    private final BroadcastReceiver mBroadcastReceiver;

    MicrophoneMuteStateListener(AudioManager audioManager, Consumer<Boolean> callback) {
        mAudioManager = audioManager;
        mCallback = callback;
        mBroadcastReceiver =
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        mCallback.accept(mAudioManager.isMicrophoneMute());
                    }
                };

        Context context = ContextUtils.getApplicationContext();
        ContextUtils.registerProtectedBroadcastReceiver(context, mBroadcastReceiver, INTENT_FILTER);
    }

    @Override
    public void destroy() {
        ContextUtils.getApplicationContext().unregisterReceiver(mBroadcastReceiver);
    }
}
