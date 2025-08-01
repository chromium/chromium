// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.optimization_guide.proto.ModelExecutionProto.ModelExecutionFeature;
import org.chromium.on_device_model.mojom.SessionParams;

/**
 * A central bridge to connect between the native and java code for on-device model. Responsible
 * for: 1) creating and managing the AiCoreSession on java side. 2) forwarding the request from the
 * native side to the AiCoreSession. 3) forwarding the response from the AiCoreSession to the native
 * side.
 */
@JNINamespace("on_device_model")
@NullMarked
class OnDeviceModelBridge {
    /**
     * Creates a new AiCoreSession.
     *
     * @param feature The feature id requested this session.
     * @param topK The top K value for sampling.
     * @param temperature The temperature value for sampling.
     * @return The AiCoreSession instance.
     */
    @CalledByNative
    private static AiCoreSession createSession(int feature, int topK, float temperature) {
        ModelExecutionFeature modelExecutionFeatureId = ModelExecutionFeature.forNumber(feature);
        SessionParams params = new SessionParams();
        params.topK = topK;
        params.temperature = temperature;
        AiCoreSessionFactory factory = ServiceLoaderUtil.maybeCreate(AiCoreSessionFactory.class);
        if (factory == null) {
            return new AiCoreSessionUpstreamImpl();
        }
        return factory.createSession(modelExecutionFeatureId, params);
    }
}
