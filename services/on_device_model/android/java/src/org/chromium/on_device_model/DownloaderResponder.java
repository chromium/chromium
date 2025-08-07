// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.chromium.build.annotations.NullMarked;

/** A responder for the backend to send download status. */
@NullMarked
public interface DownloaderResponder {
    /**
     * Called when the download has completed and the status has become available. Called at most
     * once. Not called if onUnavailable is called.
     *
     * <p>TODO(crbug.com/425408635): Return the base model name and version.
     */
    void onAvailable();

    /**
     * Called when the model is unavailable. Called at most once. Not called if onAvailable is
     * called.
     *
     * <p>TODO(crbug.com/425408635): Return the error reason.
     */
    void onUnavailable();
}
