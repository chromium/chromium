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
     * A mock implementation of AiCoreSession. Echoes the input back as the response.
     */
    public static class MockAiCoreSession implements AiCoreSession {
        @Override
        public void generate(long nativeBackendSession, String input) {
            AiCoreSessionJni.get().onResponse(nativeBackendSession, input);
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
