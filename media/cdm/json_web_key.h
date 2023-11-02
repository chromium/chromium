// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_JSON_WEB_KEY_H_
#define MEDIA_CDM_JSON_WEB_KEY_H_

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "media/base/media_export.h"

namespace media {

// The ClearKey license request format is a JSON object containing the following
// members (http://w3c.github.io/encrypted-media/#clear-key-request-format):
//   "kids" : An array of key IDs. Each element of the array is the base64url
//            encoding of the octet sequence containing the key ID value.
//   "type" : The requested MediaKeySessionType.
// An example:
//   { "kids":["67ef0gd8pvfd0","77ef0gd8pvfd0"], "type":"temporary" }

// The ClearKey license format is a JSON Web Key (JWK) Set containing
// representation of the symmetric key to be used for decryption.
// (http://w3c.github.io/encrypted-media/#clear-key-license-format)
// For each JWK in the set, the parameter values are as follows:
//   "kty" (key type)  : "oct" (octet sequence)
//   "alg" (algorithm) : "A128KW" (AES key wrap using a 128-bit key)
//   "k" (key value)   : The base64url encoding of the octet sequence
//                       containing the symmetric key value.
//   "kid" (key ID)    : The base64url encoding of the octet sequence
//                       containing the key ID value.
// The JSON object may have an optional "type" member value, which may be
// any of the SessionType values. If not specified, the default value of
// "temporary" is used.
// A JSON Web Key Set looks like the following in JSON:
//   { "keys": [ JWK1, JWK2, ... ], "type":"temporary" }
// A symmetric keys JWK looks like the following in JSON:
//   { "kty":"oct",
//     "alg":"A128KW",
//     "kid":"AQIDBAUGBwgJCgsMDQ4PEA",
//     "k":"FBUWFxgZGhscHR4fICEiIw" }

// There may be other properties specified, but they are ignored.
// Ref: http://tools.ietf.org/html/draft-ietf-jose-json-web-key and:
// http://tools.ietf.org/html/draft-jones-jose-json-private-and-symmetric-key

enum class CdmSessionType;

// Vector of key IDs.
typedef std::vector<std::vector<uint8_t>> KeyIdList;

// Vector of [key_id, key_value] pairs. Values are raw binary data, stored in
// strings for convenience.
typedef std::pair<std::string, std::string> KeyIdAndKeyPair;
typedef std::vector<KeyIdAndKeyPair> KeyIdAndKeyPairs;

// Converts a single |key|, |key_id| pair to a JSON Web Key Set.
MEDIA_EXPORT std::string GenerateJWKSet(const uint8_t* key,
                                        int key_length,
                                        const uint8_t* key_id,
                                        int key_id_length);

// Converts a set of |key|, |key_id| pairs to a JSON Web Key Set.
MEDIA_EXPORT std::string GenerateJWKSet(const KeyIdAndKeyPairs& keys,
                                        CdmSessionType session_type);

// Extracts the JSON Web Keys from a JSON Web Key Set. If |input| looks like
// a valid JWK Set, then true is returned and |keys| and |session_type| are
// updated to contain the values found. Otherwise return false.
MEDIA_EXPORT bool ExtractKeysFromJWKSet(const std::string& jwk_set,
                                        KeyIdAndKeyPairs* keys,
                                        CdmSessionType* session_type);

// Extracts the Key Ids from a Key IDs Initialization Data
// (https://w3c.github.io/encrypted-media/keyids-format.html). If |input| looks
// valid, then true is returned and |key_ids| is updated to contain the values
// found. Otherwise return false and |error_message| contains the reason.
MEDIA_EXPORT bool ExtractKeyIdsFromKeyIdsInitData(const std::string& input,
                                                  KeyIdList* key_ids,
                                                  std::string* error_message);

// Creates a license request message for the |key_ids| and |session_type|
// specified. |license| is updated to contain the resulting JSON string.
MEDIA_EXPORT void CreateLicenseRequest(const KeyIdList& key_ids,
                                       CdmSessionType session_type,
                                       std::vector<uint8_t>* license);

// Creates a keyIDs init_data message for the |key_ids| specified.
// |key_ids_init_data| is updated to contain the resulting JSON string.
MEDIA_EXPORT void CreateKeyIdsInitData(const KeyIdList& key_ids,
                                       std::vector<uint8_t>* key_ids_init_data);

MEDIA_EXPORT std::vector<uint8_t> CreateLicenseReleaseMessage(
    const KeyIdList& key_ids);

// Extract the first key from the license request message. Returns true if
// |license| is a valid license request and contains at least one key,
// otherwise false and |first_key| is not touched.
MEDIA_EXPORT bool ExtractFirstKeyIdFromLicenseRequest(
    const std::vector<uint8_t>& license,
    std::vector<uint8_t>* first_key);

}  // namespace media

#endif  // MEDIA_CDM_JSON_WEB_KEY_H_
