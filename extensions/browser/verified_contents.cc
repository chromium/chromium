// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/verified_contents.h"

#include <stddef.h>
#include <algorithm>

#include "base/base64url.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/crx_file/id_util.h"
#include "crypto/signature_verifier.h"
#include "extensions/common/extension.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

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
// dictionary that has |key| -> |value| in it, if any, or NULL.
const DictionaryValue* FindDictionaryWithValue(const ListValue* list,
                                               const std::string& key,
                                               const std::string& value) {
  for (const auto& i : *list) {
    const DictionaryValue* dictionary;
    if (!i.GetAsDictionary(&dictionary))
      continue;
    std::string found_value;
    if (dictionary->GetString(key, &found_value) && found_value == value)
      return dictionary;
  }
  return NULL;
}

// Helper to record UMA for results of initializing verified_contents.json file.
// TODO(lazyboy): Merge this with ScopedUMARecorder in computed_hashes.cc.
class ScopedUMARecorder {
 public:
  ScopedUMARecorder() = default;

  ~ScopedUMARecorder() {
    if (recorded_)
      return;
    RecordImpl(false);
  }

  void RecordSuccess() {
    recorded_ = true;
    RecordImpl(true);
  }

 private:
  void RecordImpl(bool success) {
    if (success) {
      UMA_HISTOGRAM_TIMES(
          "Extensions.ContentVerification.VerifiedContentsInitTime",
          timer_.Elapsed());
    }
    UMA_HISTOGRAM_BOOLEAN(
        "Extensions.ContentVerification.VerifiedContentsInitResult", success);
  }

 private:
  base::ElapsedTimer timer_;
  bool recorded_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScopedUMARecorder);
};

#if defined(OS_WIN)
// Returns true if |path| ends with (.| )+.
// |out_path| will contain "." and/or " " suffix removed from |path|.
bool TrimDotSpaceSuffix(const base::FilePath::StringType& path,
                        base::FilePath::StringType* out_path) {
  base::FilePath::StringType::size_type trim_pos =
      path.find_last_not_of(FILE_PATH_LITERAL(". "));
  if (trim_pos == base::FilePath::StringType::npos)
    return false;

  *out_path = path.substr(0, trim_pos + 1);
  return true;
}
#endif  // defined(OS_WIN)

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
std::unique_ptr<VerifiedContents> VerifiedContents::Create(
    base::span<const uint8_t> public_key,
    const base::FilePath& path) {
  ScopedUMARecorder uma_recorder;
  // Note: VerifiedContents constructor is private.
  auto verified_contents = base::WrapUnique(new VerifiedContents(public_key));
  std::string payload;
  if (!verified_contents->GetPayload(path, &payload))
    return nullptr;

  std::unique_ptr<base::Value> value(base::JSONReader::Read(payload));
  if (!value.get() || !value->is_dict())
    return nullptr;
  DictionaryValue* dictionary = static_cast<DictionaryValue*>(value.get());

  std::string item_id;
  if (!dictionary->GetString(kItemIdKey, &item_id) ||
      !crx_file::id_util::IdIsValid(item_id)) {
    return nullptr;
  }
  verified_contents->extension_id_ = item_id;

  std::string version_string;
  if (!dictionary->GetString(kItemVersionKey, &version_string))
    return nullptr;
  verified_contents->version_ = base::Version(version_string);
  if (!verified_contents->version_.IsValid())
    return nullptr;

  ListValue* hashes_list = nullptr;
  if (!dictionary->GetList(kContentHashesKey, &hashes_list))
    return nullptr;

  for (size_t i = 0; i < hashes_list->GetSize(); i++) {
    DictionaryValue* hashes = nullptr;
    if (!hashes_list->GetDictionary(i, &hashes))
      return nullptr;
    std::string format;
    if (!hashes->GetString(kFormatKey, &format) || format != kTreeHash)
      continue;

    int block_size = 0;
    int hash_block_size = 0;
    if (!hashes->GetInteger(kBlockSizeKey, &block_size) ||
        !hashes->GetInteger(kHashBlockSizeKey, &hash_block_size)) {
      return nullptr;
    }
    verified_contents->block_size_ = block_size;

    // We don't support using a different block_size and hash_block_size at
    // the moment.
    if (verified_contents->block_size_ != hash_block_size)
      return nullptr;

    ListValue* files = nullptr;
    if (!hashes->GetList(kFilesKey, &files))
      return nullptr;

    for (size_t j = 0; j < files->GetSize(); ++j) {
      DictionaryValue* data = nullptr;
      if (!files->GetDictionary(j, &data))
        return nullptr;
      std::string file_path_string;
      std::string encoded_root_hash;
      std::string root_hash;
      if (!data->GetString(kPathKey, &file_path_string) ||
          !base::IsStringUTF8(file_path_string) ||
          !data->GetString(kRootHashKey, &encoded_root_hash) ||
          !base::Base64UrlDecode(encoded_root_hash,
                                 base::Base64UrlDecodePolicy::IGNORE_PADDING,
                                 &root_hash)) {
        return nullptr;
      }
      base::FilePath file_path =
          base::FilePath::FromUTF8Unsafe(file_path_string);
      base::FilePath::StringType lowercase_file_path =
          base::ToLowerASCII(file_path.value());
      auto i = verified_contents->root_hashes_.insert(
          std::make_pair(lowercase_file_path, std::string()));
      i->second.swap(root_hash);

#if defined(OS_WIN)
      // Additionally store a canonicalized filename without (.| )+ suffix, so
      // that any filename with (.| )+ suffix can be matched later, see
      // HasTreeHashRoot() and TreeHashRootEquals().
      base::FilePath::StringType trimmed_path;
      if (TrimDotSpaceSuffix(lowercase_file_path, &trimmed_path))
        verified_contents->root_hashes_.insert(
            std::make_pair(trimmed_path, i->second));
#endif  // defined(OS_WIN)
    }

    break;
  }
  uma_recorder.RecordSuccess();
  return verified_contents;
}

bool VerifiedContents::HasTreeHashRoot(
    const base::FilePath& relative_path) const {
  base::FilePath::StringType path = base::ToLowerASCII(
      relative_path.NormalizePathSeparatorsTo('/').value());
  if (base::ContainsKey(root_hashes_, path))
    return true;

#if defined(OS_WIN)
  base::FilePath::StringType trimmed_path;
  if (TrimDotSpaceSuffix(path, &trimmed_path))
    return base::ContainsKey(root_hashes_, trimmed_path);
#endif  // defined(OS_WIN)
  return false;
}

bool VerifiedContents::TreeHashRootEquals(const base::FilePath& relative_path,
                                          const std::string& expected) const {
  base::FilePath::StringType normalized_relative_path =
      base::ToLowerASCII(relative_path.NormalizePathSeparatorsTo('/').value());
  if (TreeHashRootEqualsImpl(normalized_relative_path, expected))
    return true;

#if defined(OS_WIN)
  base::FilePath::StringType trimmed_relative_path;
  if (TrimDotSpaceSuffix(normalized_relative_path, &trimmed_relative_path))
    return TreeHashRootEqualsImpl(trimmed_relative_path, expected);
#endif  // defined(OS_WIN)
  return false;
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
bool VerifiedContents::GetPayload(const base::FilePath& path,
                                  std::string* payload) {
  std::string contents;
  if (!base::ReadFileToString(path, &contents))
    return false;
  std::unique_ptr<base::Value> value(base::JSONReader::Read(contents));
  if (!value.get() || !value->is_list())
    return false;
  ListValue* top_list = static_cast<ListValue*>(value.get());

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
  const DictionaryValue* dictionary =
      FindDictionaryWithValue(top_list, kDescriptionKey, kTreeHashPerFile);
  const DictionaryValue* signed_content = NULL;
  if (!dictionary ||
      !dictionary->GetDictionaryWithoutPathExpansion(kSignedContentKey,
                                                     &signed_content)) {
    return false;
  }

  const ListValue* signatures = NULL;
  if (!signed_content->GetList(kSignaturesKey, &signatures))
    return false;

  const DictionaryValue* signature_dict =
      FindDictionaryWithValue(signatures, kHeaderKidKey, kWebstoreKId);
  if (!signature_dict)
    return false;

  std::string protected_value;
  std::string encoded_signature;
  std::string decoded_signature;
  if (!signature_dict->GetString(kProtectedKey, &protected_value) ||
      !signature_dict->GetString(kSignatureKey, &encoded_signature) ||
      !base::Base64UrlDecode(encoded_signature,
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &decoded_signature))
    return false;

  std::string encoded_payload;
  if (!signed_content->GetString(kPayloadKey, &encoded_payload))
    return false;

  valid_signature_ =
      VerifySignature(protected_value, encoded_payload, decoded_signature);
  if (!valid_signature_)
    return false;

  if (!base::Base64UrlDecode(encoded_payload,
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

bool VerifiedContents::TreeHashRootEqualsImpl(
    const base::FilePath::StringType& normalized_relative_path,
    const std::string& expected) const {
  std::pair<RootHashes::const_iterator, RootHashes::const_iterator> hashes =
      root_hashes_.equal_range(normalized_relative_path);
  for (auto iter = hashes.first; iter != hashes.second; ++iter) {
    if (expected == iter->second)
      return true;
  }
  return false;
}

}  // namespace extensions
