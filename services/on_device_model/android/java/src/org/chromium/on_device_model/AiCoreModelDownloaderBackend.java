// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.on_device_model;

import org.chromium.build.annotations.NullMarked;

/**
 * The backend interface for an on-device model downloader. Request AiCore to download models
 * required for a feature.
 */
@NullMarked
public interface AiCoreModelDownloaderBackend {
    /**
     * Request AiCore to start downloading the model. Note that once the download starts, there is
     * no way to cancel it even if the native side is destroyed.
     *
     * @param responder The responder to send back the download status.
     */
    void startDownload(DownloaderResponder responder);

    /**
     * Called when the native downloader is destroyed. The implementation class should not call
     * responder after this is called.
     */
    void onNativeDestroyed();
}
