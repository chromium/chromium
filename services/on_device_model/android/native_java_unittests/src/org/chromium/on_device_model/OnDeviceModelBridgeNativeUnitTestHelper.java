// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.on_device_model;

import org.jni_zero.CalledByNative;

import org.chromium.base.ServiceLoaderUtil;

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
        @Override
        public void generate(long nativeBackendSession, InputPiece[] inputPieces) {
            StringBuilder sb = new StringBuilder();
            for (InputPiece piece : inputPieces) {
                if (piece.isText()) {
                    sb.append(piece.getText());
                } else if (piece.isToken()) {
                    switch (piece.getTokenId()) {
                        case InputPiece.Token.SYSTEM:
                            sb.append("<system>");
                            break;
                        case InputPiece.Token.MODEL:
                            sb.append("<model>");
                            break;
                        case InputPiece.Token.USER:
                            sb.append("<user>");
                            break;
                        case InputPiece.Token.END:
                            sb.append("<end>");
                            break;
                    }
                }
            }
            AiCoreSessionJni.get().onResponse(nativeBackendSession, sb.toString());
            AiCoreSessionJni.get().onComplete(nativeBackendSession);
        }
    }

    /** A mock implementation of AiCoreSessionFactory. */
    public static class MockAiCoreSessionFactory implements AiCoreSessionFactory {
        MockAiCoreSession mSession;

        public MockAiCoreSessionFactory() {
            mSession = new MockAiCoreSession();
        }

        @Override
        public AiCoreSession createSession() {
            return mSession;
        }
    }

    private MockAiCoreSessionFactory mMockAiCoreSessionFactory;

    @CalledByNative
    public void setMockAiCoreSessionFactory() {
        mMockAiCoreSessionFactory = new MockAiCoreSessionFactory();
        ServiceLoaderUtil.setInstanceForTesting(
                AiCoreSessionFactory.class, mMockAiCoreSessionFactory);
    }

    @CalledByNative
    public static OnDeviceModelBridgeNativeUnitTestHelper create() {
        return new OnDeviceModelBridgeNativeUnitTestHelper();
    }
}
