// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility.captioning;

import org.chromium.build.annotations.NullMarked;

/** Interface for platform dependent captioning bridges. */
@NullMarked
public interface SystemCaptioningBridge {
    /** Interface for listening to changed from SystemCaptioningBridge. */
    interface SystemCaptioningBridgeListener {
        /**
         * Called when system captioning settings change.
         *
         * @param settings The TextTrackSettings object containing the new settings.
         */
        void onSystemCaptioningChanged(TextTrackSettings settings);
    }

    /**
     * Sync the system's current captioning settings with the listener.
     *
     * @param listener The SystemCaptioningBridgeListener object to receive all settings.
     */
    void syncToListener(SystemCaptioningBridgeListener listener);

    /**
     * Add a listener for changes with the system CaptioningManager.
     *
     * @param listener The SystemCaptioningBridgeListener object to add.
     */
    void addListener(SystemCaptioningBridgeListener listener);

    /**
     * Remove a listener from system CaptionManager.
     *
     * @param listener The SystemCaptioningBridgeListener object to remove.
     */
    void removeListener(SystemCaptioningBridgeListener listener);
}
