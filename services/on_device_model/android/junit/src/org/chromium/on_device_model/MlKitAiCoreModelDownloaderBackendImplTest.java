// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.mlkit.genai.common.FeatureStatus;
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
    public void testCheckStatus_Available_ReportsAvailable() {
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
    public void testCheckStatus_Downloadable_ReportsDownloadable() {
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
    public void testCheckStatus_Downloading_ReportsDownloading() {
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
    public void testCheckStatus_Unavailable_ReportsUnavailable() {
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
    public void testCheckStatus_UnknownStatus_ReportsUnavailable() {
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
    public void testCheckStatus_Failure_ReportsUnavailable() {
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
    public void testCheckStatus_AfterDestroyed_ReportsUnavailable() {
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
    public void testStartDownload_Available_ReportsOnAvailable() {
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
    public void testStartDownload_Available_GetModelNameFails_ReportsUnavailable() {
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
    public void testStartDownload_Downloadable_ReportsApiNotAvailable() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture =
                Futures.immediateFuture(FeatureStatus.DOWNLOADABLE);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        backend.startDownload(responder);

        assertEquals(
                "DOWNLOADABLE should return API_NOT_AVAILABLE (not yet implemented)",
                DownloadFailureReason.API_NOT_AVAILABLE,
                responder.mFailureReason);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testStartDownload_Downloading_ReportsApiNotAvailable() {
        MlKitAiCoreModelDownloaderBackendImpl backend = createBackend();
        MockDownloaderResponder responder = new MockDownloaderResponder();

        ListenableFuture<Integer> statusFuture = Futures.immediateFuture(FeatureStatus.DOWNLOADING);
        when(mMockGenerativeModelFutures.checkStatus()).thenReturn(statusFuture);

        backend.startDownload(responder);

        assertEquals(
                "DOWNLOADING should return API_NOT_AVAILABLE (not yet implemented)",
                DownloadFailureReason.API_NOT_AVAILABLE,
                responder.mFailureReason);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testStartDownload_Unavailable_ReportsFeatureNotAvailable() {
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
    }
}
