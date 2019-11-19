// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaPlayer;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

// This class implements all the listener interface for android mediaplayer.
// Callbacks will be sent to the native class for processing.
@JNINamespace("media")
class MediaPlayerListener implements MediaPlayer.OnPreparedListener,
        MediaPlayer.OnCompletionListener,
        MediaPlayer.OnVideoSizeChangedListener,
        MediaPlayer.OnErrorListener {
    // These values are mirrored as enums in media/base/android/media_player_android.h.
    // Please ensure they stay in sync.
    private static final int MEDIA_ERROR_FORMAT = 0;
    private static final int MEDIA_ERROR_DECODE = 1;
    private static final int MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK = 2;
    private static final int MEDIA_ERROR_INVALID_CODE = 3;
    private static final int MEDIA_ERROR_SERVER_DIED = 4;

    // These values are copied from android media player.
    public static final int MEDIA_ERROR_MALFORMED = -1007;
    public static final int MEDIA_ERROR_TIMED_OUT = -110;

    // Used to determine the class instance to dispatch the native call to.
    private long mNativeMediaPlayerListener;

    private MediaPlayerListener(long nativeMediaPlayerListener) {
        mNativeMediaPlayerListener = nativeMediaPlayerListener;
    }

    @Override
    public boolean onError(MediaPlayer mp, int what, int extra) {
        int errorType;
        switch (what) {
            case MediaPlayer.MEDIA_ERROR_UNKNOWN:
                switch (extra) {
                    case MEDIA_ERROR_MALFORMED:
                        errorType = MEDIA_ERROR_DECODE;
                        break;
                    case MEDIA_ERROR_TIMED_OUT:
                        errorType = MEDIA_ERROR_INVALID_CODE;
                        break;
                    default:
                        errorType = MEDIA_ERROR_FORMAT;
                        break;
                }
                break;
            case MediaPlayer.MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK:
                errorType = MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK;
                break;
            case MediaPlayer.MEDIA_ERROR_SERVER_DIED:
                errorType = MEDIA_ERROR_SERVER_DIED;
                break;
            default:
                // There are some undocumented error codes for android media player.
                // For example, when surfaceTexture got deleted before we setVideoSuface
                // to NULL, mediaplayer will report error -38. These errors should be ignored
                // and not be treated as an error to webkit.
                errorType = MEDIA_ERROR_INVALID_CODE;
                break;
        }
        MediaPlayerListenerJni.get().onMediaError(
                mNativeMediaPlayerListener, MediaPlayerListener.this, errorType);
        return true;
    }

    @Override
    public void onVideoSizeChanged(MediaPlayer mp, int width, int height) {
        MediaPlayerListenerJni.get().onVideoSizeChanged(
                mNativeMediaPlayerListener, MediaPlayerListener.this, width, height);
    }

    @Override
    public void onCompletion(MediaPlayer mp) {
        MediaPlayerListenerJni.get().onPlaybackComplete(
                mNativeMediaPlayerListener, MediaPlayerListener.this);
    }

    @Override
    public void onPrepared(MediaPlayer mp) {
        MediaPlayerListenerJni.get().onMediaPrepared(
                mNativeMediaPlayerListener, MediaPlayerListener.this);
    }

    @CalledByNative
    private static MediaPlayerListener create(
            long nativeMediaPlayerListener, MediaPlayerBridge mediaPlayerBridge) {
        final MediaPlayerListener listener = new MediaPlayerListener(nativeMediaPlayerListener);
        if (mediaPlayerBridge != null) {
            mediaPlayerBridge.setOnCompletionListener(listener);
            mediaPlayerBridge.setOnErrorListener(listener);
            mediaPlayerBridge.setOnPreparedListener(listener);
            mediaPlayerBridge.setOnVideoSizeChangedListener(listener);
        }
        return listener;
    }

    /**
     * See media/base/android/media_player_listener.cc for all the following functions.
     */

    @NativeMethods
    interface Natives {
        void onMediaError(
                long nativeMediaPlayerListener, MediaPlayerListener caller, int errorType);
        void onVideoSizeChanged(
                long nativeMediaPlayerListener, MediaPlayerListener caller, int width, int height);
        void onMediaPrepared(long nativeMediaPlayerListener, MediaPlayerListener caller);
        void onPlaybackComplete(long nativeMediaPlayerListener, MediaPlayerListener caller);
    }
}
