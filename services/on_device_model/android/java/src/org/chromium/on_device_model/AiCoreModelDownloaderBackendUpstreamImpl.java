// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.chromium.build.annotations.NullMarked;

/** A dummy implementation of AiCoreModelDownloaderBackend. Used when AiCore is not available. */
@NullMarked
class AiCoreModelDownloaderBackendUpstreamImpl implements AiCoreModelDownloaderBackend {
    @Override
    public void startDownload(DownloaderResponder responder) {
        responder.onUnavailable(DownloadFailureReason.API_NOT_AVAILABLE);
    }

    @Override
    public void onNativeDestroyed() {}
}
