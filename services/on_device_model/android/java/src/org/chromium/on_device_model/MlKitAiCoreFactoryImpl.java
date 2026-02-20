// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import com.google.mlkit.genai.prompt.Generation;
import com.google.mlkit.genai.prompt.GenerationConfig;
import com.google.mlkit.genai.prompt.GenerativeModel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.components.optimization_guide.proto.ModelExecutionProto.ModelExecutionFeature;
import org.chromium.on_device_model.mojom.DownloaderParams;
import org.chromium.on_device_model.mojom.SessionParams;

/**
 * Factory implementation for creating session and model downloader backends using MLKit Prompt
 * APIs. This implementation uses Google MLKit's GenerativeModel for on-device AI.
 */
@NullMarked
@ServiceImpl(AiCoreFactory.class)
public class MlKitAiCoreFactoryImpl implements AiCoreFactory {
    @Override
    public AiCoreSessionBackend createSessionBackend(
            ModelExecutionFeature feature, SessionParams params) {
        // Create a GenerativeModel instance for session backend.
        GenerationConfig config = new GenerationConfig.Builder().build();
        GenerativeModel generativeModel = Generation.INSTANCE.getClient(config);

        return new MlKitAiCoreSessionBackendImpl(generativeModel, params);
    }

    @Override
    public AiCoreModelDownloaderBackend createModelDownloader(
            ModelExecutionFeature feature, DownloaderParams params) {
        // Create a GenerativeModel instance for model downloader.
        GenerationConfig config = new GenerationConfig.Builder().build();
        GenerativeModel generativeModel = Generation.INSTANCE.getClient(config);

        return new MlKitAiCoreModelDownloaderBackendImpl(generativeModel);
    }
}
