// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaDrm;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.media.MediaDrmStorageBridge.PersistentInfo;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.UUID;

/**
 * The class manages relations among eme session ID, drm session ID and keyset ID. It also records
 * the associated session information.
 *
 * <p>For temporary session, it simply maintains the in memory map from session ID to related
 * information. When session is closed, the mapping is also removed.
 *
 * <p>For persistent session, it also talks to persistent storage when loading information back to
 * memory and updating changes to disk.
 */
class MediaDrmSessionManager {
    /**
     * The class groups drm session ID, eme session ID and key set ID. It hides the conversion among
     * the three different IDs.
     */
    static class SessionId {
        private static final char[] HEX_CHAR_LOOKUP = "0123456789ABCDEF".toCharArray();

        // ID used by browser and javascript to identify the session. It's
        // the unique ID in EME world and also used as ID for this class.
        // For temporary session, eme ID should match drm ID. For persistent
        // session, EME ID is a random generated string because the persistent
        // ID (key set ID) is generated much later than eme ID.
        private final byte[] mEmeId;

        // Temporary ID used by MediaDrm session, returned by
        // MediaDrm.openSession.
        private byte[] mDrmId;

        // Persistent ID used by MediaDrm to identify persistent licenses,
        // returned by MediaDrm.provideKeyResponse.
        private byte[] mKeySetId;

        /**
         *  Convert byte array to hex string for logging.
         *  This is modified from BytesToHexString() in url/url_canon_unittest.cc.
         */
        static String toHexString(byte[] bytes) {
            StringBuilder hexString = new StringBuilder();
            for (int i = 0; i < bytes.length; ++i) {
                hexString.append(HEX_CHAR_LOOKUP[bytes[i] >>> 4]);
                hexString.append(HEX_CHAR_LOOKUP[bytes[i] & 0xf]);
            }
            return hexString.toString();
        }

        /**
         * Create session ID with random generated (UUID.randomUUID) EME session ID.
         * The ID must be unique within the origin of this object's Document over time,
         * including across Documents and browsing sessions.
         * https://w3c.github.io/encrypted-media/#dom-mediakeysession-generaterequest
         *
         * @param drmId Raw DRM ID created by MediaDrm.
         * @return Session ID with random generated EME session ID.
         */
        static SessionId createPersistentSessionId(byte[] drmId) {
            byte[] emeId =
                    ApiCompatibilityUtils.getBytesUtf8(
                            UUID.randomUUID().toString().replace('-', '0'));
            return new SessionId(emeId, drmId, /* keySetId= */ null);
        }

        /**
         * Create session ID for temporary license session. The DRM session ID is used as EME
         * session ID.
         *
         * @param drmId Raw DRM session ID created by MediaDrm.
         * @return Session ID with DRM session ID as EME session ID.
         */
        static SessionId createTemporarySessionId(byte[] drmId) {
            return new SessionId(drmId, drmId, /* keySetId= */ null);
        }

        /** Create session ID used to report session doesn't exist. */
        static SessionId createNoExistSessionId() {
            return createTemporarySessionId(new byte[0]);
        }

        private SessionId(byte[] emeId, byte[] drmId, byte[] keySetId) {
            assert emeId != null;
            assert drmId != null || keySetId != null;

            mEmeId = emeId;
            mDrmId = drmId;
            mKeySetId = keySetId;
        }

        byte[] drmId() {
            return mDrmId;
        }

        byte[] emeId() {
            return mEmeId;
        }

        byte[] keySetId() {
            return mKeySetId;
        }

        private void setKeySetId(byte[] keySetId) {
            mKeySetId = keySetId;
        }

        private void setDrmId(byte[] drmId) {
            mDrmId = drmId;
        }

        boolean isEqual(SessionId that) {
            return Arrays.equals(mEmeId, that.emeId());
        }

        String toHexString() {
            return toHexString(mEmeId);
        }

        /** Convert `mEmeId` to UTF-8 string. */
        @Override
        public String toString() {
            return new String(mEmeId, StandardCharsets.UTF_8);
        }
    }

    static class SessionInfo {
        private final SessionId mSessionId;
        private final String mMimeType;

        // Key type of license in the session. It should be one of
        // MediaDrm.KEY_TYPE_XXX.
        private int mKeyType;

        private SessionInfo(SessionId sessionId, String mimeType, int keyType) {
            assert sessionId != null;
            assert mimeType != null && !mimeType.isEmpty();

            mSessionId = sessionId;
            mMimeType = mimeType;
            mKeyType = keyType;
        }

        String mimeType() {
            return mMimeType;
        }

        int keyType() {
            return mKeyType;
        }

        // Private methods that are visible in this file only.

        private SessionId sessionId() {
            return mSessionId;
        }

        private void setKeyType(int keyType) {
            mKeyType = keyType;
        }

        private PersistentInfo toPersistentInfo() {
            assert mSessionId.keySetId() != null;

            return new PersistentInfo(
                    mSessionId.emeId(), mSessionId.keySetId(), mMimeType, mKeyType);
        }

        private static SessionInfo fromPersistentInfo(PersistentInfo persistentInfo) {
            assert persistentInfo != null;
            assert persistentInfo.emeId() != null;
            assert persistentInfo.keySetId() != null;

            SessionId sessionId =
                    new SessionId(
                            persistentInfo.emeId(), /* drmId= */ null, persistentInfo.keySetId());
            return new SessionInfo(
                    sessionId,
                    persistentInfo.mimeType(),
                    getKeyTypeFromPersistentInfo(persistentInfo));
        }

        private static int getKeyTypeFromPersistentInfo(PersistentInfo persistentInfo) {
            int keyType = persistentInfo.keyType();
            if (keyType == MediaDrm.KEY_TYPE_OFFLINE || keyType == MediaDrm.KEY_TYPE_RELEASE) {
                return keyType;
            }

            // Key type is missing. Use OFFLINE by default.
            return MediaDrm.KEY_TYPE_OFFLINE;
        }
    }

    // Maps from DRM/EME session ID to SessionInfo. SessionInfo contains
    // SessionId, so that we can:
    //   1. Get SessionInfo with EME/DRM session ID.
    //   2. Get SessionId from EME/DRM session ID.
    //   3. Get EME/DRM session ID from DRM/EME session ID.
    // SessionId always has a valid EME session ID, so all opened session should
    // have an entry in mEmeSessionInfoMap.
    private HashMap<ByteBuffer, SessionInfo> mEmeSessionInfoMap;
    private HashMap<ByteBuffer, SessionInfo> mDrmSessionInfoMap;

    // The persistent storage to record map from EME session ID to key set ID
    // for persistent license.
    private MediaDrmStorageBridge mStorage;

    public MediaDrmSessionManager(MediaDrmStorageBridge storage) {
        mEmeSessionInfoMap = new HashMap<>();
        mDrmSessionInfoMap = new HashMap<>();

        mStorage = storage;
    }

    /**
     * Set drm ID. It should only be called for persistent license session
     * without an opened drm session.
     */
    void setDrmId(SessionId sessionId, byte[] drmId) {
        SessionInfo info = get(sessionId);

        assert info != null;
        assert info.sessionId().isEqual(sessionId);

        sessionId.setDrmId(drmId);
        mDrmSessionInfoMap.put(ByteBuffer.wrap(drmId), info);
    }

    /** Set key set ID. It should only be called for persistent license session. */
    void setKeySetId(SessionId sessionId, byte[] keySetId, Callback<Boolean> callback) {
        assert get(sessionId) != null;
        assert get(sessionId).keyType() == MediaDrm.KEY_TYPE_OFFLINE;
        assert sessionId.keySetId() == null;

        sessionId.setKeySetId(keySetId);

        mStorage.saveInfo(get(sessionId).toPersistentInfo(), callback);
    }

    /**
     * Mark key as released. It should only be called for persistent license
     * session.
     */
    void setKeyType(SessionId sessionId, int keyType, Callback<Boolean> callback) {
        SessionInfo info = get(sessionId);

        assert info != null;

        info.setKeyType(keyType);
        mStorage.saveInfo(info.toPersistentInfo(), callback);
    }

    /** Load |emeId|'s session data from persistent storage. */
    void load(byte[] emeId, final Callback<SessionId> callback) {
        mStorage.loadInfo(
                emeId,
                new Callback<PersistentInfo>() {
                    @Override
                    public void onResult(PersistentInfo persistentInfo) {
                        if (persistentInfo == null) {
                            callback.onResult(null);
                            return;
                        }

                        // Loading same persistent license into different sessions isn't
                        // supported.
                        assert getSessionIdByEmeId(persistentInfo.emeId()) == null;

                        SessionInfo info = SessionInfo.fromPersistentInfo(persistentInfo);
                        mEmeSessionInfoMap.put(ByteBuffer.wrap(persistentInfo.emeId()), info);
                        callback.onResult(info.sessionId());
                    }
                });
    }

    /** Remove persistent license info from persistent storage. */
    void clearPersistentSessionInfo(SessionId sessionId, Callback<Boolean> callback) {
        sessionId.setKeySetId(null);
        mStorage.clearInfo(sessionId.emeId(), callback);
    }

    /**
     * Remove session and related infomration from memory, but doesn't touch
     * persistent storage.
     */
    void remove(SessionId sessionId) {
        SessionInfo info = get(sessionId);

        assert info != null;
        assert sessionId.isEqual(info.sessionId());

        mEmeSessionInfoMap.remove(ByteBuffer.wrap(sessionId.emeId()));
        if (sessionId.drmId() != null) {
            mDrmSessionInfoMap.remove(ByteBuffer.wrap(sessionId.drmId()));
        }
    }

    List<SessionId> getAllSessionIds() {
        ArrayList<SessionId> sessionIds = new ArrayList<>();
        for (SessionInfo info : mEmeSessionInfoMap.values()) {
            sessionIds.add(info.sessionId());
        }

        return sessionIds;
    }

    SessionInfo get(SessionId sessionId) {
        return mEmeSessionInfoMap.get(ByteBuffer.wrap(sessionId.emeId()));
    }

    void put(SessionId id, String mimeType, int keyType) {
        SessionInfo info = new SessionInfo(id, mimeType, keyType);
        mEmeSessionInfoMap.put(ByteBuffer.wrap(id.emeId()), info);

        if (id.drmId() != null) {
            mDrmSessionInfoMap.put(ByteBuffer.wrap(id.drmId()), info);
        }
    }

    SessionId getSessionIdByEmeId(byte[] emeId) {
        return getSessionIdFromMap(mEmeSessionInfoMap, emeId);
    }

    SessionId getSessionIdByDrmId(byte[] drmId) {
        return getSessionIdFromMap(mDrmSessionInfoMap, drmId);
    }

    // Private methods

    private SessionId getSessionIdFromMap(HashMap<ByteBuffer, SessionInfo> map, byte[] id) {
        SessionInfo info = map.get(ByteBuffer.wrap(id));
        if (info == null) {
            return null;
        }

        return info.sessionId();
    }
}
