// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** Request AiCore to download models required for a feature. */
@JNINamespace("on_device_model")
@NullMarked
interface AiCoreModelDownloader {
    /**
     * Request AiCore to start downloading the model. Note that once the download starts, there is
     * no way to cancel it even if the native side is destroyed.
     */
    @CalledByNative
    void startDownload(long nativeModelDownloaderAndroid);

    /**
     * Called when the native downloader is destroyed. The implementation class should not call
     * native functions after this is called.
     */
    @CalledByNative
    void onNativeDestroyed();

    @NativeMethods
    interface Natives {
        // Called when the download has completed and the status has become available.
        // Called at most once. Not called if onUnavailable is called.
        // TODO(crbug.com/425408635): Return the base model name and version.
        void onAvailable(long modelDownloaderAndroid);

        // Called when the model is unavailable. Called at most once. Not called if onAvailable is
        // called.
        // TODO(crbug.com/425408635): Return the error reason and whether the error is
        // deterministic.
        void onUnavailable(long modelDownloaderAndroid);
    }
}
