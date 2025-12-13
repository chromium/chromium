// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.chromium.build.annotations.NullMarked;

/**
 * A responder for the backend to send download status. The functions can be called from any thread.
 */
@NullMarked
public interface DownloaderResponder {
    /**
     * Called when the download has completed and the status has become available. Called at most
     * once. Not called if onUnavailable is called.
     */
    void onAvailable(String baseModelName, String baseModelVersion);

    /**
     * Called when the model is unavailable. Called at most once. Not called if onAvailable is
     * called.
     */
    void onUnavailable(@DownloadFailureReason int downloadFailureReason);
}
