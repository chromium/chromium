// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.impl.CoreImpl;
import org.chromium.shape_detection.mojom.BarcodeDetectionProvider;
import org.chromium.shape_detection.mojom.FaceDetectionProvider;
import org.chromium.shape_detection.mojom.TextDetection;

@JNINamespace("shape_detection")
class InterfaceRegistrar {
    static MessagePipeHandle messagePipeHandleFromNative(long nativeHandle) {
        return CoreImpl.getInstance().acquireNativeHandle(nativeHandle).toMessagePipeHandle();
    }

    @CalledByNative
    static void bindBarcodeDetectionProvider(long nativeHandle) {
        // Immediately wrap |nativeHandle| as it cannot be allowed to leak.
        MessagePipeHandle handle = messagePipeHandleFromNative(nativeHandle);

        BarcodeDetectionProvider impl = BarcodeDetectionProviderImpl.create();
        if (impl == null) {
            handle.close();
            return;
        }

        BarcodeDetectionProvider.MANAGER.bind(impl, handle);
    }

    @CalledByNative
    static void bindFaceDetectionProvider(long nativeHandle) {
        FaceDetectionProvider.MANAGER.bind(
                new FaceDetectionProviderImpl(), messagePipeHandleFromNative(nativeHandle));
    }

    @CalledByNative
    static void bindTextDetection(long nativeHandle) {
        // Immediately wrap |nativeHandle| as it cannot be allowed to leak.
        MessagePipeHandle handle = messagePipeHandleFromNative(nativeHandle);

        TextDetection impl = TextDetectionImpl.create();
        if (impl == null) {
            handle.close();
            return;
        }

        TextDetection.MANAGER.bind(impl, handle);
    }
}
