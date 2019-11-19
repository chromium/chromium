// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/computed_hashes.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "extensions/browser/content_verifier/content_verifier_utils.h"
#include "extensions/browser/content_verifier/scoped_uma_recorder.h"

namespace extensions {

namespace computed_hashes {
const char kBlockHashesKey[] = "block_hashes";
const char kBlockSizeKey[] = "block_size";
const char kFileHashesKey[] = "file_hashes";
const char kPathKey[] = "path";
const char kVersionKey[] = "version";
const int kVersion = 2;
}  // namespace computed_hashes

namespace {

const char kUMAComputedHashesReadResult[] =
    "Extensions.ContentVerification.ComputedHashesReadResult";
const char kUMAComputedHashesInitTime[] =
    "Extensions.ContentVerification.ComputedHashesInitTime";

}  // namespace

ComputedHashes::Reader::Reader() {
}

ComputedHashes::Reader::~Reader() {
}

bool ComputedHashes::Reader::InitFromFile(const base::FilePath& path) {
  ScopedUMARecorder<kUMAComputedHashesReadResult, kUMAComputedHashesInitTime>
      uma_recorder;
  std::string contents;
  if (!base::ReadFileToString(path, &contents))
    return false;

  base::Optional<base::Value> top_dictionary = base::JSONReader::Read(contents);
  if (!top_dictionary || !top_dictionary->is_dict())
    return false;

  // For now we don't support forwards or backwards compatability in the
  // format, so we return false on version mismatch.
  base::Optional<int> version =
      top_dictionary->FindIntKey(computed_hashes::kVersionKey);
  if (!version || *version != computed_hashes::kVersion)
    return false;

  const base::Value* all_hashes =
      top_dictionary->FindListKey(computed_hashes::kFileHashesKey);
  if (!all_hashes)
    return false;

  for (const base::Value& file_hash : all_hashes->GetList()) {
    if (!file_hash.is_dict())
      return false;

    const std::string* relative_path_utf8 =
        file_hash.FindStringKey(computed_hashes::kPathKey);
    if (!relative_path_utf8)
      return false;

    base::Optional<int> block_size =
        file_hash.FindIntKey(computed_hashes::kBlockSizeKey);
    if (!block_size)
      return false;
    if (*block_size <= 0 || ((*block_size % 1024) != 0)) {
      LOG(ERROR) << "Invalid block size: " << *block_size;
      return false;
    }

    const base::Value* block_hashes =
        file_hash.FindListKey(computed_hashes::kBlockHashesKey);
    if (!block_hashes)
      return false;

    base::span<const base::Value> hashes_list = block_hashes->GetList();

    base::FilePath relative_path =
        base::FilePath::FromUTF8Unsafe(*relative_path_utf8);
    relative_path = relative_path.NormalizePathSeparatorsTo('/');

    data_[relative_path] = HashInfo(*block_size, std::vector<std::string>());
    std::vector<std::string>* hashes = &(data_[relative_path].second);

    for (const base::Value& value : hashes_list) {
      if (!value.is_string())
        return false;

      hashes->push_back(std::string());
      const std::string& encoded = value.GetString();
      std::string* decoded = &hashes->back();
      if (!base::Base64Decode(encoded, decoded)) {
        hashes->clear();
        return false;
      }
    }
  }
  uma_recorder.RecordSuccess();
  return true;
}

bool ComputedHashes::Reader::GetHashes(const base::FilePath& relative_path,
                                       int* block_size,
                                       std::vector<std::string>* hashes) const {
  base::FilePath path = relative_path.NormalizePathSeparatorsTo('/');
  auto find_data = [&](const base::FilePath& normalized_path) {
    auto i = data_.find(normalized_path);
    if (i == data_.end()) {
      // If we didn't find the entry using exact match, it's possible the
      // developer is using a path with some letters in the incorrect case,
      // which happens to work on windows/osx. So try doing a linear scan to
      // look for a case-insensitive match. In practice most extensions don't
      // have that big a list of files so the performance penalty is probably
      // not too big here. Also for crbug.com/29941 we plan to start warning
      // developers when they are making this mistake, since their extension
      // will be broken on linux/chromeos.
      for (i = data_.begin(); i != data_.end(); ++i) {
        const base::FilePath& entry = i->first;
        if (base::FilePath::CompareEqualIgnoreCase(entry.value(),
                                                   normalized_path.value())) {
          break;
        }
      }
    }
    return i;
  };
  auto i = find_data(path);
#if defined(OS_WIN)
  if (i == data_.end()) {
    base::FilePath::StringType trimmed_path_value;
    // Also search for path with (.| )+ suffix trimmed as they are ignored in
    // windows. This matches the canonicalization behavior of
    // VerifiedContents::Create.
    if (content_verifier_utils::TrimDotSpaceSuffix(path.value(),
                                                   &trimmed_path_value)) {
      i = find_data(base::FilePath(trimmed_path_value));
    }
  }
#endif  // defined(OS_WIN)
  if (i == data_.end())
    return false;

  const HashInfo& info = i->second;
  *block_size = info.first;
  *hashes = info.second;
  return true;
}

ComputedHashes::Writer::Writer() : file_list_(new base::ListValue) {
}

ComputedHashes::Writer::~Writer() {
}

void ComputedHashes::Writer::AddHashes(const base::FilePath& relative_path,
                                       int block_size,
                                       const std::vector<std::string>& hashes) {
  auto block_hashes = std::make_unique<base::ListValue>();
  block_hashes->GetList().reserve(hashes.size());
  for (const auto& hash : hashes) {
    std::string encoded;
    base::Base64Encode(hash, &encoded);
    block_hashes->Append(std::move(encoded));
  }

  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetString(computed_hashes::kPathKey,
                  relative_path.NormalizePathSeparatorsTo('/').AsUTF8Unsafe());
  dict->SetInteger(computed_hashes::kBlockSizeKey, block_size);
  dict->Set(computed_hashes::kBlockHashesKey, std::move(block_hashes));
  file_list_->Append(std::move(dict));
}

bool ComputedHashes::Writer::WriteToFile(const base::FilePath& path) {
  std::string json;
  base::DictionaryValue top_dictionary;
  top_dictionary.SetInteger(computed_hashes::kVersionKey,
                            computed_hashes::kVersion);
  top_dictionary.Set(computed_hashes::kFileHashesKey, std::move(file_list_));

  if (!base::JSONWriter::Write(top_dictionary, &json))
    return false;
  int written = base::WriteFile(path, json.data(), json.size());
  if (static_cast<unsigned>(written) != json.size()) {
    LOG(ERROR) << "Error writing " << path.AsUTF8Unsafe()
               << " ; write result:" << written << " expected:" << json.size();
    return false;
  }
  return true;
}

std::vector<std::string> ComputedHashes::GetHashesForContent(
    const std::string& contents,
    size_t block_size) {
  size_t offset = 0;
  std::vector<std::string> hashes;
  // Even when the contents is empty, we want to output at least one hash
  // block (the hash of the empty string).
  do {
    const char* block_start = contents.data() + offset;
    DCHECK(offset <= contents.size());
    size_t bytes_to_read = std::min(contents.size() - offset, block_size);
    std::unique_ptr<crypto::SecureHash> hash(
        crypto::SecureHash::Create(crypto::SecureHash::SHA256));
    hash->Update(block_start, bytes_to_read);

    std::string buffer;
    buffer.resize(crypto::kSHA256Length);
    hash->Finish(base::data(buffer), buffer.size());
    hashes.push_back(std::move(buffer));

    // If |contents| is empty, then we want to just exit here.
    if (bytes_to_read == 0)
      break;

    offset += bytes_to_read;
  } while (offset < contents.size());

  return hashes;
}

}  // namespace extensions
