// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.mlkit.genai.common.GenAiException;
import com.google.mlkit.genai.common.StreamingCallback;
import com.google.mlkit.genai.prompt.CountTokensResponse;
import com.google.mlkit.genai.prompt.GenerateContentRequest;
import com.google.mlkit.genai.prompt.GenerateContentResponse;
import com.google.mlkit.genai.prompt.GenerativeModel;
import com.google.mlkit.genai.prompt.java.GenerativeModelFutures;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.on_device_model.mojom.GenerateOptions;
import org.chromium.on_device_model.mojom.InputPiece;
import org.chromium.on_device_model.mojom.SessionParams;

import java.util.concurrent.Executor;

/**
 * Unit tests for {@link MlKitAiCoreSessionBackendImpl}.
 *
 * <p>Uses the package-private constructor to inject mocked GenerativeModelFutures for comprehensive
 * testing of the backend behavior.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MlKitAiCoreSessionBackendImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private GenerativeModel mMockGenerativeModel;
    @Mock private GenerativeModelFutures mMockGenerativeModelFutures;

    private SessionParams mParams;
    private Executor mDirectExecutor;

    @Before
    public void setUp() {
        mParams = new SessionParams();
        mParams.topK = 10;
        mParams.temperature = 0.8f;

        // Use a direct executor that runs callbacks immediately for testing
        mDirectExecutor = Runnable::run;
    }

    private MlKitAiCoreSessionBackendImpl createBackend() {
        return new MlKitAiCoreSessionBackendImpl(
                mMockGenerativeModel, mMockGenerativeModelFutures, mParams, mDirectExecutor);
    }

    private InputPiece createTextInputPiece(String text) {
        InputPiece piece = new InputPiece();
        piece.setText(text);
        return piece;
    }

    private GenerateOptions createGenerateOptions() {
        GenerateOptions options = new GenerateOptions();
        options.maxOutputTokens = 256;
        return options;
    }

    // ==================== generate() streaming tests ====================
    // Note: GenerateContentResponse is a final class with a private constructor that cannot be
    // mocked or instantiated in tests. We test the generate() method primarily through its
    // streaming callback behavior.

    @Test
    @Feature({"OnDeviceModel"})
    public void testGenerate_StreamingCallback_ReceivesPartialResults() {
        MlKitAiCoreSessionBackendImpl backend = createBackend();
        MockSessionResponder responder = new MockSessionResponder();

        // Capture the streaming callback
        ArgumentCaptor<StreamingCallback> callbackCaptor =
                ArgumentCaptor.forClass(StreamingCallback.class);

        // Return a future that never completes - we only care about streaming
        ListenableFuture<GenerateContentResponse> pendingFuture =
                com.google.common.util.concurrent.SettableFuture.create();
        when(mMockGenerativeModelFutures.generateContent(
                        any(GenerateContentRequest.class), callbackCaptor.capture()))
                .thenReturn(pendingFuture);

        InputPiece[] inputPieces = new InputPiece[] {createTextInputPiece("Hello")};
        GenerateOptions options = createGenerateOptions();

        backend.generate(options, inputPieces, responder);

        // Simulate streaming callback with partial results
        StreamingCallback capturedCallback = callbackCaptor.getValue();
        capturedCallback.onNewText("Hello ");
        capturedCallback.onNewText("World!");

        assertEquals("Should receive streamed responses", "Hello World!", responder.mResponse);
    }

    // ==================== generate() error tests ====================

    @Test
    @Feature({"OnDeviceModel"})
    public void testGenerate_EmptyPrompt_ReturnsRequestProcessingError() {
        MlKitAiCoreSessionBackendImpl backend = createBackend();
        MockSessionResponder responder = new MockSessionResponder();

        InputPiece[] inputPieces = new InputPiece[] {};
        GenerateOptions options = createGenerateOptions();

        backend.generate(options, inputPieces, responder);

        assertEquals(
                "Empty prompt should return REQUEST_PROCESSING_ERROR",
                GenerateResult.INFERENCE_REQUEST_PROCESSING_ERROR,
                responder.mResult);
        verify(mMockGenerativeModelFutures, never())
                .generateContent(any(GenerateContentRequest.class), any(StreamingCallback.class));
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testGenerate_NullResponse_ReturnsGeneralError() {
        MlKitAiCoreSessionBackendImpl backend = createBackend();
        MockSessionResponder responder = new MockSessionResponder();

        ListenableFuture<GenerateContentResponse> responseFuture = Futures.immediateFuture(null);
        when(mMockGenerativeModelFutures.generateContent(
                        any(GenerateContentRequest.class), any(StreamingCallback.class)))
                .thenReturn(responseFuture);

        InputPiece[] inputPieces = new InputPiece[] {createTextInputPiece("Hello")};
        GenerateOptions options = createGenerateOptions();

        backend.generate(options, inputPieces, responder);

        assertEquals(
                "Null response should return GENERAL_ERROR",
                GenerateResult.INFERENCE_GENERAL_ERROR,
                responder.mResult);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testGenerate_GenAiException_AiCoreNotInstalled_ReturnsGetFeatureError() {
        MlKitAiCoreSessionBackendImpl backend = createBackend();
        MockSessionResponder responder = new MockSessionResponder();

        GenAiException mockException = mock(GenAiException.class);
        when(mockException.getErrorCode()).thenReturn(-101); // AICORE_NOT_INSTALLED

        ListenableFuture<GenerateContentResponse> responseFuture =
                Futures.immediateFailedFuture(mockException);
        when(mMockGenerativeModelFutures.generateContent(
                        any(GenerateContentRequest.class), any(StreamingCallback.class)))
                .thenReturn(responseFuture);

        InputPiece[] inputPieces = new InputPiece[] {createTextInputPiece("Hello")};
        GenerateOptions options = createGenerateOptions();

        backend.generate(options, inputPieces, responder);

        assertEquals(
                "AICORE_NOT_INSTALLED should return GET_FEATURE_ERROR",
                GenerateResult.GET_FEATURE_ERROR,
                responder.mResult);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testGenerate_GenAiException_FeatureNotAvailable_ReturnsFeatureIsNull() {
        MlKitAiCoreSessionBackendImpl backend = createBackend();
        MockSessionResponder responder = new MockSessionResponder();

        GenAiException mockException = mock(GenAiException.class);
        when(mockException.getErrorCode()).thenReturn(8); // FEATURE_NOT_AVAILABLE

        ListenableFuture<GenerateContentResponse> responseFuture =
                Futures.immediateFailedFuture(mockException);
        when(mMockGenerativeModelFutures.generateContent(
                        any(GenerateContentRequest.class), any(StreamingCallback.class)))
                .thenReturn(responseFuture);

        InputPiece[] inputPieces = new InputPiece[] {createTextInputPiece("Hello")};
        GenerateOptions options = createGenerateOptions();

        backend.generate(options, inputPieces, responder);

        assertEquals(
                "FEATURE_NOT_AVAILABLE should return FEATURE_IS_NULL",
                GenerateResult.FEATURE_IS_NULL,
                responder.mResult);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testGenerate_GenAiException_RequestTooLarge_ReturnsRequestProcessingError() {
        MlKitAiCoreSessionBackendImpl backend = createBackend();
        MockSessionResponder responder = new MockSessionResponder();

        GenAiException mockException = mock(GenAiException.class);
        when(mockException.getErrorCode()).thenReturn(12); // REQUEST_TOO_LARGE

        ListenableFuture<GenerateContentResponse> responseFuture =
                Futures.immediateFailedFuture(mockException);
        when(mMockGenerativeModelFutures.generateContent(
                        any(GenerateContentRequest.class), any(StreamingCallback.class)))
                .thenReturn(responseFuture);

        InputPiece[] inputPieces = new InputPiece[] {createTextInputPiece("Hello")};
        GenerateOptions options = createGenerateOptions();

        backend.generate(options, inputPieces, responder);

        assertEquals(
                "REQUEST_TOO_LARGE should return REQUEST_PROCESSING_ERROR",
                GenerateResult.INFERENCE_REQUEST_PROCESSING_ERROR,
                responder.mResult);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testGenerate_GenAiException_ResponsePolicyCheckFailed_ReturnsResponseError() {
        MlKitAiCoreSessionBackendImpl backend = createBackend();
        MockSessionResponder responder = new MockSessionResponder();

        GenAiException mockException = mock(GenAiException.class);
        when(mockException.getErrorCode()).thenReturn(11); // RESPONSE_POLICY_CHECK_FAILED

        ListenableFuture<GenerateContentResponse> responseFuture =
                Futures.immediateFailedFuture(mockException);
        when(mMockGenerativeModelFutures.generateContent(
                        any(GenerateContentRequest.class), any(StreamingCallback.class)))
                .thenReturn(responseFuture);

        InputPiece[] inputPieces = new InputPiece[] {createTextInputPiece("Hello")};
        GenerateOptions options = createGenerateOptions();

        backend.generate(options, inputPieces, responder);

        assertEquals(
                "RESPONSE_POLICY_CHECK_FAILED should return RESPONSE_PROCESSING_ERROR",
                GenerateResult.INFERENCE_RESPONSE_PROCESSING_ERROR,
                responder.mResult);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testGenerate_GenericException_ReturnsGeneralError() {
        MlKitAiCoreSessionBackendImpl backend = createBackend();
        MockSessionResponder responder = new MockSessionResponder();

        ListenableFuture<GenerateContentResponse> responseFuture =
                Futures.immediateFailedFuture(new RuntimeException("Network error"));
        when(mMockGenerativeModelFutures.generateContent(
                        any(GenerateContentRequest.class), any(StreamingCallback.class)))
                .thenReturn(responseFuture);

        InputPiece[] inputPieces = new InputPiece[] {createTextInputPiece("Hello")};
        GenerateOptions options = createGenerateOptions();

        backend.generate(options, inputPieces, responder);

        assertEquals(
                "Generic exception should return GENERAL_ERROR",
                GenerateResult.INFERENCE_GENERAL_ERROR,
                responder.mResult);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testGenerate_InvalidArgument_ReturnsInvalidArgumentError() {
        MlKitAiCoreSessionBackendImpl backend = createBackend();
        MockSessionResponder responder = new MockSessionResponder();

        InputPiece[] inputPieces = new InputPiece[] {createTextInputPiece("Hello")};
        GenerateOptions options = new GenerateOptions();
        // maxOutputTokens > 256 throws IllegalArgumentException in MLKit API
        options.maxOutputTokens = 1000;

        backend.generate(options, inputPieces, responder);

        assertEquals(
                "Invalid argument should return INVALID_ARGUMENT_ERROR",
                GenerateResult.INVALID_REQUEST_ARGUMENT_ERROR,
                responder.mResult);
        verify(mMockGenerativeModelFutures, never())
                .generateContent(any(GenerateContentRequest.class), any(StreamingCallback.class));
    }

    // ==================== generate() lifecycle tests ====================

    @Test
    @Feature({"OnDeviceModel"})
    public void testGenerate_AfterDestroyed_ReturnsUnknownError() {
        MlKitAiCoreSessionBackendImpl backend = createBackend();
        MockSessionResponder responder = new MockSessionResponder();

        backend.onNativeDestroyed();

        InputPiece[] inputPieces = new InputPiece[] {createTextInputPiece("Hello")};
        GenerateOptions options = createGenerateOptions();

        backend.generate(options, inputPieces, responder);

        assertEquals(
                "Should return UNKNOWN_ERROR after destroyed",
                GenerateResult.UNKNOWN_ERROR,
                responder.mResult);
        verify(mMockGenerativeModelFutures, never())
                .generateContent(any(GenerateContentRequest.class), any(StreamingCallback.class));
    }

    // ==================== getSizeInTokens() tests ====================
    // Note: Success test for getSizeInTokens() is not included because CountTokensResponse
    // is a final class that cannot be mocked in Chromium's Robolectric environment.

    @Test
    @Feature({"OnDeviceModel"})
    public void testGetSizeInTokens_EmptyPrompt_ReturnsZero() {
        MlKitAiCoreSessionBackendImpl backend = createBackend();
        MockSessionResponder responder = new MockSessionResponder();

        InputPiece[] inputPieces = new InputPiece[] {};

        backend.getSizeInTokens(inputPieces, responder);

        assertEquals("Empty prompt should return 0 tokens", 0, responder.mTokenCount);
        verify(mMockGenerativeModelFutures, never()).countTokens(any(GenerateContentRequest.class));
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testGetSizeInTokens_NullResponse_ReturnsZero() {
        MlKitAiCoreSessionBackendImpl backend = createBackend();
        MockSessionResponder responder = new MockSessionResponder();

        ListenableFuture<CountTokensResponse> responseFuture = Futures.immediateFuture(null);
        when(mMockGenerativeModelFutures.countTokens(any(GenerateContentRequest.class)))
                .thenReturn(responseFuture);

        InputPiece[] inputPieces = new InputPiece[] {createTextInputPiece("Hello")};

        backend.getSizeInTokens(inputPieces, responder);

        assertEquals("Null response should return 0 tokens", 0, responder.mTokenCount);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testGetSizeInTokens_Failure_ReturnsZero() {
        MlKitAiCoreSessionBackendImpl backend = createBackend();
        MockSessionResponder responder = new MockSessionResponder();

        ListenableFuture<CountTokensResponse> responseFuture =
                Futures.immediateFailedFuture(new RuntimeException("Count tokens failed"));
        when(mMockGenerativeModelFutures.countTokens(any(GenerateContentRequest.class)))
                .thenReturn(responseFuture);

        InputPiece[] inputPieces = new InputPiece[] {createTextInputPiece("Hello")};

        backend.getSizeInTokens(inputPieces, responder);

        assertEquals("Failed request should return 0 tokens", 0, responder.mTokenCount);
    }

    @Test
    @Feature({"OnDeviceModel"})
    public void testGetSizeInTokens_AfterDestroyed_ReturnsZero() {
        MlKitAiCoreSessionBackendImpl backend = createBackend();
        MockSessionResponder responder = new MockSessionResponder();

        backend.onNativeDestroyed();

        InputPiece[] inputPieces = new InputPiece[] {createTextInputPiece("Hello")};

        backend.getSizeInTokens(inputPieces, responder);

        assertEquals("Should return 0 tokens after destroyed", 0, responder.mTokenCount);
        verify(mMockGenerativeModelFutures, never()).countTokens(any(GenerateContentRequest.class));
    }

    // ==================== onNativeDestroyed() tests ====================

    @Test
    @Feature({"OnDeviceModel"})
    public void testOnNativeDestroyed_ClosesGenerativeModel() {
        MlKitAiCoreSessionBackendImpl backend = createBackend();

        backend.onNativeDestroyed();

        verify(mMockGenerativeModel).close();
    }

    // ==================== Helper class ====================

    /** A simple mock responder to capture callback results. */
    private static class MockSessionResponder implements SessionResponder {
        String mResponse = "";
        @GenerateResult int mResult = -1;
        int mTokenCount = -1;

        @Override
        public void onResponse(String response) {
            mResponse += response;
        }

        @Override
        public void onComplete(@GenerateResult int result) {
            mResult = result;
        }

        @Override
        public void onSizeInTokensResult(int tokenCount) {
            mTokenCount = tokenCount;
        }
    }
}
