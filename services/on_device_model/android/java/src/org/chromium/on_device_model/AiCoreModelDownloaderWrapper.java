// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/**
 * A wrapper of {@link AiCoreModelDownloaderBackend} that is called from native. Responsible for
 * handling the JNI calls.
 */
@JNINamespace("on_device_model")
@NullMarked
class AiCoreModelDownloaderWrapper {
    private final AiCoreModelDownloaderBackend mBackend;
    private final JniSafeCallback mJniSafeCallback = new JniSafeCallback();

    public AiCoreModelDownloaderWrapper(AiCoreModelDownloaderBackend backend) {
        mBackend = backend;
    }

    /**
     * Request AiCore to start downloading the model. Note that once the download starts, there is
     * no way to cancel it even if the native side is destroyed.
     */
    @CalledByNative
    void startDownload(long nativeModelDownloaderAndroid) {
        DownloaderResponder responder =
                new DownloaderResponder() {
                    @Override
                    public void onAvailable(String baseModelName, String baseModelVersion) {
                        mJniSafeCallback.run(
                                () -> {
                                    AiCoreModelDownloaderWrapperJni.get()
                                            .onAvailable(
                                                    nativeModelDownloaderAndroid,
                                                    baseModelName,
                                                    baseModelVersion);
                                });
                    }

                    @Override
                    public void onUnavailable(@DownloadFailureReason int downloadFailureReason) {
                        mJniSafeCallback.run(
                                () -> {
                                    AiCoreModelDownloaderWrapperJni.get()
                                            .onUnavailable(
                                                    nativeModelDownloaderAndroid,
                                                    downloadFailureReason);
                                });
                    }
                };
        mBackend.startDownload(responder);
    }

    /** Called when the native downloader is destroyed. */
    @CalledByNative
    void onNativeDestroyed() {
        mJniSafeCallback.onNativeDestroyed(() -> mBackend.onNativeDestroyed());
    }

    @NativeMethods
    interface Natives {
        void onAvailable(
                long modelDownloaderAndroid, String baseModelName, String baseModelVersion);

        void onUnavailable(
                long modelDownloaderAndroid, @DownloadFailureReason int downloadFailureReason);
    }
}
