// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.on_device_model;

import static org.junit.Assert.assertEquals;

import org.jni_zero.CalledByNative;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.components.optimization_guide.proto.ModelExecutionProto.ModelExecutionFeature;
import org.chromium.on_device_model.mojom.DownloaderParams;
import org.chromium.on_device_model.mojom.GenerateOptions;
import org.chromium.on_device_model.mojom.InputPiece;
import org.chromium.on_device_model.mojom.SessionParams;
import org.chromium.on_device_model.mojom.Token;

import java.util.ArrayList;
import java.util.List;

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
        // If true, the callbacks will be called asynchronously through a different thread. This
        // field should be set before generate() is called.
        private boolean mCallbackOnDifferentThread;
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
            if (mCallbackOnDifferentThread) {
                new Thread(
                                () -> {
                                    responder.onResponse(sb.toString());
                                    responder.onComplete(mGenerateResult);
                                })
                        .start();
                return;
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
     * A mock implementation of AiCoreModelDownloaderBackend. Call onAvailable() or onUnavailable()
     * to simulate the download status change.
     */
    public static class MockAiCoreModelDownloaderBackend implements AiCoreModelDownloaderBackend {
        // Below are the params received in the constructor.
        private final ModelExecutionFeature mFeature;
        private final DownloaderParams mParams;

        private DownloaderResponder mResponder;
        private boolean mNativeDestroyed;
        // If true, the callbacks will be called asynchronously through a different thread. This
        // field should be set before startDownload() is called.
        private boolean mCallbackOnDifferentThread;

        public MockAiCoreModelDownloaderBackend(
                ModelExecutionFeature feature, DownloaderParams params) {
            mFeature = feature;
            mParams = params;
        }

        @Override
        public void startDownload(DownloaderResponder responder) {
            mResponder = responder;
        }

        @Override
        public void onNativeDestroyed() {
            mNativeDestroyed = true;
        }

        public void onAvailable(String name, String version) {
            if (!mNativeDestroyed) {
                if (mCallbackOnDifferentThread) {
                    new Thread(() -> mResponder.onAvailable(name, version)).start();
                } else {
                    mResponder.onAvailable(name, version);
                }
            }
        }

        public void onUnavailable(@DownloadFailureReason int reason) {
            if (!mNativeDestroyed) {
                if (mCallbackOnDifferentThread) {
                    new Thread(
                                    () -> {
                                        mResponder.onUnavailable(reason);
                                    })
                            .start();
                } else {
                    mResponder.onUnavailable(reason);
                }
            }
        }
    }

    /** A mock implementation of AiCoreFactory. */
    public static class MockAiCoreFactory implements AiCoreFactory {
        List<MockAiCoreSessionBackend> mSessionBackends = new ArrayList<>();
        MockAiCoreModelDownloaderBackend mDownloaderBackend;

        public MockAiCoreFactory() {}

        @Override
        public AiCoreSessionBackend createSessionBackend(
                ModelExecutionFeature feature, SessionParams params) {
            MockAiCoreSessionBackend sessionBackend = new MockAiCoreSessionBackend(feature, params);
            mSessionBackends.add(sessionBackend);
            return sessionBackend;
        }

        @Override
        public AiCoreModelDownloaderBackend createModelDownloader(
                ModelExecutionFeature feature, DownloaderParams params) {
            mDownloaderBackend = new MockAiCoreModelDownloaderBackend(feature, params);
            return mDownloaderBackend;
        }

        public MockAiCoreSessionBackend getLastSessionBackend() {
            assert !mSessionBackends.isEmpty();
            return mSessionBackends.get(mSessionBackends.size() - 1);
        }
    }

    private MockAiCoreFactory mMockAiCoreFactory;

    @CalledByNative
    public static OnDeviceModelBridgeNativeUnitTestHelper create() {
        return new OnDeviceModelBridgeNativeUnitTestHelper();
    }

    @CalledByNative
    public void verifySessionParams(int index, int feature, int topK, float temperature) {
        ModelExecutionFeature modelExecutionFeatureId = ModelExecutionFeature.forNumber(feature);
        MockAiCoreSessionBackend sessionBackend = mMockAiCoreFactory.mSessionBackends.get(index);
        assertEquals(modelExecutionFeatureId, sessionBackend.mFeature);
        SessionParams params = sessionBackend.mParams;
        assertEquals(topK, params.topK);
        assertEquals(temperature, params.temperature, 0.01f);
    }

    @CalledByNative
    public void verifyGenerateOptions(int index, int maxOutputTokens) {
        GenerateOptions generateOptions =
                mMockAiCoreFactory.mSessionBackends.get(index).mGenerateOptions;
        assertEquals(maxOutputTokens, generateOptions.maxOutputTokens);
    }

    @CalledByNative
    public void verifyDownloaderParams(int feature, boolean requirePersistentMode) {
        ModelExecutionFeature modelExecutionFeatureId = ModelExecutionFeature.forNumber(feature);
        MockAiCoreModelDownloaderBackend downloaderBackend = mMockAiCoreFactory.mDownloaderBackend;
        assertEquals(modelExecutionFeatureId, downloaderBackend.mFeature);
        assertEquals(requirePersistentMode, downloaderBackend.mParams.requirePersistentMode);
    }

    @CalledByNative
    public void setMockAiCoreFactory() {
        mMockAiCoreFactory = new MockAiCoreFactory();
        ServiceLoaderUtil.setInstanceForTesting(AiCoreFactory.class, mMockAiCoreFactory);
    }

    @CalledByNative
    public void setCompleteAsync() {
        mMockAiCoreFactory.getLastSessionBackend().mCompleteAsync = true;
    }

    @CalledByNative
    public void setCallbackOnDifferentThread() {
        mMockAiCoreFactory.getLastSessionBackend().mCallbackOnDifferentThread = true;
    }

    @CalledByNative
    public void resumeOnCompleteCallback() {
        mMockAiCoreFactory.getLastSessionBackend().resumeOnCompleteCallback();
    }

    @CalledByNative
    public void setGenerateResult(int generateResult) {
        mMockAiCoreFactory.getLastSessionBackend().mGenerateResult = generateResult;
    }

    @CalledByNative
    public void setDownloaderCallbackOnDifferentThread() {
        mMockAiCoreFactory.mDownloaderBackend.mCallbackOnDifferentThread = true;
    }

    @CalledByNative
    public void triggerDownloaderOnAvailable(String name, String version) {
        mMockAiCoreFactory.mDownloaderBackend.onAvailable(name, version);
    }

    @CalledByNative
    public void triggerDownloaderOnUnavailable(int reason) {
        mMockAiCoreFactory.mDownloaderBackend.onUnavailable(reason);
    }
}
