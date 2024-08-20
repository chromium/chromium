// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/status.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"

namespace webcrypto {

bool Status::IsError() const {
  return type_ == TYPE_ERROR;
}

bool Status::IsSuccess() const {
  return type_ == TYPE_SUCCESS;
}

Status Status::Success() {
  return Status(TYPE_SUCCESS);
}

Status Status::SuccessDeriveBitsTruncation() {
  Status status(TYPE_SUCCESS);
  status.warning_type_ = blink::kWebCryptoWarningTypeDeriveBitsTruncated;
  return status;
}

Status Status::OperationError() {
  return Status(blink::kWebCryptoErrorTypeOperation, "");
}

Status Status::DataError() {
  return Status(blink::kWebCryptoErrorTypeData, "");
}

Status Status::ErrorJwkNotDictionary() {
  return Status(blink::kWebCryptoErrorTypeData,
                "JWK input could not be parsed to a JSON dictionary");
}

Status Status::ErrorJwkMemberMissing(std::string_view member_name) {
  return Status(blink::kWebCryptoErrorTypeData,
                base::StrCat({"The required JWK member \"", member_name,
                              "\" was missing"}));
}

Status Status::ErrorJwkMemberWrongType(std::string_view member_name,
                                       std::string_view expected_type) {
  return Status(blink::kWebCryptoErrorTypeData,
                base::StrCat({"The JWK member \"", member_name, "\" must be a ",
                              expected_type}));
}

Status Status::ErrorJwkBase64Decode(std::string_view member_name) {
  return Status(
      blink::kWebCryptoErrorTypeData,
      base::StrCat({"The JWK member \"", member_name,
                    "\" could not be base64url decoded or contained padding"}));
}

Status Status::ErrorJwkExtInconsistent() {
  return Status(
      blink::kWebCryptoErrorTypeData,
      "The \"ext\" member of the JWK dictionary is inconsistent what that "
      "specified by the Web Crypto call");
}

Status Status::ErrorJwkAlgorithmInconsistent() {
  return Status(blink::kWebCryptoErrorTypeData,
                "The JWK \"alg\" member was inconsistent with that specified "
                "by the Web Crypto call");
}

Status Status::ErrorJwkUnrecognizedUse() {
  return Status(blink::kWebCryptoErrorTypeData,
                "The JWK \"use\" member could not be parsed");
}

Status Status::ErrorJwkUnrecognizedKeyop() {
  return Status(blink::kWebCryptoErrorTypeData,
                "The JWK \"key_ops\" member could not be parsed");
}

Status Status::ErrorJwkUseInconsistent() {
  return Status(blink::kWebCryptoErrorTypeData,
                "The JWK \"use\" member was inconsistent with that specified "
                "by the Web Crypto call. The JWK usage must be a superset of "
                "those requested");
}

Status Status::ErrorJwkKeyopsInconsistent() {
  return Status(blink::kWebCryptoErrorTypeData,
                "The JWK \"key_ops\" member was inconsistent with that "
                "specified by the Web Crypto call. The JWK usage must be a "
                "superset of those requested");
}

Status Status::ErrorJwkUseAndKeyopsInconsistent() {
  return Status(blink::kWebCryptoErrorTypeData,
                "The JWK \"use\" and \"key_ops\" properties were both found "
                "but are inconsistent with each other.");
}

Status Status::ErrorJwkUnexpectedKty(std::string_view expected) {
  return Status(
      blink::kWebCryptoErrorTypeData,
      base::StrCat({"The JWK \"kty\" member was not \"", expected, "\""}));
}

Status Status::ErrorJwkIncorrectKeyLength() {
  return Status(blink::kWebCryptoErrorTypeData,
                "The JWK \"k\" member did not include the right length "
                "of key data for the given algorithm.");
}

Status Status::ErrorJwkEmptyBigInteger(std::string_view member_name) {
  return Status(
      blink::kWebCryptoErrorTypeData,
      base::StrCat({"The JWK \"", member_name, "\" member was empty."}));
}

Status Status::ErrorJwkBigIntegerHasLeadingZero(std::string_view member_name) {
  return Status(blink::kWebCryptoErrorTypeData,
                base::StrCat({"The JWK \"", member_name,
                              "\" member contained a leading zero."}));
}

Status Status::ErrorJwkDuplicateKeyOps() {
  return Status(blink::kWebCryptoErrorTypeData,
                "The \"key_ops\" member of the JWK dictionary contains "
                "duplicate usages.");
}

Status Status::ErrorUnsupportedImportKeyFormat() {
  return Status(blink::kWebCryptoErrorTypeNotSupported,
                "Unsupported import key format for algorithm");
}

Status Status::ErrorUnsupportedExportKeyFormat() {
  return Status(blink::kWebCryptoErrorTypeNotSupported,
                "Unsupported export key format for algorithm");
}

Status Status::ErrorImportAesKeyLength() {
  return Status(blink::kWebCryptoErrorTypeData,
                "AES key data must be 128 or 256 bits");
}

Status Status::ErrorGetAesKeyLength() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "AES key length must be 128 or 256 bits");
}

Status Status::ErrorGenerateAesKeyLength() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "AES key length must be 128 or 256 bits");
}

Status Status::ErrorAes192BitUnsupported() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "192-bit AES keys are not supported");
}

Status Status::ErrorUnexpectedKeyType() {
  return Status(blink::kWebCryptoErrorTypeInvalidAccess,
                "The key is not of the expected type");
}

Status Status::ErrorIncorrectSizeAesCbcIv() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "The \"iv\" has an unexpected length -- must be 16 bytes");
}

Status Status::ErrorIncorrectSizeAesCtrCounter() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "The \"counter\" has an unexpected length -- must be 16 bytes");
}

Status Status::ErrorInvalidAesCtrCounterLength() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "The \"length\" member must be >= 1 and <= 128");
}

Status Status::ErrorAesCtrInputTooLongCounterRepeated() {
  return Status(blink::kWebCryptoErrorTypeData,
                "The input is too large for the counter length.");
}

Status Status::ErrorImportEd25519KeyLength() {
  return Status(blink::kWebCryptoErrorTypeData,
                "Ed25519 key data must be 256 bits");
}

Status Status::ErrorX25519WrongAlgorithm() {
  return Status(blink::kWebCryptoErrorTypeInvalidAccess,
                "The algorithm for X25519 key derivation must be X25519");
}

Status Status::ErrorX25519PublicKeyWrongType() {
  return Status(
      blink::kWebCryptoErrorTypeInvalidAccess,
      "The public parameter for X25519 key derivation is not a public key");
}

Status Status::ErrorX25519PublicKeyWrongAlgorithm() {
  return Status(
      blink::kWebCryptoErrorTypeInvalidAccess,
      "The public parameter for X25519 key derivation must be for X25519");
}

Status Status::ErrorImportX25519KeyLength() {
  return Status(blink::kWebCryptoErrorTypeData,
                "X25519 key data must be 256 bits");
}

Status Status::ErrorDataTooLarge() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "The provided data is too large");
}

Status Status::ErrorDataTooSmall() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "The provided data is too small");
}

Status Status::ErrorUnsupported() {
  return ErrorUnsupported("The requested operation is unsupported");
}

Status Status::ErrorUnsupported(std::string_view message) {
  return Status(blink::kWebCryptoErrorTypeNotSupported, message);
}

Status Status::ErrorUnexpected() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "Something unexpected happened...");
}

Status Status::ErrorInvalidAesGcmTagLength() {
  return Status(
      blink::kWebCryptoErrorTypeOperation,
      "The tag length is invalid: Must be 32, 64, 96, 104, 112, 120, or 128 "
      "bits");
}

Status Status::ErrorInvalidAesKwDataLength() {
  return Status(blink::kWebCryptoErrorTypeData,
                "The AES-KW input data length is invalid: not a multiple of 8 "
                "bytes");
}

Status Status::ErrorGenerateKeyPublicExponent() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "The \"publicExponent\" must be either 3 or 65537");
}

Status Status::ErrorImportRsaEmptyModulus() {
  return Status(blink::kWebCryptoErrorTypeData, "The modulus is empty");
}

Status Status::ErrorGenerateRsaUnsupportedModulus() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "The modulus length must be a multiple of 8 bits and >= 256 "
                "and <= 16384");
}

Status Status::ErrorImportRsaEmptyExponent() {
  return Status(blink::kWebCryptoErrorTypeData,
                "No bytes for the exponent were provided");
}

Status Status::ErrorKeyNotExtractable() {
  return Status(blink::kWebCryptoErrorTypeInvalidAccess,
                "They key is not extractable");
}

Status Status::ErrorGenerateHmacKeyLengthZero() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "HMAC key length must not be zero");
}

Status Status::ErrorHmacImportEmptyKey() {
  return Status(blink::kWebCryptoErrorTypeData,
                "HMAC key data must not be empty");
}

Status Status::ErrorGetHmacKeyLengthZero() {
  return Status(blink::kWebCryptoErrorTypeType,
                "HMAC key length must not be zero");
}

Status Status::ErrorHmacImportBadLength() {
  return Status(
      blink::kWebCryptoErrorTypeData,
      "The optional HMAC key length must be shorter than the key data, and by "
      "no more than 7 bits.");
}

Status Status::ErrorCreateKeyBadUsages() {
  return Status(blink::kWebCryptoErrorTypeSyntax,
                "Cannot create a key using the specified key usages.");
}

Status Status::ErrorCreateKeyEmptyUsages() {
  return Status(blink::kWebCryptoErrorTypeSyntax,
                "Usages cannot be empty when creating a key.");
}

Status Status::ErrorImportedEcKeyIncorrectCurve() {
  return Status(
      blink::kWebCryptoErrorTypeData,
      "The imported EC key specifies a different curve than requested");
}

Status Status::ErrorJwkIncorrectCrv() {
  return Status(
      blink::kWebCryptoErrorTypeData,
      "The JWK's \"crv\" member specifies a different curve than requested");
}

Status Status::ErrorEcKeyInvalid() {
  return Status(blink::kWebCryptoErrorTypeData,
                "The imported EC key is invalid");
}

Status Status::JwkOctetStringWrongLength(std::string_view member_name,
                                         size_t expected_length,
                                         size_t actual_length) {
  return Status(
      blink::kWebCryptoErrorTypeData,
      base::StringPrintf("The JWK's \"%.*s\" member defines an octet string of "
                         "length %zu bytes but should be %zu",
                         base::checked_cast<int>(member_name.size()),
                         member_name.data(), actual_length, expected_length));
}

Status Status::ErrorEcdhPublicKeyWrongType() {
  return Status(
      blink::kWebCryptoErrorTypeInvalidAccess,
      "The public parameter for ECDH key derivation is not a public EC key");
}

Status Status::ErrorEcdhPublicKeyWrongAlgorithm() {
  return Status(
      blink::kWebCryptoErrorTypeInvalidAccess,
      "The public parameter for ECDH key derivation must be for ECDH");
}

Status Status::ErrorEcdhCurveMismatch() {
  return Status(blink::kWebCryptoErrorTypeInvalidAccess,
                "The public parameter for ECDH key derivation is for a "
                "different named curve");
}

Status Status::ErrorEcdhLengthTooBig(unsigned int max_length_bits) {
  return Status(blink::kWebCryptoErrorTypeOperation,
                base::StringPrintf(
                    "Length specified for ECDH key derivation is too large. "
                    "Maximum allowed is %u bits",
                    max_length_bits));
}

Status Status::ErrorHkdfLengthTooLong() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "The length provided for HKDF is too large.");
}

Status Status::ErrorHkdfLengthNotWholeByte() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "The length provided for HKDF is not a multiple of 8 bits.");
}

Status Status::ErrorHkdfDeriveBitsLengthNotSpecified() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "No length was specified for the HKDF Derive Bits operation.");
}

Status Status::ErrorPbkdf2InvalidLength() {
  return Status(
      blink::kWebCryptoErrorTypeOperation,
      "Length for PBKDF2 key derivation must be a multiple of 8 bits.");
}

Status Status::ErrorPbkdf2DeriveBitsLengthNotSpecified() {
  return Status(
      blink::kWebCryptoErrorTypeOperation,
      "No length was specified for the PBKDF2 Derive Bits operation.");
}

Status Status::ErrorPbkdf2DeriveBitsLengthZero() {
  return Status(
      blink::kWebCryptoErrorTypeOperation,
      "A length of 0 was specified for PBKDF2's Derive Bits operation.");
}

Status Status::ErrorPbkdf2Iterations0() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "PBKDF2 requires iterations > 0");
}

Status Status::ErrorImportExtractableKdfKey() {
  return Status(blink::kWebCryptoErrorTypeSyntax,
                "KDF keys must set extractable=false");
}

Status Status::ErrorX25519LengthTooLong() {
  return Status(blink::kWebCryptoErrorTypeOperation,
                "The length provided for X25519 is too large.");
}

Status::Status(blink::WebCryptoErrorType error_type,
               std::string_view error_details_utf8)
    : type_(TYPE_ERROR),
      error_type_(error_type),
      error_details_(error_details_utf8) {}

Status::Status(Type type) : type_(type) {
}

}  // namespace webcrypto
