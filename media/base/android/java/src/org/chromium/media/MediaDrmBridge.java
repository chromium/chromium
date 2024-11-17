// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.SuppressLint;
import android.media.MediaCrypto;
import android.media.MediaDrm;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.media.MediaDrmSessionManager.SessionId;
import org.chromium.media.MediaDrmSessionManager.SessionInfo;

import java.lang.reflect.Method;
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
// Instead, it's created to unprovision the device/origin, which is only supported on newer Android
// versions. unprovision() is triggered when user clears media licenses.
//
// NotProvisionedException: If this exception is thrown in operations other than
// createMediaCrypto(), we will fail that operation and not trying to provision again.
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
 * MediaCodecAudioDecoders or MediaCodecVideoDecoders.
 */
@JNINamespace("media")
@SuppressLint("WrongConstant")
public class MediaDrmBridge {
    private static final String TAG = "media";
    private static final String SECURITY_LEVEL = "securityLevel";
    private static final String CURRENT_HDCP_LEVEL = "hdcpLevel";
    private static final String SERVER_CERTIFICATE = "serviceCertificate";
    private static final String ORIGIN = "origin";
    private static final String PRIVACY_MODE = "privacyMode";
    private static final String SESSION_SHARING = "sessionSharing";
    private static final String ENABLE = "enable";
    private static final long INVALID_NATIVE_MEDIA_DRM_BRIDGE = 0;
    private static final String FIRST_API_LEVEL = "ro.product.first_api_level";

    // See http://dashif.org/identifiers/content_protection/ for Scheme UUIDs for different Key
    // systems.
    private static final UUID WIDEVINE_UUID =
            UUID.fromString("edef8ba9-79d6-4ace-a3c8-27dcd51d21ed");
    private static final UUID CLEARKEY_UUID =
            UUID.fromString("e2719d58-a985-b3c9-781a-b030af78d30e");

    // On Android L and before, MediaDrm doesn't support KeyStatus at all. On later Android
    // versions, key IDs are not available on sessions where getKeyRequest() has been called with
    // KEY_TYPE_RELEASE. In these cases, the EME spec recommends to use a one-byte key ID 0:
    // "Some older platforms may contain Key System implementations that do not expose key IDs,
    // making it impossible to provide a compliant user agent implementation. To maximize
    // interoperability, user agent implementations exposing such CDMs should implement this member
    // as follows: Whenever a non-empty list is appropriate, such as when the key session
    // represented by this object may contain key(s), populate the map with a single pair containing
    // the one-byte key ID 0 and the MediaKeyStatus most appropriate for the aggregated status of
    // this object."
    // See details: https://www.w3.org/TR/encrypted-media/#dom-mediakeysession-keystatuses
    private static final byte[] PLACEHOLDER_KEY_ID = new byte[] {0};

    // Special provision response to remove the cert.
    private static final byte[] UNPROVISION = ApiCompatibilityUtils.getBytesUtf8("unprovision");

    private MediaDrm mMediaDrm;
    private MediaCrypto mMediaCrypto;

    // Reference to the C++ media::MediaDrmBridge object.
    private long mNativeMediaDrmBridge;

    private final UUID mKeySystemUuid;
    private final boolean mRequiresMediaCrypto;

    // A session only for the purpose of creating a MediaCrypto object. Created
    // after construction, or after the provisioning process is successfully
    // completed. No getKeyRequest() should be called on |mMediaCryptoSession|.
    private SessionId mMediaCryptoSession;

    // The map of all opened sessions (excluding mMediaCryptoSession) to their
    // associated meta data, e.g. mime types, key types.
    private MediaDrmSessionManager mSessionManager;

    // The persistent storage to record origin provisioning information.
    private MediaDrmStorageBridge mStorage;

    // Whether the current MediaDrmBridge instance is waiting for provisioning response.
    private boolean mProvisioningPending;

    // Current 'ORIGIN" setting.
    private String mOrigin;

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
                    Log.d(TAG, "provision triggered while running deferred CreateMediaCrypto()");
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

    /** An equivalent of MediaDrm.KeyStatus, which is only available on M+. */
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
     * Creates a placeholder single element list of KeyStatus with a placeholder key ID and the
     * specified keyStatus.
     */
    private static List<KeyStatus> getPlaceholderKeysInfo(int statusCode) {
        List<KeyStatus> keysInfo = new ArrayList<KeyStatus>();
        keysInfo.add(new KeyStatus(PLACEHOLDER_KEY_ID, statusCode));
        return keysInfo;
    }

    private static UUID getUuidFromBytes(byte[] data) {
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
        return mKeySystemUuid.equals(WIDEVINE_UUID);
    }

    private boolean isClearKey() {
        return mKeySystemUuid.equals(CLEARKEY_UUID);
    }

    private MediaDrmBridge(
            UUID keySystemUuid,
            boolean requiresMediaCrypto,
            long nativeMediaDrmBridge,
            long nativeMediaDrmStorageBridge)
            throws android.media.UnsupportedSchemeException {
        mKeySystemUuid = keySystemUuid;
        mMediaDrm = new MediaDrm(keySystemUuid);
        mRequiresMediaCrypto = requiresMediaCrypto;

        mNativeMediaDrmBridge = nativeMediaDrmBridge;
        assert isNativeMediaDrmBridgeValid();

        mStorage = new MediaDrmStorageBridge(nativeMediaDrmStorageBridge);
        mSessionManager = new MediaDrmSessionManager(mStorage);

        mProvisioningPending = false;

        mMediaDrm.setOnEventListener(new EventListener());
        mMediaDrm.setOnExpirationUpdateListener(new ExpirationUpdateListener(), null);
        mMediaDrm.setOnKeyStatusChangeListener(new KeyStatusChangeListener(), null);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            mMediaDrm.setOnSessionLostStateListener(new SessionLostStateListener(), null);
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
            Log.i(TAG, "Not provisioned during openSession()");

            if (!sMediaCryptoDeferrer.isProvisioning()) {
                boolean result = startProvisioning();
                if (!result) {
                    onCreateError(MediaDrmCreateError.FAILED_TO_START_PROVISIONING);
                }
                return result;
            }

            // Cannot provision. Defer MediaCrypto creation and try again later.
            Log.d(TAG, "defer CreateMediaCrypto() calls");
            sMediaCryptoDeferrer.defer(
                    new Runnable() {
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
            onCreateError(MediaDrmCreateError.FAILED_MEDIA_CRYPTO_SESSION);
            return false;
        }

        mMediaCryptoSession = SessionId.createTemporarySessionId(mediaCryptoSessionDrmId);

        Log.d(TAG, "MediaCrypto Session created: %s", mMediaCryptoSession);

        // Create MediaCrypto object.
        // MediaCrypto#isCryptoSchemeSupported may do a disk read.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            if (MediaCrypto.isCryptoSchemeSupported(mKeySystemUuid)) {
                mMediaCrypto = new MediaCrypto(mKeySystemUuid, mMediaCryptoSession.drmId());
                Log.d(TAG, "MediaCrypto successfully created!");
                onMediaCryptoReady(mMediaCrypto);
                return true;
            } else {
                Log.e(TAG, "Cannot create MediaCrypto for unsupported scheme.");
                onCreateError(MediaDrmCreateError.UNSUPPORTED_MEDIACRYPTO_SCHEME);
            }
        } catch (android.media.MediaCryptoException e) {
            Log.e(TAG, "Cannot create MediaCrypto", e);
            onCreateError(MediaDrmCreateError.FAILED_MEDIA_CRYPTO_CREATE);
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
     * Check whether the crypto scheme is supported for the given container. If |containerMimeType|
     * is an empty string, we just return whether the crypto scheme is supported.
     *
     * @return true if the container and the crypto scheme is supported, or false otherwise.
     */
    @CalledByNative
    private static boolean isCryptoSchemeSupported(byte[] keySystemUuid, String containerMimeType) {
        UUID cryptoScheme = getUuidFromBytes(keySystemUuid);
        if (cryptoScheme == null) {
            return false;
        }

        // MediaDrm.isCryptoSchemeSupported reads from disk
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            if (containerMimeType.isEmpty()) {
                return MediaDrm.isCryptoSchemeSupported(cryptoScheme);
            }

            return MediaDrm.isCryptoSchemeSupported(cryptoScheme, containerMimeType);
        } catch (IllegalArgumentException e) {
            // A few devices have broken DRM HAL configs and throw an exception here regardless of
            // the arguments; just assume this means the scheme is not supported.
            Log.e(TAG, "Exception in isCryptoSchemeSupported", e);
            return false;
        }
    }

    /**
     * Returns the first API level for this product.
     *
     * @return the converted value for FIRST_API_LEVEL if available,
     * 0 otherwise.
     */
    @CalledByNative
    private static int getFirstApiLevel() {
        int firstApiLevel = 0;
        try {
            final Class<?> systemProperties = Class.forName("android.os.SystemProperties");
            final Method getInt = systemProperties.getMethod("getInt", String.class, int.class);
            firstApiLevel = (Integer) getInt.invoke(null, FIRST_API_LEVEL, 0);
        } catch (Exception e) {
            Log.e(
                    TAG,
                    "Exception while getting system property %s. Using default.",
                    FIRST_API_LEVEL,
                    e);
            firstApiLevel = 0;
        }
        return firstApiLevel;
    }

    /**
     * Create a new MediaDrmBridge from the crypto scheme UUID.
     *
     * @param keySystemBytes Key system UUID.
     * @param securityOrigin Security origin. Empty value means no need for origin isolated storage.
     * @param securityLevel Security level. If empty, the default one should be used.
     * @param nativeMediaDrmBridge Native C++ object of this class.
     * @param nativeMediaDrmStorageBridge Native C++ object of persistent storage.
     */
    @CalledByNative
    private static MediaDrmBridge create(
            byte[] keySystemBytes,
            String securityOrigin,
            String securityLevel,
            String message,
            boolean requiresMediaCrypto,
            long nativeMediaDrmBridge,
            long nativeMediaDrmStorageBridge) {
        Log.i(
                TAG,
                "Create MediaDrmBridge with level %s and origin %s for %s",
                securityLevel,
                securityOrigin,
                message);

        MediaDrmBridge mediaDrmBridge;
        // MediaDrm.isCryptoSchemeSupported reads from disk
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            UUID keySystemUuid = getUuidFromBytes(keySystemBytes);
            if (keySystemUuid == null || !MediaDrm.isCryptoSchemeSupported(keySystemUuid)) {
                MediaDrmBridgeJni.get()
                        .onCreateError(
                                nativeMediaDrmBridge, MediaDrmCreateError.UNSUPPORTED_DRM_SCHEME);
                return null;
            }

            mediaDrmBridge =
                    new MediaDrmBridge(
                            keySystemUuid,
                            requiresMediaCrypto,
                            nativeMediaDrmBridge,
                            nativeMediaDrmStorageBridge);
        } catch (android.media.UnsupportedSchemeException e) {
            Log.e(TAG, "Unsupported DRM scheme", e);
            MediaDrmBridgeJni.get()
                    .onCreateError(
                            nativeMediaDrmBridge, MediaDrmCreateError.UNSUPPORTED_DRM_SCHEME);
            return null;
        } catch (java.lang.IllegalArgumentException e) {
            Log.e(TAG, "Failed to create MediaDrmBridge", e);
            MediaDrmBridgeJni.get()
                    .onCreateError(
                            nativeMediaDrmBridge, MediaDrmCreateError.MEDIADRM_ILLEGAL_ARGUMENT);
            return null;
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "Failed to create MediaDrmBridge", e);
            MediaDrmBridgeJni.get()
                    .onCreateError(
                            nativeMediaDrmBridge, MediaDrmCreateError.MEDIADRM_ILLEGAL_STATE);
            return null;
        }

        if (!securityLevel.isEmpty() && !mediaDrmBridge.setSecurityLevel(securityLevel)) {
            MediaDrmBridgeJni.get()
                    .onCreateError(nativeMediaDrmBridge, MediaDrmCreateError.FAILED_SECURITY_LEVEL);
            mediaDrmBridge.release();
            return null;
        }

        if (!securityOrigin.isEmpty() && !mediaDrmBridge.setOrigin(securityOrigin)) {
            MediaDrmBridgeJni.get()
                    .onCreateError(
                            nativeMediaDrmBridge, MediaDrmCreateError.FAILED_SECURITY_ORIGIN);
            mediaDrmBridge.release();
            return null;
        }

        // When session support is required, we need to create MediaCrypto to
        // finish the CDM creation process. This may trigger the provisioning
        // process, in which case MediaCrypto will be created after provision
        // is finished.
        if (requiresMediaCrypto && !mediaDrmBridge.createMediaCrypto()) {
            // No need to call release() or onCreateError() as createMediaCrypto() does if it fails.
            return null;
        }

        return mediaDrmBridge;
    }

    /**
     * Set the security origin for the MediaDrm. All information should be isolated for different
     * origins, e.g. certificates, licenses.
     */
    private boolean setOrigin(String origin) {
        Log.d(TAG, "Set origin: %s", origin);

        if (!isWidevine()) {
            Log.d(TAG, "Property " + ORIGIN + " isn't supported");
            return true;
        }

        assert mMediaDrm != null;
        assert !origin.isEmpty();

        try {
            mMediaDrm.setPropertyString(ORIGIN, origin);
            mOrigin = origin;
            mOriginSet = true;
            return true;
        } catch (MediaDrm.MediaDrmStateException e) {
            Log.e(TAG, "Failed to set security origin %s", origin, e);
            Log.e(TAG, "getDiagnosticInfo:", e.getDiagnosticInfo());

            // displayMetrics() is only available for P or greater.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                displayMetrics();
            }
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

        String currentSecurityLevel = getSecurityLevel();
        if (currentSecurityLevel.equals("")) {
            // Failure logged by getSecurityLevel().
            return false;
        }

        Log.d(TAG, "Security level: current %s, new %s", currentSecurityLevel, securityLevel);
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
     * Provision the current origin. Normally provisioning will be triggered
     * automatically when MediaCrypto is needed (in the constructor).
     * However, this is available to preprovision an origin separately.
     * MediaDrmBridgeJni.get().onProvisioningComplete() will be called indicating success/failure.
     */
    @CalledByNative
    private void provision() {
        // This should only be called if no MediaCrypto needed.
        assert mMediaDrm != null;
        assert !mProvisioningPending;
        assert !mRequiresMediaCrypto;

        // Provision only works for origin isolated storage.
        if (!mOriginSet) {
            Log.e(TAG, "Calling provision() without an origin.");
            MediaDrmBridgeJni.get()
                    .onProvisioningComplete(mNativeMediaDrmBridge, MediaDrmBridge.this, false);
            return;
        }

        // The security level used for provisioning cannot be set and is cached from when a need for
        // provisioning is last detected. So if we call startProvisioning() it will use the default
        // security level, which may not match the security level needed. As a result this code must
        // call openSession(), which will result in the security level being cached. We don't care
        // about the session, so if it opens simply close it.
        try {
            // This will throw a NotProvisionedException if provisioning needed. If it succeeds,
            // assume this origin ID is already provisioned.
            byte[] drmId = openSession();

            // Provisioning is not required. If a session was actually opened, close it.
            if (drmId != null) {
                SessionId sessionId = SessionId.createTemporarySessionId(drmId);
                closeSessionNoException(sessionId);
            }

            // Indicate that provisioning succeeded.
            MediaDrmBridgeJni.get()
                    .onProvisioningComplete(mNativeMediaDrmBridge, MediaDrmBridge.this, true);

        } catch (android.media.NotProvisionedException e) {
            if (!startProvisioning()) {
                // Indicate that provisioning failed.
                MediaDrmBridgeJni.get()
                        .onProvisioningComplete(mNativeMediaDrmBridge, MediaDrmBridge.this, false);
            }
        }
    }

    /** Unprovision the current origin, a.k.a removing the cert for current origin. */
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

    /** Destroy the MediaDrmBridge object. */
    @CalledByNative
    private void destroy() {
        Log.i(TAG, "Destroying MediaDrmBridge for origin %s", mOrigin);
        mNativeMediaDrmBridge = INVALID_NATIVE_MEDIA_DRM_BRIDGE;
        if (mMediaDrm != null) {
            release();
        }
    }

    /** Release all allocated resources and finish all pending operations. */
    private void release() {
        // Note that mNativeMediaDrmBridge may have already been reset (see destroy()).

        assert mMediaDrm != null;

        // Close all open sessions.
        for (SessionId sessionId : mSessionManager.getAllSessionIds()) {
            Log.i(TAG, "Force closing session %s", sessionId);
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
    private MediaDrm.KeyRequest getKeyRequest(
            SessionId sessionId,
            byte[] data,
            String mime,
            int keyType,
            HashMap<String, String> optionalParameters) {
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
        } catch (android.media.NotProvisionedException e) {
            Log.e(
                    TAG,
                    "The origin needs re-provision. Unprovision the origin so that the next "
                            + "MediaDrmBridge creation can trigger the provision flow.",
                    e);
            unprovision();
        } catch (java.lang.IllegalStateException e) {
            // We've seen both MediaDrmStateException and MediaDrmResetException happening.
            // Since both are IllegalStateExceptions, so they will be handled here.
            // See b/21307186 and crbug.com/1169050 for details.
            Log.e(TAG, "Failed to getKeyRequest().", e);
        }

        if (request == null) {
            Log.e(TAG, "getKeyRequest(%s) failed", sessionId);
        } else {
            Log.d(TAG, "getKeyRequest(%s) succeeded", sessionId);
        }

        return request;
    }

    /**
     * createSession interface to be called from native using primitive types.
     * @see createSession(byte[], String, HashMap<String, String>, long)
     */
    @CalledByNative
    private void createSessionFromNative(
            byte[] initData,
            String mime,
            int keyType,
            String[] optionalParamsArray,
            long promiseId) {
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
    private void createSession(
            byte[] initData,
            String mime,
            int keyType,
            HashMap<String, String> optionalParameters,
            long promiseId) {
        Log.d(TAG, "createSession()");

        if (mMediaDrm == null) {
            Log.e(TAG, "createSession() called when MediaDrm is null.");
            onPromiseRejected(
                    promiseId, MediaDrmSystemCode.NO_MEDIA_DRM, "MediaDrm released previously.");
            return;
        }

        assert mMediaCryptoSession != null;
        assert !mProvisioningPending;

        byte[] drmId = null;
        try {
            drmId = openSession();
        } catch (android.media.NotProvisionedException e) {
            Log.e(TAG, "Device not provisioned", e);
            onPromiseRejected(
                    promiseId,
                    MediaDrmSystemCode.NOT_PROVISIONED,
                    "Device not provisioned during createSession().");
            return;
        }

        if (drmId == null) {
            onPromiseRejected(
                    promiseId, MediaDrmSystemCode.OPEN_SESSION_FAILED, "Open session failed.");
            return;
        }

        assert keyType == MediaDrm.KEY_TYPE_STREAMING || keyType == MediaDrm.KEY_TYPE_OFFLINE;
        SessionId sessionId =
                (keyType == MediaDrm.KEY_TYPE_OFFLINE)
                        ? SessionId.createPersistentSessionId(drmId)
                        : SessionId.createTemporarySessionId(drmId);

        MediaDrm.KeyRequest request =
                getKeyRequest(sessionId, initData, mime, keyType, optionalParameters);
        if (request == null) {
            closeSessionNoException(sessionId);
            onPromiseRejected(
                    promiseId,
                    MediaDrmSystemCode.GET_KEY_REQUEST_FAILED,
                    "Generate request failed.");
            return;
        }

        // Success!
        Log.i(TAG, "createSession(): Session (%s) created for origin %s.", sessionId, mOrigin);
        onPromiseResolvedWithSession(promiseId, sessionId);
        onSessionMessage(sessionId, request);
        mSessionManager.put(sessionId, mime, keyType);
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

    /** Similar with getSessionIdByEmeId, just search for raw DRM session id. */
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
        if (mMediaDrm == null) {
            onPromiseRejected(
                    promiseId,
                    MediaDrmSystemCode.NO_MEDIA_DRM,
                    "closeSession() called when MediaDrm is null.");
            return;
        }

        SessionId sessionId = getSessionIdByEmeId(emeSessionId);
        if (sessionId == null) {
            onPromiseRejected(
                    promiseId,
                    MediaDrmSystemCode.INVALID_SESSION_ID,
                    "Invalid sessionId in closeSession(): " + SessionId.toHexString(emeSessionId));
            return;
        }

        Log.i(TAG, "closeSession(%s)", sessionId);
        try {
            // Some implementations don't have removeKeys, crbug/475632
            mMediaDrm.removeKeys(sessionId.drmId());
        } catch (Exception e) {
            Log.e(TAG, "removeKeys failed: ", e);
        }

        closeSessionNoException(sessionId);
        mSessionManager.remove(sessionId);
        // Code in media_key_session.cc expects the closed event to happen before the close()
        // promise is resolved.
        onSessionClosed(sessionId);
        onPromiseResolved(promiseId);
        Log.i(TAG, "Session %s closed", sessionId);
    }

    /**
     * Close the session without worry about the exception, because some implementations let this
     * method throw exception, crbug/611865.
     */
    private void closeSessionNoException(SessionId sessionId) {
        Log.i(TAG, "Closing session %s", sessionId);
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
        if (mMediaDrm == null) {
            onPromiseRejected(
                    promiseId,
                    MediaDrmSystemCode.NO_MEDIA_DRM,
                    "updateSession() called when MediaDrm is null.");
            return;
        }

        final SessionId sessionId = getSessionIdByEmeId(emeSessionId);
        if (sessionId == null) {
            assert false; // Should never happen.
            onPromiseRejected(
                    promiseId,
                    MediaDrmSystemCode.INVALID_SESSION_ID,
                    "Invalid session in updateSession: " + SessionId.toHexString(emeSessionId));
            return;
        }

        Log.i(TAG, "updateSession(%s)", sessionId);
        int systemCode = MediaDrmSystemCode.UPDATE_FAILED;
        try {
            SessionInfo sessionInfo = mSessionManager.get(sessionId);
            if (sessionInfo == null) {
                assert false; // Should never happen.
                onPromiseRejected(
                        promiseId,
                        MediaDrmSystemCode.INVALID_SESSION_ID,
                        "Internal error: No info for session: " + sessionId);
                return;
            }

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
            } else if (sessionInfo.keyType() == MediaDrm.KEY_TYPE_OFFLINE
                    && keySetId != null
                    && keySetId.length > 0) {
                mSessionManager.setKeySetId(sessionId, keySetId, cb);
            } else {
                // This can be either temporary license update or server certificate update.
                cb.onResult(true);
            }

            return;
        } catch (android.media.NotProvisionedException e) {
            Log.e(TAG, "failed to provide key response", e);
            systemCode = MediaDrmSystemCode.NOT_PROVISIONED;
            unprovision();
        } catch (android.media.DeniedByServerException e) {
            Log.e(TAG, "failed to provide key response", e);
            systemCode = MediaDrmSystemCode.DENIED_BY_SERVER;
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "failed to provide key response", e);
            systemCode = MediaDrmSystemCode.ILLEGAL_STATE;
        } catch (java.lang.IllegalArgumentException e) {
            Log.e(TAG, "failed to provide key response", e);
            systemCode = MediaDrmSystemCode.UPDATE_FAILED;
        }
        onPromiseRejected(promiseId, systemCode, "Update session failed.");
        release();
    }

    /** Load persistent license from storage. */
    @CalledByNative
    private void loadSession(byte[] emeId, final long promiseId) {
        Log.d(TAG, "loadSession()");
        assert !mProvisioningPending;

        mSessionManager.load(
                emeId,
                new Callback<SessionId>() {
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
     * Load session back to memory with MediaDrm. Load persistent storage before calling this. It
     * will fail if persistent storage isn't loaded.
     */
    private void loadSessionWithLoadedStorage(SessionId sessionId, final long promiseId) {
        byte[] drmId = null;
        Log.i(TAG, "loadSession(%s)", sessionId);
        try {
            drmId = openSession();
            if (drmId == null) {
                onPromiseRejected(
                        promiseId,
                        MediaDrmSystemCode.OPEN_SESSION_FAILED,
                        "Failed to open session to load license.");
                return;
            }

            mSessionManager.setDrmId(sessionId, drmId);
            assert Arrays.equals(sessionId.drmId(), drmId);

            SessionInfo sessionInfo = mSessionManager.get(sessionId);
            if (sessionInfo == null) {
                assert false; // Should never happen.
                onPromiseRejected(
                        promiseId,
                        MediaDrmSystemCode.INVALID_SESSION_ID,
                        "Internal error: No info for session: " + sessionId);
                return;
            }

            // If persistent license (KEY_TYPE_OFFLINE) is released but we don't receive the ack
            // from the server, we should avoid restoring the keys. Report success to JS so that
            // they can release it again.
            if (sessionInfo.keyType() == MediaDrm.KEY_TYPE_RELEASE) {
                Log.w(TAG, "Persistent license is waiting for release ack.");
                onPromiseResolvedWithSession(promiseId, sessionId);

                // Report keystatuseschange event to JS. Ideally we should report the event with
                // list of known key IDs. However we can't get the key IDs from MediaDrm. Just
                // report with placeholder key IDs.
                onSessionKeysChange(
                        sessionId,
                        getPlaceholderKeysInfo(MediaDrm.KeyStatus.STATUS_EXPIRED).toArray(),
                        /* hasAdditionalUsableKey= */ false,
                        /* isKeyRelease= */ true);
                return;
            }

            assert sessionInfo.keyType() == MediaDrm.KEY_TYPE_OFFLINE;

            // Defer event handlers until license is loaded.
            assert mSessionEventDeferrer == null;
            mSessionEventDeferrer = new SessionEventDeferrer(sessionId);

            assert sessionId.keySetId() != null;
            mMediaDrm.restoreKeys(sessionId.drmId(), sessionId.keySetId());

            onPromiseResolvedWithSession(promiseId, sessionId);

            mSessionEventDeferrer.fire();
            mSessionEventDeferrer = null;
        } catch (android.media.NotProvisionedException e) {
            // If device isn't provisioned, storage loading should fail.
            onPersistentLicenseLoadFail(sessionId, promiseId, e);
        } catch (java.lang.IllegalStateException e) {
            assert sessionId.drmId() != null;
            onPersistentLicenseLoadFail(sessionId, promiseId, e);
        }
    }

    private void onPersistentLicenseNoExist(long promiseId) {
        // Chromium CDM API requires resolve the promise with empty session id for non-exist
        // license. See media/base/content_decryption_module.h LoadSession for more details.
        onPromiseResolvedWithSession(promiseId, SessionId.createNoExistSessionId());
    }

    // If persistent license load fails, we want to clean the storage and report it to JS as license
    // doesn't exist.
    private void onPersistentLicenseLoadFail(
            SessionId sessionId, final long promiseId, Exception e) {
        Log.w(TAG, "Persistent license load failed for session %s", sessionId, e);
        closeSessionNoException(sessionId);
        mSessionManager.clearPersistentSessionInfo(
                sessionId,
                new Callback<Boolean>() {
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
     * Remove session from device. This will mark the key as released and generate a key release
     * request. The license is removed from the device when the session is updated with a license
     * release response.
     */
    @CalledByNative
    private void removeSession(byte[] emeId, final long promiseId) {
        SessionId sessionId = getSessionIdByEmeId(emeId);
        if (sessionId == null) {
            onPromiseRejected(
                    promiseId, MediaDrmSystemCode.INVALID_SESSION_ID, "Session doesn't exist");
            return;
        }

        Log.i(TAG, "removeSession(%s)", sessionId);
        final SessionInfo sessionInfo = mSessionManager.get(sessionId);
        if (sessionInfo == null) {
            assert false; // Should never happen.
            onPromiseRejected(
                    promiseId,
                    MediaDrmSystemCode.INVALID_SESSION_ID,
                    "Internal error: No info for session: " + sessionId);
            return;
        }

        if (sessionInfo.keyType() == MediaDrm.KEY_TYPE_STREAMING) {
            // TODO(yucliu): Support 'remove' of temporary session.
            onPromiseRejected(
                    promiseId,
                    MediaDrmSystemCode.NOT_PERSISTENT_LICENSE,
                    "Removing temporary session isn't implemented");
            return;
        }

        assert sessionId.keySetId() != null;

        // Persist the key type before removing the keys completely.
        // 1. If we fails to persist the key type, both the persistent storage and MediaDrm think
        // the keys are alive. JS can just remove the session again.
        // 2. If we are able to persist the key type but don't get the callback, persistent storage
        // thinks keys are removed but MediaDrm thinks keys are alive. JS thinks keys are removed
        // next time it loads the keys, which matches the expectation of this function.
        mSessionManager.setKeyType(
                sessionId,
                MediaDrm.KEY_TYPE_RELEASE,
                new Callback<Boolean>() {
                    @Override
                    public void onResult(Boolean success) {
                        if (!success) {
                            onPromiseRejected(
                                    promiseId,
                                    MediaDrmSystemCode.SET_KEY_TYPE_RELEASE_FAILED,
                                    "Fail to update persistent storage");
                            return;
                        }

                        doRemoveSession(sessionId, sessionInfo.mimeType(), promiseId);
                    }
                });
    }

    private void doRemoveSession(SessionId sessionId, String mimeType, long promiseId) {
        // Get key release request.
        MediaDrm.KeyRequest request =
                getKeyRequest(sessionId, null, mimeType, MediaDrm.KEY_TYPE_RELEASE, null);

        if (request == null) {
            onPromiseRejected(
                    promiseId,
                    MediaDrmSystemCode.GET_KEY_RELEASE_REQUEST_FAILED,
                    "Fail to generate key release request");
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
    }

    /**
     * Return the current HDCP level of this MediaDrm object. In case of failure this returns the
     * empty string, which is treated by the native side as "HDCP_VERSION_NONE".
     */
    @CalledByNative
    private String getCurrentHdcpLevel() {

        // May return empty string on failure.
        return getPropertyString(CURRENT_HDCP_LEVEL);
    }

    /**
     * Return the security level of this MediaDrm object. In case of failure this returns the empty
     * string, which is treated by the native side as "DEFAULT".
     * TODO(jrummell): Revisit this in the future if the security level gets used for more things.
     */
    @CalledByNative
    private String getSecurityLevel() {

        /// May return empty string on failure.
        return getPropertyString(SECURITY_LEVEL);
    }

    /** Return the version property. In case of failure this returns an empty string. */
    @CalledByNative
    private String getVersion() {
        // PROPERTY_VERSION is supported by all CDMs, but oemCryptoBuildInformation is only
        // supported by Widevine.
        String version = getPropertyString(MediaDrm.PROPERTY_VERSION);
        Log.i(TAG, "Version: %s", version);
        if (isWidevine() && Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            Log.i(
                    TAG,
                    "oemCryptoBuildInformation: %s",
                    getPropertyString("oemCryptoBuildInformation"));
        }
        return version;
    }

    /**
     * Return the `property` string of this DRM object. In case of failure this returns the empty
     * string.
     */
    private String getPropertyString(String property) {
        if (mMediaDrm == null) {
            Log.e(TAG, "getPropertyString(%s): MediaDrm is null.", property);
            return "";
        }

        try {
            return mMediaDrm.getPropertyString(property);
        } catch (Exception e) {
            // getPropertyString() may fail with android.media.MediaDrmResetException or
            // android.media.MediaDrm.MediaDrmStateException. It has also been failing with
            // android.media.ResourceBusyException on some devices. To handle all possible errors
            // catching all exceptions.
            Log.e(TAG, "Failed to get property %s", property, e);
            return "";
        }
    }

    @CalledByNativeForTesting
    private boolean setPropertyStringForTesting(String property, String value) {
        try {
            mMediaDrm.setPropertyString(property, value);
        } catch (Exception e) {
            Log.e(TAG, "Failed to set property %s", property, e);
            return false;
        }
        return true;
    }

    /**
     * Start provisioning. Returns true if a provisioning request can be generated and has been
     * forwarded to C++ code for handling, false otherwise.
     */
    private boolean startProvisioning() {
        Log.d(TAG, "startProvisioning");
        assert !mProvisioningPending;
        mProvisioningPending = true;
        assert mMediaDrm != null;

        if (!isNativeMediaDrmBridgeValid()) {
            return false;
        }

        if (mRequiresMediaCrypto) {
            sMediaCryptoDeferrer.onProvisionStarted();
        }

        // Due to error handling and API requirements, call a version appropriate function to do the
        // actual getProvisionRequest() call.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return startProvisioningPreQ();
        } else {
            return startProvisioningQorLater(/* retryAllowed= */ true);
        }
    }

    /**
     * Start provisioning on Android P or earlier. Returns true if a provisioning request can be
     * generated and has been forwarded to C++ code for handling, false otherwise.
     */
    private boolean startProvisioningPreQ() {
        MediaDrm.ProvisionRequest request;
        try {
            request = mMediaDrm.getProvisionRequest();
        } catch (java.lang.IllegalStateException e) {
            // getProvisionRequest() may fail with android.media.MediaDrm.MediaDrmStateException or
            // android.media.MediaDrmResetException, both of which extend IllegalStateException. As
            // these specific exceptions are only available in API 21 and 23 respectively, using the
            // base exception so that this will work for all API versions.
            Log.e(TAG, "Failed to get provisioning request", e);
            return false;
        }

        Log.i(TAG, "Provisioning origin ID %s", mOriginSet ? mOrigin : "<none>");
        MediaDrmBridgeJni.get()
                .onProvisionRequest(
                        mNativeMediaDrmBridge,
                        MediaDrmBridge.this,
                        request.getDefaultUrl(),
                        request.getData());
        return true;
    }

    /**
     * Start provisioning on Android Q or later, as it allows for better error diagnostics. Returns
     * true if a provisioning request can be generated and has been forwarded to C++ code for
     * handling, false otherwise.
     *
     * @param retryAllowed Flag set to true if transient failures should be retried.
     */
    @RequiresApi(Build.VERSION_CODES.Q)
    private boolean startProvisioningQorLater(boolean retryAllowed) {
        MediaDrm.ProvisionRequest request;
        try {
            request = mMediaDrm.getProvisionRequest();
        } catch (MediaDrm.SessionException e) {
            // SessionException may be thrown when an operation failed in a way that is likely to
            // succeed on a subsequent attempt. However, checking for transient errors is only
            // available on S and later. Try only once to repeat it if possible.
            if (retryAllowed && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && e.isTransient()) {
                return startProvisioningQorLater(false);
            }
            Log.e(TAG, "Failed to get provisioning request", e);
            displayMetrics();
            return false;
        } catch (MediaDrm.MediaDrmStateException e) {
            Log.e(TAG, "Failed to get provisioning request", e);
            Log.e(TAG, "getDiagnosticInfo:", e.getDiagnosticInfo());
            displayMetrics();
            return false;
        } catch (java.lang.IllegalStateException e) {
            // This should only be MediaDrmResetException, but catching all IllegalStateExceptions
            // to be compatible with the pre-Q version.
            Log.e(TAG, "Failed to get provisioning request", e);
            displayMetrics();
            return false;
        }

        Log.i(TAG, "Provisioning origin ID %s", mOriginSet ? mOrigin : "<none>");
        MediaDrmBridgeJni.get()
                .onProvisionRequest(
                        mNativeMediaDrmBridge,
                        MediaDrmBridge.this,
                        request.getDefaultUrl(),
                        request.getData());
        return true;
    }

    /** Display MediaDrm metrics to the error log if available. */
    @RequiresApi(Build.VERSION_CODES.P)
    private void displayMetrics() {
        assert mMediaDrm != null;

        // Property "metrics" specific to Widevine.
        if (isWidevine()) {
            try {
                byte[] metrics = mMediaDrm.getPropertyByteArray("metrics");
                if (metrics != null) {
                    // SessionId class converts an arbitrary byte[] to string.
                    Log.e(TAG, "metrics: ", SessionId.toHexString(metrics));
                }
            } catch (Exception e) {
                // Ignore any errors if this fails as it's just logging additional data
                // and we don't want the caller to fail if this doesn't work.
            }
        }
    }

    /**
     * Called when the provision response is received.
     *
     * @param isResponseReceived Flag set to true if communication with
     * provision server was successful.
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
     * @return false if the response is invalid or on error, true otherwise.
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
     *  Provisioning complete. Continue to createMediaCrypto() if required.
     *
     * @param success Whether provisioning has succeeded or not.
     */
    void onProvisioned(boolean success) {
        if (!mRequiresMediaCrypto) {
            // No MediaCrypto required, so notify provisioning complete.
            MediaDrmBridgeJni.get()
                    .onProvisioningComplete(mNativeMediaDrmBridge, MediaDrmBridge.this, success);
            if (!success) {
                release();
            }
            return;
        }

        if (!success) {
            release();
            return;
        }

        if (!mOriginSet) {
            createMediaCrypto();
            return;
        }

        // When |mOriginSet|, notify the storage onProvisioned, and continue
        // creating MediaCrypto after that.
        mStorage.onProvisioned(
                new Callback<Boolean>() {
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
            MediaDrmBridgeJni.get()
                    .onMediaCryptoReady(mNativeMediaDrmBridge, MediaDrmBridge.this, mediaCrypto);
        }
    }

    private void onPromiseResolved(final long promiseId) {
        if (isNativeMediaDrmBridgeValid()) {
            MediaDrmBridgeJni.get()
                    .onPromiseResolved(mNativeMediaDrmBridge, MediaDrmBridge.this, promiseId);
        }
    }

    private void onPromiseResolvedWithSession(final long promiseId, final SessionId sessionId) {
        if (isNativeMediaDrmBridgeValid()) {
            MediaDrmBridgeJni.get()
                    .onPromiseResolvedWithSession(
                            mNativeMediaDrmBridge,
                            MediaDrmBridge.this,
                            promiseId,
                            sessionId.emeId());
        }
    }

    private void onPromiseRejected(
            final long promiseId, final long systemCode, final String errorMessage) {
        Log.e(TAG, "onPromiseRejected: %s", errorMessage);
        if (isNativeMediaDrmBridgeValid()) {
            MediaDrmBridgeJni.get()
                    .onPromiseRejected(
                            mNativeMediaDrmBridge,
                            MediaDrmBridge.this,
                            promiseId,
                            systemCode,
                            errorMessage);
        }
    }

    private void onSessionMessage(final SessionId sessionId, final MediaDrm.KeyRequest request) {
        if (!isNativeMediaDrmBridgeValid()) return;

        int requestType = request.getRequestType();
        MediaDrmBridgeJni.get()
                .onSessionMessage(
                        mNativeMediaDrmBridge,
                        MediaDrmBridge.this,
                        sessionId.emeId(),
                        requestType,
                        request.getData());
    }

    private void onSessionClosed(final SessionId sessionId) {
        if (isNativeMediaDrmBridgeValid()) {
            MediaDrmBridgeJni.get()
                    .onSessionClosed(mNativeMediaDrmBridge, MediaDrmBridge.this, sessionId.emeId());
        }
    }

    private void onSessionKeysChange(
            final SessionId sessionId,
            final Object[] keysInfo,
            final boolean hasAdditionalUsableKey,
            final boolean isKeyRelease) {
        if (isNativeMediaDrmBridgeValid()) {
            MediaDrmBridgeJni.get()
                    .onSessionKeysChange(
                            mNativeMediaDrmBridge,
                            MediaDrmBridge.this,
                            sessionId.emeId(),
                            keysInfo,
                            hasAdditionalUsableKey,
                            isKeyRelease);
        }
    }

    private void onSessionExpirationUpdate(final SessionId sessionId, final long expirationTime) {
        if (isNativeMediaDrmBridgeValid()) {
            MediaDrmBridgeJni.get()
                    .onSessionExpirationUpdate(
                            mNativeMediaDrmBridge,
                            MediaDrmBridge.this,
                            sessionId.emeId(),
                            expirationTime);
        }
    }

    private void onCreateError(final int error) {
        if (isNativeMediaDrmBridgeValid()) {
            MediaDrmBridgeJni.get().onCreateError(mNativeMediaDrmBridge, error);
        }
    }

    private class EventListener implements MediaDrm.OnEventListener {
        @Override
        public void onEvent(
                MediaDrm mediaDrm, byte[] drmSessionId, int event, int extra, byte[] data) {
            if (drmSessionId == null) {
                // Prior to Android M EVENT_PROVISION_REQUIRED was used to signify that provisioning
                // was required before the session could be created. Unprovisioned errors are
                // handled elsewhere, so no need to log a message.
                if (event != MediaDrm.EVENT_PROVISION_REQUIRED) {
                    Log.e(TAG, "EventListener: No session for event %d.", event);
                }
                return;
            }

            SessionId sessionId = getSessionIdByDrmId(drmSessionId);
            if (sessionId == null) {
                // May happen if the event gets scheduled after the session is gone.
                Log.w(
                        TAG,
                        "EventListener: Invalid session %s",
                        SessionId.toHexString(drmSessionId));
                return;
            }

            SessionInfo sessionInfo = mSessionManager.get(sessionId);
            if (sessionInfo == null) {
                // May happen if the event gets scheduled after the session is gone.
                Log.w(TAG, "EventListener: No info for session %s", sessionId);
                return;
            }

            MediaDrm.KeyRequest request = null;
            switch (event) {
                case MediaDrm.EVENT_KEY_REQUIRED:
                    Log.d(TAG, "MediaDrm.EVENT_KEY_REQUIRED for session %s", sessionId);
                    request =
                            getKeyRequest(
                                    sessionId,
                                    data,
                                    sessionInfo.mimeType(),
                                    sessionInfo.keyType(),
                                    null);
                    if (request != null) {
                        onSessionMessage(sessionId, request);
                    } else {
                        Log.e(TAG, "EventListener: getKeyRequest failed.");
                        return;
                    }
                    break;
                case MediaDrm.EVENT_KEY_EXPIRED:
                    Log.d(TAG, "MediaDrm.EVENT_KEY_EXPIRED for session %s", sessionId);
                    break;
                    // (b/271451225) This event is generated during ClearKey implementation in
                    // Android.
                case MediaDrm.EVENT_VENDOR_DEFINED:
                    Log.d(TAG, "MediaDrm.EVENT_VENDOR_DEFINED for session %s", sessionId);
                    request =
                            getKeyRequest(
                                    sessionId,
                                    data,
                                    sessionInfo.mimeType(),
                                    sessionInfo.keyType(),
                                    null);
                    if (request != null) {
                        onSessionMessage(sessionId, request);
                    } else {
                        Log.e(TAG, "EventListener: getKeyRequest failed.");
                        return;
                    }
                    break;
                default:
                    Log.w(TAG, "Ignoring MediaDrm event %d for session %s" + event, sessionId);
                    break;
            }
        }
    }

    // TODO(b/263310318): Add tests using setPropertyStringForTesting("drmErrorTest", "lostState")
    // which triggers this onSessionLostState for ClearKey. Android's ClearKey is not currently used
    // as we use AesDecryptor, so implement tests once we make the switch to Android's ClearKey.
    @RequiresApi(Build.VERSION_CODES.Q)
    private class SessionLostStateListener implements MediaDrm.OnSessionLostStateListener {

        @Override
        public void onSessionLostState(MediaDrm md, byte[] drmSessionId) {
            final SessionId sessionId = getSessionIdByDrmId(drmSessionId);

            deferEventHandleIfNeeded(
                    sessionId,
                    new Runnable() {
                        @Override
                        public void run() {
                            if (sessionId == null) {
                                Log.w(
                                        TAG,
                                        "SessionLost: Unknown session %s",
                                        SessionId.toHexString(drmSessionId));
                                return;
                            }

                            Log.d(TAG, "SessionLost: " + sessionId);
                            // TODO(crbug.com/40181810): Consider passing a reason for sessionClosed
                            // that more closely
                            // represents a lost state.
                            onSessionClosed(sessionId);
                        }
                    });
        }
    }

    private class KeyStatusChangeListener implements MediaDrm.OnKeyStatusChangeListener {
        private List<KeyStatus> getKeysInfo(List<MediaDrm.KeyStatus> keyInformation) {
            List<KeyStatus> keysInfo = new ArrayList<KeyStatus>();
            for (MediaDrm.KeyStatus keyStatus : keyInformation) {
                keysInfo.add(new KeyStatus(keyStatus.getKeyId(), keyStatus.getStatusCode()));
            }
            return keysInfo;
        }

        @Override
        public void onKeyStatusChange(
                MediaDrm md,
                byte[] drmSessionId,
                final List<MediaDrm.KeyStatus> keyInformation,
                final boolean hasNewUsableKey) {
            final SessionId sessionId = getSessionIdByDrmId(drmSessionId);

            deferEventHandleIfNeeded(
                    sessionId,
                    new Runnable() {
                        @Override
                        public void run() {
                            if (sessionId == null) {
                                Log.w(
                                        TAG,
                                        "KeyStatusChange: Unknown session %s",
                                        SessionId.toHexString(drmSessionId));
                                return;
                            }

                            SessionInfo sessionInfo = mSessionManager.get(sessionId);
                            if (sessionInfo == null) {
                                Log.w(TAG, "KeyStatusChange: No info for session %s", sessionId);
                                return;
                            }

                            boolean isKeyRelease =
                                    sessionInfo.keyType() == MediaDrm.KEY_TYPE_RELEASE;

                            Log.i(TAG, "KeysStatusChange(%s): %b", sessionId, hasNewUsableKey);
                            onSessionKeysChange(
                                    sessionId,
                                    getKeysInfo(keyInformation).toArray(),
                                    hasNewUsableKey,
                                    isKeyRelease);
                        }
                    });
        }
    }

    private class ExpirationUpdateListener implements MediaDrm.OnExpirationUpdateListener {
        @Override
        public void onExpirationUpdate(
                MediaDrm md, byte[] drmSessionId, final long expirationTime) {
            final SessionId sessionId = getSessionIdByDrmId(drmSessionId);

            deferEventHandleIfNeeded(
                    sessionId,
                    new Runnable() {
                        @Override
                        public void run() {
                            if (sessionId == null) {
                                Log.w(
                                        TAG,
                                        "ExpirationUpdate: Unknown session %s",
                                        SessionId.toHexString(drmSessionId));
                                return;
                            }

                            Log.i(
                                    TAG,
                                    "ExpirationUpdate(%s): %tF %tT",
                                    sessionId,
                                    expirationTime,
                                    expirationTime);
                            onSessionExpirationUpdate(sessionId, expirationTime);
                        }
                    });
        }
    }

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
                onPromiseRejected(
                        mPromiseId,
                        MediaDrmSystemCode.KEY_UPDATE_FAILED,
                        "failed to update key after response accepted");
                return;
            }

            Log.i(
                    TAG,
                    "Key successfully %s for session %s",
                    mIsKeyRelease ? "released" : "added",
                    mSessionId);
            onPromiseResolved(mPromiseId);
        }
    }

    // At the native side, must post the task immediately to avoid reentrancy issues.
    @NativeMethods
    interface Natives {
        void onMediaCryptoReady(
                long nativeMediaDrmBridge, MediaDrmBridge caller, MediaCrypto mediaCrypto);

        void onProvisionRequest(
                long nativeMediaDrmBridge,
                MediaDrmBridge caller,
                String defaultUrl,
                byte[] requestData);

        void onProvisioningComplete(
                long nativeMediaDrmBridge, MediaDrmBridge caller, boolean success);

        void onPromiseResolved(long nativeMediaDrmBridge, MediaDrmBridge caller, long promiseId);

        void onPromiseResolvedWithSession(
                long nativeMediaDrmBridge,
                MediaDrmBridge caller,
                long promiseId,
                byte[] emeSessionId);

        void onPromiseRejected(
                long nativeMediaDrmBridge,
                MediaDrmBridge caller,
                long promiseId,
                long systemCode,
                String errorMessage);

        void onSessionMessage(
                long nativeMediaDrmBridge,
                MediaDrmBridge caller,
                byte[] emeSessionId,
                int requestType,
                byte[] message);

        void onSessionClosed(long nativeMediaDrmBridge, MediaDrmBridge caller, byte[] emeSessionId);

        void onSessionKeysChange(
                long nativeMediaDrmBridge,
                MediaDrmBridge caller,
                byte[] emeSessionId,
                Object[] keysInfo,
                boolean hasAdditionalUsableKey,
                boolean isKeyRelease);

        void onSessionExpirationUpdate(
                long nativeMediaDrmBridge,
                MediaDrmBridge caller,
                byte[] emeSessionId,
                long expirationTime);

        void onCreateError(long nativeMediaDrmBridge, int errorCode);
    }
}
