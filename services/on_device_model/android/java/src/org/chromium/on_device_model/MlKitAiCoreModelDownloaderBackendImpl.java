// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.mlkit.genai.common.FeatureStatus;
import com.google.mlkit.genai.prompt.GenerativeModel;
import com.google.mlkit.genai.prompt.java.GenerativeModelFutures;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;

import java.util.concurrent.Executor;

/**
 * Implementation of AiCoreModelDownloaderBackend using MLKit APIs. This backend checks base model
 * status and handles model downloading through MLKit's GenerativeModel download system.
 */
@NullMarked
class MlKitAiCoreModelDownloaderBackendImpl implements AiCoreModelDownloaderBackend {
    private final GenerativeModel mGenerativeModel;
    private final GenerativeModelFutures mGenerativeModelFutures;
    private final Executor mExecutor;

    // Flag to indicate if the native counterpart has been destroyed.
    private boolean mIsDestroyed;

    MlKitAiCoreModelDownloaderBackendImpl(GenerativeModel generativeModel) {
        this(
                generativeModel,
                GenerativeModelFutures.from(generativeModel),
                new Executor() {
                    @Override
                    public void execute(Runnable command) {
                        ThreadUtils.getUiThreadHandler().post(command);
                    }
                });
    }

    /**
     * Package-private constructor for testing. Allows injection of mocked GenerativeModelFutures
     * and Executor.
     */
    MlKitAiCoreModelDownloaderBackendImpl(
            GenerativeModel generativeModel,
            GenerativeModelFutures generativeModelFutures,
            Executor executor) {
        mGenerativeModel = generativeModel;
        mGenerativeModelFutures = generativeModelFutures;
        mExecutor = executor;
    }

    @Override
    public void checkStatus(DownloaderResponder responder) {
        if (mIsDestroyed) {
            responder.onStatusCheckResult(ModelStatus.UNAVAILABLE);
            return;
        }

        performStatusCheck(responder);
    }

    @Override
    public void startDownload(DownloaderResponder responder) {
        if (mIsDestroyed) {
            responder.onUnavailable(DownloadFailureReason.UNKNOWN_ERROR);
            return;
        }

        performDownload(responder);
    }

    /**
     * Performs a status check and reports the model status.
     *
     * @param responder The responder to notify of status
     */
    private void performStatusCheck(DownloaderResponder responder) {
        ListenableFuture<Integer> statusFuture = mGenerativeModelFutures.checkStatus();

        Futures.addCallback(
                statusFuture,
                new FutureCallback<Integer>() {
                    @Override
                    public void onSuccess(@FeatureStatus Integer status) {
                        if (mIsDestroyed) return;

                        handleStatusCheckResult(status, responder);
                    }

                    @Override
                    public void onFailure(Throwable t) {
                        if (!mIsDestroyed) {
                            responder.onStatusCheckResult(ModelStatus.UNAVAILABLE);
                        }
                    }
                },
                mExecutor);
    }

    /**
     * Performs a download and reports the result.
     *
     * @param responder The responder to notify of download results
     */
    private void performDownload(DownloaderResponder responder) {
        ListenableFuture<Integer> statusFuture = mGenerativeModelFutures.checkStatus();

        Futures.addCallback(
                statusFuture,
                new FutureCallback<Integer>() {
                    @Override
                    public void onSuccess(@FeatureStatus Integer status) {
                        if (mIsDestroyed) return;

                        handleDownloadResult(status, responder);
                    }

                    @Override
                    public void onFailure(Throwable t) {
                        if (!mIsDestroyed) {
                            responder.onUnavailable(DownloadFailureReason.GET_FEATURE_STATUS_ERROR);
                        }
                    }
                },
                mExecutor);
    }

    /**
     * Handles the feature status for checkStatus and reports via onStatusCheckResult.
     *
     * @param status The feature status from checkStatus()
     * @param responder The responder to notify
     */
    private void handleStatusCheckResult(@FeatureStatus int status, DownloaderResponder responder) {
        switch (status) {
            case FeatureStatus.AVAILABLE:
                responder.onStatusCheckResult(ModelStatus.AVAILABLE);
                break;
            case FeatureStatus.DOWNLOADABLE:
                responder.onStatusCheckResult(ModelStatus.DOWNLOADABLE);
                break;
            case FeatureStatus.DOWNLOADING:
                responder.onStatusCheckResult(ModelStatus.DOWNLOADING);
                break;
            default:
                responder.onStatusCheckResult(ModelStatus.UNAVAILABLE);
                break;
        }
    }

    /**
     * Handles the feature status for startDownload and reports via onAvailable/onUnavailable.
     *
     * @param status The feature status from checkStatus()
     * @param responder The responder to notify
     */
    private void handleDownloadResult(@FeatureStatus int status, DownloaderResponder responder) {
        switch (status) {
            case FeatureStatus.AVAILABLE:
                // Base model is already available.
                Futures.addCallback(
                        mGenerativeModelFutures.getBaseModelName(),
                        new FutureCallback<String>() {
                            @Override
                            public void onSuccess(String modelName) {
                                if (!mIsDestroyed) {
                                    // Model version is not available via MLKit APIs, use "1.0" as
                                    // placeholder.
                                    responder.onAvailable(modelName, "1.0");
                                }
                            }

                            @Override
                            public void onFailure(Throwable t) {
                                if (!mIsDestroyed) {
                                    responder.onUnavailable(
                                            DownloadFailureReason.GET_FEATURE_STATUS_ERROR);
                                }
                            }
                        },
                        mExecutor);
                break;

            case FeatureStatus.DOWNLOADABLE:
            case FeatureStatus.DOWNLOADING:
                // For both DOWNLOADABLE and DOWNLOADING status, we initiate the download. These
                // functions will be implemented in another CL and for now, just notify that the
                // download api is not implemented yet.
                // TODO(crbug.com/477033510): Initiate download and handle download start,
                // in progress, completion callbacks.
                responder.onUnavailable(DownloadFailureReason.API_NOT_AVAILABLE);
                break;

            default:
                // FeatureStatus.UNAVAILABLE: it means the device is not eligible to have the
                // on-device model capability due to various reasons (e.g. AICore APK not installed,
                // feature not available, etc).
                responder.onUnavailable(DownloadFailureReason.FEATURE_NOT_AVAILABLE);
                break;
        }
    }

    @Override
    public void onNativeDestroyed() {
        mIsDestroyed = true;

        // Close the GenerativeModel instance to release resources when the native downloader is
        // destroyed.
        mGenerativeModel.close();
    }
}
