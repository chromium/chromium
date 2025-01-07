// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.content.browser.MediaSessionImpl;

/** The MediaSession Java wrapper to allow communicating with the native MediaSession object. */
public abstract class MediaSession {
    /**
     * @return The MediaSession associated with |contents|.
     */
    @Nullable
    public static MediaSession fromWebContents(WebContents contents) {
        // TODO(zqzhang): directly call WebContentsImpl.getMediaSession() when WebContentsImpl
        // package restriction is removed.
        return MediaSessionImpl.fromWebContents(contents);
    }

    /**
     * @return The list of observers.
     */
    public abstract ObserverList.RewindableIterator<MediaSessionObserver> getObserversForTesting();

    /** Resumes the media session. */
    public abstract void resume();

    /** Suspends the media session. */
    public abstract void suspend();

    /** Stops the media session. */
    public abstract void stop();

    /**
     * Seeks the media session by the specified number of milliseconds. The
     * number of millseconds can be positive or negative to seek fowards or
     * backwards. It should not be zero.
     */
    public abstract void seek(long millis);

    /**
     * Seeks the media session to a specific point relative from the beginning
     * of the media. The number of milliseconds should not be negative.
     */
    public abstract void seekTo(long millis);

    /** Notify the media session that an action has been performed. */
    public abstract void didReceiveAction(int action);

    /** Request audio focus from the system. */
    public abstract void requestSystemAudioFocus();

    /** Returns whether the media session can be resumed/suspended. */
    public abstract boolean isControllable();
}
