// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.optimization_guide.proto.ModelExecutionProto.ModelExecutionFeature;
import org.chromium.on_device_model.mojom.DownloaderParams;
import org.chromium.on_device_model.mojom.SessionParams;

/**
 * A central bridge to connect between the native and java code for on-device model. Responsible
 * for: 1) creating AiCoreSession for inference. 2) creating AiCoreModelDownloader for model
 * download.
 */
@JNINamespace("on_device_model")
@NullMarked
class OnDeviceModelBridge {
    /**
     * Creates a new AiCoreSession.
     *
     * @param feature The feature id requested this session. This is a proto enum
     *     ModelExecutionFeature.
     * @param topK The top K value for sampling.
     * @param temperature The temperature value for sampling.
     * @return The AiCoreSessionWrapper instance.
     */
    @CalledByNative
    private static AiCoreSessionWrapper createSession(int feature, int topK, float temperature) {
        ModelExecutionFeature modelExecutionFeatureId = ModelExecutionFeature.forNumber(feature);
        SessionParams params = new SessionParams();
        params.topK = topK;
        params.temperature = temperature;
        AiCoreFactory factory = ServiceLoaderUtil.maybeCreate(AiCoreFactory.class);
        AiCoreSessionBackend backend;
        if (factory == null) {
            backend = new AiCoreSessionBackendUpstreamImpl();
        } else {
            backend = factory.createSessionBackend(modelExecutionFeatureId, params);
        }
        return new AiCoreSessionWrapper(backend);
    }

    /**
     * Creates a new AiCoreModelDownloader.
     *
     * @param feature The feature id requested this downloader. This is a proto enum
     *     ModelExecutionFeature.
     * @param requirePersistentMode Whether the download is gated by persistent mode.
     * @return The AiCoreModelDownloaderWrapper instance.
     */
    @CalledByNative
    private static AiCoreModelDownloaderWrapper createModelDownloader(
            int feature, boolean requirePersistentMode) {
        ModelExecutionFeature modelExecutionFeatureId = ModelExecutionFeature.forNumber(feature);
        DownloaderParams params = new DownloaderParams();
        params.requirePersistentMode = requirePersistentMode;
        AiCoreFactory factory = ServiceLoaderUtil.maybeCreate(AiCoreFactory.class);
        AiCoreModelDownloaderBackend backend;
        if (factory == null) {
            backend = new AiCoreModelDownloaderBackendUpstreamImpl();
        } else {
            backend = factory.createModelDownloader(modelExecutionFeatureId, params);
        }
        return new AiCoreModelDownloaderWrapper(backend);
    }
}
