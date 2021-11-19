// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.util.Base64;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.fido.common.Transport;
import com.google.android.gms.fido.fido2.api.common.Attachment;
import com.google.android.gms.fido.fido2.api.common.AttestationConveyancePreference;
import com.google.android.gms.fido.fido2.api.common.AuthenticationExtensions;
import com.google.android.gms.fido.fido2.api.common.AuthenticationExtensionsClientOutputs;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorAssertionResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorAttestationResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorSelectionCriteria;
import com.google.android.gms.fido.fido2.api.common.ErrorCode;
import com.google.android.gms.fido.fido2.api.common.FidoAppIdExtension;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredential;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialCreationOptions;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialDescriptor;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialParameters;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialRpEntity;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialType;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialUserEntity;
import com.google.android.gms.fido.fido2.api.common.UserVerificationMethodExtension;
import com.google.android.gms.fido.fido2.api.common.UvmEntries;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.blink.mojom.AuthenticatorAttachment;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.AuthenticatorTransport;
import org.chromium.blink.mojom.CommonCredentialInfo;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.ResidentKeyRequirement;
import org.chromium.blink.mojom.UvmEntry;
import org.chromium.mojo_base.mojom.TimeDelta;

import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Utility class that provides conversions between GmsCore Fido2
 * and authenticator.mojom data structures.
 */
@JNINamespace("webauthn")
public final class Fido2Helper {
    private static final String TAG = "Fido2Helper";
    private static final int ECDSA_COSE_IDENTIFIER = -7;
    private static final String NON_EMPTY_ALLOWLIST_ERROR_MSG =
            "Authentication request must have non-empty allowList";
    private static final String NON_VALID_ALLOWED_CREDENTIALS_ERROR_MSG =
            "Request doesn't have a valid list of allowed credentials.";
    private static final String NO_SCREENLOCK_ERROR_MSG =
            "The device is not secured with any screen lock";
    private static final String CREDENTIAL_EXISTS_ERROR_MSG =
            "One of the excluded credentials exists on the local device";
    private static final String LOW_LEVEL_ERROR_MSG = "Low level error 0x6a80";
    @VisibleForTesting
    public static final double MIN_TIMEOUT_SECONDS = 10;
    @VisibleForTesting
    public static final double MAX_TIMEOUT_SECONDS = 600;

    /**
     * Converts mojo options to gmscore options.
     * @param options Options passed in from the renderer.
     * @return Options to be passed to Fido2 API.
     * @throws NoSuchAlgorithmException
     */
    public static PublicKeyCredentialCreationOptions toMakeCredentialOptions(
            org.chromium.blink.mojom.PublicKeyCredentialCreationOptions options)
            throws NoSuchAlgorithmException {
        // Pack incoming options as Fido2's BrowserMakeCredentialOptions.
        String rpIcon = options.relyingParty.icon != null ? options.relyingParty.icon.url : null;
        PublicKeyCredentialRpEntity rp = new PublicKeyCredentialRpEntity(
                options.relyingParty.id, options.relyingParty.name, rpIcon);

        String userIcon = options.user.icon != null ? options.user.icon.url : null;
        PublicKeyCredentialUserEntity user = new PublicKeyCredentialUserEntity(
                options.user.id, options.user.name, userIcon, options.user.displayName);

        List<PublicKeyCredentialParameters> parameters = new ArrayList<>();
        for (org.chromium.blink.mojom.PublicKeyCredentialParameters param :
                options.publicKeyParameters) {
            if (param.algorithmIdentifier == ECDSA_COSE_IDENTIFIER
                    && param.type == org.chromium.blink.mojom.PublicKeyCredentialType.PUBLIC_KEY) {
                parameters.add(new PublicKeyCredentialParameters(
                        PublicKeyCredentialType.PUBLIC_KEY.toString(), param.algorithmIdentifier));
            }
        }

        // Check that at least one incoming param is supported by the FIDO2 API.
        if (parameters.size() == 0 && options.publicKeyParameters.length != 0) {
            Log.e(TAG, "None of the requested parameters are supported.");
            throw new NoSuchAlgorithmException();
        }

        List<PublicKeyCredentialDescriptor> excludeCredentials =
                convertCredentialDescriptor(options.excludeCredentials);

        AuthenticatorSelectionCriteria selection =
                convertSelectionCriteria(options.authenticatorSelection);

        AttestationConveyancePreference attestationPreference =
                convertAttestationPreference(options.attestation);

        PublicKeyCredentialCreationOptions credentialCreationOptions =
                new PublicKeyCredentialCreationOptions.Builder()
                        .setRp(rp)
                        .setUser(user)
                        .setChallenge(options.challenge)
                        .setParameters(parameters)
                        .setTimeoutSeconds(adjustTimeout(options.timeout))
                        .setExcludeList(excludeCredentials)
                        .setAuthenticatorSelection(selection)
                        .setAttestationConveyancePreference(attestationPreference)
                        .build();
        return credentialCreationOptions;
    }

    /**
     * Converts gmscore AuthenticatorAttestationResponse to mojo MakeCredentialAuthenticatorResponse
     * @param data Response from the Fido2 API.
     * @return Response to be passed to the renderer.
     */
    public static MakeCredentialAuthenticatorResponse toMakeCredentialResponse(
            AuthenticatorAttestationResponse data) throws NoSuchAlgorithmException {
        MakeCredentialAuthenticatorResponse response = new MakeCredentialAuthenticatorResponse();
        CommonCredentialInfo info = new CommonCredentialInfo();

        response.attestationObject = data.getAttestationObject();
        AttestationObjectParts parts = new AttestationObjectParts();
        if (!Fido2HelperJni.get().parseAttestationObject(response.attestationObject, parts)) {
            // A failure to parse the attestation object is fatal to the request
            // on desktop and so the same behavior is used here.
            throw new NoSuchAlgorithmException();
        }
        response.publicKeyAlgo = parts.coseAlgorithm;
        info.authenticatorData = parts.authenticatorData;
        response.publicKeyDer = parts.spki;

        // An empty transports array indicates that we don't have any
        // information about the available transports.
        response.transports = new int[] {};

        info.id = encodeId(data.getKeyHandle());
        info.rawId = data.getKeyHandle();
        info.clientDataJson = data.getClientDataJSON();
        response.info = info;
        return response;
    }

    public static MakeCredentialAuthenticatorResponse toMakeCredentialResponse(
            PublicKeyCredential data) throws NoSuchAlgorithmException {
        MakeCredentialAuthenticatorResponse response =
                toMakeCredentialResponse((AuthenticatorAttestationResponse) data.getResponse());
        return response;
    }

    /**
     * Converts mojo options to gmscore options.
     * @param options Options passed in from the renderer.
     * @return Options to be passed to Fido2 API.
     */
    public static com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialRequestOptions
    toGetAssertionOptions(PublicKeyCredentialRequestOptions options) {
        List<PublicKeyCredentialDescriptor> allowCredentials =
                convertCredentialDescriptor(options.allowCredentials);

        FidoAppIdExtension fidoAppIdExtension =
                (options.appid != null) ? new FidoAppIdExtension(options.appid) : null;
        UserVerificationMethodExtension userVerificationMethodExtension =
                new UserVerificationMethodExtension(options.userVerificationMethods);
        AuthenticationExtensions authenticationExtensions =
                new AuthenticationExtensions.Builder()
                        .setFido2Extension(fidoAppIdExtension)
                        .setUserVerificationMethodExtension(userVerificationMethodExtension)
                        .build();

        // Pack options as Fido2's BrowserPublicKeyCredentialRequestOptions.
        com.google.android.gms.fido.fido2.api.common
                .PublicKeyCredentialRequestOptions credentialRequestOptions =
                new com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialRequestOptions
                        .Builder()
                        .setChallenge(options.challenge)
                        .setTimeoutSeconds(adjustTimeout(options.timeout))
                        .setRpId(options.relyingPartyId)
                        .setAllowList(allowCredentials)
                        /* TODO add back UserVerificationRequirement when the FIDO2 API supports it
                         */
                        .setAuthenticationExtensions(authenticationExtensions)
                        .build();
        return credentialRequestOptions;
    }

    /**
     * Helper method that creates GetAssertionAuthenticatorResponse objects.
     */
    public static GetAssertionAuthenticatorResponse toGetAssertionResponse(
            AuthenticatorAssertionResponse data, boolean appIdExtensionUsed) {
        GetAssertionAuthenticatorResponse response = new GetAssertionAuthenticatorResponse();
        CommonCredentialInfo info = new CommonCredentialInfo();
        response.signature = data.getSignature();
        response.echoAppidExtension = appIdExtensionUsed;
        info.authenticatorData = data.getAuthenticatorData();
        info.id = encodeId(data.getKeyHandle());
        info.rawId = data.getKeyHandle();
        info.clientDataJson = data.getClientDataJSON();
        response.info = info;
        return response;
    }

    public static GetAssertionAuthenticatorResponse toGetAssertionResponse(
            PublicKeyCredential data, boolean appIdExtensionUsed, @Nullable String clientDataJson) {
        GetAssertionAuthenticatorResponse response = toGetAssertionResponse(
                (AuthenticatorAssertionResponse) data.getResponse(), appIdExtensionUsed);
        AuthenticationExtensionsClientOutputs extensionsClientOutputs =
                data.getClientExtensionResults();

        if (clientDataJson != null) {
            response.info.clientDataJson = clientDataJson.getBytes();
        }

        if (extensionsClientOutputs != null && extensionsClientOutputs.getUvmEntries() != null) {
            response.echoUserVerificationMethods = true;
            response.userVerificationMethods =
                    getUserVerificationMethods(extensionsClientOutputs.getUvmEntries());
        }
        return response;
    }

    /**
     * Helper method to convert AuthenticatorErrorResponse errors.
     * @param errorCode
     * @return error code corresponding to an AuthenticatorStatus.
     */
    public static int convertError(ErrorCode errorCode, String errorMsg) {
        // TODO(b/113347251): Use specific error codes instead of strings when GmsCore Fido2
        // provides them.
        switch (errorCode) {
            case SECURITY_ERR:
                // AppId or rpID fails validation.
                return AuthenticatorStatus.INVALID_DOMAIN;
            case TIMEOUT_ERR:
                return AuthenticatorStatus.NOT_ALLOWED_ERROR;
            case ENCODING_ERR:
                // Error encoding results (after user consent).
                return AuthenticatorStatus.UNKNOWN_ERROR;
            case NOT_ALLOWED_ERR:
                // The implementation doesn't support resident keys.
                if (errorMsg != null
                        && (errorMsg.equals(NON_EMPTY_ALLOWLIST_ERROR_MSG)
                                || errorMsg.equals(NON_VALID_ALLOWED_CREDENTIALS_ERROR_MSG))) {
                    return AuthenticatorStatus.EMPTY_ALLOW_CREDENTIALS;
                }
                // The request is not allowed, possibly because the user denied permission.
                return AuthenticatorStatus.NOT_ALLOWED_ERROR;
            case DATA_ERR:
            // Incoming requests were malformed/inadequate. Fallthrough.
            case NOT_SUPPORTED_ERR:
                // Request parameters were not supported.
                return AuthenticatorStatus.ANDROID_NOT_SUPPORTED_ERROR;
            case CONSTRAINT_ERR:
                if (errorMsg != null && errorMsg.equals(NO_SCREENLOCK_ERROR_MSG)) {
                    return AuthenticatorStatus.USER_VERIFICATION_UNSUPPORTED;
                } else {
                    // The user attempted to use a credential that was already registered.
                    return AuthenticatorStatus.CREDENTIAL_EXCLUDED;
                }
            case INVALID_STATE_ERR:
                if (errorMsg != null && errorMsg.equals(CREDENTIAL_EXISTS_ERROR_MSG)) {
                    return AuthenticatorStatus.CREDENTIAL_EXCLUDED;
                }
            // else fallthrough.
            case UNKNOWN_ERR:
                if (errorMsg != null && errorMsg.equals(LOW_LEVEL_ERROR_MSG)) {
                    // The error message returned from GmsCore when the user attempted to use a
                    // credential that is not registered with a U2F security key.
                    return AuthenticatorStatus.NOT_ALLOWED_ERROR;
                }
            // fall through
            default:
                return AuthenticatorStatus.UNKNOWN_ERROR;
        }
    }

    /**
     * Base64 encodes the raw id.
     * @param keyHandle the raw id (key handle of the credential).
     * @return Base64-encoded id.
     */
    private static String encodeId(byte[] keyHandle) {
        return Base64.encodeToString(
                keyHandle, Base64.URL_SAFE | Base64.NO_PADDING | Base64.NO_WRAP);
    }

    /**
     * Parses uvmEntries into userVerificationMethods
     * @param uvmEntries defined in gmscore.
     * @return userVerificationMethods defined in authenticato.mojom.
     */
    private static UvmEntry[] getUserVerificationMethods(UvmEntries uvmEntries) {
        List<com.google.android.gms.fido.fido2.api.common.UvmEntry> uvmEntryList =
                uvmEntries.getUvmEntryList();
        UvmEntry[] userVerificationMethods = new UvmEntry[uvmEntryList.size()];
        for (int i = 0; i < uvmEntryList.size(); i++) {
            UvmEntry uvmEntry = new UvmEntry();
            uvmEntry.userVerificationMethod = uvmEntryList.get(i).getUserVerificationMethod();
            uvmEntry.keyProtectionType = uvmEntryList.get(i).getKeyProtectionType();
            uvmEntry.matcherProtectionType = uvmEntryList.get(i).getMatcherProtectionType();
            userVerificationMethods[i] = uvmEntry;
        }
        return userVerificationMethods;
    }

    private static List<PublicKeyCredentialDescriptor> convertCredentialDescriptor(
            org.chromium.blink.mojom.PublicKeyCredentialDescriptor[] mojoDescriptors) {
        if (mojoDescriptors == null) {
            return null;
        }

        List<PublicKeyCredentialDescriptor> descriptors = new ArrayList<>();
        for (org.chromium.blink.mojom.PublicKeyCredentialDescriptor descriptor : mojoDescriptors) {
            descriptors.add(
                    new PublicKeyCredentialDescriptor(PublicKeyCredentialType.PUBLIC_KEY.toString(),
                            descriptor.id, toTransportList(descriptor.transports)));
        }
        return descriptors;
    }

    private static AuthenticatorSelectionCriteria convertSelectionCriteria(
            org.chromium.blink.mojom.AuthenticatorSelectionCriteria mojoSelection) {
        if (mojoSelection == null) return null;

        return new AuthenticatorSelectionCriteria.Builder()
                .setAttachment(convertAttachment(mojoSelection.authenticatorAttachment))
                .setRequireResidentKey(mojoSelection.residentKey == ResidentKeyRequirement.REQUIRED)
                .build();
    }

    private static List<Transport> toTransportList(int[] mojoTransports) {
        List<Transport> fidoTransports = new ArrayList<>();
        for (int transport : mojoTransports) {
            fidoTransports.add(convertTransport(transport));
        }
        return fidoTransports;
    }

    private static Transport convertTransport(int transport) {
        switch (transport) {
            case AuthenticatorTransport.USB:
                return Transport.USB;
            case AuthenticatorTransport.NFC:
                return Transport.NFC;
            case AuthenticatorTransport.BLE:
                return Transport.BLUETOOTH_LOW_ENERGY;
            case AuthenticatorTransport.INTERNAL:
                return Transport.INTERNAL;
            default:
                return Transport.USB;
        }
    }

    private static Attachment convertAttachment(int attachment) {
        if (attachment == AuthenticatorAttachment.NO_PREFERENCE) {
            return null;
        } else if (attachment == AuthenticatorAttachment.CROSS_PLATFORM) {
            return Attachment.CROSS_PLATFORM;
        } else {
            return Attachment.PLATFORM;
        }
    }

    private static AttestationConveyancePreference convertAttestationPreference(int preference) {
        switch (preference) {
            case org.chromium.blink.mojom.AttestationConveyancePreference.NONE:
                return AttestationConveyancePreference.NONE;
            case org.chromium.blink.mojom.AttestationConveyancePreference.INDIRECT:
                return AttestationConveyancePreference.INDIRECT;
            case org.chromium.blink.mojom.AttestationConveyancePreference.DIRECT:
                return AttestationConveyancePreference.DIRECT;
            case org.chromium.blink.mojom.AttestationConveyancePreference.ENTERPRISE:
                return AttestationConveyancePreference.NONE;
            default:
                return AttestationConveyancePreference.NONE;
        }
    }

    /**
     * Adjusts a timeout between a reasonable minimum and maximum.
     *
     * @param timeout The unadjusted timeout as specified by the website. May be null.
     * @return The adjusted timeout in seconds.
     */
    private static double adjustTimeout(TimeDelta timeout) {
        if (timeout == null) return MAX_TIMEOUT_SECONDS;

        return Math.max(MIN_TIMEOUT_SECONDS,
                Math.min(MAX_TIMEOUT_SECONDS,
                        TimeUnit.MICROSECONDS.toSeconds(timeout.microseconds)));
    }

    // AttestationObjectParts is used to group together the return values of
    // |parseAttestationObject|, below.
    public static final class AttestationObjectParts {
        @CalledByNative("AttestationObjectParts")
        void setAll(byte[] authenticatorData, byte[] spki, int coseAlgorithm) {
            this.authenticatorData = authenticatorData;
            this.spki = spki;
            this.coseAlgorithm = coseAlgorithm;
        }

        public byte[] authenticatorData;
        public byte[] spki;
        public int coseAlgorithm;
    }

    @NativeMethods
    interface Natives {
        // parseAttestationObject parses a CTAP2 attestation[1] and extracts the
        // parts that the browser provides via Javascript API [2].
        //
        // [1] https://www.w3.org/TR/webauthn/#attestation-object
        // [2] https://w3c.github.io/webauthn/#sctn-public-key-easy
        boolean parseAttestationObject(byte[] attestationObject, AttestationObjectParts result);
    }
}
