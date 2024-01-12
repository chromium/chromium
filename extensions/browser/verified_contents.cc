// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/verified_contents.h"

#include <stddef.h>

#include <algorithm>
#include <string_view>

#include "base/base64url.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/crx_file/id_util.h"
#include "crypto/signature_verifier.h"
#include "extensions/browser/content_verifier/content_verifier_utils.h"
#include "extensions/common/extension.h"

namespace {

const char kBlockSizeKey[] = "block_size";
const char kContentHashesKey[] = "content_hashes";
const char kDescriptionKey[] = "description";
const char kFilesKey[] = "files";
const char kFormatKey[] = "format";
const char kHashBlockSizeKey[] = "hash_block_size";
const char kHeaderKidKey[] = "header.kid";
const char kItemIdKey[] = "item_id";
const char kItemVersionKey[] = "item_version";
const char kPathKey[] = "path";
const char kPayloadKey[] = "payload";
const char kProtectedKey[] = "protected";
const char kRootHashKey[] = "root_hash";
const char kSignatureKey[] = "signature";
const char kSignaturesKey[] = "signatures";
const char kSignedContentKey[] = "signed_content";
const char kTreeHashPerFile[] = "treehash per file";
const char kTreeHash[] = "treehash";
const char kWebstoreKId[] = "webstore";

// Helper function to iterate over a list of dictionaries, returning the
// dictionary that has |key| -> |value| in it, if any, or null.
const base::Value::Dict* FindDictionaryWithValue(const base::Value::List& list,
                                                 const std::string& key,
                                                 const std::string& value) {
  for (const base::Value& item : list) {
    if (!item.is_dict())
      continue;
    // Finds a path because the |key| may include '.'.
    const std::string* found_value = item.GetDict().FindStringByDottedPath(key);
    if (found_value && *found_value == value)
      return &item.GetDict();
  }
  return nullptr;
}

}  // namespace

namespace extensions {

VerifiedContents::VerifiedContents(base::span<const uint8_t> public_key)
    : public_key_(public_key),
      valid_signature_(false),  // Guilty until proven innocent.
      block_size_(0) {}

VerifiedContents::~VerifiedContents() {
}

// The format of the payload json is:
// {
//   "item_id": "<extension id>",
//   "item_version": "<extension version>",
//   "content_hashes": [
//     {
//       "block_size": 4096,
//       "hash_block_size": 4096,
//       "format": "treehash",
//       "files": [
//         {
//           "path": "foo/bar",
//           "root_hash": "<base64url encoded bytes>"
//         },
//         ...
//       ]
//     }
//   ]
// }
// static.
std::unique_ptr<VerifiedContents> VerifiedContents::CreateFromFile(
    base::span<const uint8_t> public_key,
    const base::FilePath& path) {
  std::string contents;
  if (!base::ReadFileToString(path, &contents))
    return nullptr;
  return Create(public_key, contents);
}

std::unique_ptr<VerifiedContents> VerifiedContents::Create(
    base::span<const uint8_t> public_key,
    std::string_view contents) {
  // Note: VerifiedContents constructor is private.
  auto verified_contents = base::WrapUnique(new VerifiedContents(public_key));
  std::string payload;
  if (!verified_contents->GetPayload(contents, &payload))
    return nullptr;

  std::optional<base::Value> dictionary_value = base::JSONReader::Read(payload);
  if (!dictionary_value || !dictionary_value->is_dict()) {
    return nullptr;
  }

  base::Value::Dict& dictionary = dictionary_value->GetDict();
  const std::string* item_id = dictionary.FindString(kItemIdKey);
  if (!item_id || !crx_file::id_util::IdIsValid(*item_id))
    return nullptr;

  verified_contents->extension_id_ = *item_id;

  const std::string* version_string = dictionary.FindString(kItemVersionKey);
  if (!version_string)
    return nullptr;

  verified_contents->version_ = base::Version(*version_string);
  if (!verified_contents->version_.IsValid())
    return nullptr;

  const base::Value::List* hashes_list = dictionary.FindList(kContentHashesKey);
  if (!hashes_list)
    return nullptr;

  for (const base::Value& hashes : *hashes_list) {
    const base::Value::Dict* hashes_dict = hashes.GetIfDict();
    if (!hashes_dict) {
      return nullptr;
    }

    const std::string* format = hashes_dict->FindString(kFormatKey);
    if (!format || *format != kTreeHash)
      continue;

    std::optional<int> block_size = hashes_dict->FindInt(kBlockSizeKey);
    std::optional<int> hash_block_size =
        hashes_dict->FindInt(kHashBlockSizeKey);
    if (!block_size || !hash_block_size)
      return nullptr;

    verified_contents->block_size_ = *block_size;

    // We don't support using a different block_size and hash_block_size at
    // the moment.
    if (verified_contents->block_size_ != *hash_block_size)
      return nullptr;

    const base::Value::List* files = hashes_dict->FindList(kFilesKey);
    if (!files)
      return nullptr;

    for (const base::Value& data : *files) {
      const base::Value::Dict* data_dict = data.GetIfDict();
      if (!data_dict) {
        return nullptr;
      }

      const std::string* file_path_string = data_dict->FindString(kPathKey);
      const std::string* encoded_root_hash =
          data_dict->FindString(kRootHashKey);
      std::string root_hash;
      if (!file_path_string || !encoded_root_hash ||
          !base::IsStringUTF8(*file_path_string) ||
          !base::Base64UrlDecode(*encoded_root_hash,
                                 base::Base64UrlDecodePolicy::IGNORE_PADDING,
                                 &root_hash)) {
        return nullptr;
      }

      content_verifier_utils::CanonicalRelativePath canonical_path =
          content_verifier_utils::CanonicalizeRelativePath(
              base::FilePath::FromUTF8Unsafe(*file_path_string));
      auto i = verified_contents->root_hashes_.insert(
          std::make_pair(canonical_path, std::string()));
      i->second.swap(root_hash);
    }

    break;
  }
  return verified_contents;
}

bool VerifiedContents::HasTreeHashRoot(
    const base::FilePath& relative_path) const {
  return base::Contains(
      root_hashes_,
      content_verifier_utils::CanonicalizeRelativePath(relative_path));
}

bool VerifiedContents::TreeHashRootEquals(const base::FilePath& relative_path,
                                          const std::string& expected) const {
  return TreeHashRootEqualsForCanonicalPath(
      content_verifier_utils::CanonicalizeRelativePath(relative_path),
      expected);
}

// We're loosely following the "JSON Web Signature" draft spec for signing
// a JSON payload:
//
//   http://tools.ietf.org/html/draft-ietf-jose-json-web-signature-26
//
// The idea is that you have some JSON that you want to sign, so you
// base64-encode that and put it as the "payload" field in a containing
// dictionary. There might be signatures of it done with multiple
// algorithms/parameters, so the payload is followed by a list of one or more
// signature sections. Each signature section specifies the
// algorithm/parameters in a JSON object which is base64url encoded into one
// string and put into a "protected" field in the signature. Then the encoded
// "payload" and "protected" strings are concatenated with a "." in between
// them and those bytes are signed and the resulting signature is base64url
// encoded and placed in the "signature" field. To allow for extensibility, we
// wrap this, so we can include additional kinds of payloads in the future. E.g.
// [
//   {
//     "description": "treehash per file",
//     "signed_content": {
//       "payload": "<base64url encoded JSON to sign>",
//       "signatures": [
//         {
//           "protected": "<base64url encoded JSON with algorithm/parameters>",
//           "header": {
//             <object with metadata about this signature, eg a key identifier>
//           }
//           "signature":
//              "<base64url encoded signature over payload || . || protected>"
//         },
//         ... <zero or more additional signatures> ...
//       ]
//     }
//   }
// ]
// There might be both a signature generated with a webstore private key and a
// signature generated with the extension's private key - for now we only
// verify the webstore one (since the id is in the payload, so we can trust
// that it is for a given extension), but in the future we may validate using
// the extension's key too (eg for non-webstore hosted extensions such as
// enterprise installs).
bool VerifiedContents::GetPayload(std::string_view contents,
                                  std::string* payload) {
  std::optional<base::Value> top_list = base::JSONReader::Read(contents);
  if (!top_list || !top_list->is_list())
    return false;

  // Find the "treehash per file" signed content, e.g.
  // [
  //   {
  //     "description": "treehash per file",
  //     "signed_content": {
  //       "signatures": [ ... ],
  //       "payload": "..."
  //     }
  //   }
  // ]
  const base::Value::Dict* dictionary = FindDictionaryWithValue(
      top_list->GetList(), kDescriptionKey, kTreeHashPerFile);
  if (!dictionary)
    return false;

  const base::Value::Dict* signed_content =
      dictionary->FindDict(kSignedContentKey);
  if (!signed_content)
    return false;

  const base::Value::List* signatures =
      signed_content->FindList(kSignaturesKey);
  if (!signatures)
    return false;

  const base::Value::Dict* signature_dict =
      FindDictionaryWithValue(*signatures, kHeaderKidKey, kWebstoreKId);
  if (!signature_dict)
    return false;

  const std::string* protected_value =
      signature_dict->FindString(kProtectedKey);
  const std::string* encoded_signature =
      signature_dict->FindString(kSignatureKey);
  std::string decoded_signature;
  if (!protected_value || !encoded_signature ||
      !base::Base64UrlDecode(*encoded_signature,
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &decoded_signature))
    return false;

  const std::string* encoded_payload = signed_content->FindString(kPayloadKey);
  if (!encoded_payload)
    return false;

  valid_signature_ =
      VerifySignature(*protected_value, *encoded_payload, decoded_signature);
  if (!valid_signature_)
    return false;

  if (!base::Base64UrlDecode(*encoded_payload,
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             payload))
    return false;

  return true;
}

bool VerifiedContents::VerifySignature(const std::string& protected_value,
                                       const std::string& payload,
                                       const std::string& signature_bytes) {
  crypto::SignatureVerifier signature_verifier;
  if (!signature_verifier.VerifyInit(
          crypto::SignatureVerifier::RSA_PKCS1_SHA256,
          base::as_bytes(base::make_span(signature_bytes)), public_key_)) {
    VLOG(1) << "Could not verify signature - VerifyInit failure";
    return false;
  }

  signature_verifier.VerifyUpdate(
      base::as_bytes(base::make_span(protected_value)));

  std::string dot(".");
  signature_verifier.VerifyUpdate(base::as_bytes(base::make_span(dot)));

  signature_verifier.VerifyUpdate(base::as_bytes(base::make_span(payload)));

  if (!signature_verifier.VerifyFinal()) {
    VLOG(1) << "Could not verify signature - VerifyFinal failure";
    return false;
  }
  return true;
}

bool VerifiedContents::TreeHashRootEqualsForCanonicalPath(
    const content_verifier_utils::CanonicalRelativePath&
        canonical_relative_path,
    const std::string& expected) const {
  std::pair<RootHashes::const_iterator, RootHashes::const_iterator> hashes =
      root_hashes_.equal_range(canonical_relative_path);
  for (auto iter = hashes.first; iter != hashes.second; ++iter) {
    if (expected == iter->second)
      return true;
  }
  return false;
}

}  // namespace extensions
