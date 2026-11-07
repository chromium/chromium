// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.media.MediaCrypto;
import android.media.MediaDrm;

import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.media.MediaDrmStorageBridge.NativeMediaDrmStorageBridge;
import org.chromium.media.MediaDrmStorageBridge.PersistentInfo;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link MediaDrmStorageBridge} and {@link MediaDrmBridge#create} error cleanup. */
@RunWith(BaseRobolectricTestRunner.class)
public class MediaDrmStorageBridgeTest {
    private FakeMediaDrmStorageBridgeNatives mStorageNatives;
    private AtomicInteger mLastCreateError;

    private static class FakeMediaDrmStorageBridgeNatives implements MediaDrmStorageBridge.Natives {
        int mProvisionedCalls;
        int mLoadInfoCalls;
        int mSaveInfoCalls;
        int mClearInfoCalls;

        @Override
        public void onProvisioned(JniPtr<NativeMediaDrmStorageBridge> self, Callback<Boolean> cb) {
            mProvisionedCalls++;
            cb.onResult(true);
        }

        @Override
        public void onLoadInfo(
                JniPtr<NativeMediaDrmStorageBridge> self,
                byte[] sessionId,
                Callback<PersistentInfo> cb) {
            mLoadInfoCalls++;
            cb.onResult(new PersistentInfo(sessionId, new byte[] {1}, "video/mp4", 2));
        }

        @Override
        public void onSaveInfo(
                JniPtr<NativeMediaDrmStorageBridge> self,
                PersistentInfo info,
                Callback<Boolean> cb) {
            mSaveInfoCalls++;
            cb.onResult(true);
        }

        @Override
        public void onClearInfo(
                JniPtr<NativeMediaDrmStorageBridge> self, byte[] sessionId, Callback<Boolean> cb) {
            mClearInfoCalls++;
            cb.onResult(true);
        }
    }

    @Before
    public void setUp() {
        mStorageNatives = new FakeMediaDrmStorageBridgeNatives();
        MediaDrmStorageBridgeJni.setInstanceForTesting(mStorageNatives);
        mLastCreateError = new AtomicInteger(-1);
        MediaDrmBridgeJni.setInstanceForTesting(
                new MediaDrmBridge.Natives() {
                    @Override
                    public void onMediaCryptoReady(
                            long nativeMediaDrmBridge, MediaCrypto mediaCrypto) {}

                    @Override
                    public void onProvisionRequest(
                            long nativeMediaDrmBridge, String defaultUrl, byte[] requestData) {}

                    @Override
                    public void onProvisioningComplete(
                            long nativeMediaDrmBridge, boolean success) {}

                    @Override
                    public void onPromiseResolved(long nativeMediaDrmBridge, long promiseId) {}

                    @Override
                    public void onPromiseResolvedWithSession(
                            long nativeMediaDrmBridge, long promiseId, byte[] emeSessionId) {}

                    @Override
                    public void onPromiseRejected(
                            long nativeMediaDrmBridge,
                            long promiseId,
                            long systemCode,
                            String errorMessage) {}

                    @Override
                    public void onSessionMessage(
                            long nativeMediaDrmBridge,
                            byte[] emeSessionId,
                            int requestType,
                            byte[] message) {}

                    @Override
                    public void onSessionClosed(
                            long nativeMediaDrmBridge, byte[] emeSessionId, int reason) {}

                    @Override
                    public void onSessionKeysChange(
                            long nativeMediaDrmBridge,
                            byte[] emeSessionId,
                            Object[] keysInfo,
                            boolean hasAdditionalUsableKey,
                            boolean isKeyRelease) {}

                    @Override
                    public void onSessionExpirationUpdate(
                            long nativeMediaDrmBridge, byte[] emeSessionId, long expirationTime) {}

                    @Override
                    public void onCreateError(long nativeMediaDrmBridge, int errorCode) {
                        mLastCreateError.set(errorCode);
                    }
                });
    }

    @Test
    public void testCreateFailureReleasesStoragePointer() {
        JniRawPtr<NativeMediaDrmStorageBridge> storagePtr = JniRawPtr.createForTesting(1L);
        MediaDrmBridge bridge =
                MediaDrmBridge.create(
                        new byte[0],
                        "",
                        MediaDrm.SECURITY_LEVEL_UNKNOWN,
                        "test",
                        false,
                        100L,
                        storagePtr);
        assertNull(bridge);
        assertEquals(MediaDrmCreateError.UNSUPPORTED_DRM_SCHEME, mLastCreateError.get());
        // BaseRobolectricTestRule asserts that storagePtr was released via LifetimeAssert.
    }

    @Test
    public void testStorageBridgeOperationsAndRelease() {
        JniRawPtr<NativeMediaDrmStorageBridge> storagePtr = JniRawPtr.createForTesting(2L);
        MediaDrmStorageBridge storage = new MediaDrmStorageBridge(storagePtr);

        AtomicBoolean provisionedResult = new AtomicBoolean(false);
        storage.onProvisioned(provisionedResult::set);
        assertTrue(provisionedResult.get());
        assertEquals(1, mStorageNatives.mProvisionedCalls);

        AtomicReference<PersistentInfo> loadedInfo = new AtomicReference<>();
        storage.loadInfo(new byte[] {9}, loadedInfo::set);
        assertEquals("video/mp4", loadedInfo.get().mimeType());
        assertEquals(1, mStorageNatives.mLoadInfoCalls);

        AtomicBoolean saveResult = new AtomicBoolean(false);
        storage.saveInfo(loadedInfo.get(), saveResult::set);
        assertTrue(saveResult.get());
        assertEquals(1, mStorageNatives.mSaveInfoCalls);

        AtomicBoolean clearResult = new AtomicBoolean(false);
        storage.clearInfo(new byte[] {9}, clearResult::set);
        assertTrue(clearResult.get());
        assertEquals(1, mStorageNatives.mClearInfoCalls);

        storage.release();
        storage.release();

        AtomicBoolean postReleaseSave = new AtomicBoolean(true);
        storage.saveInfo(loadedInfo.get(), postReleaseSave::set);
        assertFalse(postReleaseSave.get());
        assertEquals(1, mStorageNatives.mSaveInfoCalls);
    }
}
