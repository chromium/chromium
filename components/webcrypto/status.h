// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_STATUS_H_
#define COMPONENTS_WEBCRYPTO_STATUS_H_

#include <stddef.h>

#include <string>
#include <string_view>

#include "third_party/blink/public/platform/web_crypto.h"

namespace webcrypto {

// Status indicates whether an operation completed successfully, or with an
// error. The error is used for verification in unit-tests, as well as for
// display to the user.
//
// As such, it is important that errors DO NOT reveal any sensitive material
// (like key bytes).
class Status {
 public:
  Status() : type_(TYPE_ERROR) {}

  // Returns true if the Status represents an error (any one of them).
  bool IsError() const;

  // Returns true if the Status represent success.
  bool IsSuccess() const;

  // Returns a UTF-8 error message (non-localized) describing the error.
  const std::string& error_details() const { return error_details_; }

  blink::WebCryptoErrorType error_type() const { return error_type_; }

  blink::WebCryptoWarningType warning_type() const { return warning_type_; }

  // Constructs a status representing success.
  static Status Success();

  // Constructs a status representing success, but informing the derived key has
  // been truncated.
  static Status SuccessDeriveBitsTruncation();

  // Constructs a status representing a generic operation error. It contains no
  // extra details.
  static Status OperationError();

  // Constructs a status representing a generic data error. It contains no
  // extra details.
  static Status DataError();

  // ------------------------------------
  // Errors when importing a JWK formatted key
  // ------------------------------------

  // The key bytes could not parsed as JSON dictionary. This either
  // means there was a parsing error, or the JSON object was not
  // convertable to a dictionary.
  static Status ErrorJwkNotDictionary();

  // The required JWK member |member_name| was missing.
  static Status ErrorJwkMemberMissing(std::string_view member_name);

  // The JWK member |member_name| was not of type |expected_type|.
  static Status ErrorJwkMemberWrongType(std::string_view member_name,
                                        std::string_view expected_type);

  // The JWK member |member_name| was a string, however could not be
  // successfully base64 decoded.
  static Status ErrorJwkBase64Decode(std::string_view member_name);

  // The "ext" parameter was specified but was
  // incompatible with the value requested by the Web Crypto call.
  static Status ErrorJwkExtInconsistent();

  // The "alg" parameter is incompatible with the (optional) Algorithm
  // specified by the Web Crypto import operation.
  static Status ErrorJwkAlgorithmInconsistent();

  // The "use" parameter was specified, however it couldn't be converted to an
  // equivalent Web Crypto usage.
  static Status ErrorJwkUnrecognizedUse();

  // The "key_ops" parameter was specified, however one of the values in the
  // array couldn't be converted to an equivalent Web Crypto usage.
  static Status ErrorJwkUnrecognizedKeyop();

  // The "use" parameter was specified, however it is incompatible with that
  // specified by the Web Crypto import operation.
  static Status ErrorJwkUseInconsistent();

  // The "key_ops" parameter was specified, however it is incompatible with that
  // specified by the Web Crypto import operation.
  static Status ErrorJwkKeyopsInconsistent();

  // Both the "key_ops" and the "use" parameters were specified, however they
  // are incompatible with each other.
  static Status ErrorJwkUseAndKeyopsInconsistent();

  // The "kty" parameter was given and was a string, however it was not the
  // expected value.
  static Status ErrorJwkUnexpectedKty(std::string_view expected);

  // The amount of key data provided was incompatible with the selected
  // algorithm. For instance if the algorith name was A128CBC then EXACTLY
  // 128-bits of key data must have been provided. If 192-bits of key data were
  // given that is an error.
  static Status ErrorJwkIncorrectKeyLength();

  // The JWK member |member_name| is supposed to represent a big-endian unsigned
  // integer, however was the empty string.
  static Status ErrorJwkEmptyBigInteger(std::string_view member_name);

  // The big-endian unsigned integer |member_name| contained leading zeros. This
  // violates the JWA requirement that such octet strings be minimal.
  static Status ErrorJwkBigIntegerHasLeadingZero(std::string_view member_name);

  // The key_ops lists a usage more than once.
  static Status ErrorJwkDuplicateKeyOps();

  // ------------------------------------
  // Other errors
  // ------------------------------------

  // Tried importing a key using an unsupported format for the key type (for
  // instance importing an HMAC key using format=spki).
  static Status ErrorUnsupportedImportKeyFormat();

  // Tried exporting a key using an unsupported format for the key type (for
  // instance exporting an HMAC key using format=spki).
  static Status ErrorUnsupportedExportKeyFormat();

  // The key data buffer provided for importKey() is an incorrect length for
  // AES.
  static Status ErrorImportAesKeyLength();

  // The length specified when deriving an AES key was not 128 or 256 bits.
  static Status ErrorGetAesKeyLength();

  // Attempted to generate an AES key with an invalid length.
  static Status ErrorGenerateAesKeyLength();

  // 192-bit AES keys are valid, however unsupported (http://crbug.com/533699)
  static Status ErrorAes192BitUnsupported();

  // The wrong key was used for the operation. For instance, a public key was
  // used to verify a RsaSsaPkcs1v1_5 signature, or tried exporting a private
  // key using spki format.
  static Status ErrorUnexpectedKeyType();

  // When doing an AES-CBC encryption/decryption, the "iv" parameter was not 16
  // bytes.
  static Status ErrorIncorrectSizeAesCbcIv();

  // When doing AES-CTR encryption/decryption, the "counter" parameter was not
  // 16 bytes.
  static Status ErrorIncorrectSizeAesCtrCounter();

  // When doing AES-CTR encryption/decryption, the "length" parameter for the
  // counter was out of range.
  static Status ErrorInvalidAesCtrCounterLength();

  // The input to encrypt/decrypt was too large. Based on the counter size, it
  // would cause the counter to wraparound and repeat earlier values.
  static Status ErrorAesCtrInputTooLongCounterRepeated();

  // The data provided to an encrypt/decrypt/sign/verify operation was too
  // large. This can either represent an internal limitation (for instance
  // representing buffer lengths as uints).
  static Status ErrorDataTooLarge();

  // The data provided to an encrypt/decrypt/sign/verify operation was too
  // small. This usually represents an algorithm restriction (for instance
  // AES-KW requires a minimum of 24 bytes input data).
  static Status ErrorDataTooSmall();

  // Something was unsupported or unimplemented. This can mean the algorithm in
  // question was unsupported, some parameter combination was unsupported, or
  // something has not yet been implemented.
  static Status ErrorUnsupported();
  static Status ErrorUnsupported(std::string_view message);

  // Something unexpected happened in the code, which implies there is a
  // source-level bug. These should not happen, but safer to fail than simply
  // DCHECK.
  static Status ErrorUnexpected();

  // The authentication tag length specified for AES-GCM encrypt/decrypt was
  // not 32, 64, 96, 104, 112, 120, or 128.
  static Status ErrorInvalidAesGcmTagLength();

  // The input data given to an AES-KW encrypt/decrypt operation was not a
  // multiple of 8 bytes, as required by RFC 3394.
  static Status ErrorInvalidAesKwDataLength();

  // The "publicExponent" used to generate a key was invalid or unsupported.
  // Only values of 3 and 65537 are allowed.
  static Status ErrorGenerateKeyPublicExponent();

  // The modulus bytes were empty when importing an RSA public key.
  static Status ErrorImportRsaEmptyModulus();

  // The modulus length was unsupported when generating an RSA key pair.
  static Status ErrorGenerateRsaUnsupportedModulus();

  // The exponent bytes were empty when importing an RSA public key.
  static Status ErrorImportRsaEmptyExponent();

  // An unextractable key was used by an operation which exports the key data.
  static Status ErrorKeyNotExtractable();

  // Attempted to generate an HMAC key using a key length of 0.
  static Status ErrorGenerateHmacKeyLengthZero();

  // Attempted to import an HMAC key containing no data.
  static Status ErrorHmacImportEmptyKey();

  // Attempted to derive an HMAC key with zero length.
  static Status ErrorGetHmacKeyLengthZero();

  // Attempted to import an HMAC key using a bad optional length.
  static Status ErrorHmacImportBadLength();

  // Attempted to create a key (either by importKey(), generateKey(), or
  // unwrapKey()) however the key usages were not applicable for the key type
  // and algorithm.
  static Status ErrorCreateKeyBadUsages();

  // No usages were specified when generating/importing a secret or private key.
  static Status ErrorCreateKeyEmptyUsages();

  // An EC key imported using SPKI/PKCS8 format had the wrong curve specified in
  // the key.
  static Status ErrorImportedEcKeyIncorrectCurve();

  // The "crv" member for a JWK did not match the expectations from importKey()
  static Status ErrorJwkIncorrectCrv();

  // The EC key failed validation (coordinates don't lie on curve, out of range,
  // etc.)
  static Status ErrorEcKeyInvalid();

  // The octet string |member_name| was expected to be |expected_length| bytes
  // long, but was instead |actual_length| bytes long.
  static Status JwkOctetStringWrongLength(std::string_view member_name,
                                          size_t expected_length,
                                          size_t actual_length);

  // The public key given for ECDH key derivation was not an EC public key.
  static Status ErrorEcdhPublicKeyWrongType();

  // The public key's algorithm was not ECDH.
  static Status ErrorEcdhPublicKeyWrongAlgorithm();

  // The public and private keys given to ECDH key derivation were not for the
  // same named curve.
  static Status ErrorEcdhCurveMismatch();

  // The requested bit length for ECDH key derivation was too large.
  static Status ErrorEcdhLengthTooBig(unsigned int max_length_bits);

  // The requested length for HKDF was too large.
  static Status ErrorHkdfLengthTooLong();

  // The length to HKDF's deriveBits() was not a multiple of 8.
  static Status ErrorHkdfLengthNotWholeByte();

  // No length parameter was provided for HKDF's Derive Bits operation.
  static Status ErrorHkdfDeriveBitsLengthNotSpecified();

  // The requested bit length for PBKDF2 key derivation was invalid.
  static Status ErrorPbkdf2InvalidLength();

  // No length parameter was provided for PBKDF2's Derive Bits operation.
  static Status ErrorPbkdf2DeriveBitsLengthNotSpecified();

  // PBKDF2's deriveBits() was called with an unsupported length of 0.
  static Status ErrorPbkdf2DeriveBitsLengthZero();

  // PBKDF2 was called with iterations == 0.
  static Status ErrorPbkdf2Iterations0();

  // Tried importing a key with extractable=true for one of the *KDF
  // algorithms.
  static Status ErrorImportExtractableKdfKey();

  // The key data buffer provided for importKey() is an incorrect length for
  // Ed25519.
  static Status ErrorImportEd25519KeyLength();

  // The algorithm given for X25519 key derivation was not X25519.
  static Status ErrorX25519WrongAlgorithm();

  // The public key given for X25519 key derivation was not a public key.
  static Status ErrorX25519PublicKeyWrongType();

  // The public key's algorithm was not X25519.
  static Status ErrorX25519PublicKeyWrongAlgorithm();

  // The requested length for X25519 was too large.
  static Status ErrorX25519LengthTooLong();

  // The key data buffer provided for importKey() is an incorrect length for
  // X25519.
  static Status ErrorImportX25519KeyLength();

 private:
  enum Type { TYPE_ERROR, TYPE_SUCCESS };

  // Constructs an error with the specified error type and message.
  Status(blink::WebCryptoErrorType error_type,
         std::string_view error_details_utf8);

  // Constructs a success or error without any details.
  explicit Status(Type type);

  Type type_;
  blink::WebCryptoErrorType error_type_;
  blink::WebCryptoWarningType warning_type_{blink::kWebCryptoWarningTypeNone};
  std::string error_details_;
};

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_STATUS_H_
