// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.mlkit.genai.common.DownloadCallback;
import com.google.mlkit.genai.common.FeatureStatus;
import com.google.mlkit.genai.common.GenAiException;
import com.google.mlkit.genai.prompt.GenerativeModel;
import com.google.mlkit.genai.prompt.java.GenerativeModelFutures;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.concurrent.Executor;

/**
 * Unit tests for {@link MlKitAiCoreModelDownloaderBackendImpl}.
 *
 * <p>Uses the package-private constructor to inject mocked GenerativeModelFutures for comprehensive
 * testing of the backend behavior.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MlKitAiCoreModelDownloaderBackendImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private GenerativeModel mMockGenerativeModel;
    @Mock private GenerativeModelFutures mMockGenerativeModelFutures;

    private Executor mDirectExecutor;

    @Before
    public void setUp() {
        // Use a direct executor that runs callbacks immediately for testing
        mDirectExecutor = Runnable::run;
    }

    private MlKitAiCoreModelDownloaderBackendImpl createBackend() {
        return new MlKitAiCoreModelDownloaderBackendImpl(
                mMockGenerativeModel, mMockGenerativeModelFutures, mDirectExecutor);
    }

    // ==================== checkStatus() tests ====================

    @Test
    @Feature({"OnDeviceModel"})
    public void testCheckStatus_Available_ReportsAvailableStatus() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture = Futures.immediateFuture(FeatureStatus.AVAILABLE);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        backend.checkStatus(responder);

        assertEquals(
                "AVAILABLE status should return ModelStatus.AVAILABLE",
                ModelStatus.AVAILABLE,
                responder.mModelStatus);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testCheckStatus_Downloadable_ReportsDownloadableStatus() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture =
                Futures.immediateFuture(FeatureStatus.DOWNLOADABLE);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        backend.checkStatus(responder);

        assertEquals(
                "DOWNLOADABLE status should return ModelStatus.DOWNLOADABLE",
                ModelStatus.DOWNLOADABLE,
                responder.mModelStatus);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testCheckStatus_Downloading_ReportsDownloadingStatus() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture = Futures.immediateFuture(FeatureStatus.DOWNLOADING);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        backend.checkStatus(responder);

        assertEquals(
                "DOWNLOADING status should return ModelStatus.DOWNLOADING",
                ModelStatus.DOWNLOADING,
                responder.mModelStatus);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testCheckStatus_Unavailable_ReportsUnavailableStatus() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture = Futures.immediateFuture(FeatureStatus.UNAVAILABLE);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        backend.checkStatus(responder);

        assertEquals(
                "UNAVAILABLE status should return ModelStatus.UNAVAILABLE",
                ModelStatus.UNAVAILABLE,
                responder.mModelStatus);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testCheckStatus_UnknownStatus_ReportsUnavailableStatus() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        // An unknown/unexpected status value
        ListenableFuture<Integer> statusFuture = Futures.immediateFuture(999);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        backend.checkStatus(responder);

        assertEquals(
                "Unknown status should return ModelStatus.UNAVAILABLE",
                ModelStatus.UNAVAILABLE,
                responder.mModelStatus);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testCheckStatus_CheckStatusFails_ReportsUnavailableStatus() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture =
                Futures.immediateFailedFuture(new RuntimeException("Network error"));
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        backend.checkStatus(responder);

        assertEquals(
                "Failed status check should return ModelStatus.UNAVAILABLE",
                ModelStatus.UNAVAILABLE,
                responder.mModelStatus);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testCheckStatus_AfterDestroyed_ReportsUnavailableStatus() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        backend.onNativeDestroyed();
        backend.checkStatus(responder);

        assertEquals(
                "Should report UNAVAILABLE after destroyed",
                ModelStatus.UNAVAILABLE,
                responder.mModelStatus);
        verify(mMockGenerativeModelFutures, never()).checkStatus();
    }

    // ==================== startDownload() tests ====================

    @Test
    @Feature({"OnDeviceModel"})
    public void testStartDownload_ModelAvailable_ReportsModelInfo() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture = Futures.immediateFuture(FeatureStatus.AVAILABLE);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        String modelName = "test-model-name";
        ListenableFuture<String> modelNameFuture = Futures.immediateFuture(modelName);
        when(mMockGenerativeModelFutures.getBaseModelName()).thenReturn(modelNameFuture);

        backend.startDownload(responder);

        assertEquals("Should return correct model name", modelName, responder.mModelName);
        assertEquals("Should return version 1.0", "1.0", responder.mModelVersion);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testStartDownload_GetModelInfoFailsWhenAvailable_ReportsGetFeatureStatusError() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture = Futures.immediateFuture(FeatureStatus.AVAILABLE);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        ListenableFuture<String> modelNameFuture =
                Futures.immediateFailedFuture(new RuntimeException("Failed to get model name"));
        when(mMockGenerativeModelFutures.getBaseModelName()).thenReturn(modelNameFuture);

        backend.startDownload(responder);

        assertEquals(
                "Failed getBaseModelName should return GET_FEATURE_STATUS_ERROR",
                DownloadFailureReason.GET_FEATURE_STATUS_ERROR,
                responder.mFailureReason);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testStartDownload_ModelUnavailable_ReportsFeatureNotAvailable() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture = Futures.immediateFuture(FeatureStatus.UNAVAILABLE);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        backend.startDownload(responder);

        assertEquals(
                "UNAVAILABLE should return FEATURE_NOT_AVAILABLE",
                DownloadFailureReason.FEATURE_NOT_AVAILABLE,
                responder.mFailureReason);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testStartDownload_DownloadSucceeds_ReportsModelInfo() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture =
                Futures.immediateFuture(FeatureStatus.DOWNLOADABLE);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        ListenableFuture<Void> downloadFuture = Futures.immediateVoidFuture();
        when(mMockGenerativeModelFutures.download(any(DownloadCallback.class)))
                .thenReturn(downloadFuture);

        String modelName = "downloaded-model";
        when(mMockGenerativeModelFutures.getBaseModelName())
                .thenReturn(Futures.immediateFuture(modelName));

        backend.startDownload(responder);

        assertEquals(
                "Successful download should return correct model name",
                modelName,
                responder.mModelName);
        assertEquals("Should return version 1.0", "1.0", responder.mModelVersion);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testStartDownload_DownloadInProgress_ReportsProgress() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture =
                Futures.immediateFuture(FeatureStatus.DOWNLOADABLE);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        // Capture the DownloadCallback created by fireDownload() and invoke its progress methods.
        doAnswer(
                        invocation -> {
                            DownloadCallback callback = invocation.getArgument(0);
                            callback.onDownloadStarted(1000);
                            callback.onDownloadProgress(500);
                            return Futures.immediateVoidFuture();
                        })
                .when(mMockGenerativeModelFutures)
                .download(any(DownloadCallback.class));

        backend.startDownload(responder);

        // Verify progress updates were reported correctly. First update is from onDownloadStarted,
        // second is from onDownloadProgress.
        assertEquals(2, responder.mProgressUpdateCount);
        assertEquals(500, responder.mLastDownloadedBytes);
        assertEquals(1000, responder.mLastTotalBytes);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testStartDownload_DownloadFails_ReportsDownloadGeneralError() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture =
                Futures.immediateFuture(FeatureStatus.DOWNLOADABLE);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        ListenableFuture<Void> downloadFuture =
                Futures.immediateFailedFuture(new RuntimeException("Download failed"));
        when(mMockGenerativeModelFutures.download(any(DownloadCallback.class)))
                .thenReturn(downloadFuture);

        backend.startDownload(responder);

        assertEquals(
                "Failed download should return DOWNLOAD_GENERAL_ERROR",
                DownloadFailureReason.DOWNLOAD_GENERAL_ERROR,
                responder.mFailureReason);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testStartDownload_DownloadFailsIncompatible_ReportsGetFeatureError() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture =
                Futures.immediateFuture(FeatureStatus.DOWNLOADABLE);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        GenAiException mockException = mock(GenAiException.class);
        when(mockException.getErrorCode()).thenReturn(-101); // AICORE_INCOMPATIBLE

        ListenableFuture<Void> downloadFuture = Futures.immediateFailedFuture(mockException);
        when(mMockGenerativeModelFutures.download(any(DownloadCallback.class)))
                .thenReturn(downloadFuture);

        backend.startDownload(responder);

        assertEquals(
                "AICORE_INCOMPATIBLE should return GET_FEATURE_ERROR",
                DownloadFailureReason.GET_FEATURE_ERROR,
                responder.mFailureReason);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testStartDownload_DownloadFailsNotAvailable_ReportsFeatureIsNull() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture =
                Futures.immediateFuture(FeatureStatus.DOWNLOADABLE);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        GenAiException mockException = mock(GenAiException.class);
        when(mockException.getErrorCode()).thenReturn(8); // NOT_AVAILABLE

        ListenableFuture<Void> downloadFuture = Futures.immediateFailedFuture(mockException);
        when(mMockGenerativeModelFutures.download(any(DownloadCallback.class)))
                .thenReturn(downloadFuture);

        backend.startDownload(responder);

        assertEquals(
                "NOT_AVAILABLE should return FEATURE_IS_NULL",
                DownloadFailureReason.FEATURE_IS_NULL,
                responder.mFailureReason);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testStartDownload_DownloadFailsNotEnoughDiskSpace_ReportsNotEnoughDiskSpaceError() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture =
                Futures.immediateFuture(FeatureStatus.DOWNLOADABLE);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        GenAiException mockException = mock(GenAiException.class);
        when(mockException.getErrorCode()).thenReturn(501); // NOT_ENOUGH_DISK_SPACE

        ListenableFuture<Void> downloadFuture = Futures.immediateFailedFuture(mockException);
        when(mMockGenerativeModelFutures.download(any(DownloadCallback.class)))
                .thenReturn(downloadFuture);

        backend.startDownload(responder);

        assertEquals(
                "NOT_ENOUGH_DISK_SPACE should return DOWNLOAD_NOT_ENOUGH_DISK_SPACE_ERROR",
                DownloadFailureReason.DOWNLOAD_NOT_ENOUGH_DISK_SPACE_ERROR,
                responder.mFailureReason);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testStartDownload_GetModelInfoFailsAfterDownload_ReportsGetFeatureStatusError() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture =
                Futures.immediateFuture(FeatureStatus.DOWNLOADABLE);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        ListenableFuture<Void> downloadFuture = Futures.immediateVoidFuture();
        when(mMockGenerativeModelFutures.download(any(DownloadCallback.class)))
                .thenReturn(downloadFuture);

        when(mMockGenerativeModelFutures.getBaseModelName())
                .thenReturn(
                        Futures.immediateFailedFuture(
                                new RuntimeException("Failed to get model name")));

        backend.startDownload(responder);

        assertEquals(
                "Failed getBaseModelName after download should return GET_FEATURE_STATUS_ERROR",
                DownloadFailureReason.GET_FEATURE_STATUS_ERROR,
                responder.mFailureReason);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testStartDownload_AppendedDownloadSucceeds_ReportsModelInfo() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        // DOWNLOADING follows the same fireDownload() path as DOWNLOADABLE.
        ListenableFuture<Integer> statusFuture = Futures.immediateFuture(FeatureStatus.DOWNLOADING);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        ListenableFuture<Void> downloadFuture = Futures.immediateVoidFuture();
        when(mMockGenerativeModelFutures.download(any(DownloadCallback.class)))
                .thenReturn(downloadFuture);

        String modelName = "downloading-model";
        when(mMockGenerativeModelFutures.getBaseModelName())
                .thenReturn(Futures.immediateFuture(modelName));

        backend.startDownload(responder);

        assertEquals(modelName, responder.mModelName);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testStartDownload_CheckStatusFails_ReportsGetFeatureStatusError() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture =
                Futures.immediateFailedFuture(new RuntimeException("Check status failed"));
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        backend.startDownload(responder);

        assertEquals(
                "Failed checkStatus should return GET_FEATURE_STATUS_ERROR",
                DownloadFailureReason.GET_FEATURE_STATUS_ERROR,
                responder.mFailureReason);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testStartDownload_AfterDestroyed_ReportsUnknownError() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        backend.onNativeDestroyed();
        backend.startDownload(responder);

        assertEquals(
                "Should report UNKNOWN_ERROR after destroyed",
                DownloadFailureReason.UNKNOWN_ERROR,
                responder.mFailureReason);
        verify(mMockGenerativeModelFutures, never()).checkStatus();
    }

    // ==================== onNativeDestroyed() tests ====================

    @Test
    @Feature({"OnDeviceModel"})
    public void testOnNativeDestroyed_ClosesGenerativeModel() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();

        backend.onNativeDestroyed();

        verify(mMockGenerativeModel).close();
    }

    // ==================== Helper class ====================

    /** A simple mock responder to capture callback results. */
    private static class MockDownloaderResponder implements DownloaderResponder {
        String mModelName;
        String mModelVersion;
        @DownloadFailureReason int mFailureReason = -1;
        @ModelStatus int mModelStatus = -1;
        long mLastDownloadedBytes = -1;
        long mLastTotalBytes = -1;
        int mProgressUpdateCount;

        @Override
        public void onAvailable(String baseModelName, String baseModelVersion) {
            mModelName = baseModelName;
            mModelVersion = baseModelVersion;
        }

        @Override
        public void onUnavailable(@DownloadFailureReason int downloadFailureReason) {
            mFailureReason = downloadFailureReason;
        }

        @Override
        public void onStatusCheckResult(@ModelStatus int modelStatus) {
            mModelStatus = modelStatus;
        }

        @Override
        public void onDownloadProgress(long downloadedBytes, long totalBytes) {
            mLastDownloadedBytes = downloadedBytes;
            mLastTotalBytes = totalBytes;
            mProgressUpdateCount++;
        }
    }
}
