// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.MediaSessionObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.services.media_session.MediaImage;
import org.chromium.services.media_session.MediaMetadata;
import org.chromium.services.media_session.MediaPosition;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/**
 * The MediaSessionImpl Java wrapper to allow communicating with the native MediaSessionImpl object.
 * The object is owned by Java WebContentsImpl instead of native to avoid introducing a new garbage
 * collection root.
 */
@JNINamespace("content")
public class MediaSessionImpl extends MediaSession {
    private long mNativeMediaSessionAndroid;

    private ObserverList<MediaSessionObserver> mObservers;
    private ObserverList.RewindableIterator<MediaSessionObserver> mObserversIterator;

    private boolean mIsControllable;
    private Boolean mIsSuspended;
    private MediaMetadata mMetadata;
    private List<MediaImage> mImagesList;
    private HashSet<Integer> mActionSet;
    private MediaPosition mPosition;

    public static MediaSessionImpl fromWebContents(WebContents webContents) {
        return MediaSessionImplJni.get().getMediaSessionFromWebContents(webContents);
    }

    public void addObserver(MediaSessionObserver observer) {
        mObservers.addObserver(observer);
        if (mIsSuspended != null) {
            observer.mediaSessionStateChanged(mIsControllable, mIsSuspended);
        }
        if (mMetadata != null) {
            observer.mediaSessionMetadataChanged(mMetadata);
        }
        if (mImagesList != null) {
            observer.mediaSessionArtworkChanged(mImagesList);
        }
        if (mPosition != null) {
            observer.mediaSessionPositionChanged(mPosition);
        }
        if (mActionSet != null) {
            observer.mediaSessionActionsChanged(mActionSet);
        }
    }

    public void removeObserver(MediaSessionObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public ObserverList.RewindableIterator<MediaSessionObserver> getObserversForTesting() {
        return mObservers.rewindableIterator();
    }

    @Override
    public void resume() {
        MediaSessionImplJni.get().resume(mNativeMediaSessionAndroid, MediaSessionImpl.this);
    }

    @Override
    public void suspend() {
        MediaSessionImplJni.get().suspend(mNativeMediaSessionAndroid, MediaSessionImpl.this);
    }

    @Override
    public void stop() {
        MediaSessionImplJni.get().stop(mNativeMediaSessionAndroid, MediaSessionImpl.this);
    }

    @Override
    public void seek(long millis) {
        assert millis != 0 : "Attempted to seek by an unspecified number of milliseconds";
        MediaSessionImplJni.get().seek(mNativeMediaSessionAndroid, MediaSessionImpl.this, millis);
    }

    @Override
    public void seekTo(long millis) {
        assert millis >= 0 : "Attempted to seek to a negative posision";
        MediaSessionImplJni.get().seekTo(mNativeMediaSessionAndroid, MediaSessionImpl.this, millis);
    }

    @Override
    public void didReceiveAction(int action) {
        MediaSessionImplJni.get()
                .didReceiveAction(mNativeMediaSessionAndroid, MediaSessionImpl.this, action);
    }

    @Override
    public void requestSystemAudioFocus() {
        MediaSessionImplJni.get()
                .requestSystemAudioFocus(mNativeMediaSessionAndroid, MediaSessionImpl.this);
    }

    @Override
    public boolean isControllable() {
        return mIsControllable;
    }

    @CalledByNative
    private boolean hasObservers() {
        return !mObservers.isEmpty();
    }

    @CalledByNative
    private void mediaSessionDestroyed() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext(); ) {
            mObserversIterator.next().mediaSessionDestroyed();
        }
        for (mObserversIterator.rewind(); mObserversIterator.hasNext(); ) {
            mObserversIterator.next().stopObserving();
        }
        mObservers.clear();
        mNativeMediaSessionAndroid = 0;
    }

    @CalledByNative
    private void mediaSessionStateChanged(boolean isControllable, boolean isSuspended) {
        mIsControllable = isControllable;
        mIsSuspended = isSuspended;

        for (mObserversIterator.rewind(); mObserversIterator.hasNext(); ) {
            mObserversIterator.next().mediaSessionStateChanged(isControllable, isSuspended);
        }
    }

    @CalledByNative
    private void mediaSessionMetadataChanged(MediaMetadata metadata) {
        mMetadata = metadata;
        for (mObserversIterator.rewind(); mObserversIterator.hasNext(); ) {
            mObserversIterator.next().mediaSessionMetadataChanged(metadata);
        }
    }

    @CalledByNative
    private void mediaSessionActionsChanged(int[] actions) {
        HashSet<Integer> actionSet = new HashSet<Integer>();
        for (int action : actions) actionSet.add(action);
        mActionSet = actionSet;

        for (mObserversIterator.rewind(); mObserversIterator.hasNext(); ) {
            mObserversIterator.next().mediaSessionActionsChanged(actionSet);
        }
    }

    @CalledByNative
    private void mediaSessionArtworkChanged(MediaImage[] images) {
        mImagesList = Arrays.asList(images);

        for (mObserversIterator.rewind(); mObserversIterator.hasNext(); ) {
            mObserversIterator.next().mediaSessionArtworkChanged(mImagesList);
        }
    }

    @CalledByNative
    private void mediaSessionPositionChanged(@Nullable MediaPosition position) {
        mPosition = position;
        for (mObserversIterator.rewind(); mObserversIterator.hasNext(); ) {
            mObserversIterator.next().mediaSessionPositionChanged(position);
        }
    }

    @CalledByNative
    private static MediaSessionImpl create(long nativeMediaSession) {
        return new MediaSessionImpl(nativeMediaSession);
    }

    private MediaSessionImpl(long nativeMediaSession) {
        mNativeMediaSessionAndroid = nativeMediaSession;
        mObservers = new ObserverList<MediaSessionObserver>();
        mObserversIterator = mObservers.rewindableIterator();
    }

    @NativeMethods
    interface Natives {
        void resume(long nativeMediaSessionAndroid, MediaSessionImpl caller);

        void suspend(long nativeMediaSessionAndroid, MediaSessionImpl caller);

        void stop(long nativeMediaSessionAndroid, MediaSessionImpl caller);

        void seek(long nativeMediaSessionAndroid, MediaSessionImpl caller, long millis);

        void seekTo(long nativeMediaSessionAndroid, MediaSessionImpl caller, long millis);

        void didReceiveAction(long nativeMediaSessionAndroid, MediaSessionImpl caller, int action);

        void requestSystemAudioFocus(long nativeMediaSessionAndroid, MediaSessionImpl caller);

        MediaSessionImpl getMediaSessionFromWebContents(WebContents contents);
    }
}
