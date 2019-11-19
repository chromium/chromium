// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_log_response_parser.h"

#include <memory>

#include "base/base64.h"
#include "base/json/json_value_converter.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/signed_tree_head.h"

namespace net {

namespace ct {

namespace {

// Structure for making JSON decoding easier. The string fields
// are base64-encoded so will require further decoding.
struct JsonSignedTreeHead {
  int tree_size;
  double timestamp;
  std::string sha256_root_hash;
  DigitallySigned signature;

  static void RegisterJSONConverter(
      base::JSONValueConverter<JsonSignedTreeHead>* converted);
};

bool ConvertSHA256RootHash(base::StringPiece s, std::string* result) {
  if (!base::Base64Decode(s, result)) {
    DVLOG(1) << "Failed decoding sha256_root_hash";
    return false;
  }

  if (result->length() != kSthRootHashLength) {
    DVLOG(1) << "sha256_root_hash is expected to be 32 bytes, but is "
             << result->length() << " bytes.";
    return false;
  }

  return true;
}

bool ConvertTreeHeadSignature(base::StringPiece s, DigitallySigned* result) {
  std::string tree_head_signature;
  if (!base::Base64Decode(s, &tree_head_signature)) {
    DVLOG(1) << "Failed decoding tree_head_signature";
    return false;
  }

  base::StringPiece sp(tree_head_signature);
  if (!DecodeDigitallySigned(&sp, result)) {
    DVLOG(1) << "Failed decoding signature to DigitallySigned";
    return false;
  }
  return true;
}

void JsonSignedTreeHead::RegisterJSONConverter(
    base::JSONValueConverter<JsonSignedTreeHead>* converter) {
  converter->RegisterIntField("tree_size", &JsonSignedTreeHead::tree_size);
  converter->RegisterDoubleField("timestamp", &JsonSignedTreeHead::timestamp);
  converter->RegisterCustomField("sha256_root_hash",
                                 &JsonSignedTreeHead::sha256_root_hash,
                                 &ConvertSHA256RootHash);
  converter->RegisterCustomField<DigitallySigned>(
      "tree_head_signature",
      &JsonSignedTreeHead::signature,
      &ConvertTreeHeadSignature);
}

bool IsJsonSTHStructurallyValid(const JsonSignedTreeHead& sth) {
  if (sth.tree_size < 0) {
    DVLOG(1) << "Tree size in Signed Tree Head JSON is negative: "
             << sth.tree_size;
    return false;
  }

  if (sth.timestamp < 0) {
    DVLOG(1) << "Timestamp in Signed Tree Head JSON is negative: "
             << sth.timestamp;
    return false;
  }

  if (sth.sha256_root_hash.empty()) {
    DVLOG(1) << "Missing SHA256 root hash from Signed Tree Head JSON.";
    return false;
  }

  if (sth.signature.signature_data.empty()) {
    DVLOG(1) << "Missing signature from Signed Tree Head JSON.";
    return false;
  }

  return true;
}

// Structure for making JSON decoding easier. The string fields
// are base64-encoded so will require further decoding.
struct JsonConsistencyProof {
  std::vector<std::unique_ptr<std::string>> proof_nodes;

  static void RegisterJSONConverter(
      base::JSONValueConverter<JsonConsistencyProof>* converter);
};

bool ConvertIndividualProofNode(const base::Value* value, std::string* result) {
  std::string b64_encoded_node;
  if (!value->GetAsString(&b64_encoded_node))
    return false;

  if (!ConvertSHA256RootHash(b64_encoded_node, result))
    return false;

  return true;
}

void JsonConsistencyProof::RegisterJSONConverter(
    base::JSONValueConverter<JsonConsistencyProof>* converter) {
  converter->RegisterRepeatedCustomValue<std::string>(
      "consistency", &JsonConsistencyProof::proof_nodes,
      &ConvertIndividualProofNode);
}

}  // namespace

bool FillSignedTreeHead(const base::Value& json_signed_tree_head,
                        SignedTreeHead* signed_tree_head) {
  JsonSignedTreeHead parsed_sth;
  base::JSONValueConverter<JsonSignedTreeHead> converter;
  if (!converter.Convert(json_signed_tree_head, &parsed_sth)) {
    DVLOG(1) << "Invalid Signed Tree Head JSON.";
    return false;
  }

  if (!IsJsonSTHStructurallyValid(parsed_sth))
    return false;

  signed_tree_head->version = SignedTreeHead::V1;
  signed_tree_head->tree_size = parsed_sth.tree_size;
  signed_tree_head->timestamp = base::Time::FromJsTime(parsed_sth.timestamp);
  signed_tree_head->signature = parsed_sth.signature;
  memcpy(signed_tree_head->sha256_root_hash,
         parsed_sth.sha256_root_hash.c_str(),
         kSthRootHashLength);
  return true;
}

bool FillConsistencyProof(const base::Value& json_consistency_proof,
                          std::vector<std::string>* consistency_proof) {
  JsonConsistencyProof parsed_proof;
  base::JSONValueConverter<JsonConsistencyProof> converter;
  if (!converter.Convert(json_consistency_proof, &parsed_proof)) {
    DVLOG(1) << "Invalid consistency proof.";
    return false;
  }

  const base::DictionaryValue* dict_value = nullptr;
  if (!json_consistency_proof.GetAsDictionary(&dict_value) ||
      !dict_value->HasKey("consistency")) {
    DVLOG(1) << "Missing consistency field.";
    return false;
  }

  consistency_proof->reserve(parsed_proof.proof_nodes.size());
  for (const auto& proof_node : parsed_proof.proof_nodes) {
    consistency_proof->push_back(*proof_node);
  }

  return true;
}

}  // namespace ct

}  // namespace net
