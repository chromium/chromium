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
        private final MockAiCoreSettings mSettings;
        private boolean mNativeDestroyed;
        // Below are the params received in the generate() call.
        private SessionResponder mResponder;
        private GenerateOptions mGenerateOptions;
        // Below are the params received in the constructor.
        private final ModelExecutionFeature mFeature;
        private final SessionParams mParams;

        public MockAiCoreSessionBackend(
                ModelExecutionFeature feature, SessionParams params, MockAiCoreSettings settings) {
            mFeature = feature;
            mParams = params;
            mSettings = settings;
        }

        private String placeholder(int token) {
            switch (token) {
                case Token.SYSTEM:
                    return "<system>";
                case Token.MODEL:
                    return "<model>";
                case Token.USER:
                    return "<user>";
                case Token.END:
                    return "<end>";
                default:
                    throw new UnsupportedOperationException("Unsupported token: " + token);
            }
        }

        private String inputToString(InputPiece[] inputPieces) {
            StringBuilder sb = new StringBuilder();
            for (InputPiece inputPiece : inputPieces) {
                switch (inputPiece.which()) {
                    case InputPiece.Tag.Token:
                        sb.append(placeholder(inputPiece.getToken()));
                        break;
                    case InputPiece.Tag.Text:
                        sb.append(inputPiece.getText());
                        break;
                    case InputPiece.Tag.Bitmap:
                        sb.append("<image>");
                        break;
                    default:
                        throw new UnsupportedOperationException(
                                "Unsupported input piece: " + inputPiece.which());
                }
            }
            return sb.toString();
        }

        @Override
        public void generate(
                GenerateOptions generateOptions,
                InputPiece[] inputPieces,
                SessionResponder responder) {
            mGenerateOptions = generateOptions;
            Runnable sendResponses;
            if (mSettings.mExecuteResult.length == 0) {
                sendResponses = () -> responder.onResponse(inputToString(inputPieces));
            } else {
                sendResponses =
                        () -> {
                            for (String result : mSettings.mExecuteResult) {
                                responder.onResponse(result);
                            }
                        };
            }

            if (mSettings.mSessionCallbackOnDifferentThread) {
                new Thread(
                                () -> {
                                    sendResponses.run();
                                    responder.onComplete(mSettings.mGenerateResult);
                                })
                        .start();
                return;
            }
            sendResponses.run();

            if (mSettings.mCompleteAsync) {
                mResponder = responder;
            } else {
                responder.onComplete(mSettings.mGenerateResult);
            }
        }

        @Override
        public void getSizeInTokens(InputPiece[] inputPieces, SessionResponder responder) {
            final int finalTokenSize =
                    mSettings.mSizeInTokens != 0
                            ? mSettings.mSizeInTokens
                            : inputToString(inputPieces).length();
            if (mSettings.mSessionCallbackOnDifferentThread) {
                new Thread(() -> responder.onSizeInTokensResult(finalTokenSize)).start();
            } else {
                responder.onSizeInTokensResult(finalTokenSize);
            }
        }

        @Override
        public void onNativeDestroyed() {
            mNativeDestroyed = true;
        }

        public void resumeOnCompleteCallback() {
            assert mSettings.mCompleteAsync;
            if (mNativeDestroyed) {
                return;
            }
            mResponder.onComplete(mSettings.mGenerateResult);
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
        private boolean mIsModelDownloader;
        private boolean mIsStatusChecker;
        private final MockAiCoreSettings mSettings;

        public MockAiCoreModelDownloaderBackend(
                ModelExecutionFeature feature,
                DownloaderParams params,
                MockAiCoreSettings settings) {
            mFeature = feature;
            mParams = params;
            mSettings = settings;
        }

        @Override
        public void startDownload(DownloaderResponder responder) {
            mIsModelDownloader = true;
            mResponder = responder;

            if (mSettings.mModelInfo != null) {
                responder.onAvailable(mSettings.mModelInfo.mName, mSettings.mModelInfo.mVersion);
            }
        }

        @Override
        public void checkStatus(DownloaderResponder responder) {
            mIsStatusChecker = true;
            if (mSettings.mDefaultStatusCheckResult != -1) {
                responder.onStatusCheckResult(mSettings.mDefaultStatusCheckResult);
                return;
            }
            mResponder = responder;
        }

        @Override
        public void onNativeDestroyed() {
            mNativeDestroyed = true;
        }

        public void onAvailable(String name, String version) {
            if (!mNativeDestroyed) {
                assert mIsModelDownloader;
                if (mSettings.mDownloaderCallbackOnDifferentThread) {
                    new Thread(() -> mResponder.onAvailable(name, version)).start();
                } else {
                    mResponder.onAvailable(name, version);
                }
            }
        }

        public void onUnavailable(@DownloadFailureReason int reason) {
            if (!mNativeDestroyed) {
                assert mIsModelDownloader;
                if (mSettings.mDownloaderCallbackOnDifferentThread) {
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

        public void onStatusCheckResult(@ModelStatus int modelStatus) {
            if (!mNativeDestroyed) {
                assert mIsStatusChecker;
                if (mSettings.mDownloaderCallbackOnDifferentThread) {
                    new Thread(() -> mResponder.onStatusCheckResult(modelStatus)).start();
                } else {
                    mResponder.onStatusCheckResult(modelStatus);
                }
            }
        }

        public void onDownloadProgress(long downloadedBytes, long totalBytes) {
            if (!mNativeDestroyed) {
                assert mIsModelDownloader;
                if (mSettings.mDownloaderCallbackOnDifferentThread) {
                    new Thread(
                                    () -> {
                                        mResponder.onDownloadProgress(downloadedBytes, totalBytes);
                                    })
                            .start();
                } else {
                    mResponder.onDownloadProgress(downloadedBytes, totalBytes);
                }
            }
        }
    }

    /** A mock implementation of AiCoreFactory. */
    public static class MockAiCoreFactory implements AiCoreFactory {
        List<MockAiCoreSessionBackend> mSessionBackends = new ArrayList<>();
        List<MockAiCoreModelDownloaderBackend> mDownloaderBackends = new ArrayList<>();
        private final MockAiCoreSettings mSettings;

        public MockAiCoreFactory(MockAiCoreSettings settings) {
            mSettings = settings;
        }

        @Override
        public AiCoreSessionBackend createSessionBackend(
                ModelExecutionFeature feature, SessionParams params) {
            MockAiCoreSessionBackend sessionBackend =
                    new MockAiCoreSessionBackend(feature, params, mSettings);
            mSessionBackends.add(sessionBackend);
            return sessionBackend;
        }

        @Override
        public AiCoreModelDownloaderBackend createModelDownloader(
                ModelExecutionFeature feature, DownloaderParams params) {
            MockAiCoreModelDownloaderBackend backend =
                    new MockAiCoreModelDownloaderBackend(feature, params, mSettings);
            mDownloaderBackends.add(backend);
            return backend;
        }

        public MockAiCoreSessionBackend getLastSessionBackend() {
            assert !mSessionBackends.isEmpty();
            return mSessionBackends.get(mSessionBackends.size() - 1);
        }

        /** Returns the first model downloader backend. */
        public MockAiCoreModelDownloaderBackend getModelDownloaderBackend() {
            for (MockAiCoreModelDownloaderBackend b : mDownloaderBackends) {
                if (b.mIsModelDownloader) {
                    return b;
                }
            }
            return null;
        }

        /** Returns the first status checker backend. */
        public MockAiCoreModelDownloaderBackend getStatusCheckerBackend() {
            for (MockAiCoreModelDownloaderBackend b : mDownloaderBackends) {
                if (b.mIsStatusChecker) {
                    return b;
                }
            }
            return null;
        }

        /** Returns the number of alive status checker backends. */
        public int getStatusCheckerCount() {
            int count = 0;
            for (MockAiCoreModelDownloaderBackend b : mDownloaderBackends) {
                if (!b.mNativeDestroyed && b.mIsStatusChecker) {
                    count++;
                }
            }
            return count;
        }
    }

    public static class ModelInfo {
        public String mName;
        public String mVersion;

        public ModelInfo(String name, String version) {
            mName = name;
            mVersion = version;
        }
    }

    /** Encapsulates generate result configuration. */
    public static class MockAiCoreSettings {
        public @GenerateResult int mGenerateResult = GenerateResult.SUCCESS;
        // If true, the session responder's onComplete callback will be called asynchronously
        // through resumeOnCompleteCallback.
        public boolean mCompleteAsync;
        // If true, the session responder's callbacks will be called asynchronously through a
        // different thread.
        public boolean mSessionCallbackOnDifferentThread;
        // If true, the downloader responder's callbacks will be called asynchronously through a
        // different thread.
        public boolean mDownloaderCallbackOnDifferentThread;
        // If non-negative, checkStatus() in AiCoreModelDownloaderBackend auto-responds with this
        // result. -1 means unset.
        public int mDefaultStatusCheckResult = -1;
        // If non-zero, getSizeInTokens() returns this value instead of computing from input.
        public int mSizeInTokens;
        // If non-empty, generate() uses these strings as responses instead of echoing input.
        public String[] mExecuteResult = new String[0];
        // If non-null, the downloader backend will call onAvailable with this model info when
        // startDownload is called.
        public ModelInfo mModelInfo;

        @CalledByNative
        public void setSizeInTokens(int sizeInTokens) {
            mSizeInTokens = sizeInTokens;
        }

        @CalledByNative
        public void setGenerateResult(int generateResult) {
            mGenerateResult = generateResult;
        }

        @CalledByNative
        public void setCompleteAsync(boolean completeAsync) {
            mCompleteAsync = completeAsync;
        }

        @CalledByNative
        public void setSessionCallbackOnDifferentThread(boolean sessionCallbackOnDifferentThread) {
            mSessionCallbackOnDifferentThread = sessionCallbackOnDifferentThread;
        }

        @CalledByNative
        public void setDownloaderCallbackOnDifferentThread(
                boolean downloaderCallbackOnDifferentThread) {
            mDownloaderCallbackOnDifferentThread = downloaderCallbackOnDifferentThread;
        }

        @CalledByNative
        public void setDefaultStatusCheckResult(int defaultStatusCheckResult) {
            mDefaultStatusCheckResult = defaultStatusCheckResult;
        }

        @CalledByNative
        public void setExecuteResult(String[] executeResult) {
            mExecuteResult = executeResult;
        }
    }

    private MockAiCoreFactory mMockAiCoreFactory;
    private final MockAiCoreSettings mSettings = new MockAiCoreSettings();

    @CalledByNative
    public static OnDeviceModelBridgeNativeUnitTestHelper create() {
        return new OnDeviceModelBridgeNativeUnitTestHelper();
    }

    @CalledByNative
    public MockAiCoreSettings getMockAiCoreSettings() {
        return mSettings;
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
        MockAiCoreModelDownloaderBackend downloaderBackend =
                mMockAiCoreFactory.getModelDownloaderBackend();
        assertEquals(modelExecutionFeatureId, downloaderBackend.mFeature);
        assertEquals(requirePersistentMode, downloaderBackend.mParams.requirePersistentMode);
    }

    /**
     * Sets a default AiCoreFactory that uses upstream (dummy) implementations. This factory returns
     * AiCoreSessionBackendUpstreamImpl and AiCoreModelDownloaderBackendUpstreamImpl which report
     * API_NOT_AVAILABLE. Use this for tests that need to verify behavior when MLKit is not
     * available.
     */
    @CalledByNative
    public static void setDefaultAiCoreFactory() {
        ServiceLoaderUtil.setInstanceForTesting(
                AiCoreFactory.class,
                new AiCoreFactory() {
                    @Override
                    public AiCoreSessionBackend createSessionBackend(
                            ModelExecutionFeature feature, SessionParams params) {
                        return new AiCoreSessionBackendUpstreamImpl();
                    }

                    @Override
                    public AiCoreModelDownloaderBackend createModelDownloader(
                            ModelExecutionFeature feature, DownloaderParams params) {
                        return new AiCoreModelDownloaderBackendUpstreamImpl();
                    }
                });
    }

    @CalledByNative
    public void setMockAiCoreFactory() {
        mMockAiCoreFactory = new MockAiCoreFactory(mSettings);
        ServiceLoaderUtil.setInstanceForTesting(AiCoreFactory.class, mMockAiCoreFactory);
    }

    @CalledByNative
    public void resumeOnCompleteCallback() {
        mMockAiCoreFactory.getLastSessionBackend().resumeOnCompleteCallback();
    }

    @CalledByNative
    public void unInstallModel() {
        mSettings.mModelInfo = null;
    }

    @CalledByNative
    public void triggerDownloaderOnAvailable(String name, String version) {
        if (mMockAiCoreFactory.getModelDownloaderBackend() != null) {
            mMockAiCoreFactory.getModelDownloaderBackend().onAvailable(name, version);
        } else {
            // cache the model info in settings so that when startDownload is called, the backend
            // can respond with this info.
            mSettings.mModelInfo = new ModelInfo(name, version);
        }
    }

    @CalledByNative
    public void triggerDownloaderOnUnavailable(int reason) {
        mMockAiCoreFactory.getModelDownloaderBackend().onUnavailable(reason);
    }

    @CalledByNative
    public void triggerDownloaderOnDownloadProgress(long downloadedBytes, long totalBytes) {
        mMockAiCoreFactory
                .getModelDownloaderBackend()
                .onDownloadProgress(downloadedBytes, totalBytes);
    }

    @CalledByNative
    public void triggerDownloaderOnStatusCheckResult(int modelStatus) {
        mMockAiCoreFactory.getStatusCheckerBackend().onStatusCheckResult(modelStatus);
    }

    @CalledByNative
    public int getStatusCheckerCount() {
        return mMockAiCoreFactory.getStatusCheckerCount();
    }

    /**
     * Triggers onStatusCheckResult on all status checker backends. This is useful for testing the
     * BarrierClosure that waits for all AICore features' status checks to complete before firing
     * init callbacks.
     */
    @CalledByNative
    public void triggerAllDownloadersOnStatusCheckResult(int modelStatus) {
        List<MockAiCoreModelDownloaderBackend> downloadersSnapshot =
                new ArrayList<>(mMockAiCoreFactory.mDownloaderBackends);
        for (MockAiCoreModelDownloaderBackend backend : downloadersSnapshot) {
            if (!backend.mNativeDestroyed && backend.mIsStatusChecker) {
                backend.onStatusCheckResult(modelStatus);
            }
        }
    }
}
