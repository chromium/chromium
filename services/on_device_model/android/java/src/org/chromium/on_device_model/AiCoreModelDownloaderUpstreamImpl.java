// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.chromium.build.annotations.NullMarked;

/** A dummy implementation of AiCoreModelDownloader. Used when AiCore is not available. */
@NullMarked
class AiCoreModelDownloaderUpstreamImpl implements AiCoreModelDownloader {
    @Override
    public void startDownload(long nativeModelDownloaderAndroid) {
        AiCoreModelDownloaderJni.get().onUnavailable(nativeModelDownloaderAndroid);
    }

    @Override
    public void onNativeDestroyed() {}
}
