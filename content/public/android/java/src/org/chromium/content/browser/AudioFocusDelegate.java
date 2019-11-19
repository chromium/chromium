// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.media.AudioManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * AudioFocusDelegate is the Java counterpart of content::AudioFocusDelegateAndroid.
 * It is being used to communicate from content::AudioFocusDelegateAndroid
 * (C++) to the Android system. A AudioFocusDelegate is implementingf
 * OnAudioFocusChangeListener, making it an audio focus holder for Android. Thus
 * two instances of AudioFocusDelegate can't have audio focus at the same
 * time. A AudioFocusDelegate will use the type requested from its C++
 * counterpart and will resume its play using the same type if it were to
 * happen, for example, when it got temporarily suspended by a transient sound
 * like a notification.
 */
@JNINamespace("content")
public class AudioFocusDelegate implements AudioManager.OnAudioFocusChangeListener {
    private static final String TAG = "MediaSession";

    private int mFocusType;
    private boolean mIsDucking;

    // Native pointer to C++ content::AudioFocusDelegateAndroid.
    // It will be set to 0 when the native AudioFocusDelegateAndroid object is destroyed.
    private long mNativeAudioFocusDelegateAndroid;

    private AudioFocusDelegate(long nativeAudioFocusDelegateAndroid) {
        mNativeAudioFocusDelegateAndroid = nativeAudioFocusDelegateAndroid;
    }

    @CalledByNative
    private static AudioFocusDelegate create(long nativeAudioFocusDelegateAndroid) {
        return new AudioFocusDelegate(nativeAudioFocusDelegateAndroid);
    }

    @CalledByNative
    private void tearDown() {
        assert ThreadUtils.runningOnUiThread();
        abandonAudioFocus();
        mNativeAudioFocusDelegateAndroid = 0;
    }

    @CalledByNative
    private boolean requestAudioFocus(boolean transientFocus) {
        assert ThreadUtils.runningOnUiThread();
        mFocusType = transientFocus ? AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK
                                    : AudioManager.AUDIOFOCUS_GAIN;
        return requestAudioFocusInternal();
    }

    @CalledByNative
    private void abandonAudioFocus() {
        assert ThreadUtils.runningOnUiThread();
        AudioManager am = (AudioManager) ContextUtils.getApplicationContext().getSystemService(
                Context.AUDIO_SERVICE);
        am.abandonAudioFocus(this);
    }

    @CalledByNative
    private boolean isFocusTransient() {
        return mFocusType == AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK;
    }

    private boolean requestAudioFocusInternal() {
        AudioManager am = (AudioManager) ContextUtils.getApplicationContext().getSystemService(
                Context.AUDIO_SERVICE);

        int result = am.requestAudioFocus(this, AudioManager.STREAM_MUSIC, mFocusType);
        return result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
    }

    @Override
    public void onAudioFocusChange(int focusChange) {
        assert ThreadUtils.runningOnUiThread();
        if (mNativeAudioFocusDelegateAndroid == 0) return;

        switch (focusChange) {
            case AudioManager.AUDIOFOCUS_GAIN:
                if (mIsDucking) {
                    AudioFocusDelegateJni.get().onStopDucking(
                            mNativeAudioFocusDelegateAndroid, AudioFocusDelegate.this);
                    mIsDucking = false;
                } else {
                    AudioFocusDelegateJni.get().onResume(
                            mNativeAudioFocusDelegateAndroid, AudioFocusDelegate.this);
                }
                break;
            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                AudioFocusDelegateJni.get().onSuspend(
                        mNativeAudioFocusDelegateAndroid, AudioFocusDelegate.this);
                break;
            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                mIsDucking = true;
                AudioFocusDelegateJni.get().recordSessionDuck(
                        mNativeAudioFocusDelegateAndroid, AudioFocusDelegate.this);
                AudioFocusDelegateJni.get().onStartDucking(
                        mNativeAudioFocusDelegateAndroid, AudioFocusDelegate.this);
                break;
            case AudioManager.AUDIOFOCUS_LOSS:
                abandonAudioFocus();
                AudioFocusDelegateJni.get().onSuspend(
                        mNativeAudioFocusDelegateAndroid, AudioFocusDelegate.this);
                break;
            default:
                Log.w(TAG, "onAudioFocusChange called with unexpected value %d", focusChange);
                break;
        }
    }

    @NativeMethods
    interface Natives {
        void onSuspend(long nativeAudioFocusDelegateAndroid, AudioFocusDelegate caller);
        void onResume(long nativeAudioFocusDelegateAndroid, AudioFocusDelegate caller);
        void onStartDucking(long nativeAudioFocusDelegateAndroid, AudioFocusDelegate caller);
        void onStopDucking(long nativeAudioFocusDelegateAndroid, AudioFocusDelegate caller);
        void recordSessionDuck(long nativeAudioFocusDelegateAndroid, AudioFocusDelegate caller);
    }
}
