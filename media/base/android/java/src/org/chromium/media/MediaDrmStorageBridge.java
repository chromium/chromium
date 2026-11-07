// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
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
    @JniType("::media::MediaDrmStorageBridge")
    interface NativeMediaDrmStorageBridge extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeMediaDrmStorageBridge> mNativeMediaDrmStorageBridge;

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

        @CalledByNative
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

        @CalledByNative
        byte[] emeId() {
            return mEmeId;
        }

        @CalledByNative
        byte[] keySetId() {
            return mKeySetId;
        }

        @CalledByNative
        String mimeType() {
            return mMimeType;
        }

        @CalledByNative
        int keyType() {
            return mKeyType;
        }
    }

    MediaDrmStorageBridge(
            @Nullable JniRawPtr<NativeMediaDrmStorageBridge> nativeMediaDrmStorageBridge) {
        mNativeMediaDrmStorageBridge = nativeMediaDrmStorageBridge;
    }

    void release() {
        if (mNativeMediaDrmStorageBridge != null) {
            mNativeMediaDrmStorageBridge.release();
            mNativeMediaDrmStorageBridge = null;
        }
    }

    /** Called when device provisioning is finished. */
    void onProvisioned(Callback<Boolean> cb) {
        if (mNativeMediaDrmStorageBridge != null) {
            MediaDrmStorageBridgeJni.get().onProvisioned(mNativeMediaDrmStorageBridge, cb);
        } else {
            cb.onResult(true);
        }
    }

    /** Load |emeId|'s storage into memory. */
    void loadInfo(byte[] emeId, Callback<@Nullable PersistentInfo> cb) {
        if (mNativeMediaDrmStorageBridge != null) {
            MediaDrmStorageBridgeJni.get()
                    .onLoadInfo(mNativeMediaDrmStorageBridge, emeId, (Callback<PersistentInfo>) cb);
        } else {
            cb.onResult(null);
        }
    }

    /** Save persistent information. Override the existing value. */
    void saveInfo(PersistentInfo info, Callback<Boolean> cb) {
        if (mNativeMediaDrmStorageBridge != null) {
            MediaDrmStorageBridgeJni.get().onSaveInfo(mNativeMediaDrmStorageBridge, info, cb);
        } else {
            cb.onResult(false);
        }
    }

    /** Remove persistent information related |emeId|. */
    void clearInfo(byte[] emeId, Callback<Boolean> cb) {
        if (mNativeMediaDrmStorageBridge != null) {
            MediaDrmStorageBridgeJni.get().onClearInfo(mNativeMediaDrmStorageBridge, emeId, cb);
        } else {
            cb.onResult(true);
        }
    }

    @NativeMethods
    interface Natives {
        void onProvisioned(JniPtr<NativeMediaDrmStorageBridge> self, Callback<Boolean> cb);

        void onLoadInfo(
                JniPtr<NativeMediaDrmStorageBridge> self,
                byte[] sessionId,
                Callback<PersistentInfo> cb);

        void onSaveInfo(
                JniPtr<NativeMediaDrmStorageBridge> self,
                PersistentInfo info,
                Callback<Boolean> cb);

        void onClearInfo(
                JniPtr<NativeMediaDrmStorageBridge> self, byte[] sessionId, Callback<Boolean> cb);
    }
}
