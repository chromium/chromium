// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.on_device_model;

import static org.junit.Assert.assertEquals;

import org.jni_zero.CalledByNative;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.components.optimization_guide.proto.ModelExecutionProto.ModelExecutionFeature;
import org.chromium.on_device_model.mojom.GenerateOptions;
import org.chromium.on_device_model.mojom.InputPiece;
import org.chromium.on_device_model.mojom.SessionParams;
import org.chromium.on_device_model.mojom.Token;

/**
 * Helper class to verify the JNI bridge. Invoked by native unit tests:
 * (services/on_device_model/android/backend_session_impl_android_unittest.cc).
 */
public class OnDeviceModelBridgeNativeUnitTestHelper {
    /**
     * A mock implementation of AiCoreSession. Parses the input to a string and echoes the input
     * back as the response.
     */
    public static class MockAiCoreSessionBackend implements AiCoreSessionBackend {
        // If true, the onComplete callback will be called asynchronously through
        // resumeOnCompleteCallback. This field should be set before generate() is called.
        private boolean mCompleteAsync;
        private @GenerateResult int mGenerateResult;
        private boolean mNativeDestroyed;
        // Below are the params received in the generate() call.
        private SessionResponder mResponder;
        private GenerateOptions mGenerateOptions;
        // Below are the params received in the constructor.
        private final ModelExecutionFeature mFeature;
        private final SessionParams mParams;

        public MockAiCoreSessionBackend(ModelExecutionFeature feature, SessionParams params) {
            mFeature = feature;
            mParams = params;
            mGenerateResult = GenerateResult.SUCCESS;
        }

        @Override
        public void generate(
                GenerateOptions generateOptions,
                InputPiece[] inputPieces,
                SessionResponder responder) {
            mGenerateOptions = generateOptions;
            StringBuilder sb = new StringBuilder();
            for (InputPiece inputPiece : inputPieces) {
                switch (inputPiece.which()) {
                    case InputPiece.Tag.Token:
                        switch (inputPiece.getToken()) {
                            case Token.SYSTEM:
                                sb.append("<system>");
                                break;
                            case Token.MODEL:
                                sb.append("<model>");
                                break;
                            case Token.USER:
                                sb.append("<user>");
                                break;
                            case Token.END:
                                sb.append("<end>");
                                break;
                        }
                        break;
                    case InputPiece.Tag.Text:
                        sb.append(inputPiece.getText());
                        break;
                }
            }
            responder.onResponse(sb.toString());
            if (mCompleteAsync) {
                mResponder = responder;
            } else {
                responder.onComplete(mGenerateResult);
            }
        }

        @Override
        public void onNativeDestroyed() {
            mNativeDestroyed = true;
        }

        public void resumeOnCompleteCallback() {
            assert mCompleteAsync;
            if (mNativeDestroyed) {
                return;
            }
            mResponder.onComplete(mGenerateResult);
        }
    }

    /**
     * A mock implementation of AiCoreModelDownloader. Call onAvailable() or onUnavailable() to
     * simulate the download status change.
     */
    public static class MockAiCoreModelDownloader implements AiCoreModelDownloader {
        private long mNativeModelDownloaderAndroid;

        @Override
        public void startDownload(long nativeModelDownloaderAndroid) {
            mNativeModelDownloaderAndroid = nativeModelDownloaderAndroid;
        }

        @Override
        public void onNativeDestroyed() {
            mNativeModelDownloaderAndroid = 0;
        }

        public void onAvailable() {
            if (mNativeModelDownloaderAndroid != 0) {
                AiCoreModelDownloaderJni.get().onAvailable(mNativeModelDownloaderAndroid);
            }
        }

        public void onUnavailable() {
            if (mNativeModelDownloaderAndroid != 0) {
                AiCoreModelDownloaderJni.get().onUnavailable(mNativeModelDownloaderAndroid);
            }
        }
    }

    /** A mock implementation of AiCoreFactory. */
    public static class MockAiCoreFactory implements AiCoreFactory {
        MockAiCoreSessionBackend mSessionBackend;
        MockAiCoreModelDownloader mDownloader;

        public MockAiCoreFactory() {}

        @Override
        public AiCoreSessionBackend createSessionBackend(
                ModelExecutionFeature feature, SessionParams params) {
            mSessionBackend = new MockAiCoreSessionBackend(feature, params);
            return mSessionBackend;
        }

        @Override
        public AiCoreModelDownloader createModelDownloader(ModelExecutionFeature feature) {
            mDownloader = new MockAiCoreModelDownloader();
            return mDownloader;
        }
    }

    private MockAiCoreFactory mMockAiCoreFactory;

    @CalledByNative
    public static OnDeviceModelBridgeNativeUnitTestHelper create() {
        return new OnDeviceModelBridgeNativeUnitTestHelper();
    }

    @CalledByNative
    public void verifySessionParams(int feature, int topK, float temperature) {
        ModelExecutionFeature modelExecutionFeatureId = ModelExecutionFeature.forNumber(feature);
        assertEquals(modelExecutionFeatureId, mMockAiCoreFactory.mSessionBackend.mFeature);
        SessionParams params = mMockAiCoreFactory.mSessionBackend.mParams;
        assertEquals(topK, params.topK);
        assertEquals(temperature, params.temperature, 0.01f);
    }

    @CalledByNative
    public void verifyGenerateOptions(int maxOutputTokens) {
        GenerateOptions generateOptions = mMockAiCoreFactory.mSessionBackend.mGenerateOptions;
        assertEquals(maxOutputTokens, generateOptions.maxOutputTokens);
    }

    @CalledByNative
    public void setMockAiCoreFactory() {
        mMockAiCoreFactory = new MockAiCoreFactory();
        ServiceLoaderUtil.setInstanceForTesting(AiCoreFactory.class, mMockAiCoreFactory);
    }

    @CalledByNative
    public void setCompleteAsync() {
        mMockAiCoreFactory.mSessionBackend.mCompleteAsync = true;
    }

    @CalledByNative
    public void resumeOnCompleteCallback() {
        mMockAiCoreFactory.mSessionBackend.resumeOnCompleteCallback();
    }

    @CalledByNative
    public void setGenerateResult(int generateResult) {
        mMockAiCoreFactory.mSessionBackend.mGenerateResult = generateResult;
    }

    @CalledByNative
    public void triggerDownloaderOnAvailable() {
        mMockAiCoreFactory.mDownloader.onAvailable();
    }

    @CalledByNative
    public void triggerDownloaderOnUnavailable() {
        mMockAiCoreFactory.mDownloader.onUnavailable();
    }
}
