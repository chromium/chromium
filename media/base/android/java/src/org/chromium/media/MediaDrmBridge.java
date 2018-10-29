// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.media.MediaCrypto;
import android.media.MediaDrm;
import android.os.Build;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.media.MediaDrmSessionManager.SessionId;
import org.chromium.media.MediaDrmSessionManager.SessionInfo;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Queue;
import java.util.UUID;

// Implementation Notes of MediaDrmBridge:
//
// MediaCrypto Creation: If requiresMediaCrypto is true, the caller is guaranteed to wait until
// MediaCrypto is created to call any other methods. A mMediaCryptoSession is opened after MediaDrm
// is created. This session will NOT be added to mSessionManager and will only be used to create the
// MediaCrypto object. createMediaCrypto() may trigger the provisioning process, where MediaCrypto
// creation will resume after provisioning completes.
//
// Unprovision: If requiresMediaCrypto is false, MediaDrmBridge is not created for playback.
// Instead, it's created to unprovision the device/origin. resetDeviceCredentials() will trigger the
// provisioning process to reprovision the device. This is used on older Android versions since
// direct unprovision is not supported. On newer Android versions, unprovision() will unprovision
// the origin, as part of clear media licenses.
//
// NotProvisionedException: If this exception is thrown in operations other than createMediaCrypto()
// and resetDeviceCredentials(), we will fail that operation and not trying to provision again.
//
// Session Manager: Each createSession() call creates a new session. All created sessions are
// managed in mSessionManager except for mMediaCryptoSession.
//
// Error Handling: Whenever an unexpected error occurred, we'll call release() to release all
// resources immediately, clear all states and fail all pending operations. After that all calls to
// this object will fail (e.g. return null or reject the promise). All public APIs and callbacks
// should check mMediaBridge to make sure release() hasn't been called.

/**
 * A wrapper of the android MediaDrm class. Each MediaDrmBridge manages multiple sessions for
 * MediaCodecAudioDecoders, and AndroidVideoDecodeAccelerators or MediaCodecVideoDecoders.
 */
@JNINamespace("media")
@MainDex
@SuppressLint("WrongConstant")
@TargetApi(Build.VERSION_CODES.KITKAT)
public class MediaDrmBridge {
    private static final String TAG = "cr_media";
    private static final String SECURITY_LEVEL = "securityLevel";
    private static final String SERVER_CERTIFICATE = "serviceCertificate";
    private static final String ORIGIN = "origin";
    private static final String PRIVACY_MODE = "privacyMode";
    private static final String SESSION_SHARING = "sessionSharing";
    private static final String ENABLE = "enable";
    private static final long INVALID_NATIVE_MEDIA_DRM_BRIDGE = 0;

    // Error message returned by MediaDrm functions.
    private static final String MEDIA_DRM_ERROR_LICENSE_RELEASED =
            "android.media.MediaDrm.error_neg_2948";

    // Scheme UUID for Widevine. See http://dashif.org/identifiers/protection/
    private static final UUID WIDEVINE_UUID =
            UUID.fromString("edef8ba9-79d6-4ace-a3c8-27dcd51d21ed");

    // On Android L and before, MediaDrm doesn't support KeyStatus. Use a dummy
    // key ID to report key status info.
    // See details: https://github.com/w3c/encrypted-media/issues/32
    private static final byte[] DUMMY_KEY_ID = new byte[] {0};

    // Special provision response to remove the cert.
    private static final byte[] UNPROVISION = ApiCompatibilityUtils.getBytesUtf8("unprovision");

    private MediaDrm mMediaDrm;
    private MediaCrypto mMediaCrypto;
    private long mNativeMediaDrmBridge;
    private UUID mSchemeUUID;
    private final boolean mRequiresMediaCrypto;

    // A session only for the purpose of creating a MediaCrypto object. Created
    // after construction, or after the provisioning process is successfully
    // completed. No getKeyRequest() should be called on |mMediaCryptoSession|.
    private SessionId mMediaCryptoSession;

    // The map of all opened sessions (excluding mMediaCryptoSession) to their
    // associated meta data, e.g. mime types, key types.
    private MediaDrmSessionManager mSessionManager;

    // The persistent storage to record origin provisioning informations.
    private MediaDrmStorageBridge mStorage;

    private boolean mResetDeviceCredentialsPending;

    // Whether the current MediaDrmBridge instance is waiting for provisioning response.
    private boolean mProvisioningPending;

    // Boolean to track if 'ORIGIN' is set in MediaDrm.
    private boolean mOriginSet;

    private SessionEventDeferrer mSessionEventDeferrer;

    // Defer the creation of MediaCryptor creation. Only used when mRequiresMediaCrypto is true.
    private static final MediaCryptoDeferrer sMediaCryptoDeferrer = new MediaCryptoDeferrer();

    private static class MediaCryptoDeferrer {
        // Whether any MediaDrmBridge instance is waiting for provisioning response.
        private boolean mIsProvisioning;

        // Pending events to fire after provisioning is finished.
        private final Queue<Runnable> mEventHandlers;

        MediaCryptoDeferrer() {
            mIsProvisioning = false;
            mEventHandlers = new ArrayDeque<Runnable>();
        }

        boolean isProvisioning() {
            return mIsProvisioning;
        }

        void onProvisionStarted() {
            assert !mIsProvisioning;
            mIsProvisioning = true;
        }

        void defer(Runnable handler) {
            assert mIsProvisioning;
            mEventHandlers.add(handler);
        }

        void onProvisionDone() {
            assert mIsProvisioning;
            mIsProvisioning = false;

            // This will cause createMediaCrypto() on another MediaDrmBridge object and could cause
            // reentrance into the shared static sMediaCryptoDeferrer. For example, during
            // createMediaCrypto(), we could hit NotProvisionedException again, and call
            // isProvisioning() to check whether it can start provisioning or not. If so, it'll
            // call onProvisionStarted(). To avoid the case where we call createMediaCrypto() and
            // then immediately call defer(), we'll return early whenever mIsProvisioning becomes
            // true.
            while (!mEventHandlers.isEmpty()) {
                Log.d(TAG, "run deferred CreateMediaCrypto() calls");
                Runnable r = mEventHandlers.element();
                mEventHandlers.remove();

                r.run();

                if (mIsProvisioning) {
                    Log.d(TAG, "provision triggerred while running deferred CreateMediaCrypto()");
                    return;
                }
            }
        }
    }

    // Block MediaDrm event for |mSessionId|. MediaDrm may fire event before the
    // functions return. This may break Chromium CDM API's assumption. For
    // example, when loading session, 'restoreKeys' will trigger key status
    // change event. But the session isn't known to Chromium CDM because the
    // promise isn't resolved. The class can block and collect these events and
    // fire these events later.
    private static class SessionEventDeferrer {
        private final SessionId mSessionId;
        private final ArrayList<Runnable> mEventHandlers;

        SessionEventDeferrer(SessionId sessionId) {
            mSessionId = sessionId;
            mEventHandlers = new ArrayList<>();
        }

        boolean shouldDefer(SessionId sessionId) {
            return mSessionId.isEqual(sessionId);
        }

        void defer(Runnable handler) {
            mEventHandlers.add(handler);
        }

        void fire() {
            for (Runnable r : mEventHandlers) {
                r.run();
            }

            mEventHandlers.clear();
        }
    }

    /**
     *  An equivalent of MediaDrm.KeyStatus, which is only available on M+.
     */
    @MainDex
    private static class KeyStatus {
        private final byte[] mKeyId;
        private final int mStatusCode;

        private KeyStatus(byte[] keyId, int statusCode) {
            mKeyId = keyId;
            mStatusCode = statusCode;
        }

        @CalledByNative("KeyStatus")
        private byte[] getKeyId() {
            return mKeyId;
        }

        @CalledByNative("KeyStatus")
        private int getStatusCode() {
            return mStatusCode;
        }
    }

    /**
     *  Creates a dummy single element list of KeyStatus with a dummy key ID and
     *  the specified keyStatus.
     */
    private static List<KeyStatus> getDummyKeysInfo(int statusCode) {
        List<KeyStatus> keysInfo = new ArrayList<KeyStatus>();
        keysInfo.add(new KeyStatus(DUMMY_KEY_ID, statusCode));
        return keysInfo;
    }

    private static UUID getUUIDFromBytes(byte[] data) {
        if (data.length != 16) {
            return null;
        }
        long mostSigBits = 0;
        long leastSigBits = 0;
        for (int i = 0; i < 8; i++) {
            mostSigBits = (mostSigBits << 8) | (data[i] & 0xff);
        }
        for (int i = 8; i < 16; i++) {
            leastSigBits = (leastSigBits << 8) | (data[i] & 0xff);
        }
        return new UUID(mostSigBits, leastSigBits);
    }

    private boolean isNativeMediaDrmBridgeValid() {
        return mNativeMediaDrmBridge != INVALID_NATIVE_MEDIA_DRM_BRIDGE;
    }

    private boolean isWidevine() {
        return mSchemeUUID.equals(WIDEVINE_UUID);
    }

    @TargetApi(Build.VERSION_CODES.M)
    private MediaDrmBridge(UUID schemeUUID, boolean requiresMediaCrypto, long nativeMediaDrmBridge,
            long nativeMediaDrmStorageBridge) throws android.media.UnsupportedSchemeException {
        mSchemeUUID = schemeUUID;
        mMediaDrm = new MediaDrm(schemeUUID);
        mRequiresMediaCrypto = requiresMediaCrypto;

        mNativeMediaDrmBridge = nativeMediaDrmBridge;
        assert isNativeMediaDrmBridgeValid();

        mStorage = new MediaDrmStorageBridge(nativeMediaDrmStorageBridge);
        mSessionManager = new MediaDrmSessionManager(mStorage);

        mResetDeviceCredentialsPending = false;
        mProvisioningPending = false;

        mMediaDrm.setOnEventListener(new EventListener());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            mMediaDrm.setOnExpirationUpdateListener(new ExpirationUpdateListener(), null);
            mMediaDrm.setOnKeyStatusChangeListener(new KeyStatusChangeListener(), null);
        }

        if (isWidevine()) {
            mMediaDrm.setPropertyString(PRIVACY_MODE, ENABLE);
            mMediaDrm.setPropertyString(SESSION_SHARING, ENABLE);
        }
    }

    /**
     * Create a MediaCrypto object.
     *
     * @return false upon fatal error in creating MediaCrypto. Returns true
     * otherwise, including the following two cases:
     *   1. MediaCrypto is successfully created and notified.
     *   2. Device is not provisioned and MediaCrypto creation will be tried
     *      again after the provisioning process is completed.
     *
     *  When false is returned, release() is called within the function, which
     *  will notify the native code with a null MediaCrypto, if needed.
     */
    private boolean createMediaCrypto() {
        assert mMediaDrm != null;
        assert !mProvisioningPending;
        assert mMediaCryptoSession == null;

        // Open media crypto session.
        byte[] mediaCryptoSessionDrmId = null;
        try {
            mediaCryptoSessionDrmId = openSession();
        } catch (android.media.NotProvisionedException e) {
            Log.d(TAG, "Not provisioned during openSession()");

            if (!sMediaCryptoDeferrer.isProvisioning()) {
                startProvisioning();
                return true;
            }

            // Cannot provision. Defer MediaCrypto creation and try again later.
            Log.d(TAG, "defer CreateMediaCrypto() calls");
            sMediaCryptoDeferrer.defer(new Runnable() {
                @Override
                public void run() {
                    createMediaCrypto();
                }
            });

            return true;
        }

        if (mediaCryptoSessionDrmId == null) {
            Log.e(TAG, "Cannot create MediaCrypto Session.");
            // No need to release() here since openSession() does so on failure.
            return false;
        }

        mMediaCryptoSession = SessionId.createTemporarySessionId(mediaCryptoSessionDrmId);

        Log.d(TAG, "MediaCrypto Session created: %s", mMediaCryptoSession.toHexString());

        // Create MediaCrypto object.
        try {
            if (MediaCrypto.isCryptoSchemeSupported(mSchemeUUID)) {
                mMediaCrypto = new MediaCrypto(mSchemeUUID, mMediaCryptoSession.drmId());
                Log.d(TAG, "MediaCrypto successfully created!");
                onMediaCryptoReady(mMediaCrypto);
                return true;
            } else {
                Log.e(TAG, "Cannot create MediaCrypto for unsupported scheme.");
            }
        } catch (android.media.MediaCryptoException e) {
            Log.e(TAG, "Cannot create MediaCrypto", e);
        }

        release();
        return false;
    }

    /**
     * Open a new session.
     *
     * @return ID of the session opened. Returns null if unexpected error happened.
     */
    private byte[] openSession() throws android.media.NotProvisionedException {
        assert mMediaDrm != null;
        try {
            byte[] sessionId = mMediaDrm.openSession();
            // Make a clone here in case the underlying byte[] is modified.
            return sessionId.clone();
        } catch (java.lang.RuntimeException e) { // TODO(xhwang): Drop this?
            Log.e(TAG, "Cannot open a new session", e);
            release();
            return null;
        } catch (android.media.NotProvisionedException e) {
            // Throw NotProvisionedException so that we can startProvisioning().
            throw e;
        } catch (android.media.MediaDrmException e) {
            // Other MediaDrmExceptions (e.g. ResourceBusyException) are not
            // recoverable.
            Log.e(TAG, "Cannot open a new session", e);
            release();
            return null;
        }
    }

    /**
     * Check whether the crypto scheme is supported for the given container.
     * If |containerMimeType| is an empty string, we just return whether
     * the crypto scheme is supported.
     *
     * @return true if the container and the crypto scheme is supported, or
     * false otherwise.
     */
    @CalledByNative
    private static boolean isCryptoSchemeSupported(byte[] schemeUUID, String containerMimeType) {
        UUID cryptoScheme = getUUIDFromBytes(schemeUUID);

        if (containerMimeType.isEmpty()) {
            return MediaDrm.isCryptoSchemeSupported(cryptoScheme);
        }

        return MediaDrm.isCryptoSchemeSupported(cryptoScheme, containerMimeType);
    }

    /**
     * Create a new MediaDrmBridge from the crypto scheme UUID.
     *
     * @param schemeUUID Crypto scheme UUID.
     * @param securityOrigin Security origin. Empty value means no need for origin isolated storage.
     * @param securityLevel Security level. If empty, the default one should be used.
     * @param nativeMediaDrmBridge Native object of this class.
     * @param nativeMediaDrmStorageBridge Native object of persistent storage.
     */
    @CalledByNative
    private static MediaDrmBridge create(byte[] schemeUUID, String securityOrigin,
            String securityLevel, boolean requiresMediaCrypto, long nativeMediaDrmBridge,
            long nativeMediaDrmStorageBridge) {
        UUID cryptoScheme = getUUIDFromBytes(schemeUUID);
        if (cryptoScheme == null || !MediaDrm.isCryptoSchemeSupported(cryptoScheme)) {
            return null;
        }

        MediaDrmBridge mediaDrmBridge = null;
        try {
            mediaDrmBridge = new MediaDrmBridge(cryptoScheme, requiresMediaCrypto,
                    nativeMediaDrmBridge, nativeMediaDrmStorageBridge);
        } catch (android.media.UnsupportedSchemeException e) {
            Log.e(TAG, "Unsupported DRM scheme", e);
            return null;
        } catch (java.lang.IllegalArgumentException e) {
            Log.e(TAG, "Failed to create MediaDrmBridge", e);
            return null;
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "Failed to create MediaDrmBridge", e);
            return null;
        }

        if (!securityLevel.isEmpty() && !mediaDrmBridge.setSecurityLevel(securityLevel)) {
            return null;
        }

        if (!securityOrigin.isEmpty() && !mediaDrmBridge.setOrigin(securityOrigin)) {
            return null;
        }

        // When session support is required, we need to create MediaCrypto to
        // finish the CDM creation process. This may trigger the provisioning
        // process, in which case MediaCrypto will be created after provision
        // is finished.
        if (requiresMediaCrypto && !mediaDrmBridge.createMediaCrypto()) {
            return null;
        }

        return mediaDrmBridge;
    }

    /**
     * Set the security origin for the MediaDrm. All information should be isolated for different
     * origins, e.g. certificates, licenses.
     */
    private boolean setOrigin(String origin) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;

        if (!isWidevine()) {
            Log.d(TAG, "Property " + ORIGIN + " isn't supported");
            return true;
        }

        assert mMediaDrm != null;
        assert !origin.isEmpty();

        try {
            mMediaDrm.setPropertyString(ORIGIN, origin);
            mOriginSet = true;
            return true;
        } catch (java.lang.IllegalArgumentException e) {
            Log.e(TAG, "Failed to set security origin %s", origin, e);
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "Failed to set security origin %s", origin, e);
        }

        Log.e(TAG, "Security origin %s not supported!", origin);
        return false;
    }

    /**
     * Set the security level that the MediaDrm object uses.
     * This function should be called right after we construct MediaDrmBridge
     * and before we make any other calls.
     *
     * @param securityLevel Security level to be set.
     * @return whether the security level was successfully set.
     */
    private boolean setSecurityLevel(String securityLevel) {
        if (!isWidevine()) {
            Log.d(TAG, "Security level is not supported.");
            return true;
        }

        assert mMediaDrm != null;
        assert !securityLevel.isEmpty();

        String currentSecurityLevel = mMediaDrm.getPropertyString(SECURITY_LEVEL);
        Log.e(TAG, "Security level: current %s, new %s", currentSecurityLevel, securityLevel);
        if (securityLevel.equals(currentSecurityLevel)) {
            // No need to set the same security level again. This is not just
            // a shortcut! Setting the same security level actually causes an
            // exception in MediaDrm!
            return true;
        }

        try {
            mMediaDrm.setPropertyString(SECURITY_LEVEL, securityLevel);
            return true;
        } catch (java.lang.IllegalArgumentException e) {
            Log.e(TAG, "Failed to set security level %s", securityLevel, e);
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "Failed to set security level %s", securityLevel, e);
        }

        Log.e(TAG, "Security level %s not supported!", securityLevel);
        return false;
    }

    /**
     * Set the server certificate.
     *
     * @param certificate Server certificate to be set.
     * @return whether the server certificate was successfully set.
     */
    @CalledByNative
    private boolean setServerCertificate(byte[] certificate) {
        if (!isWidevine()) {
            Log.d(TAG, "Setting server certificate is not supported.");
            return true;
        }

        try {
            mMediaDrm.setPropertyByteArray(SERVER_CERTIFICATE, certificate);
            return true;
        } catch (java.lang.IllegalArgumentException e) {
            Log.e(TAG, "Failed to set server certificate", e);
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "Failed to set server certificate", e);
        }

        return false;
    }

    /**
     * Reset the device DRM credentials.
     */
    @CalledByNative
    private void resetDeviceCredentials() {
        if (mMediaDrm == null) {
            onResetDeviceCredentialsCompleted(false);
            return;
        }

        mResetDeviceCredentialsPending = true;
        startProvisioning();
    }

    /**
     * Unprovision the current origin, a.k.a removing the cert for current origin.
     */
    @CalledByNative
    private void unprovision() {
        if (mMediaDrm == null) {
            return;
        }

        // Unprovision only works for origin isolated storage.
        if (!mOriginSet) {
            return;
        }

        provideProvisionResponse(UNPROVISION);
    }

    /**
     * Destroy the MediaDrmBridge object.
     */
    @CalledByNative
    private void destroy() {
        mNativeMediaDrmBridge = INVALID_NATIVE_MEDIA_DRM_BRIDGE;
        if (mMediaDrm != null) {
            release();
        }
    }

    /**
     * Release all allocated resources and finish all pending operations.
     */
    private void release() {
        // Note that mNativeMediaDrmBridge may have already been reset (see destroy()).

        assert mMediaDrm != null;

        // Close all open sessions.
        for (SessionId sessionId : mSessionManager.getAllSessionIds()) {
            try {
                // Some implementations don't have removeKeys, crbug/475632
                mMediaDrm.removeKeys(sessionId.drmId());
            } catch (Exception e) {
                Log.e(TAG, "removeKeys failed: ", e);
            }

            closeSessionNoException(sessionId);
            onSessionClosed(sessionId);
        }
        mSessionManager = new MediaDrmSessionManager(mStorage);

        // Close mMediaCryptoSession if it's open.
        if (mMediaCryptoSession != null) {
            closeSessionNoException(mMediaCryptoSession);
            mMediaCryptoSession = null;
        }

        // Fail device credentials resetting.
        if (mResetDeviceCredentialsPending) {
            mResetDeviceCredentialsPending = false;
            onResetDeviceCredentialsCompleted(false);
        }

        if (mMediaDrm != null) {
            mMediaDrm.release();
            mMediaDrm = null;
        }

        if (mMediaCrypto != null) {
            mMediaCrypto.release();
            mMediaCrypto = null;
        } else {
            // MediaCrypto never notified. Notify a null one now.
            onMediaCryptoReady(null);
        }
    }

    /**
     * Get a key request.
     *
     * @param sessionId ID of session on which we need to get the key request.
     * @param data Data needed to get the key request.
     * @param mime Mime type to get the key request.
     * @param keyType Key type for the requested key.
     * @param optionalParameters Optional parameters to pass to the DRM plugin.
     *
     * @return the key request.
     */
    private MediaDrm.KeyRequest getKeyRequest(SessionId sessionId, byte[] data, String mime,
            int keyType, HashMap<String, String> optionalParameters)
            throws android.media.NotProvisionedException {
        assert mMediaDrm != null;
        assert mMediaCryptoSession != null;
        assert !mProvisioningPending;

        if (optionalParameters == null) {
            optionalParameters = new HashMap<String, String>();
        }

        MediaDrm.KeyRequest request = null;

        try {
            byte[] scopeId =
                    keyType == MediaDrm.KEY_TYPE_RELEASE ? sessionId.keySetId() : sessionId.drmId();
            assert scopeId != null;
            request = mMediaDrm.getKeyRequest(scopeId, data, mime, keyType, optionalParameters);
        } catch (IllegalStateException e) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
                    && e instanceof android.media.MediaDrm.MediaDrmStateException) {
                // See b/21307186 for details.
                Log.e(TAG, "MediaDrmStateException fired during getKeyRequest().", e);
            }
        }

        String result = (request != null) ? "successed" : "failed";
        Log.d(TAG, "getKeyRequest %s!", result);

        return request;
    }

    /**
     * createSession interface to be called from native using primitive types.
     * @see createSession(byte[], String, HashMap<String, String>, long)
     */
    @CalledByNative
    private void createSessionFromNative(byte[] initData, String mime, int keyType,
            String[] optionalParamsArray, long promiseId) {
        HashMap<String, String> optionalParameters = new HashMap<String, String>();
        if (optionalParamsArray != null) {
            if (optionalParamsArray.length % 2 != 0) {
                throw new IllegalArgumentException(
                        "Additional data array doesn't have equal keys/values");
            }
            for (int i = 0; i < optionalParamsArray.length; i += 2) {
                optionalParameters.put(optionalParamsArray[i], optionalParamsArray[i + 1]);
            }
        }
        createSession(initData, mime, keyType, optionalParameters, promiseId);
    }

    /**
     * Create a session, and generate a request with |initData| and |mime|.
     *
     * @param initData Data needed to generate the key request.
     * @param mime Mime type.
     * @param keyType Key type.
     * @param optionalParameters Additional data to pass to getKeyRequest.
     * @param promiseId Promise ID for this call.
     */
    private void createSession(byte[] initData, String mime, int keyType,
            HashMap<String, String> optionalParameters, long promiseId) {
        Log.d(TAG, "createSession()");

        if (mMediaDrm == null) {
            Log.e(TAG, "createSession() called when MediaDrm is null.");
            onPromiseRejected(promiseId, "MediaDrm released previously.");
            return;
        }

        assert mMediaCryptoSession != null;
        assert !mProvisioningPending;

        boolean newSessionOpened = false;
        SessionId sessionId = null;
        try {
            byte[] drmId = openSession();
            if (drmId == null) {
                onPromiseRejected(promiseId, "Open session failed.");
                return;
            }
            newSessionOpened = true;
            assert keyType == MediaDrm.KEY_TYPE_STREAMING || keyType == MediaDrm.KEY_TYPE_OFFLINE;
            sessionId = (keyType == MediaDrm.KEY_TYPE_OFFLINE)
                    ? SessionId.createPersistentSessionId(drmId)
                    : SessionId.createTemporarySessionId(drmId);

            MediaDrm.KeyRequest request =
                    getKeyRequest(sessionId, initData, mime, keyType, optionalParameters);
            if (request == null) {
                closeSessionNoException(sessionId);
                onPromiseRejected(promiseId, "Generate request failed.");
                return;
            }

            // Success!
            Log.d(TAG, "createSession(): Session (%s) created.", sessionId.toHexString());
            onPromiseResolvedWithSession(promiseId, sessionId);
            onSessionMessage(sessionId, request);
            mSessionManager.put(sessionId, mime, keyType);
        } catch (android.media.NotProvisionedException e) {
            Log.e(TAG, "Device not provisioned", e);
            if (newSessionOpened) {
                closeSessionNoException(sessionId);
            }
            onPromiseRejected(promiseId, "Device not provisioned during createSession().");
        }
    }

    /**
     * Search and return the SessionId for raw EME/DRM session id.
     *
     * @param emeId Raw EME session Id.
     * @return SessionId of |emeId| if exists and isn't a MediaCryptoSession, null otherwise.
     */
    private SessionId getSessionIdByEmeId(byte[] emeId) {
        if (mMediaCryptoSession == null) {
            Log.e(TAG, "Session doesn't exist because media crypto session is not created.");
            return null;
        }

        SessionId sessionId = mSessionManager.getSessionIdByEmeId(emeId);
        if (sessionId == null) {
            return null;
        }

        assert !mMediaCryptoSession.isEqual(sessionId);

        return sessionId;
    }

    /**
     * Similar with getSessionIdByEmeId, just search for raw DRM session id.
     */
    private SessionId getSessionIdByDrmId(byte[] drmId) {
        if (mMediaCryptoSession == null) {
            Log.e(TAG, "Session doesn't exist because media crypto session is not created.");
            return null;
        }

        SessionId sessionId = mSessionManager.getSessionIdByDrmId(drmId);
        if (sessionId == null) {
            return null;
        }

        assert !mMediaCryptoSession.isEqual(sessionId);

        return sessionId;
    }

    /**
     * Close a session that was previously created by createSession().
     *
     * @param emeSessionId ID of session to be closed.
     * @param promiseId Promise ID of this call.
     */
    @CalledByNative
    private void closeSession(byte[] emeSessionId, long promiseId) {
        Log.d(TAG, "closeSession()");
        if (mMediaDrm == null) {
            onPromiseRejected(promiseId, "closeSession() called when MediaDrm is null.");
            return;
        }

        SessionId sessionId = getSessionIdByEmeId(emeSessionId);
        if (sessionId == null) {
            onPromiseRejected(promiseId,
                    "Invalid sessionId in closeSession(): " + SessionId.toHexString(emeSessionId));
            return;
        }

        try {
            // Some implementations don't have removeKeys, crbug/475632
            mMediaDrm.removeKeys(sessionId.drmId());
        } catch (Exception e) {
            Log.e(TAG, "removeKeys failed: ", e);
        }

        closeSessionNoException(sessionId);
        mSessionManager.remove(sessionId);
        onPromiseResolved(promiseId);
        onSessionClosed(sessionId);
        Log.d(TAG, "Session %s closed", sessionId.toHexString());
    }

    /**
     * Close the session without worry about the exception, because some
     * implementations let this method throw exception, crbug/611865.
     */
    private void closeSessionNoException(SessionId sessionId) {
        try {
            mMediaDrm.closeSession(sessionId.drmId());
        } catch (Exception e) {
            Log.e(TAG, "closeSession failed: ", e);
        }
    }

    /**
     * Update a session with response.
     *
     * @param emeSessionId Reference ID of session to be updated.
     * @param response Response data from the server.
     * @param promiseId Promise ID of this call.
     */
    @CalledByNative
    private void updateSession(byte[] emeSessionId, byte[] response, final long promiseId) {
        Log.d(TAG, "updateSession()");
        if (mMediaDrm == null) {
            onPromiseRejected(promiseId, "updateSession() called when MediaDrm is null.");
            return;
        }

        final SessionId sessionId = getSessionIdByEmeId(emeSessionId);
        if (sessionId == null) {
            assert false; // Should never happen.
            onPromiseRejected(promiseId,
                    "Invalid session in updateSession: " + SessionId.toHexString(emeSessionId));
            return;
        }

        try {
            SessionInfo sessionInfo = mSessionManager.get(sessionId);
            boolean isKeyRelease = sessionInfo.keyType() == MediaDrm.KEY_TYPE_RELEASE;

            byte[] keySetId = null;
            if (isKeyRelease) {
                Log.d(TAG, "updateSession() for key release");
                assert sessionId.keySetId() != null;
                mMediaDrm.provideKeyResponse(sessionId.keySetId(), response);
            } else {
                keySetId = mMediaDrm.provideKeyResponse(sessionId.drmId(), response);
            }

            KeyUpdatedCallback cb = new KeyUpdatedCallback(sessionId, promiseId, isKeyRelease);

            if (isKeyRelease) {
                mSessionManager.clearPersistentSessionInfo(sessionId, cb);
            } else if (sessionInfo.keyType() == MediaDrm.KEY_TYPE_OFFLINE && keySetId != null
                    && keySetId.length > 0) {
                mSessionManager.setKeySetId(sessionId, keySetId, cb);
            } else {
                // This can be either temporary license update or server certificate update.
                cb.onResult(true);
            }

            return;
        } catch (android.media.NotProvisionedException e) {
            // TODO(xhwang): Should we handle this?
            Log.e(TAG, "failed to provide key response", e);
        } catch (android.media.DeniedByServerException e) {
            Log.e(TAG, "failed to provide key response", e);
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "failed to provide key response", e);
        }
        onPromiseRejected(promiseId, "Update session failed.");
        release();
    }

    /**
     * Load persistent license from storage.
     */
    @CalledByNative
    @TargetApi(Build.VERSION_CODES.M)
    private void loadSession(byte[] emeId, final long promiseId) {
        Log.d(TAG, "loadSession()");
        assert !mProvisioningPending;

        mSessionManager.load(emeId, new Callback<SessionId>() {
            @Override
            public void onResult(SessionId sessionId) {
                if (sessionId == null) {
                    onPersistentLicenseNoExist(promiseId);
                    return;
                }

                loadSessionWithLoadedStorage(sessionId, promiseId);
            }
        });
    }

    /**
     * Load session back to memory with MediaDrm. Load persistent storage
     * before calling this. It will fail if persistent storage isn't loaded.
     */
    @TargetApi(Build.VERSION_CODES.M)
    private void loadSessionWithLoadedStorage(SessionId sessionId, final long promiseId) {
        byte[] drmId = null;
        try {
            drmId = openSession();
            if (drmId == null) {
                onPromiseRejected(promiseId, "Failed to open session to load license.");
                return;
            }

            mSessionManager.setDrmId(sessionId, drmId);
            assert Arrays.equals(sessionId.drmId(), drmId);
            assert mSessionManager.get(sessionId).keyType() == MediaDrm.KEY_TYPE_OFFLINE;

            // Defer event handlers until license is loaded.
            assert mSessionEventDeferrer == null;
            mSessionEventDeferrer = new SessionEventDeferrer(sessionId);

            assert sessionId.keySetId() != null;
            mMediaDrm.restoreKeys(sessionId.drmId(), sessionId.keySetId());

            onPersistentLicenseLoaded(sessionId, promiseId);
        } catch (android.media.NotProvisionedException e) {
            // If device isn't provisioned, storage loading should fail.
            Log.w(TAG, "Persistent license load fail because origin isn't provisioned.");
            onPersistentLicenseLoadFail(sessionId, promiseId);
        } catch (java.lang.IllegalStateException e) {
            assert sessionId.drmId() != null;

            // If persistent license (KEY_TYPE_OFFLINE) is released but we don't receive the ack
            // from the server, loading the key again will fail. Report success to JS so that
            // they can release it again.
            if (e instanceof MediaDrm.MediaDrmStateException) {
                MediaDrm.MediaDrmStateException stateException =
                        (MediaDrm.MediaDrmStateException) e;
                if (stateException.getDiagnosticInfo().equals(MEDIA_DRM_ERROR_LICENSE_RELEASED)) {
                    Log.w(TAG, "Persistent license is waiting for release ack.");
                    onPersistentLicenseLoaded(sessionId, promiseId);

                    // Report keystatuseschange event to JS.
                    onSessionKeysChange(sessionId,
                            getDummyKeysInfo(MediaDrm.KeyStatus.STATUS_EXPIRED).toArray(),
                            false /* hasAdditionalUsableKey */, false /* isKeyRelease */);
                    return;
                }
            }

            onPersistentLicenseLoadFail(sessionId, promiseId);
        }
    }

    private void onPersistentLicenseLoaded(SessionId sessionId, long promiseId) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;

        onPromiseResolvedWithSession(promiseId, sessionId);

        assert mSessionEventDeferrer != null;
        mSessionEventDeferrer.fire();
        mSessionEventDeferrer = null;
    }

    private void onPersistentLicenseNoExist(long promiseId) {
        // Chromium CDM API requires resolve the promise with empty session id for non-exist
        // license. See media/base/content_decryption_module.h LoadSession for more details.
        onPromiseResolvedWithSession(promiseId, SessionId.createNoExistSessionId());
    }

    // If persistent license load fails, we want to clean the storage and report it to JS as license
    // doesn't exist.
    private void onPersistentLicenseLoadFail(SessionId sessionId, final long promiseId) {
        closeSessionNoException(sessionId);
        mSessionManager.clearPersistentSessionInfo(sessionId, new Callback<Boolean>() {
            @Override
            public void onResult(Boolean success) {
                if (!success) {
                    Log.w(TAG, "Failed to clear persistent storage for non-exist license");
                }

                onPersistentLicenseNoExist(promiseId);
            }
        });
    }

    /**
     * Remove session from device. This will mark the key as released and
     * generate a key release request. The license is removed from the device
     * when the session is updated with a license release response.
     */
    @CalledByNative
    private void removeSession(byte[] emeId, long promiseId) {
        Log.d(TAG, "removeSession()");
        SessionId sessionId = getSessionIdByEmeId(emeId);

        if (sessionId == null) {
            onPromiseRejected(promiseId, "Session doesn't exist");
            return;
        }

        SessionInfo sessionInfo = mSessionManager.get(sessionId);
        if (sessionInfo.keyType() != MediaDrm.KEY_TYPE_OFFLINE) {
            // TODO(yucliu): Support 'remove' of temporary session.
            onPromiseRejected(promiseId, "Removing temporary session isn't implemented");
            return;
        }

        assert sessionId.keySetId() != null;

        mSessionManager.markKeyReleased(sessionId);

        try {
            // Get key release request.
            MediaDrm.KeyRequest request = getKeyRequest(
                    sessionId, null, sessionInfo.mimeType(), MediaDrm.KEY_TYPE_RELEASE, null);

            if (request == null) {
                onPromiseRejected(promiseId, "Fail to generate key release request");
                return;
            }

            // According to EME spec:
            // https://www.w3.org/TR/encrypted-media/#dom-mediakeysession-remove
            // 5.5 ... run the Queue a "message" Event ...
            // 5.6 Resolve promise
            // Since event is queued, JS will receive event after promise is
            // resolved. So resolve the promise before firing the event here.
            onPromiseResolved(promiseId);
            onSessionMessage(sessionId, request);
        } catch (android.media.NotProvisionedException e) {
            Log.e(TAG, "removeSession called on unprovisioned device");
            onPromiseRejected(promiseId, "Unknown failure");
        }
    }

    /**
     * Return the security level of this DRM object.
     */
    @CalledByNative
    private String getSecurityLevel() {
        if (mMediaDrm == null || !isWidevine()) {
            Log.e(TAG, "getSecurityLevel(): MediaDrm is null or security level is not supported.");
            return "";
        }
        return mMediaDrm.getPropertyString("securityLevel");
    }

    private void startProvisioning() {
        Log.d(TAG, "startProvisioning");
        assert !mProvisioningPending;
        mProvisioningPending = true;
        assert mMediaDrm != null;

        if (!isNativeMediaDrmBridgeValid()) {
            return;
        }

        if (mRequiresMediaCrypto) {
            sMediaCryptoDeferrer.onProvisionStarted();
        }

        MediaDrm.ProvisionRequest request = mMediaDrm.getProvisionRequest();
        nativeOnStartProvisioning(
                mNativeMediaDrmBridge, request.getDefaultUrl(), request.getData());
    }

    /**
     * Called when the provision response is received.
     *
     * @param isResponseReceived Flag set to true if commincation with provision server was
     * successful.
     * @param response Response data from the provision server.
     */
    @CalledByNative
    private void processProvisionResponse(boolean isResponseReceived, byte[] response) {
        Log.d(TAG, "processProvisionResponse()");
        assert mMediaCryptoSession == null;

        assert mProvisioningPending;
        mProvisioningPending = false;

        boolean success = false;

        // If |mMediaDrm| is released, there is no need to callback native.
        if (mMediaDrm != null) {
            success = isResponseReceived ? provideProvisionResponse(response) : false;
        }

        if (mResetDeviceCredentialsPending) {
            assert !mRequiresMediaCrypto;
            onResetDeviceCredentialsCompleted(success);
            mResetDeviceCredentialsPending = false;
            return;
        }

        // This may call release() internally. However, sMediaCryptoDeferrer.onProvisionDone() will
        // still be called below to ensure provisioning failure here doesn't block other
        // MediaDrmBridge instances from proceeding.
        onProvisioned(success);

        if (mRequiresMediaCrypto) {
            sMediaCryptoDeferrer.onProvisionDone();
        }
    }

    /**
     * Provides the provision response to MediaDrm.
     *
     * @returns false if the response is invalid or on error, true otherwise.
     */
    boolean provideProvisionResponse(byte[] response) {
        if (response == null || response.length == 0) {
            Log.e(TAG, "Invalid provision response.");
            return false;
        }

        try {
            mMediaDrm.provideProvisionResponse(response);
            return true;
        } catch (android.media.DeniedByServerException e) {
            Log.e(TAG, "failed to provide provision response", e);
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "failed to provide provision response", e);
        }
        return false;
    }

    /*
     *  Continue to createMediaCrypto() after provisioning.
     *
     * @param success Whether provisioning has succeeded or not.
     */
    void onProvisioned(boolean success) {
        if (!success) {
            release();
            return;
        }

        assert mRequiresMediaCrypto;

        if (!mOriginSet) {
            createMediaCrypto();
            return;
        }

        // When |mOriginSet|, notify the storage onProvisioned, and continue
        // creating MediaCrypto after that.
        mStorage.onProvisioned(new Callback<Boolean>() {
            @Override
            public void onResult(Boolean initSuccess) {
                assert mMediaCryptoSession == null;

                if (!initSuccess) {
                    Log.e(TAG, "Failed to initialize storage for origin");
                    release();
                    return;
                }

                createMediaCrypto();
            }
        });
    }

    /**
     * Delay session event handler if |mSessionEventDeferrer| exists and
     * matches |sessionId|. Otherwise run the handler immediately.
     */
    private void deferEventHandleIfNeeded(SessionId sessionId, Runnable handler) {
        if (mSessionEventDeferrer != null && mSessionEventDeferrer.shouldDefer(sessionId)) {
            mSessionEventDeferrer.defer(handler);
            return;
        }

        handler.run();
    }

    // Helper functions to make native calls.

    private void onMediaCryptoReady(MediaCrypto mediaCrypto) {
        if (isNativeMediaDrmBridgeValid()) {
            nativeOnMediaCryptoReady(mNativeMediaDrmBridge, mediaCrypto);
        }
    }

    private void onPromiseResolved(final long promiseId) {
        if (isNativeMediaDrmBridgeValid()) {
            nativeOnPromiseResolved(mNativeMediaDrmBridge, promiseId);
        }
    }

    private void onPromiseResolvedWithSession(final long promiseId, final SessionId sessionId) {
        if (isNativeMediaDrmBridgeValid()) {
            nativeOnPromiseResolvedWithSession(mNativeMediaDrmBridge, promiseId, sessionId.emeId());
        }
    }

    private void onPromiseRejected(final long promiseId, final String errorMessage) {
        Log.e(TAG, "onPromiseRejected: %s", errorMessage);
        if (isNativeMediaDrmBridgeValid()) {
            nativeOnPromiseRejected(mNativeMediaDrmBridge, promiseId, errorMessage);
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    private void onSessionMessage(final SessionId sessionId, final MediaDrm.KeyRequest request) {
        if (!isNativeMediaDrmBridgeValid()) return;

        int requestType = MediaDrm.KeyRequest.REQUEST_TYPE_INITIAL;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            requestType = request.getRequestType();
        } else {
            // Prior to M, getRequestType() is not supported. Do our best guess here: Assume
            // requests with a URL are renewals and all others are initial requests.
            requestType = request.getDefaultUrl().isEmpty()
                    ? MediaDrm.KeyRequest.REQUEST_TYPE_INITIAL
                    : MediaDrm.KeyRequest.REQUEST_TYPE_RENEWAL;
        }

        nativeOnSessionMessage(
                mNativeMediaDrmBridge, sessionId.emeId(), requestType, request.getData());
    }

    private void onSessionClosed(final SessionId sessionId) {
        if (isNativeMediaDrmBridgeValid()) {
            nativeOnSessionClosed(mNativeMediaDrmBridge, sessionId.emeId());
        }
    }

    private void onSessionKeysChange(final SessionId sessionId, final Object[] keysInfo,
            final boolean hasAdditionalUsableKey, final boolean isKeyRelease) {
        if (isNativeMediaDrmBridgeValid()) {
            nativeOnSessionKeysChange(mNativeMediaDrmBridge, sessionId.emeId(), keysInfo,
                    hasAdditionalUsableKey, isKeyRelease);
        }
    }

    private void onSessionExpirationUpdate(final SessionId sessionId, final long expirationTime) {
        if (isNativeMediaDrmBridgeValid()) {
            nativeOnSessionExpirationUpdate(
                    mNativeMediaDrmBridge, sessionId.emeId(), expirationTime);
        }
    }

    private void onResetDeviceCredentialsCompleted(final boolean success) {
        if (isNativeMediaDrmBridgeValid()) {
            nativeOnResetDeviceCredentialsCompleted(mNativeMediaDrmBridge, success);
        }
    }

    @MainDex
    private class EventListener implements MediaDrm.OnEventListener {
        @Override
        public void onEvent(
                MediaDrm mediaDrm, byte[] drmSessionId, int event, int extra, byte[] data) {
            if (drmSessionId == null) {
                Log.e(TAG, "EventListener: Null session.");
                return;
            }
            SessionId sessionId = getSessionIdByDrmId(drmSessionId);

            if (sessionId == null) {
                Log.e(TAG, "EventListener: Invalid session %s",
                        SessionId.toHexString(drmSessionId));
                return;
            }

            SessionInfo sessionInfo = mSessionManager.get(sessionId);
            switch (event) {
                case MediaDrm.EVENT_KEY_REQUIRED:
                    Log.d(TAG, "MediaDrm.EVENT_KEY_REQUIRED");
                    MediaDrm.KeyRequest request = null;
                    try {
                        request = getKeyRequest(sessionId, data, sessionInfo.mimeType(),
                                sessionInfo.keyType(), null);
                    } catch (android.media.NotProvisionedException e) {
                        Log.e(TAG, "Device not provisioned", e);
                        return;
                    }
                    if (request != null) {
                        onSessionMessage(sessionId, request);
                    } else {
                        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
                            onSessionKeysChange(sessionId,
                                    getDummyKeysInfo(MediaDrm.KeyStatus.STATUS_INTERNAL_ERROR)
                                            .toArray(),
                                    false, false);
                        }
                        Log.e(TAG, "EventListener: getKeyRequest failed.");
                        return;
                    }
                    break;
                case MediaDrm.EVENT_KEY_EXPIRED:
                    Log.d(TAG, "MediaDrm.EVENT_KEY_EXPIRED");
                    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
                        onSessionKeysChange(sessionId,
                                getDummyKeysInfo(MediaDrm.KeyStatus.STATUS_EXPIRED).toArray(),
                                false, sessionInfo.keyType() == MediaDrm.KEY_TYPE_RELEASE);
                    }
                    break;
                case MediaDrm.EVENT_VENDOR_DEFINED:
                    Log.d(TAG, "MediaDrm.EVENT_VENDOR_DEFINED");
                    assert false; // Should never happen.
                    break;
                default:
                    Log.e(TAG, "Invalid DRM event " + event);
                    return;
            }
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    @MainDex
    private class KeyStatusChangeListener implements MediaDrm.OnKeyStatusChangeListener {
        private List<KeyStatus> getKeysInfo(List<MediaDrm.KeyStatus> keyInformation) {
            List<KeyStatus> keysInfo = new ArrayList<KeyStatus>();
            for (MediaDrm.KeyStatus keyStatus : keyInformation) {
                keysInfo.add(new KeyStatus(keyStatus.getKeyId(), keyStatus.getStatusCode()));
            }
            return keysInfo;
        }

        @Override
        public void onKeyStatusChange(MediaDrm md, byte[] drmSessionId,
                final List<MediaDrm.KeyStatus> keyInformation, final boolean hasNewUsableKey) {
            final SessionId sessionId = getSessionIdByDrmId(drmSessionId);

            assert sessionId != null;
            assert mSessionManager.get(sessionId) != null;

            final boolean isKeyRelease =
                    mSessionManager.get(sessionId).keyType() == MediaDrm.KEY_TYPE_RELEASE;

            deferEventHandleIfNeeded(sessionId, new Runnable() {
                @Override
                public void run() {
                    Log.d(TAG,
                            "KeysStatusChange: " + sessionId.toHexString() + ", "
                                    + hasNewUsableKey);
                    onSessionKeysChange(sessionId, getKeysInfo(keyInformation).toArray(),
                            hasNewUsableKey, isKeyRelease);
                }
            });
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    @MainDex
    private class ExpirationUpdateListener implements MediaDrm.OnExpirationUpdateListener {
        @Override
        public void onExpirationUpdate(
                MediaDrm md, byte[] drmSessionId, final long expirationTime) {
            final SessionId sessionId = getSessionIdByDrmId(drmSessionId);

            assert sessionId != null;

            deferEventHandleIfNeeded(sessionId, new Runnable() {
                @Override
                public void run() {
                    Log.d(TAG,
                            "ExpirationUpdate: " + sessionId.toHexString() + ", " + expirationTime);
                    onSessionExpirationUpdate(sessionId, expirationTime);
                }
            });
        }
    }

    @MainDex
    private class KeyUpdatedCallback implements Callback<Boolean> {
        private final SessionId mSessionId;
        private final long mPromiseId;
        private final boolean mIsKeyRelease;

        KeyUpdatedCallback(SessionId sessionId, long promiseId, boolean isKeyRelease) {
            mSessionId = sessionId;
            mPromiseId = promiseId;
            mIsKeyRelease = isKeyRelease;
        }

        @Override
        public void onResult(Boolean success) {
            if (!success) {
                onPromiseRejected(mPromiseId, "failed to update key after response accepted");
                return;
            }

            Log.d(TAG, "Key successfully %s for session %s", mIsKeyRelease ? "released" : "added",
                    mSessionId.toHexString());
            onPromiseResolved(mPromiseId);

            if (!mIsKeyRelease && Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
                onSessionKeysChange(mSessionId,
                        getDummyKeysInfo(MediaDrm.KeyStatus.STATUS_USABLE).toArray(), true,
                        mIsKeyRelease);
            }
        }
    }

    // Native functions. At the native side, must post the task immediately to
    // avoid reentrancy issues.
    private native void nativeOnMediaCryptoReady(
            long nativeMediaDrmBridge, MediaCrypto mediaCrypto);

    private native void nativeOnStartProvisioning(
            long nativeMediaDrmBridge, String defaultUrl, byte[] requestData);

    private native void nativeOnPromiseResolved(long nativeMediaDrmBridge, long promiseId);
    private native void nativeOnPromiseResolvedWithSession(
            long nativeMediaDrmBridge, long promiseId, byte[] emeSessionId);
    private native void nativeOnPromiseRejected(
            long nativeMediaDrmBridge, long promiseId, String errorMessage);

    private native void nativeOnSessionMessage(
            long nativeMediaDrmBridge, byte[] emeSessionId, int requestType, byte[] message);
    private native void nativeOnSessionClosed(long nativeMediaDrmBridge, byte[] emeSessionId);
    private native void nativeOnSessionKeysChange(long nativeMediaDrmBridge, byte[] emeSessionId,
            Object[] keysInfo, boolean hasAdditionalUsableKey, boolean isKeyRelease);
    private native void nativeOnSessionExpirationUpdate(
            long nativeMediaDrmBridge, byte[] emeSessionId, long expirationTime);

    private native void nativeOnResetDeviceCredentialsCompleted(
            long nativeMediaDrmBridge, boolean success);
}
