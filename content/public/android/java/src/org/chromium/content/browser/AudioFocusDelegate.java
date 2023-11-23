// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

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
    private AudioFocusRequest mFocusRequest;

    // Native pointer to C++ content::AudioFocusDelegateAndroid.
    // It will be set to 0 when the native AudioFocusDelegateAndroid object is destroyed.
    private long mNativeAudioFocusDelegateAndroid;

    // Handle to the UI thread to ensure callbacks are on the correct thread.
    private final Handler mHandler;

    private AudioFocusDelegate(long nativeAudioFocusDelegateAndroid) {
        mNativeAudioFocusDelegateAndroid = nativeAudioFocusDelegateAndroid;
        mHandler = new Handler(ThreadUtils.getUiThreadLooper());
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
        mFocusType =
                transientFocus
                        ? AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK
                        : AudioManager.AUDIOFOCUS_GAIN;
        return requestAudioFocusInternal();
    }

    @CalledByNative
    private void abandonAudioFocus() {
        assert ThreadUtils.runningOnUiThread();
        AudioManager am =
                (AudioManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.AUDIO_SERVICE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            if (mFocusRequest != null) {
                am.abandonAudioFocusRequest(mFocusRequest);
                mFocusRequest = null;
            }
        } else {
            am.abandonAudioFocus(this);
        }
    }

    @CalledByNative
    private boolean isFocusTransient() {
        return mFocusType == AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK;
    }

    private boolean requestAudioFocusInternal() {
        AudioManager am =
                (AudioManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.AUDIO_SERVICE);

        int result;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            AudioAttributes playbackAttributes =
                    new AudioAttributes.Builder()
                            .setUsage(AudioAttributes.USAGE_MEDIA)
                            .setContentType(AudioAttributes.CONTENT_TYPE_UNKNOWN)
                            .build();
            mFocusRequest =
                    new AudioFocusRequest.Builder(mFocusType)
                            .setAudioAttributes(playbackAttributes)
                            .setAcceptsDelayedFocusGain(false)
                            .setWillPauseWhenDucked(false)
                            .setOnAudioFocusChangeListener(this, mHandler)
                            .build();
            result = am.requestAudioFocus(mFocusRequest);
        } else {
            result = am.requestAudioFocus(this, AudioManager.STREAM_MUSIC, mFocusType);
        }

        return result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
    }

    @Override
    public void onAudioFocusChange(int focusChange) {
        assert ThreadUtils.runningOnUiThread();
        if (mNativeAudioFocusDelegateAndroid == 0) return;

        switch (focusChange) {
            case AudioManager.AUDIOFOCUS_GAIN:
                if (mIsDucking) {
                    AudioFocusDelegateJni.get()
                            .onStopDucking(
                                    mNativeAudioFocusDelegateAndroid, AudioFocusDelegate.this);
                    mIsDucking = false;
                } else {
                    AudioFocusDelegateJni.get()
                            .onResume(mNativeAudioFocusDelegateAndroid, AudioFocusDelegate.this);
                }
                break;
            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                AudioFocusDelegateJni.get()
                        .onSuspend(mNativeAudioFocusDelegateAndroid, AudioFocusDelegate.this);
                break;
            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                mIsDucking = true;
                AudioFocusDelegateJni.get()
                        .recordSessionDuck(
                                mNativeAudioFocusDelegateAndroid, AudioFocusDelegate.this);
                AudioFocusDelegateJni.get()
                        .onStartDucking(mNativeAudioFocusDelegateAndroid, AudioFocusDelegate.this);
                break;
            case AudioManager.AUDIOFOCUS_LOSS:
                abandonAudioFocus();
                AudioFocusDelegateJni.get()
                        .onSuspend(mNativeAudioFocusDelegateAndroid, AudioFocusDelegate.this);
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
