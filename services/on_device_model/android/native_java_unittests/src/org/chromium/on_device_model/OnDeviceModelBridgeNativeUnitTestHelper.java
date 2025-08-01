// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.on_device_model;

import static org.junit.Assert.assertEquals;

import org.jni_zero.CalledByNative;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.components.optimization_guide.proto.ModelExecutionProto.ModelExecutionFeature;
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
    public static class MockAiCoreSession implements AiCoreSession {
        // If true, the onComplete callback will be called asynchronously through
        // resumeOnCompleteCallback.
        private boolean mCompleteAsync;
        private boolean mNativeDestroyed;
        private long mNativeBackendSession;
        private final ModelExecutionFeature mFeature;
        private final SessionParams mParams;

        public MockAiCoreSession(ModelExecutionFeature feature, SessionParams params) {
            mFeature = feature;
            mParams = params;
        }

        @Override
        public void generate(long nativeBackendSession, Object[] inputPieces) {
            StringBuilder sb = new StringBuilder();
            for (Object piece : inputPieces) {
                assert piece instanceof InputPiece;
                InputPiece inputPiece = (InputPiece) piece;
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
            AiCoreSessionJni.get().onResponse(nativeBackendSession, sb.toString());
            if (mCompleteAsync) {
                // Safe the native backend session pointer for later.
                mNativeBackendSession = nativeBackendSession;
            } else {
                AiCoreSessionJni.get().onComplete(nativeBackendSession);
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
            AiCoreSessionJni.get().onComplete(mNativeBackendSession);
        }
    }

    /** A mock implementation of AiCoreSessionFactory. */
    public static class MockAiCoreSessionFactory implements AiCoreSessionFactory {
        MockAiCoreSession mSession;

        public MockAiCoreSessionFactory() {}

        @Override
        public AiCoreSession createSession(ModelExecutionFeature feature, SessionParams params) {
            mSession = new MockAiCoreSession(feature, params);
            return mSession;
        }
    }

    private MockAiCoreSessionFactory mMockAiCoreSessionFactory;

    @CalledByNative
    public void verifySessionParams(int feature, int topK, float temperature) {
        ModelExecutionFeature modelExecutionFeatureId = ModelExecutionFeature.forNumber(feature);
        assertEquals(modelExecutionFeatureId, mMockAiCoreSessionFactory.mSession.mFeature);
        SessionParams params = mMockAiCoreSessionFactory.mSession.mParams;
        assertEquals(topK, params.topK);
        assertEquals(temperature, params.temperature, 0.01f);
    }

    @CalledByNative
    public void setMockAiCoreSessionFactory() {
        mMockAiCoreSessionFactory = new MockAiCoreSessionFactory();
        ServiceLoaderUtil.setInstanceForTesting(
                AiCoreSessionFactory.class, mMockAiCoreSessionFactory);
    }

    @CalledByNative
    public void setCompleteAsync() {
        mMockAiCoreSessionFactory.mSession.mCompleteAsync = true;
    }

    @CalledByNative
    public void resumeOnCompleteCallback() {
        mMockAiCoreSessionFactory.mSession.resumeOnCompleteCallback();
    }

    @CalledByNative
    public static OnDeviceModelBridgeNativeUnitTestHelper create() {
        return new OnDeviceModelBridgeNativeUnitTestHelper();
    }
}
