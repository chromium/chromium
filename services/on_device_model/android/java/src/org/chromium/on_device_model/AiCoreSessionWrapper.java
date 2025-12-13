// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.on_device_model.mojom.GenerateOptions;
import org.chromium.on_device_model.mojom.InputPiece;

/** A wrapper of {@link AiCoreSessionBackend} that is called from native. */
@JNINamespace("on_device_model")
@NullMarked
class AiCoreSessionWrapper {
    private final AiCoreSessionBackend mBackend;
    private final JniSafeCallback mJniSafeCallback = new JniSafeCallback();

    public AiCoreSessionWrapper(AiCoreSessionBackend backend) {
        mBackend = backend;
    }

    @CalledByNative
    public void generate(long nativeBackendSession, Object generateOptions, Object[] inputPieces) {
        final GenerateOptions castedGenerateOptions = (GenerateOptions) generateOptions;
        final InputPiece[] castedInputPieces = new InputPiece[inputPieces.length];
        for (int i = 0; i < inputPieces.length; i++) {
            castedInputPieces[i] = (InputPiece) inputPieces[i];
        }
        SessionResponder responder =
                new SessionResponder() {
                    @Override
                    public void onResponse(String response) {
                        mJniSafeCallback.run(
                                () -> {
                                    AiCoreSessionWrapperJni.get()
                                            .onResponse(nativeBackendSession, response);
                                });
                    }

                    @Override
                    public void onComplete(@GenerateResult int result) {
                        mJniSafeCallback.run(
                                () -> {
                                    AiCoreSessionWrapperJni.get()
                                            .onComplete(nativeBackendSession, result);
                                });
                    }
                };
        mBackend.generate(castedGenerateOptions, castedInputPieces, responder);
    }

    @CalledByNative
    public void onNativeDestroyed() {
        mJniSafeCallback.onNativeDestroyed(() -> mBackend.onNativeDestroyed());
    }

    @NativeMethods
    interface Natives {
        void onResponse(long backendSession, String response);

        void onComplete(long backendSession, @GenerateResult int result);
    }
}
