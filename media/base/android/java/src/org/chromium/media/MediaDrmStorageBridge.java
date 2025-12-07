// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Origin isolated media drm scope id storage. Isolated origin is guaranteed by native
 * implementation. Thus no origin information is stored here.
 */
@JNINamespace("media")
@NullMarked
class MediaDrmStorageBridge {
    private static final long INVALID_NATIVE_MEDIA_DRM_STORAGE_BRIDGE = -1;

    private final long mNativeMediaDrmStorageBridge;

    /** Information that need to be persistent on the device. Exposed to JNI. */
    static class PersistentInfo {
        // EME session ID, which is generated randomly.
        private final byte[] mEmeId;

        // Key set ID used to identify persistent license in MediaDrm.
        private final byte[] mKeySetId;

        // Mime type for the license.
        private final String mMimeType;

        // Key type of session. It can be any value. Caller should check it before actual using it.
        private final int mKeyType;

        @CalledByNative("PersistentInfo")
        private static PersistentInfo create(
                byte[] emeId, byte[] keySetId, String mime, int keyType) {
            return new PersistentInfo(emeId, keySetId, mime, keyType);
        }

        PersistentInfo(byte[] emeId, byte[] keySetId, String mime, int keyType) {
            mEmeId = emeId;
            mKeySetId = keySetId;
            mMimeType = mime;
            mKeyType = keyType;
        }

        @CalledByNative("PersistentInfo")
        byte[] emeId() {
            return mEmeId;
        }

        @CalledByNative("PersistentInfo")
        byte[] keySetId() {
            return mKeySetId;
        }

        @CalledByNative("PersistentInfo")
        String mimeType() {
            return mMimeType;
        }

        @CalledByNative("PersistentInfo")
        int keyType() {
            return mKeyType;
        }
    }

    MediaDrmStorageBridge(long nativeMediaDrmStorageBridge) {
        mNativeMediaDrmStorageBridge = nativeMediaDrmStorageBridge;
        assert isNativeMediaDrmStorageValid();
    }

    /** Called when device provisioning is finished. */
    void onProvisioned(Callback<Boolean> cb) {
        if (isNativeMediaDrmStorageValid()) {
            MediaDrmStorageBridgeJni.get().onProvisioned(mNativeMediaDrmStorageBridge, cb);
        } else {
            cb.onResult(true);
        }
    }

    /** Load |emeId|'s storage into memory. */
    void loadInfo(byte[] emeId, Callback<@Nullable PersistentInfo> cb) {
        if (isNativeMediaDrmStorageValid()) {
            MediaDrmStorageBridgeJni.get()
                    .onLoadInfo(mNativeMediaDrmStorageBridge, emeId, (Callback<PersistentInfo>) cb);
        } else {
            cb.onResult(null);
        }
    }

    /** Save persistent information. Override the existing value. */
    void saveInfo(PersistentInfo info, Callback<Boolean> cb) {
        if (isNativeMediaDrmStorageValid()) {
            MediaDrmStorageBridgeJni.get().onSaveInfo(mNativeMediaDrmStorageBridge, info, cb);
        } else {
            cb.onResult(false);
        }
    }

    /** Remove persistent information related |emeId|. */
    void clearInfo(byte[] emeId, Callback<Boolean> cb) {
        if (isNativeMediaDrmStorageValid()) {
            MediaDrmStorageBridgeJni.get().onClearInfo(mNativeMediaDrmStorageBridge, emeId, cb);
        } else {
            cb.onResult(true);
        }
    }

    private boolean isNativeMediaDrmStorageValid() {
        return mNativeMediaDrmStorageBridge != INVALID_NATIVE_MEDIA_DRM_STORAGE_BRIDGE;
    }

    @NativeMethods
    interface Natives {
        void onProvisioned(long nativeMediaDrmStorageBridge, Callback<Boolean> cb);

        void onLoadInfo(
                long nativeMediaDrmStorageBridge, byte[] sessionId, Callback<PersistentInfo> cb);

        void onSaveInfo(
                long nativeMediaDrmStorageBridge, PersistentInfo info, Callback<Boolean> cb);

        void onClearInfo(long nativeMediaDrmStorageBridge, byte[] sessionId, Callback<Boolean> cb);
    }
}
