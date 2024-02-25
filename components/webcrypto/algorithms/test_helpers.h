// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHMS_TEST_HELPERS_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHMS_TEST_HELPERS_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_crypto_algorithm.h"
#include "third_party/blink/public/platform/web_crypto_key.h"

// Compare the input in hex, because `base::span` supports neither equality nor
// printing.
#define EXPECT_BYTES_EQ(expected, actual) \
  EXPECT_EQ(base::HexEncode(expected), base::HexEncode(actual))

#define EXPECT_BYTES_EQ_HEX(expected_hex, actual_bytes) \
  EXPECT_BYTES_EQ(HexStringToBytes(expected_hex), actual_bytes)

namespace blink {
class WebCryptoAlgorithm;
}

namespace webcrypto {

// Base class for WebCrypto tests. All WebCrypto tests must derive from this
// to ensure that Blink has been properly initialized. In particular,
// the WebCrypto tests use blink::WebCryptoAlgorithm, which in turn relies on
// PartitionAlloc.
class WebCryptoTestBase : public testing::Test {
 protected:
  static void SetUpTestSuite();
};

class Status;

// These functions are used by GTEST to support EXPECT_EQ() for
// webcrypto::Status.
void PrintTo(const Status& status, ::std::ostream* os);
bool operator==(const Status& a, const Status& b);
bool operator!=(const Status& a, const Status& b);

// Gives a human-readable description of |status| and any error it represents.
std::string StatusToString(const Status& status);

blink::WebCryptoAlgorithm CreateRsaHashedKeyGenAlgorithm(
    blink::WebCryptoAlgorithmId algorithm_id,
    const blink::WebCryptoAlgorithmId hash_id,
    unsigned int modulus_length,
    const std::vector<uint8_t>& public_exponent);

// Returns a slightly modified version of the input vector.
//
//  - For non-empty inputs a single bit is inverted.
//  - For empty inputs, a byte is added.
std::vector<uint8_t> Corrupted(const std::vector<uint8_t>& input);

std::vector<uint8_t> HexStringToBytes(std::string_view hex);

// Serialize |value| to json, then return that json as a byte vector.
std::vector<uint8_t> MakeJsonVector(const base::ValueView& value);

// ----------------------------------------------------------------
// Helpers for working with JSON data files for test expectations.
// ----------------------------------------------------------------

// Reads "//components/test/data/webcrypto/" + test_file_name as a JSON
// file, asserts that the contained JSON is a list, and returns that list.
base::Value::List ReadJsonTestFileAsList(const char* test_file_name);

// Reads a string property from the dictionary |dict| with path |property_name|
// (which can include periods for nested dictionaries). Interprets the
// string as a hex encoded string and converts it to a bytes list.
//
// Returns empty vector on failure.
std::vector<uint8_t> GetBytesFromHexString(const base::Value::Dict& dict,
                                           std::string_view property_name);

// Reads a string property with path "property_name" and converts it to a
// WebCryptoAlgorithm. Returns null algorithm on failure.
blink::WebCryptoAlgorithm GetDigestAlgorithm(const base::Value::Dict& dict,
                                             const char* property_name);

// Returns true if any of the vectors in the input list have identical content.
bool CopiesExist(const std::vector<std::vector<uint8_t>>& bufs);

blink::WebCryptoAlgorithm CreateAesKeyGenAlgorithm(
    blink::WebCryptoAlgorithmId aes_alg_id,
    uint16_t length);

// The following key pair is comprised of the SPKI (public key) and PKCS#8
// (private key) representations of the key pair provided in Example 1 of the
// NIST test vectors at
// ftp://ftp.rsa.com/pub/rsalabs/tmp/pkcs1v15sign-vectors.txt
extern const unsigned int kModulusLengthBits;
extern const char* const kPublicKeySpkiDerHex;
extern const char* const kPrivateKeyPkcs8DerHex;

// The modulus and exponent (in hex) of kPublicKeySpkiDerHex
extern const char* const kPublicKeyModulusHex;
extern const char* const kPublicKeyExponentHex;

blink::WebCryptoKey ImportSecretKeyFromRaw(
    const std::vector<uint8_t>& key_raw,
    const blink::WebCryptoAlgorithm& algorithm,
    blink::WebCryptoKeyUsageMask usage);

void ImportRsaKeyPair(const std::vector<uint8_t>& spki_der,
                      const std::vector<uint8_t>& pkcs8_der,
                      const blink::WebCryptoAlgorithm& algorithm,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask public_key_usages,
                      blink::WebCryptoKeyUsageMask private_key_usages,
                      blink::WebCryptoKey* public_key,
                      blink::WebCryptoKey* private_key);

Status ImportKeyJwkFromDict(const base::ValueView& dict,
                            const blink::WebCryptoAlgorithm& algorithm,
                            bool extractable,
                            blink::WebCryptoKeyUsageMask usages,
                            blink::WebCryptoKey* key);

// Parses a vector of JSON into a dictionary.
std::optional<base::Value::Dict> GetJwkDictionary(
    const std::vector<uint8_t>& json);

// Verifies the input dictionary contains the expected values. Exact matches are
// required on the fields examined.
::testing::AssertionResult VerifyJwk(
    const base::Value::Dict& dict,
    std::string_view kty_expected,
    std::string_view alg_expected,
    blink::WebCryptoKeyUsageMask use_mask_expected);

::testing::AssertionResult VerifySecretJwk(
    const std::vector<uint8_t>& json,
    std::string_view alg_expected,
    std::string_view k_expected_hex,
    blink::WebCryptoKeyUsageMask use_mask_expected);

// Verifies that the JSON in the input vector contains the provided
// expected values. Exact matches are required on the fields examined.
::testing::AssertionResult VerifyPublicJwk(
    const std::vector<uint8_t>& json,
    std::string_view alg_expected,
    std::string_view n_expected_hex,
    std::string_view e_expected_hex,
    blink::WebCryptoKeyUsageMask use_mask_expected);

// Helper that tests importing ane exporting of symmetric keys as JWK.
void ImportExportJwkSymmetricKey(
    int key_len_bits,
    const blink::WebCryptoAlgorithm& import_algorithm,
    blink::WebCryptoKeyUsageMask usages,
    std::string_view jwk_alg);

// Wrappers around GenerateKey() which expect the result to be either a secret
// key or a public/private keypair. If the result does not match the
// expectation, then it fails with Status::ErrorUnexpected().
Status GenerateSecretKey(const blink::WebCryptoAlgorithm& algorithm,
                         bool extractable,
                         blink::WebCryptoKeyUsageMask usages,
                         blink::WebCryptoKey* key);
Status GenerateKeyPair(const blink::WebCryptoAlgorithm& algorithm,
                       bool extractable,
                       blink::WebCryptoKeyUsageMask usages,
                       blink::WebCryptoKey* public_key,
                       blink::WebCryptoKey* private_key);

// Reads a key format string as used in some JSON test files and converts it to
// a WebCryptoKeyFormat.
blink::WebCryptoKeyFormat GetKeyFormatFromJsonTestCase(
    const base::Value::Dict& test);

// Extracts the key data bytes from |test| as used insome JSON test files.
std::vector<uint8_t> GetKeyDataFromJsonTestCase(
    const base::Value::Dict& test,
    blink::WebCryptoKeyFormat key_format);

// Reads the "crv" string from a JSON test case and returns it as a
// WebCryptoNamedCurve.
blink::WebCryptoNamedCurve GetCurveNameFromDictionary(
    const base::Value::Dict& dict);

blink::WebCryptoNamedCurve CurveNameToCurve(std::string_view name);

// Creates an HMAC import algorithm whose inner hash algorithm is determined by
// the specified algorithm ID. It is an error to call this method with a hash
// algorithm that is not SHA*.
blink::WebCryptoAlgorithm CreateHmacImportAlgorithm(
    blink::WebCryptoAlgorithmId hash_id,
    unsigned int length_bits);

// Same as above but without specifying a length.
blink::WebCryptoAlgorithm CreateHmacImportAlgorithmNoLength(
    blink::WebCryptoAlgorithmId hash_id);

// Creates a WebCryptoAlgorithm without any parameters.
blink::WebCryptoAlgorithm CreateAlgorithm(blink::WebCryptoAlgorithmId id);

// Creates an import algorithm for RSA algorithms that take a hash.
// It is an error to call this with a hash_id that is not a SHA*.
blink::WebCryptoAlgorithm CreateRsaHashedImportAlgorithm(
    blink::WebCryptoAlgorithmId id,
    blink::WebCryptoAlgorithmId hash_id);

// Creates an import algorithm for EC keys.
blink::WebCryptoAlgorithm CreateEcImportAlgorithm(
    blink::WebCryptoAlgorithmId id,
    blink::WebCryptoNamedCurve named_curve);

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHMS_TEST_HELPERS_H_
