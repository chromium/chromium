// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import static java.nio.charset.StandardCharsets.UTF_8;

import android.content.Intent;
import android.os.ConditionVariable;
import android.os.Parcel;
import android.os.SystemClock;
import android.util.Base64;

import androidx.annotation.Nullable;

import com.google.common.io.BaseEncoding;

import org.junit.Assert;

import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.blink.mojom.AuthenticationExtensionsClientInputs;
import org.chromium.blink.mojom.AuthenticatorAttachment;
import org.chromium.blink.mojom.AuthenticatorSelectionCriteria;
import org.chromium.blink.mojom.CableAuthentication;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PaymentCredentialInstrument;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PrfValues;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialDescriptor;
import org.chromium.blink.mojom.PublicKeyCredentialParameters;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRpEntity;
import org.chromium.blink.mojom.PublicKeyCredentialType;
import org.chromium.blink.mojom.PublicKeyCredentialUserEntity;
import org.chromium.blink.mojom.UvmEntry;
import org.chromium.content.browser.ClientDataJsonImpl;
import org.chromium.content.browser.ClientDataJsonImplJni;
import org.chromium.mojo_base.mojom.TimeDelta;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.url.mojom.Url;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;
import java.util.concurrent.TimeUnit;

/* NO_BUILDER:
 *
 * Several comments below describe how binary blobs in this file were produced. However, they use
 * Builder classes that no longer appear to exist. Because of this, it's difficult to update these
 * blobs.
 *
 * The blobs are in Android's Parcel format. This format defines a tag-value object that represents
 * a (tag, bytestring) pair. A tag-value is serialized as a little-endian, uint32 where the bottom
 * 16 bits are the tag, and the top 16 bits are the length of the value. If the length is 0xffff,
 * then a second uint32 is used to store the actual length. (This two-word format appears to be
 * always used in practice, even when the length would fit in 16 bits.) The bytestring contains are
 * then the |length| following bytes.
 *
 * A Parcel consists of a tag-value object with tag 0x4f45 and whose value is the rest of the
 * Parcel data. That value contains a series of tag-values that define the members of the
 * destination object. Unknown tags are skipped over.
 *
 * The semantics of the values of the inner values are tag-specific. In the case of byte[] objects,
 * the value is, itself, a tag-value object. Since this has its own length, there can be padding at
 * the end and they seem to be padded with zeros to the nearest four-byte boundary.
 */

/* CONVERT_TO_JAVA
 *
 * Since the Builder classes disappeared (see NO_BUILDER tag, above) and since
 * we've actually dropped using the FIDO SDK in Chromium, test data is often
 * now captured from a device. To do this, print the data in question using:
 *   Base64.encodeToString(data, Base64.NO_WRAP);
 *
 * Then it can be converted to a Java array using this Python 3 code:
 *
 * import codecs
 * import sys
 *
 * def f(x):
 *   if x < 128:
 *     return x
 *   return x - 256
 *
 * b = codecs.decode(bytes(sys.argv[1], 'ascii'), 'base64')
 * print([f(x) for x in b])
 */

/** A Helper class for testing Fido2ApiHandlerInternal. */
public class Fido2ApiTestHelper {
    // Test data.
    private static final int OBJECT_MAGIC = 20293;

    /**
     * This byte array was produced by
     * com.google.android.gms.fido.fido2.api.common.AuthenticatorAttestationResponse with test data,
     * i.e.:
     *
     * <pre>{@code
     * AuthenticatorAttestationResponse response = new AuthenticatorAttestationResponse.Builder()
     *      .setAttestationObject(TEST_ATTESTATION_OBJECT)
     *      .setClientDataJSON(TEST_CLIENT_DATA_JSON)
     *      .setKeyHandle(TEST_KEY_HANDLE)
     *      .build().serializeToBytes();
     * }</pre>
     *
     * <p>NOTE: See NO_BUILDER comment, above. Additionally this byte array was modified by
     * prepending an object header and tag with value four so that it's compatible with
     * FIDO2_KEY_CREDENTIAL_EXTRA.
     */
    private static final byte[] TEST_AUTHENTICATOR_ATTESTATION_RESPONSE =
            new byte[] {
                69, 79, -1, -1, 60, 1, 0, 0, 4, 0, -1, -1, 52, 1, 0, 0, 69, 79, -1, -1, 44, 1, 0, 0,
                2, 0, -1, -1, 36, 0, 0, 0, 32, 0, 0, 0, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7,
                8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 9, 3, 0, -1, -1, 8, 0, 0, 0, 3, 0,
                0, 0, 4, 5, 6, 0, 4, 0, -1, -1, -24, 0, 0, 0, -30, 0, 0, 0, -93, 99, 102, 109, 116,
                100, 110, 111, 110, 101, 103, 97, 116, 116, 83, 116, 109, 116, -96, 104, 97, 117,
                116, 104, 68, 97, 116, 97, 88, -60, 38, -67, 114, 120, -66, 70, 55, 97, -15, -6,
                -95, -79, 10, -76, -60, -8, 38, 112, 38, -100, 65, 12, 114, 106, 31, -42, -32, 88,
                85, -31, -101, 70, 65, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 64, 124, 80, -60, -114, 69, -117, 44, -120, 122, -62, 63, 104, 18, -66, 2, -3,
                -56, 35, -24, 66, -4, 74, 48, -128, -52, 80, -100, 46, 97, 93, -25, -21, -53, 40,
                123, 90, -107, -20, 111, -4, 15, 64, 122, 15, -84, -21, -33, -15, 26, 11, 35, 36,
                -49, 116, 52, -74, 107, 63, 113, -59, 125, -27, -120, -63, -91, 1, 2, 3, 38, 32, 1,
                33, 88, 32, -75, -80, 118, 102, -14, 124, -108, -9, -27, -91, 59, -48, -92, -102,
                -38, -44, 92, 95, 14, -62, 41, -117, -70, 101, 9, 64, 35, 31, -20, 79, -71, -71, 34,
                88, 32, -24, -33, 64, 97, -31, -34, 96, -83, -119, -25, 21, -14, -56, -70, -37,
                -116, -21, -33, -128, -66, 61, 41, 107, 16, -25, 120, 106, -113, 54, -62, -102, 42,
                0, 0
            };

    /**
     * This byte array was captured from a device and resulted from creating a passkey.
     *
     * <p>It contains fields such as the list of transports, which is useful for testing. See
     * CONVERT_TO_JAVA tag, above, about creating it.
     */
    private static final byte[] TEST_AUTHENTICATOR_PASSKEY_ATTESTATION_RESPONSE =
            new byte[] {
                69, 79, -1, -1, -84, 2, 0, 0, 1, 0, -1, -1, 52, 0, 0, 0, 22, 0, 0, 0, 99, 0, 71, 0,
                67, 0, 103, 0, 71, 0, 99, 0, 71, 0, 54, 0, 115, 0, 75, 0, 90, 0, 48, 0, 114, 0, 84,
                0, 103, 0, 78, 0, 121, 0, 95, 0, 119, 0, 57, 0, 87, 0, 65, 0, 0, 0, 0, 0, 2, 0, -1,
                -1, 28, 0, 0, 0, 10, 0, 0, 0, 112, 0, 117, 0, 98, 0, 108, 0, 105, 0, 99, 0, 45, 0,
                107, 0, 101, 0, 121, 0, 0, 0, 0, 0, 3, 0, -1, -1, 20, 0, 0, 0, 16, 0, 0, 0, 112, 96,
                -96, 25, -63, -70, -80, -90, 116, -83, 56, 13, -53, -4, 61, 88, 4, 0, -1, -1, 8, 2,
                0, 0, 69, 79, -1, -1, 0, 2, 0, 0, 2, 0, -1, -1, 20, 0, 0, 0, 16, 0, 0, 0, 112, 96,
                -96, 25, -63, -70, -80, -90, 116, -83, 56, 13, -53, -4, 61, 88, 3, 0, -1, -1, -72,
                0, 0, 0, -79, 0, 0, 0, 123, 34, 116, 121, 112, 101, 34, 58, 34, 119, 101, 98, 97,
                117, 116, 104, 110, 46, 99, 114, 101, 97, 116, 101, 34, 44, 34, 99, 104, 97, 108,
                108, 101, 110, 103, 101, 34, 58, 34, 100, 103, 101, 55, 76, 107, 120, 73, 98, 121,
                98, 100, 77, 102, 81, 118, 111, 103, 49, 99, 79, 98, 57, 68, 122, 80, 76, 70, 69,
                119, 107, 79, 52, 95, 90, 55, 111, 117, 109, 79, 48, 69, 99, 34, 44, 34, 111, 114,
                105, 103, 105, 110, 34, 58, 34, 104, 116, 116, 112, 115, 58, 92, 47, 92, 47, 115,
                101, 99, 117, 114, 105, 116, 121, 107, 101, 121, 115, 46, 105, 110, 102, 111, 34,
                44, 34, 97, 110, 100, 114, 111, 105, 100, 80, 97, 99, 107, 97, 103, 101, 78, 97,
                109, 101, 34, 58, 34, 99, 111, 109, 46, 103, 111, 111, 103, 108, 101, 46, 97, 110,
                100, 114, 111, 105, 100, 46, 97, 112, 112, 115, 46, 99, 104, 114, 111, 109, 101, 34,
                125, 0, 0, 0, 4, 0, -1, -1, -72, 0, 0, 0, -78, 0, 0, 0, -93, 99, 102, 109, 116, 100,
                110, 111, 110, 101, 103, 97, 116, 116, 83, 116, 109, 116, -96, 104, 97, 117, 116,
                104, 68, 97, 116, 97, 88, -108, 38, -67, 114, 120, -66, 70, 55, 97, -15, -6, -95,
                -79, 10, -76, -60, -8, 38, 112, 38, -100, 65, 12, 114, 106, 31, -42, -32, 88, 85,
                -31, -101, 70, 69, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                16, 112, 96, -96, 25, -63, -70, -80, -90, 116, -83, 56, 13, -53, -4, 61, 88, -91, 1,
                2, 3, 38, 32, 1, 33, 88, 32, 44, 116, -29, 17, -108, 13, 49, 87, 1, 111, 117, 117,
                -110, -42, 6, -108, 108, -120, -2, 31, 62, 75, 4, 51, -5, 73, 70, -84, -30, -123,
                -38, 98, 34, 88, 32, -14, -91, 17, -54, 52, -36, -82, -36, 60, 19, -34, 79, -103,
                80, -71, 92, -40, 113, 12, 98, 107, -88, 95, 7, -27, 39, -43, 52, -111, -85, -77,
                14, 0, 0, 5, 0, -1, -1, 92, 0, 0, 0, 6, 0, 0, 0, 3, 0, 0, 0, 98, 0, 108, 0, 101, 0,
                0, 0, 2, 0, 0, 0, 98, 0, 116, 0, 0, 0, 0, 0, 5, 0, 0, 0, 99, 0, 97, 0, 98, 0, 108,
                0, 101, 0, 0, 0, 8, 0, 0, 0, 105, 0, 110, 0, 116, 0, 101, 0, 114, 0, 110, 0, 97, 0,
                108, 0, 0, 0, 0, 0, 3, 0, 0, 0, 110, 0, 102, 0, 99, 0, 0, 0, 3, 0, 0, 0, 117, 0,
                115, 0, 98, 0, 0, 0, 8, 0, -1, -1, 24, 0, 0, 0, 8, 0, 0, 0, 112, 0, 108, 0, 97, 0,
                116, 0, 102, 0, 111, 0, 114, 0, 109, 0, 0, 0, 0, 0
            };

    /**
     * This byte array was produced by
     * com.google.android.gms.fido.fido2.api.common.AuthenticatorAssertionResponse with test data,
     * i.e.:
     *
     * <pre>{@code
     * AuthenticatorAssertionResponse.Builder()
     *      .setAuthenticatorData(TEST_AUTHENTICATOR_DATA)
     *      .setSignature(TEST_SIGNATURE)
     *      .setClientDataJSON(TEST_CLIENT_DATA_JSON)
     *      .setKeyHandle(TEST_KEY_HANDLE)
     *      .build().serializeToBytes();
     * }</pre>
     *
     * <p>NOTE: See NO_BUILDER comment, above. Additionally this byte array was modified by
     * prepending an object header and tag with value five so that it's compatible with
     * FIDO2_KEY_CREDENTIAL_EXTRA.
     */
    private static final byte[] TEST_AUTHENTICATOR_ASSERTION_RESPONSE =
            new byte[] {
                69, 79, -1, -1, 108, 0, 0, 0, 5, 0, -1, -1, 100, 0, 0, 0, 69, 79, -1, -1, 92, 0, 0,
                0, 2, 0, -1, -1, 36, 0, 0, 0, 32, 0, 0, 0, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6,
                7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 9, 3, 0, -1, -1, 8, 0, 0, 0, 3,
                0, 0, 0, 4, 5, 6, 0, 4, 0, -1, -1, 8, 0, 0, 0, 3, 0, 0, 0, 7, 8, 9, 0, 5, 0, -1, -1,
                8, 0, 0, 0, 3, 0, 0, 0, 10, 11, 12, 0
            };

    /**
     * This byte array is produced by
     * com.google.android.gms.fido.fido2.api.common.PublicKeyCredential with test data, i.e.:
     *
     * <pre>{@code
     * AuthenticatorAssertionResponse response = new AuthenticatorAssertionResponse.Builder()
     *      .setAuthenticatorData(TEST_AUTHENTICATOR_DATA)
     *      .setSignature(TEST_SIGNATURE)
     *      .setClientDataJSON(TEST_CLIENT_DATA_JSON)
     *      .setKeyHandle(TEST_KEY_HANDLE)
     *      .build();
     * UvmEntry uvmEntry0 = new UvmEntry.Builder()
     *      .setUserVerificationMethod(TEST_USER_VERIFICATION_METHOD[0])
     *      .setKeyProtectionType(TEST_KEY_PROTECTION_TYPE[0])
     *      .setMatcherProtectionType(TEST_MATCHER_PROTECTION_TYPE[0])
     *      .build();
     * UvmEntry uvmEntry1 = new UvmEntry.Builder()
     *      .setUserVerificationMethod(TEST_USER_VERIFICATION_METHOD[1])
     *      .setKeyProtectionType(TEST_KEY_PROTECTION_TYPE[1])
     *      .setMatcherProtectionType(TEST_MATCHER_PROTECTION_TYPE[1])
     *      .build();
     * UvmEntries uvmEntries = new UvmEntries.Builder()
     *      .addUvmEntry(uvmEntry0)
     *      .addUvmEntry(uvmEntry1)
     *      .build();
     * AuthenticationExtensionsClientOutputs authenticationExtensionsClientOutputs =
     *      new AuthenticationExtensionsClientOutputs.Builder()
     *              .setUserVerificationMethodEntries(uvmEntries)
     *              .build();
     * PublicKeyCredential publicKeyCredential = new PublicKeyCredential.Builder()
     *      .setResponse(response)
     *      .setAuthenticationExtensionsClientOutputs(authenticationExtensionsClientOutputs)
     *      .build().serializeToBytes();
     * }</pre>
     *
     * <p>NOTE: See NO_BUILDER comment, above.
     */
    private static final byte[] TEST_ASSERTION_PUBLIC_KEY_CREDENTIAL_WITH_UVM =
            new byte[] {
                69, 79, -1, -1, 4, 1, 0, 0, 2, 0, -1, -1, 28, 0, 0, 0, 10, 0, 0, 0, 112, 0, 117, 0,
                98, 0, 108, 0, 105, 0, 99, 0, 45, 0, 107, 0, 101, 0, 121, 0, 0, 0, 0, 0, 5, 0, -1,
                -1, 100, 0, 0, 0, 69, 79, -1, -1, 92, 0, 0, 0, 2, 0, -1, -1, 36, 0, 0, 0, 32, 0, 0,
                0, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7,
                8, 5, 6, 7, 9, 3, 0, -1, -1, 8, 0, 0, 0, 3, 0, 0, 0, 4, 5, 6, 0, 4, 0, -1, -1, 8, 0,
                0, 0, 3, 0, 0, 0, 7, 8, 9, 0, 5, 0, -1, -1, 8, 0, 0, 0, 3, 0, 0, 0, 10, 11, 12, 0,
                7, 0, -1, -1, 108, 0, 0, 0, 69, 79, -1, -1, 100, 0, 0, 0, 1, 0, -1, -1, 92, 0, 0, 0,
                69, 79, -1, -1, 84, 0, 0, 0, 1, 0, -1, -1, 76, 0, 0, 0, 2, 0, 0, 0, 32, 0, 0, 0, 69,
                79, -1, -1, 24, 0, 0, 0, 1, 0, 4, 0, 2, 0, 0, 0, 2, 0, 4, 0, 2, 0, 0, 0, 3, 0, 4, 0,
                4, 0, 0, 0, 32, 0, 0, 0, 69, 79, -1, -1, 24, 0, 0, 0, 1, 0, 4, 0, 0, 2, 0, 0, 2, 0,
                4, 0, 1, 0, 0, 0, 3, 0, 4, 0, 1, 0, 0, 0
            };

    /** Get credential response captured with prf extension enabled. */
    private static final byte[] TEST_ASSERTION_PUBLIC_KEY_CREDENTIAL_WITH_PRF =
            new byte[] {
                69, 79, -1, -1, -104, 2, 0, 0, 1, 0, -1, -1, 52, 0, 0, 0, 22, 0, 0, 0, 52, 0, 121,
                0, 82, 0, 87, 0, 50, 0, 56, 0, 89, 0, 108, 0, 103, 0, 119, 0, 57, 0, 99, 0, 68, 0,
                70, 0, 83, 0, 65, 0, 69, 0, 95, 0, 99, 0, 68, 0, 48, 0, 65, 0, 0, 0, 0, 0, 2, 0, -1,
                -1, 28, 0, 0, 0, 10, 0, 0, 0, 112, 0, 117, 0, 98, 0, 108, 0, 105, 0, 99, 0, 45, 0,
                107, 0, 101, 0, 121, 0, 0, 0, 0, 0, 3, 0, -1, -1, 20, 0, 0, 0, 16, 0, 0, 0, -29, 36,
                86, -37, -58, 37, -125, 15, 92, 12, 84, -128, 19, -9, 3, -48, 5, 0, -1, -1, -96, 1,
                0, 0, 69, 79, -1, -1, -104, 1, 0, 0, 2, 0, -1, -1, 20, 0, 0, 0, 16, 0, 0, 0, -29,
                36, 86, -37, -58, 37, -125, 15, 92, 12, 84, -128, 19, -9, 3, -48, 3, 0, -1, -1, -40,
                0, 0, 0, -45, 0, 0, 0, 123, 34, 116, 121, 112, 101, 34, 58, 34, 119, 101, 98, 97,
                117, 116, 104, 110, 46, 103, 101, 116, 34, 44, 34, 99, 104, 97, 108, 108, 101, 110,
                103, 101, 34, 58, 34, 81, 113, 109, 48, 87, 76, 89, 87, 108, 99, 120, 95, 98, 106,
                112, 84, 100, 113, 65, 80, 52, 50, 98, 102, 107, 118, 48, 97, 74, 116, 90, 95, 90,
                120, 71, 72, 69, 113, 55, 74, 117, 55, 70, 112, 107, 117, 69, 79, 78, 73, 71, 110,
                80, 113, 51, 102, 109, 68, 75, 45, 104, 55, 72, 73, 73, 54, 74, 71, 79, 86, 113, 72,
                56, 52, 57, 53, 49, 66, 68, 83, 71, 45, 118, 68, 76, 65, 34, 44, 34, 111, 114, 105,
                103, 105, 110, 34, 58, 34, 104, 116, 116, 112, 115, 58, 92, 47, 92, 47, 119, 101,
                98, 97, 117, 116, 104, 110, 46, 105, 111, 34, 44, 34, 97, 110, 100, 114, 111, 105,
                100, 80, 97, 99, 107, 97, 103, 101, 78, 97, 109, 101, 34, 58, 34, 99, 111, 109, 46,
                103, 111, 111, 103, 108, 101, 46, 97, 110, 100, 114, 111, 105, 100, 46, 97, 112,
                112, 115, 46, 99, 104, 114, 111, 109, 101, 34, 125, 0, 4, 0, -1, -1, 44, 0, 0, 0,
                37, 0, 0, 0, 116, -90, -22, -110, 19, -55, -100, 47, 116, -78, 36, -110, -77, 32,
                -49, 64, 38, 42, -108, -63, -87, 80, -96, 57, 127, 41, 37, 11, 96, -124, 30, -16,
                29, 0, 0, 0, 0, 0, 0, 0, 5, 0, -1, -1, 76, 0, 0, 0, 70, 0, 0, 0, 48, 68, 2, 32, 80,
                79, -84, 125, 98, -42, -17, 18, 52, 61, -53, -41, -88, -21, -126, -123, 108, -44,
                -20, -6, 95, -82, -67, 111, -13, 43, -127, -123, 105, 106, -29, -9, 2, 32, 11, 44,
                -2, -84, 60, -38, -49, -45, 69, 69, -88, -29, 29, -29, -5, -115, -13, -50, -105, 92,
                -108, 74, -125, 35, 87, 95, -26, -29, -27, 64, -111, -73, 0, 0, 6, 0, -1, -1, 12, 0,
                0, 0, 7, 0, 0, 0, 78, 68, 81, 48, 78, 68, 81, 0, 7, 0, -1, -1, 76, 0, 0, 0, 69, 79,
                -1, -1, 68, 0, 0, 0, 4, 0, -1, -1, 60, 0, 0, 0, 69, 79, -1, -1, 52, 0, 0, 0, 1, 0,
                4, 0, 0, 0, 0, 0, 2, 0, -1, -1, 36, 0, 0, 0, 32, 0, 0, 0, -49, 63, -92, 1, -28, 95,
                -13, -108, -64, 100, 81, 6, 53, -105, 125, 108, 37, 7, 73, 14, 36, -69, 65, 25, 17,
                -91, 61, 29, -43, 93, 70, 32, 8, 0, -1, -1, 24, 0, 0, 0, 8, 0, 0, 0, 112, 0, 108, 0,
                97, 0, 116, 0, 102, 0, 111, 0, 114, 0, 109, 0, 0, 0, 0, 0
            };

    /**
     * The value of the prf extension response in the sample, {@link
     * Fido2ApiTestHelper#TEST_ASSERTION_PUBLIC_KEY_CREDENTIAL_WITH_PRF}.
     */
    private static final byte[] TEST_ASSERTION_PRF_VALUES_BYTES = {
        -49, 63, -92, 1, -28, 95, -13, -108, -64, 100, 81, 6, 53, -105, 125, 108, 37, 7, 73, 14, 36,
        -69, 65, 25, 17, -91, 61, 29, -43, 93, 70, 32
    };

    private static final byte[] TEST_KEY_HANDLE =
            BaseEncoding.base16()
                    .decode("0506070805060708050607080506070805060708050607080506070805060709");
    private static final String TEST_ENCODED_KEY_HANDLE =
            Base64.encodeToString(
                    TEST_KEY_HANDLE, Base64.URL_SAFE | Base64.NO_PADDING | Base64.NO_WRAP);
    private static final byte[] TEST_ATTESTATION_OBJECT =
            new byte[] {
                -93, 99, 102, 109, 116, 100, 110, 111, 110, 101, 103, 97, 116, 116, 83, 116, 109,
                116, -96, 104, 97, 117, 116, 104, 68, 97, 116, 97, 88, -60, 38, -67, 114, 120, -66,
                70, 55, 97, -15, -6, -95, -79, 10, -76, -60, -8, 38, 112, 38, -100, 65, 12, 114,
                106, 31, -42, -32, 88, 85, -31, -101, 70, 65, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 64, 124, 80, -60, -114, 69, -117, 44, -120, 122, -62, 63,
                104, 18, -66, 2, -3, -56, 35, -24, 66, -4, 74, 48, -128, -52, 80, -100, 46, 97, 93,
                -25, -21, -53, 40, 123, 90, -107, -20, 111, -4, 15, 64, 122, 15, -84, -21, -33, -15,
                26, 11, 35, 36, -49, 116, 52, -74, 107, 63, 113, -59, 125, -27, -120, -63, -91, 1,
                2, 3, 38, 32, 1, 33, 88, 32, -75, -80, 118, 102, -14, 124, -108, -9, -27, -91, 59,
                -48, -92, -102, -38, -44, 92, 95, 14, -62, 41, -117, -70, 101, 9, 64, 35, 31, -20,
                79, -71, -71, 34, 88, 32, -24, -33, 64, 97, -31, -34, 96, -83, -119, -25, 21, -14,
                -56, -70, -37, -116, -21, -33, -128, -66, 61, 41, 107, 16, -25, 120, 106, -113, 54,
                -62, -102, 42
            };

    // TEST_DISCOVERABLE_CREDENTIAL_ASSERTION is the payload of an Intent response for an assertion
    // with an empty allowList. This was captured from Play Services.
    private static final byte[] TEST_DISCOVERABLE_CREDENTIAL_ASSERTION =
            new byte[] {
                69, 79, -1, -1, 100, 1, 0, 0, 1, 0, -1, -1, 52, 0, 0, 0, 22, 0, 0, 0, 99, 0, 72, 0,
                116, 0, 85, 0, 70, 0, 86, 0, 112, 0, 82, 0, 88, 0, 99, 0, 73, 0, 50, 0, 109, 0, 67,
                0, 49, 0, 76, 0, 119, 0, 106, 0, 95, 0, 85, 0, 72, 0, 65, 0, 0, 0, 0, 0, 2, 0, -1,
                -1, 28, 0, 0, 0, 10, 0, 0, 0, 112, 0, 117, 0, 98, 0, 108, 0, 105, 0, 99, 0, 45, 0,
                107, 0, 101, 0, 121, 0, 0, 0, 0, 0, 3, 0, -1, -1, 20, 0, 0, 0, 16, 0, 0, 0, 112,
                123, 84, 21, 90, 81, 93, -62, 54, -104, 45, 75, -62, 63, -44, 28, 5, 0, -1, -1, -32,
                0, 0, 0, 69, 79, -1, -1, -40, 0, 0, 0, 2, 0, -1, -1, 20, 0, 0, 0, 16, 0, 0, 0, 112,
                123, 84, 21, 90, 81, 93, -62, 54, -104, 45, 75, -62, 63, -44, 28, 3, 0, -1, -1, 16,
                0, 0, 0, 9, 0, 0, 0, 60, 105, 110, 118, 97, 108, 105, 100, 62, 0, 0, 0, 4, 0, -1,
                -1, 44, 0, 0, 0, 37, 0, 0, 0, -28, 83, 41, -48, 58, 32, 104, -47, -54, -9, -9, -69,
                10, -23, 84, -26, -80, -26, 37, -105, 69, -13, 47, 72, 41, -9, 80, -16, 80, 17, -7,
                -62, 5, 0, 0, 0, 0, 0, 0, 0, 5, 0, -1, -1, 76, 0, 0, 0, 70, 0, 0, 0, 48, 68, 2, 32,
                86, -36, 80, 3, 65, -90, 66, 76, 100, -126, -34, 82, -22, 96, 3, 71, 3, 68, 57, 62,
                -4, -80, 34, 119, 97, 30, 100, -65, -36, 95, -68, 19, 2, 32, 91, 52, -110, -105,
                -104, 105, 94, 41, 63, -97, -86, 15, 35, 117, 61, 73, 119, -113, 95, 69, 44, -13,
                -22, 61, 32, 79, 53, 106, 127, -67, 32, 4, 0, 0, 6, 0, -1, -1, 20, 0, 0, 0, 15, 0,
                0, 0, 98, 111, 98, 64, 101, 120, 97, 109, 112, 108, 101, 46, 99, 111, 109, 0
            };

    // Serialized registration response converted from JSON received from the Credential Manager
    // API.
    private static final byte[] TEST_SERIALIZED_CREDMAN_MAKE_CREDENTIAL_RESPONSE =
            new byte[] {
                72, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 127, 7, 0, 0, 80, 1,
                0, 0, 0, 0, 0, 0, 24, 2, 0, 0, 0, 0, 0, 0, 32, 2, 0, 0, 0, 0, 0, 0, 88, 2, 0, 0, 0,
                0, 0, 0, 121, 127, 127, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 40, 0, 0, 0, 0, 0,
                0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 48, 0, 0, 0, 0, 0, 0, 0, 56, 0, 0, 0, 0, 0, 0, 0, 80,
                0, 0, 0, 0, 0, 0, 0, 18, 0, 0, 0, 10, 0, 0, 0, 100, 71, 86, 122, 100, 67, 66, 112,
                90, 65, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 7, 0, 0, 0, 116, 101, 115, 116, 32, 105, 100,
                0, 29, 0, 0, 0, 21, 0, 0, 0, 116, 101, 115, 116, 32, 99, 108, 105, 101, 110, 116,
                32, 100, 97, 116, 97, 32, 106, 115, 111, 110, 0, 0, 0, 44, 0, 0, 0, 36, 0, 0, 0, 38,
                61, 114, 120, 62, 70, 55, 97, 113, 122, 33, 49, 10, 52, 68, 120, 38, 112, 38, 28,
                65, 12, 114, 106, 31, 86, 96, 88, 85, 97, 27, 70, 93, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 4, 39, 41, 1, 40, 110, 3, 80, 15, 47, 41, 58,
                19, 51, 47, 127, 27, 40, 33, 99, 49, 9, 37, 68, 106, 84, 45, 115, 43, 28, 110, 22,
                37, 1, 2, 3, 38, 32, 1, 33, 88, 32, 22, 69, 112, 93, 97, 55, 24, 88, 99, 78, 82, 22,
                61, 23, 119, 47, 51, 94, 68, 116, 101, 45, 126, 101, 120, 104, 68, 110, 61, 57, 18,
                117, 34, 88, 32, 113, 19, 12, 37, 54, 32, 60, 39, 101, 115, 54, 117, 31, 126, 42,
                68, 15, 37, 32, 99, 6, 61, 26, 103, 84, 38, 33, 7, 81, 62, 12, 2, 0, 0, 0, 0, 74, 0,
                0, 0, 66, 0, 0, 0, 35, 99, 102, 109, 116, 100, 110, 111, 110, 101, 103, 97, 116,
                116, 83, 116, 109, 116, 32, 104, 97, 117, 116, 104, 68, 97, 116, 97, 88, 36, 38, 61,
                114, 120, 62, 70, 55, 97, 113, 122, 33, 49, 10, 52, 68, 120, 38, 112, 38, 28, 65,
                12, 114, 106, 31, 86, 96, 88, 85, 97, 27, 70, 93, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 4, 39, 41, 1, 40, 110, 3, 80, 15, 47, 41, 58, 19,
                51, 47, 127, 27, 40, 33, 99, 49, 9, 37, 68, 106, 84, 45, 115, 43, 28, 110, 22, 37,
                1, 2, 3, 38, 32, 1, 33, 88, 32, 22, 69, 112, 93, 97, 55, 24, 88, 99, 78, 82, 22, 61,
                23, 119, 47, 51, 94, 68, 116, 101, 45, 126, 101, 120, 104, 68, 110, 61, 57, 18, 117,
                34, 88, 32, 113, 19, 12, 37, 54, 32, 60, 39, 101, 115, 54, 117, 31, 126, 42, 68, 15,
                37, 32, 99, 6, 61, 26, 103, 84, 38, 33, 7, 81, 62, 12, 2, 0, 0, 0, 0, 0, 0, 12, 0,
                0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 24, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0, 0, 4, 0, 0, 0, 1,
                2, 3, 4, 0, 0, 0, 0, 12, 0, 0, 0, 4, 0, 0, 0, 5, 6, 7, 8, 0, 0, 0, 0, 99, 0, 0, 0,
                91, 0, 0, 0, 48, 89, 48, 19, 6, 7, 42, 6, 72, 78, 61, 2, 1, 6, 8, 42, 6, 72, 78, 61,
                3, 1, 7, 3, 66, 0, 4, 22, 69, 112, 93, 97, 55, 24, 88, 99, 78, 82, 22, 61, 23, 119,
                47, 51, 94, 68, 116, 101, 45, 126, 101, 120, 104, 68, 110, 61, 57, 18, 117, 113, 19,
                12, 37, 54, 32, 60, 39, 101, 115, 54, 117, 31, 126, 42, 68, 15, 37, 32, 99, 6, 61,
                26, 103, 84, 38, 33, 7, 81, 62, 12, 2, 0, 0, 0, 0, 0
            };

    // Serialized assertion response converted from JSON received from the Credential Manager API.
    private static final byte[] TEST_SERIALIZED_CREDMAN_GET_CREDENTIAL_RESPONSE =
            new byte[] {
                48, 0, 0, 0, 0, 0, 0, 0, 40, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, -64, 0, 0,
                0, 0, 0, 0, 0, 8, 1, 0, 0, 0, 0, 0, 0, 32, 1, 0, 0, 0, 0, 0, 0, 40, 0, 0, 0, 0, 0,
                0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 56, 0, 0, 0, 0, 0, 0, 0, 72, 0, 0, 0, 0, 0, 0, 0, 88,
                0, 0, 0, 0, 0, 0, 0, 30, 0, 0, 0, 22, 0, 0, 0, 78, 81, 119, 83, 88, 81, 109, 80,
                113, 99, 107, 112, 54, 86, 66, 114, 117, 74, 52, 84, 45, 65, 0, 0, 24, 0, 0, 0, 16,
                0, 0, 0, 53, 12, 18, 93, 9, -113, -87, -55, 41, -23, 80, 107, -72, -98, 19, -8, 17,
                0, 0, 0, 9, 0, 0, 0, 60, 105, 110, 118, 97, 108, 105, 100, 62, 0, 0, 0, 0, 0, 0, 0,
                45, 0, 0, 0, 37, 0, 0, 0, -56, 89, -63, 83, -10, -10, 58, 88, -74, 13, 49, -64, 95,
                85, -99, 61, -90, 100, -74, -120, 11, 20, 82, 85, -69, -47, 2, 101, 101, -105, -58,
                -109, 29, 0, 0, 0, 0, 0, 0, 0, 79, 0, 0, 0, 71, 0, 0, 0, 48, 69, 2, 33, 0, -104, -2,
                -11, -58, 98, 106, -47, 80, 63, 82, 35, 47, 121, -87, -40, -119, 39, 121, -58, 107,
                -119, -108, 90, -22, -12, -36, -113, -56, -112, -63, 21, 44, 2, 32, 47, 7, 8, 107,
                110, 36, -60, -49, -32, 118, 54, -92, 84, 124, 77, -80, 87, -9, 7, -50, -1, 24, -49,
                -7, -116, -42, -93, -23, -111, 91, 80, 87, 0, 26, 0, 0, 0, 18, 0, 0, 0, 49, 55, 51,
                48, 54, 51, 55, 54, 56, 46, 57, 56, 53, 54, 49, 54, 57, 53, 0, 0, 0, 0, 0, 0, 56, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            };

    // TEST_USER_HANDLE is the user ID contained within `TEST_DISCOVERABLE_CREDENTIAL_ASSERTION`.
    public static final byte[] TEST_USER_HANDLE = "bob@example.com".getBytes(UTF_8);

    private static final byte[] TEST_CLIENT_DATA_JSON = new byte[] {4, 5, 6};
    private static final byte[] TEST_AUTHENTICATOR_DATA = new byte[] {7, 8, 9};
    private static final byte[] TEST_SIGNATURE = new byte[] {10, 11, 12};
    private static final long TIMEOUT_MS = scaleTimeout(TimeUnit.SECONDS.toMillis(1));
    private static final int[] TEST_USER_VERIFICATION_METHOD = new int[] {0x00000002, 0x00000200};
    private static final short[] TEST_KEY_PROTECTION_TYPE = new short[] {0x0002, 0x0001};
    private static final short[] TEST_MATCHER_PROTECTION_TYPE = new short[] {0x0004, 0x0001};
    private static final String TEST_SERIALIZED_MAKE_CREDENTIAL_REQUEST_JSON =
            "{serialized_make_request}";
    private static final String TEST_SERIALIZED_GET_ASSERTION_REQUEST_JSON =
            "{serialized_get_request}";

    /**
     * Builds a test intent to be returned by a successful call to makeCredential.
     *
     * @return Intent containing the response from the Fido2 API.
     */
    public static Intent createSuccessfulMakeCredentialIntent() {
        Intent intent = new Intent();
        intent.putExtra(Fido2Api.CREDENTIAL_EXTRA, TEST_AUTHENTICATOR_ATTESTATION_RESPONSE);
        return intent;
    }

    /**
     * Builds a test intent to be returned by a successful call to makeCredential.
     *
     * @return Intent containing the response from the Fido2 API.
     */
    public static Intent createSuccessfulPasskeyMakeCredentialIntent() {
        Intent intent = new Intent();
        intent.putExtra(Fido2Api.CREDENTIAL_EXTRA, TEST_AUTHENTICATOR_PASSKEY_ATTESTATION_RESPONSE);
        return intent;
    }

    private static Intent intentFromPath(String path) {
        byte[] response;
        try {
            response = Files.readAllBytes(Paths.get(UrlUtils.getTestFilePath(path)));
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

        Intent intent = new Intent();
        intent.putExtra(Fido2Api.CREDENTIAL_EXTRA, response);
        return intent;
    }

    public static Intent createSuccessfulMakeCredentialIntentWithAttestation() {
        // This blob is too large (9KB) to reasonably include as a literal.
        return intentFromPath("webauthn/android_make_credential_bundle_with_attestation");
    }

    public static Intent createSuccessfulMakeCredentialIntentWithCredProps() {
        return intentFromPath("webauthn/android_make_credential_bundle_with_credprops_rk_true");
    }

    /**
     * Construct default options for a makeCredential request.
     *
     * @return Options for the Fido2 API.
     */
    public static PublicKeyCredentialCreationOptions createDefaultMakeCredentialOptions()
            throws Exception {
        PublicKeyCredentialCreationOptions options = new PublicKeyCredentialCreationOptions();
        options.challenge = "climb a mountain".getBytes("UTF8");
        options.hints = new int[0];

        options.relyingParty = new PublicKeyCredentialRpEntity();
        options.relyingParty.id = "subdomain.example.test";
        options.relyingParty.name = "Acme";

        options.user = new PublicKeyCredentialUserEntity();
        options.user.id = "1098237235409872".getBytes("UTF8");
        options.user.name = "avery.a.jones@example.com";
        options.user.displayName = "Avery A. Jones";

        options.timeout = new TimeDelta();
        options.timeout.microseconds = TimeUnit.MILLISECONDS.toMicros(TIMEOUT_MS);

        PublicKeyCredentialParameters parameters = new PublicKeyCredentialParameters();
        parameters.algorithmIdentifier = -7;
        parameters.type = PublicKeyCredentialType.PUBLIC_KEY;
        options.publicKeyParameters = new PublicKeyCredentialParameters[] {parameters};

        PublicKeyCredentialDescriptor descriptor = new PublicKeyCredentialDescriptor();
        descriptor.type = 0;
        descriptor.id = new byte[] {8, 7, 6};
        descriptor.transports = new int[] {0};
        options.excludeCredentials = new PublicKeyCredentialDescriptor[] {descriptor};

        options.authenticatorSelection = new AuthenticatorSelectionCriteria();
        /* TODO add UserVerificationRequirement and ResidentKeyRequirement when the FIDO2 API
         * supports it */
        options.authenticatorSelection.authenticatorAttachment =
                AuthenticatorAttachment.CROSS_PLATFORM;

        options.attestationFormats = new String[0];
        return options;
    }

    /**
     * Verifies that the returned response matches expected values.
     *
     * @param response The response from the Fido2 API.
     */
    public static void validateMakeCredentialResponse(
            MakeCredentialAuthenticatorResponse response) {
        Assert.assertArrayEquals(response.attestationObject, TEST_ATTESTATION_OBJECT);
        Assert.assertArrayEquals(response.info.rawId, TEST_KEY_HANDLE);
        Assert.assertEquals(response.info.id, TEST_ENCODED_KEY_HANDLE);
        Assert.assertArrayEquals(response.info.clientDataJson, TEST_CLIENT_DATA_JSON);
    }

    /**
     * Constructs default options for a getAssertion request.
     *
     * @return Options for the Fido2 API
     */
    public static PublicKeyCredentialRequestOptions createDefaultGetAssertionOptions()
            throws Exception {
        PublicKeyCredentialRequestOptions options = new PublicKeyCredentialRequestOptions();
        options.extensions = new AuthenticationExtensionsClientInputs();
        options.challenge = "climb a mountain".getBytes("UTF8");
        options.timeout = new TimeDelta();
        options.timeout.microseconds = TimeUnit.MILLISECONDS.toMicros(TIMEOUT_MS);
        options.relyingPartyId = "subdomain.example.test";
        options.hints = new int[0];

        PublicKeyCredentialDescriptor descriptor = new PublicKeyCredentialDescriptor();
        descriptor.type = 0;
        descriptor.id = new byte[] {8, 7, 6};
        descriptor.transports = new int[] {0};
        options.allowCredentials = new PublicKeyCredentialDescriptor[] {descriptor};

        options.extensions.cableAuthenticationData = new CableAuthentication[] {};
        options.extensions.prfInputs = new PrfValues[] {};
        return options;
    }

    /**
     * Builds a test intent without uvm extension to be returned by a successful call to
     * makeCredential.
     *
     * @return Intent containing the response from the Fido2 API.
     */
    public static Intent createSuccessfulGetAssertionIntent() {
        Intent intent = new Intent();
        intent.putExtra(Fido2Api.CREDENTIAL_EXTRA, TEST_AUTHENTICATOR_ASSERTION_RESPONSE);
        return intent;
    }

    /**
     * Builds a test intent with uvm extension to be returned by a successful call to
     * makeCredential.
     *
     * @return Intent containing the response from the Fido2 API.
     */
    public static Intent createSuccessfulGetAssertionIntentWithUvm() {
        Intent intent = new Intent();
        intent.putExtra(Fido2Api.CREDENTIAL_EXTRA, TEST_ASSERTION_PUBLIC_KEY_CREDENTIAL_WITH_UVM);
        return intent;
    }

    /**
     * Builds a test intent with prf extension to be returned by a successful call to
     * makeCredential.
     *
     * @return Intent containing the response from the Fido2 API.
     */
    public static Intent createSuccessfulGetAssertionIntentWithPrf() {
        Intent intent = new Intent();
        intent.putExtra(Fido2Api.CREDENTIAL_EXTRA, TEST_ASSERTION_PUBLIC_KEY_CREDENTIAL_WITH_PRF);
        return intent;
    }

    /**
     * Verifies the values in the prf extension.
     *
     * @param prfValues The {@link PrfValues} from Fido2Api's extensions.
     */
    public static void validatePrfResults(PrfValues prfValues) {
        Assert.assertNotNull(prfValues);
        Assert.assertArrayEquals(prfValues.first, TEST_ASSERTION_PRF_VALUES_BYTES);
    }

    /**
     * Builds a test intent that contains a response to an assertion that had an empty allowList.
     */
    public static Intent createSuccessfulGetAssertionIntentWithUserId() {
        Intent intent = new Intent();
        intent.putExtra(Fido2Api.CREDENTIAL_EXTRA, TEST_DISCOVERABLE_CREDENTIAL_ASSERTION);
        return intent;
    }

    /**
     * Verifies that the returned userVerificationMethod matches expected values.
     *
     * @param userVerificationMethods The userVerificationMethods from the Fido2 API.
     */
    public static void validateUserVerificationMethods(
            boolean echoUserVerificationMethods, UvmEntry[] userVerificationMethods) {
        if (echoUserVerificationMethods) {
            Assert.assertEquals(
                    userVerificationMethods.length, TEST_USER_VERIFICATION_METHOD.length);
            for (int i = 0; i < userVerificationMethods.length; i++) {
                Assert.assertEquals(
                        userVerificationMethods[i].userVerificationMethod,
                        TEST_USER_VERIFICATION_METHOD[i]);
                Assert.assertEquals(
                        userVerificationMethods[i].keyProtectionType, TEST_KEY_PROTECTION_TYPE[i]);
                Assert.assertEquals(
                        userVerificationMethods[i].matcherProtectionType,
                        TEST_MATCHER_PROTECTION_TYPE[i]);
            }
        }
    }

    /**
     * Verifies that the returned response matches expected values.
     *
     * @param response The response from the Fido2 API.
     */
    public static void validateGetAssertionResponse(GetAssertionAuthenticatorResponse response) {
        Assert.assertArrayEquals(response.info.authenticatorData, TEST_AUTHENTICATOR_DATA);
        Assert.assertArrayEquals(response.signature, TEST_SIGNATURE);
        Assert.assertArrayEquals(response.info.rawId, TEST_KEY_HANDLE);
        Assert.assertEquals(response.info.id, TEST_ENCODED_KEY_HANDLE);
        Assert.assertArrayEquals(response.info.clientDataJson, TEST_CLIENT_DATA_JSON);
        validateUserVerificationMethods(
                response.extensions.echoUserVerificationMethods,
                response.extensions.userVerificationMethods);
    }

    /**
     * Verifies that the response did not return before timeout.
     *
     * @param startTimeMs The start time of the operation.
     */
    public static void verifyRespondedBeforeTimeout(long startTimeMs) {
        long elapsedTime = SystemClock.elapsedRealtime() - startTimeMs;
        Assert.assertTrue(elapsedTime < TIMEOUT_MS);
    }

    private static void appendErrorResponseToParcel(
            int errorCode, @Nullable String message, Parcel parcel) {
        final int a = writeHeader(OBJECT_MAGIC, parcel);
        final int b = writeHeader(6, parcel);
        final int c = writeHeader(OBJECT_MAGIC, parcel);

        int z = writeHeader(2, parcel);
        parcel.writeInt(errorCode);
        writeLength(z, parcel);

        if (message != null) {
            z = writeHeader(3, parcel);
            parcel.writeString(message);
            writeLength(z, parcel);
        }

        writeLength(c, parcel);
        writeLength(b, parcel);
        writeLength(a, parcel);
    }

    private static int writeHeader(int tag, Parcel parcel) {
        parcel.writeInt(0xffff0000 | tag);
        return startLength(parcel);
    }

    private static int startLength(Parcel parcel) {
        int pos = parcel.dataPosition();
        parcel.writeInt(0xdddddddd);
        return pos;
    }

    private static void writeLength(int pos, Parcel parcel) {
        int totalLength = parcel.dataPosition();
        parcel.setDataPosition(pos);
        parcel.writeInt(totalLength - pos - 4);
        parcel.setDataPosition(totalLength);
    }

    /**
     * Constructs an intent that returns an error response from the Fido2 API.
     *
     * @param errorCode Numeric values corresponding to a Fido2 error.
     * @return an Intent containing the error response.
     */
    public static Intent createErrorIntent(int errorCode, @Nullable String errorMsg) {
        Parcel parcel = Parcel.obtain();
        appendErrorResponseToParcel(errorCode, errorMsg, parcel);

        Intent intent = new Intent();
        intent.putExtra(Fido2Api.CREDENTIAL_EXTRA, parcel.marshall());
        return intent;
    }

    /** Creates a PaymentOptions object with normal values. */
    public static PaymentOptions createPaymentOptions() {
        PaymentOptions options = new PaymentOptions();
        options.total = new PaymentCurrencyAmount();
        options.total.currency = "USD";
        options.total.value = "888";
        options.instrument = new PaymentCredentialInstrument();
        options.instrument.displayName = "MaxPay";
        options.instrument.icon = new Url();
        options.instrument.icon.url = "https://www.google.com/icon.png";
        options.payeeOrigin = new org.chromium.url.internal.mojom.Origin();
        options.payeeOrigin.scheme = "https";
        options.payeeOrigin.host = "test.example";
        options.payeeOrigin.port = 443;
        return options;
    }

    /**
     * Mocks ClientDataJson so that it returns the provided result.
     *
     * @param mocker The JNI mocker
     * @param mockResult The mock value for {@link ClientDataJson#buildClientDataJson} to return.
     */
    public static void mockClientDataJson(JniMocker mocker, String mockResult) {
        ClientDataJsonImpl.Natives clientDataJsonJni =
                new ClientDataJsonImpl.Natives() {
                    @Override
                    public String buildClientDataJson(
                            int clientDataRequestType,
                            String callerOrigin,
                            byte[] challenge,
                            boolean isCrossOrigin,
                            ByteBuffer optionsByteBuffer,
                            String relyingPartyId,
                            org.chromium.url.Origin topOrigin) {
                        return mockResult;
                    }
                };
        mocker.mock(ClientDataJsonImplJni.TEST_HOOKS, clientDataJsonJni);
    }

    /**
     * Creates a {@link WebauthnCredentailDetails} object for testing.
     *
     * @return a newly created {@link WebauthnCredentialDetails}.
     */
    public static WebauthnCredentialDetails getCredentialDetails() {
        WebauthnCredentialDetails credential = new WebauthnCredentialDetails();
        credential.mUserId = "1098237235409872".getBytes(UTF_8);
        credential.mUserName = "avery.a.jones@example.com";
        credential.mUserDisplayName = "Avery A. Jones";
        credential.mCredentialId = new byte[] {8, 7, 6};
        credential.mIsDiscoverable = true;
        credential.mIsPayment = false;
        return credential;
    }

    public static void mockFido2CredentialRequestJni(JniMocker mocker) {
        Fido2CredentialRequest.Natives fido2CredentialRequestJni =
                new Fido2CredentialRequest.Natives() {
                    @Override
                    public String createOptionsToJson(ByteBuffer serializedOptions) {
                        return TEST_SERIALIZED_MAKE_CREDENTIAL_REQUEST_JSON;
                    }

                    @Override
                    public byte[] makeCredentialResponseFromJson(String json) {
                        return TEST_SERIALIZED_CREDMAN_MAKE_CREDENTIAL_RESPONSE;
                    }

                    @Override
                    public String getOptionsToJson(ByteBuffer serializedOptions) {
                        return TEST_SERIALIZED_GET_ASSERTION_REQUEST_JSON;
                    }

                    @Override
                    public byte[] getCredentialResponseFromJson(String json) {
                        return TEST_SERIALIZED_CREDMAN_GET_CREDENTIAL_RESPONSE;
                    }
                };
        mocker.mock(Fido2CredentialRequestJni.TEST_HOOKS, fido2CredentialRequestJni);
    }

    public static AuthenticatorCallback getAuthenticatorCallback() {
        return new AuthenticatorCallback();
    }

    /** Callback class to pass to Fido2CredentialRequest WebAuthn operations. */
    public static class AuthenticatorCallback {
        private Integer mStatus;
        private MakeCredentialAuthenticatorResponse mMakeCredentialResponse;
        private GetAssertionAuthenticatorResponse mGetAssertionAuthenticatorResponse;
        private List<byte[]> mGetMatchingCredentialIdsResponse;

        // Signals when request is complete.
        private final ConditionVariable mDone = new ConditionVariable();

        AuthenticatorCallback() {}

        public void onRegisterResponse(int status, MakeCredentialAuthenticatorResponse response) {
            assert mStatus == null;
            mStatus = status;
            mMakeCredentialResponse = response;
            unblock();
        }

        public void onSignResponse(int status, GetAssertionAuthenticatorResponse response) {
            assert mStatus == null;
            mStatus = status;
            mGetAssertionAuthenticatorResponse = response;
            unblock();
        }

        public void onGetMatchingCredentialIds(List<byte[]> matchingCredentialIds) {
            mGetMatchingCredentialIdsResponse = matchingCredentialIds;
            unblock();
        }

        public void onError(int status) {
            assert mStatus == null;
            mStatus = status;
            unblock();
        }

        public Integer getStatus() {
            return mStatus;
        }

        public MakeCredentialAuthenticatorResponse getMakeCredentialResponse() {
            return mMakeCredentialResponse;
        }

        public GetAssertionAuthenticatorResponse getGetAssertionResponse() {
            return mGetAssertionAuthenticatorResponse;
        }

        public List<byte[]> getGetMatchingCredentialIdsResponse() {
            return mGetMatchingCredentialIdsResponse;
        }

        public void blockUntilCalled() {
            mDone.block();
        }

        private void unblock() {
            mDone.open();
        }
    }
}
