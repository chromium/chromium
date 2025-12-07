// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.optimization_guide.proto.ModelExecutionProto.ModelExecutionFeature;
import org.chromium.on_device_model.mojom.DownloaderParams;
import org.chromium.on_device_model.mojom.SessionParams;

/**
 * A factory to create AiCoreSessionBackend and AiCoreModelDownloaderBackend. This is null when the
 * AiCore API is not available. Downstream code may provide a different factory via the @ServiceImpl
 * annotation.
 */
@NullMarked
public interface AiCoreFactory {
    AiCoreSessionBackend createSessionBackend(ModelExecutionFeature feature, SessionParams params);

    AiCoreModelDownloaderBackend createModelDownloader(
            ModelExecutionFeature feature, DownloaderParams params);
}
