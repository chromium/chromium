// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;

import java.util.function.Consumer;

/**
 * Listens for changes to the SCO state, invoking the provided callback on the main thread whenever
 * SCO is enabled or disabled. The initial SCO state can be assumed to be false - if this is not the
 * case in reality, the callback will immediately be invoked with a value of true.
 *
 * <p>Note that SCO state change detection is based on the
 * AudioManager.ACTION_SCO_AUDIO_STATE_UPDATED broadcast, which does not get triggered by SCO state
 * changes originating from APIs from the android.bluetooth package. While Chrome does not use such
 * APIs, external apps may, in which case the listener will not accurately inform of SCO state
 * changes.
 */
@NullMarked
class ScoStateListener implements Destroyable {
    private static final String TAG = "ScoStateListener";

    private static final IntentFilter INTENT_FILTER =
            new IntentFilter(AudioManager.ACTION_SCO_AUDIO_STATE_UPDATED);

    private final Consumer<Boolean> mCallback;
    private final BroadcastReceiver mBroadcastReceiver;

    /**
     * The current SCO state according to the most recent broadcast. Because the broadcasts are
     * sticky, if SCO is on at the time start() is called, the BroadcastReceiver will immediately
     * receive a broadcast informing about such. Thus, this value can be initialized to false.
     */
    private boolean mState;

    ScoStateListener(Consumer<Boolean> callback) {
        mCallback = callback;

        mBroadcastReceiver =
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        onScoStateChangeBroadcast(intent);
                    }
                };
        Context context = ContextUtils.getApplicationContext();
        ContextUtils.registerProtectedBroadcastReceiver(context, mBroadcastReceiver, INTENT_FILTER);
    }

    @Override
    public void destroy() {
        // No LifetimeAssert here due to browser cleanup not being run consistently on Android.
        Context context = ContextUtils.getApplicationContext();
        context.unregisterReceiver(mBroadcastReceiver);
    }

    private void onScoStateChangeBroadcast(Intent intent) {
        int newStateIntExtra =
                intent.getIntExtra(
                        AudioManager.EXTRA_SCO_AUDIO_STATE,
                        /* defaultValue= */ AudioManager.SCO_AUDIO_STATE_ERROR);
        boolean newState;
        switch (newStateIntExtra) {
            case AudioManager.SCO_AUDIO_STATE_DISCONNECTED:
            case AudioManager.SCO_AUDIO_STATE_CONNECTING:
                newState = false;
                break;
            case AudioManager.SCO_AUDIO_STATE_CONNECTED:
                newState = true;
                break;
            case AudioManager.SCO_AUDIO_STATE_ERROR:
                Log.w(TAG, "Error occurred when getting the SCO state");
                return;
            default:
                Log.w(TAG, "Unexpected SCO state extra value: " + newStateIntExtra);
                return;
        }

        if (mState == newState) {
            return;
        }
        mState = newState;
        mCallback.accept(mState);
    }
}
