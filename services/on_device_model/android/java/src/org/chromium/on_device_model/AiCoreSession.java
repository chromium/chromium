// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** A session that uses AiCore to generate output. */
@JNINamespace("on_device_model")
@NullMarked
interface AiCoreSession {
    /**
     * Request AiCore to generate text. Response will be delivered via the native callback
     * (onResponse and onComplete).
     *
     * @param nativeBackendSession The pointer to the native BackendSession. Used to deliver the
     *     result back to the native side.
     * @param inputPieces The input pieces to generate the response. Should always be an instance of
     *     mojom::InputPiece.
     */
    @CalledByNative
    void generate(long nativeBackendSession, Object[] inputPieces);

    /**
     * Called when the native session is destroyed. The implementation class should not call native
     * functions after this is called.
     */
    @CalledByNative
    void onNativeDestroyed();

    @NativeMethods
    interface Natives {
        void onComplete(long backendSession);

        void onResponse(long backendSession, String response);
    }
}
