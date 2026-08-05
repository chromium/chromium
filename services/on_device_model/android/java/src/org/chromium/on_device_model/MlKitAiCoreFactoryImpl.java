// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import com.google.mlkit.common.MlKit;
import com.google.mlkit.genai.prompt.Generation;
import com.google.mlkit.genai.prompt.GenerationConfig;
import com.google.mlkit.genai.prompt.GenerativeModel;

import org.chromium.base.ContextUtils;
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
    // Guards one-time lazy initialization of MLKit. See ensureMlKitInitialized().
    private static final Object sInitLock = new Object();
    // volatile is required for the double-checked locking in ensureMlKitInitialized().
    private static volatile boolean sMlKitInitialized;

    @Override
    public AiCoreSessionBackend createSessionBackend(
            ModelExecutionFeature feature, SessionParams params) {
        ensureMlKitInitialized();

        // Create a GenerativeModel instance for session backend.
        GenerationConfig config = new GenerationConfig.Builder().build();
        GenerativeModel generativeModel = Generation.INSTANCE.getClient(config);

        return new MlKitAiCoreSessionBackendImpl(generativeModel, params);
    }

    @Override
    public AiCoreModelDownloaderBackend createModelDownloader(
            ModelExecutionFeature feature, DownloaderParams params) {
        ensureMlKitInitialized();

        // Create a GenerativeModel instance for model downloader.
        GenerationConfig config = new GenerationConfig.Builder().build();
        GenerativeModel generativeModel = Generation.INSTANCE.getClient(config);

        return new MlKitAiCoreModelDownloaderBackendImpl(generativeModel);
    }

    /**
     * Initializes MLKit exactly once, lazily, on first use of an AICore-backed feature.
     *
     * <p>MLKit's self-registering {@code MlKitInitProvider} ContentProvider is removed from the
     * merged manifest (see chrome/android/java/AndroidManifest.xml) so that MLKit does not run on
     * the main thread during app startup, where it regresses cold-start launch time. Because the
     * provider no longer initializes MLKit, we must do it manually before the first MLKit API call,
     * otherwise MLKit is not functional. {@link MlKit#initialize} throws if called more than once,
     * so it is guarded to run only on the first call.
     */
    private static void ensureMlKitInitialized() {
        if (sMlKitInitialized) return;
        synchronized (sInitLock) {
            if (sMlKitInitialized) return;
            MlKit.initialize(ContextUtils.getApplicationContext());
            sMlKitInitialized = true;
        }
    }
}
