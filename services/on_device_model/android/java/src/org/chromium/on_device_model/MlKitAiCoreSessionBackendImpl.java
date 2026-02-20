// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.mlkit.genai.common.GenAiException;
import com.google.mlkit.genai.common.StreamingCallback;
import com.google.mlkit.genai.prompt.CountTokensResponse;
import com.google.mlkit.genai.prompt.GenerateContentRequest;
import com.google.mlkit.genai.prompt.GenerateContentResponse;
import com.google.mlkit.genai.prompt.GenerativeModel;
import com.google.mlkit.genai.prompt.TextPart;
import com.google.mlkit.genai.prompt.java.GenerativeModelFutures;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.on_device_model.mojom.GenerateOptions;
import org.chromium.on_device_model.mojom.InputPiece;
import org.chromium.on_device_model.mojom.SessionParams;

import java.util.concurrent.Executor;

/**
 * Implementation of AiCoreSessionBackend using MLKit Prompt APIs. This backend uses GenerativeModel
 * to process text generation requests.
 */
@NullMarked
class MlKitAiCoreSessionBackendImpl implements AiCoreSessionBackend {
    private final GenerativeModel mGenerativeModel;
    private final GenerativeModelFutures mGenerativeModelFutures;
    private final Executor mExecutor;
    private final SessionParams mSessionParams;

    // Flag to indicate if the native counterpart has been destroyed.
    private boolean mIsDestroyed;

    MlKitAiCoreSessionBackendImpl(GenerativeModel generativeModel, SessionParams params) {
        this(
                generativeModel,
                GenerativeModelFutures.from(generativeModel),
                params,
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
    MlKitAiCoreSessionBackendImpl(
            GenerativeModel generativeModel,
            GenerativeModelFutures generativeModelFutures,
            SessionParams params,
            Executor executor) {
        mGenerativeModel = generativeModel;
        mGenerativeModelFutures = generativeModelFutures;
        mSessionParams = params;
        mExecutor = executor;
    }

    @Override
    public void generate(
            GenerateOptions generateOptions, InputPiece[] inputPieces, SessionResponder responder) {
        if (mIsDestroyed) {
            responder.onComplete(GenerateResult.UNKNOWN_ERROR);
            return;
        }

        String prompt = buildPromptFromInputPieces(inputPieces);
        if (prompt.isEmpty()) {
            // Empty request prompt, return request processing error.
            responder.onComplete(GenerateResult.INFERENCE_REQUEST_PROCESSING_ERROR);
            return;
        }

        // Create streaming callback for partial results.
        StreamingCallback streamingCallback =
                new StreamingCallback() {
                    @Override
                    public void onNewText(String partialResult) {
                        if (!mIsDestroyed) {
                            responder.onResponse(partialResult);
                        }
                    }
                };

        // Build GenerateContentRequest with prompt and generation options.
        GenerateContentRequest request;
        try {
            request = buildGenerateContentRequest(prompt, generateOptions);
        } catch (IllegalArgumentException e) {
            // Building the request failed due to invalid arguments (e.g., maxOutputTokens > 256).
            responder.onComplete(GenerateResult.INVALID_REQUEST_ARGUMENT_ERROR);
            return;
        } catch (Exception e) {
            // Building the request failed for other reasons.
            responder.onComplete(GenerateResult.INFERENCE_REQUEST_PROCESSING_ERROR);
            return;
        }

        // Generate content using MLKit with streaming callback
        ListenableFuture<GenerateContentResponse> future =
                mGenerativeModelFutures.generateContent(request, streamingCallback);

        Futures.addCallback(
                future,
                new FutureCallback<GenerateContentResponse>() {
                    @Override
                    public void onSuccess(GenerateContentResponse result) {
                        if (!mIsDestroyed) {
                            if (result != null
                                    && result.getCandidates() != null
                                    && !result.getCandidates().isEmpty()) {
                                // Generation completes successfully for all finish reasons even
                                // for max tokens reached reason.
                                // TODO(crbug.com/477033510): Consider defining a specific result
                                // code for max tokens reached finish reason if needed.
                                responder.onComplete(GenerateResult.SUCCESS);
                            } else {
                                responder.onComplete(GenerateResult.INFERENCE_GENERAL_ERROR);
                            }
                        }
                    }

                    @Override
                    public void onFailure(Throwable t) {
                        if (!mIsDestroyed) {
                            int result =
                                    (t instanceof GenAiException)
                                            ? mapGenAiExceptionToGenerateResult((GenAiException) t)
                                            : GenerateResult.INFERENCE_GENERAL_ERROR;
                            responder.onComplete(result);
                        }
                    }
                },
                mExecutor);
    }

    @Override
    public void getSizeInTokens(InputPiece[] inputPieces, SessionResponder responder) {
        if (mIsDestroyed) {
            responder.onSizeInTokensResult(0);
            return;
        }

        String prompt = buildPromptFromInputPieces(inputPieces);
        if (prompt.isEmpty()) {
            // Empty prompt, return 0 tokens.
            responder.onSizeInTokensResult(0);
            return;
        }

        // Build GenerateContentRequest with text input.
        GenerateContentRequest request =
                new GenerateContentRequest.Builder(new TextPart(prompt)).build();

        // Count tokens using MLKit API.
        ListenableFuture<CountTokensResponse> countTokensFuture =
                mGenerativeModelFutures.countTokens(request);

        Futures.addCallback(
                countTokensFuture,
                new FutureCallback<CountTokensResponse>() {
                    @Override
                    public void onSuccess(CountTokensResponse result) {
                        if (!mIsDestroyed && result != null) {
                            responder.onSizeInTokensResult(result.getTotalTokens());
                        } else {
                            responder.onSizeInTokensResult(0);
                        }
                    }

                    @Override
                    public void onFailure(Throwable t) {
                        // Return 0 on failure to maintain consistency.
                        responder.onSizeInTokensResult(0);
                    }
                },
                mExecutor);
    }

    @Override
    public void onNativeDestroyed() {
        mIsDestroyed = true;

        // Close the GenerativeModel instance to release resources when the native downloader is
        // destroyed.
        mGenerativeModel.close();
    }

    /**
     * Builds a text prompt from an array of InputPieces by concatenating text pieces.
     *
     * @param inputPieces Array of input pieces to process
     * @return The concatenated text prompt
     */
    private String buildPromptFromInputPieces(InputPiece[] inputPieces) {
        StringBuilder promptBuilder = new StringBuilder();
        for (InputPiece piece : inputPieces) {
            // InputPiece.Tag includes four types: Token, Text, Bitmap and Audio. Token, Bitmap and
            // Audio inputs are not supported in AiCore currently. Text + Image combination is
            // supported in AiCore, but not required by on-device web APIs. So far we only handle
            // Text type.
            switch (piece.which()) {
                case InputPiece.Tag.Text:
                    promptBuilder.append(piece.getText());
                    break;
                case InputPiece.Tag.Token:
                    // TODO(crbug.com/477033510): Handle token markers for conversation structure.
                    // MLKit Prompt API doesn't support structured Content with roles, so we need
                    // to determine the best way to represent these tokens.
                    break;
                case InputPiece.Tag.Bitmap:
                case InputPiece.Tag.Audio:
                default:
                    // TODO(crbug.com/425408635): Support image and audio input.
                    // Unsupported types are ignored for now.
                    break;
            }
        }
        return promptBuilder.toString();
    }

    /**
     * Builds a GenerateContentRequest with configured parameters.
     *
     * @param prompt The text prompt for generation
     * @param generateOptions Optional generation options
     * @return The built GenerateContentRequest
     * @throws IllegalArgumentException if request parameters are invalid
     */
    private GenerateContentRequest buildGenerateContentRequest(
            String prompt, GenerateOptions generateOptions) {
        GenerateContentRequest.Builder requestBuilder =
                new GenerateContentRequest.Builder(new TextPart(prompt));

        // Configure sampling parameters from SessionParams.
        if (mSessionParams != null) {
            requestBuilder.setTemperature(mSessionParams.temperature);
            requestBuilder.setTopK(mSessionParams.topK);
        }

        // Configure additional parameters from GenerateOptions if provided.
        if (generateOptions != null) {
            requestBuilder.setMaxOutputTokens(generateOptions.maxOutputTokens);
        }

        return requestBuilder.build();
    }

    /**
     * Maps GenAiException error codes to GenerateResult values.
     *
     * <p>GenAiException error code reference:
     * https://developers.google.com/android/reference/com/google/mlkit/genai/common/GenAiException.ErrorCode
     *
     * @param e The GenAiException from the generation failure
     * @return The corresponding GenerateResult
     */
    @GenerateResult
    private int mapGenAiExceptionToGenerateResult(GenAiException e) {
        int errorCode = e.getErrorCode();

        switch (errorCode) {
            case -101: // AICORE_INCOMPATIBLE: AICore is not installed or its version is too low.
                return GenerateResult.GET_FEATURE_ERROR;

            case 8: // NOT_AVAILABLE: Feature is not available.
                return GenerateResult.FEATURE_IS_NULL;

            case 4: // REQUEST_PROCESSING_ERROR: Request doesn't pass certain policy check.
            case 12: // REQUEST_TOO_LARGE: Request is too large to be processed.
            case -100: // REQUEST_TOO_SMALL: Request is too small to be processed.
                return GenerateResult.INFERENCE_REQUEST_PROCESSING_ERROR;

            case 11: // RESPONSE_PROCESSING_ERROR: Generated response doesn't pass certain
            // policy check.
            case 15: // RESPONSE_GENERATION_ERROR: The model cannot generate a response due to
                // request not passing certain policy check.
                return GenerateResult.INFERENCE_RESPONSE_PROCESSING_ERROR;

            case 7: // CANCELLED: The request is cancelled.
            case 9: // BUSY: The service is currently busy.
            case 27: // PER_APP_BATTERY_USE_QUOTA_EXCEEDED: A long-duration quota for the
            // calling app's uid has been exceeded.
            case 30: // BACKGROUND_USE_BLOCKED: Background usage is blocked.
            case 501: // NOT_ENOUGH_DISK_SPACE: Not enough storage.
            case 604: // NEEDS_SYSTEM_UPDATE: Android version is too low.
            case -102: // INVALID_INPUT_IMAGE: Invalid input image.
            case -103: // CACHE_PROCESSING_ERROR: Error during cache processing for prompt prefix.
                return GenerateResult.INFERENCE_GENERAL_ERROR;

            default:
                return GenerateResult.INFERENCE_GENERAL_ERROR;
        }
    }
}
