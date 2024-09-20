// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/computed_hashes.h"

#include <memory>
#include <set>
#include <utility>

#include "base/base64.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

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

using SortedFilePathSet = std::set<base::FilePath>;

}  // namespace

ComputedHashes::Data::Data() = default;
ComputedHashes::Data::~Data() = default;
ComputedHashes::Data::Data(ComputedHashes::Data&& data) = default;
ComputedHashes::Data& ComputedHashes::Data::operator=(
    ComputedHashes::Data&& data) = default;

ComputedHashes::Data::HashInfo::HashInfo(int block_size,
                                         std::vector<std::string> hashes,
                                         base::FilePath relative_unix_path)
    : block_size(block_size),
      hashes(std::move(hashes)),
      relative_unix_path(std::move(relative_unix_path)) {}
ComputedHashes::Data::HashInfo::~HashInfo() = default;

ComputedHashes::Data::HashInfo::HashInfo(ComputedHashes::Data::HashInfo&&) =
    default;
ComputedHashes::Data::HashInfo& ComputedHashes::Data::HashInfo::operator=(
    ComputedHashes::Data::HashInfo&&) = default;

const ComputedHashes::Data::HashInfo* ComputedHashes::Data::GetItem(
    const base::FilePath& relative_path) const {
  CanonicalRelativePath canonical_path =
      content_verifier_utils::CanonicalizeRelativePath(relative_path);
  auto iter = items_.find(canonical_path);
  return iter == items_.end() ? nullptr : &iter->second;
}

void ComputedHashes::Data::Add(const base::FilePath& relative_path,
                               int block_size,
                               std::vector<std::string> hashes) {
  CanonicalRelativePath canonical_path =
      content_verifier_utils::CanonicalizeRelativePath(relative_path);
  items_.insert(std::make_pair(
      canonical_path, HashInfo(block_size, std::move(hashes),
                               relative_path.NormalizePathSeparatorsTo('/'))));
}

void ComputedHashes::Data::Remove(const base::FilePath& relative_path) {
  CanonicalRelativePath canonical_path =
      content_verifier_utils::CanonicalizeRelativePath(relative_path);
  items_.erase(canonical_path);
}

const std::map<CanonicalRelativePath, ComputedHashes::Data::HashInfo>&
ComputedHashes::Data::items() const {
  return items_;
}

ComputedHashes::ComputedHashes(Data&& data) : data_(std::move(data)) {}
ComputedHashes::~ComputedHashes() = default;
ComputedHashes::ComputedHashes(ComputedHashes&&) = default;
ComputedHashes& ComputedHashes::operator=(ComputedHashes&&) = default;

// static
std::optional<ComputedHashes> ComputedHashes::CreateFromFile(
    const base::FilePath& path,
    Status* status) {
  DCHECK(status);
  *status = Status::UNKNOWN;
  std::string contents;
  if (!base::ReadFileToString(path, &contents)) {
    *status = Status::READ_FAILED;
    return std::nullopt;
  }

  std::optional<base::Value> top_dictionary = base::JSONReader::Read(contents);
  base::Value::Dict* dictionary =
      top_dictionary ? top_dictionary->GetIfDict() : nullptr;
  if (!dictionary) {
    *status = Status::PARSE_FAILED;
    return std::nullopt;
  }

  // For now we don't support forwards or backwards compatibility in the
  // format, so we return nullopt on version mismatch.
  std::optional<int> version =
      dictionary->FindInt(computed_hashes::kVersionKey);
  if (!version || *version != computed_hashes::kVersion) {
    *status = Status::PARSE_FAILED;
    return std::nullopt;
  }

  const base::Value::List* all_hashes =
      dictionary->FindList(computed_hashes::kFileHashesKey);
  if (!all_hashes) {
    *status = Status::PARSE_FAILED;
    return std::nullopt;
  }

  ComputedHashes::Data data;
  for (const base::Value& file_hash : *all_hashes) {
    const base::Value::Dict* file_hash_dict = file_hash.GetIfDict();
    if (!file_hash_dict) {
      *status = Status::PARSE_FAILED;
      return std::nullopt;
    }

    const std::string* relative_path_utf8 =
        file_hash_dict->FindString(computed_hashes::kPathKey);
    if (!relative_path_utf8) {
      *status = Status::PARSE_FAILED;
      return std::nullopt;
    }

    std::optional<int> block_size =
        file_hash_dict->FindInt(computed_hashes::kBlockSizeKey);
    if (!block_size) {
      *status = Status::PARSE_FAILED;
      return std::nullopt;
    }
    if (*block_size <= 0 || ((*block_size % 1024) != 0)) {
      LOG(ERROR) << "Invalid block size: " << *block_size;
      *status = Status::PARSE_FAILED;
      return std::nullopt;
    }

    const base::Value::List* block_hashes =
        file_hash_dict->FindList(computed_hashes::kBlockHashesKey);
    if (!block_hashes) {
      *status = Status::PARSE_FAILED;
      return std::nullopt;
    }

    base::FilePath relative_path =
        base::FilePath::FromUTF8Unsafe(*relative_path_utf8);
    std::vector<std::string> hashes;

    for (const base::Value& value : *block_hashes) {
      if (!value.is_string()) {
        *status = Status::PARSE_FAILED;
        return std::nullopt;
      }

      hashes.push_back(std::string());
      const std::string& encoded = value.GetString();
      std::string* decoded = &hashes.back();
      if (!base::Base64Decode(encoded, decoded)) {
        *status = Status::PARSE_FAILED;
        return std::nullopt;
      }
    }
    data.Add(relative_path, *block_size, std::move(hashes));
  }
  *status = Status::SUCCESS;
  return ComputedHashes(std::move(data));
}

// static
std::optional<ComputedHashes::Data> ComputedHashes::Compute(
    const base::FilePath& extension_root,
    int block_size,
    const IsCancelledCallback& is_cancelled,
    const ShouldComputeHashesCallback& should_compute_hashes_for_resource) {
  base::FileEnumerator enumerator(extension_root, /*recursive=*/true,
                                  base::FileEnumerator::FILES);
  // First discover all the file paths and put them in a sorted set.
  SortedFilePathSet paths;
  while (true) {
    if (is_cancelled && is_cancelled.Run()) {
      return std::nullopt;
    }

    base::FilePath full_path = enumerator.Next();
    if (full_path.empty()) {
      break;
    }
    paths.insert(full_path);
  }

  // Now iterate over all the paths in sorted order and compute the block hashes
  // for each one.
  Data data;
  for (const auto& full_path : paths) {
    if (is_cancelled && is_cancelled.Run()) {
      return std::nullopt;
    }

    base::FilePath relative_path;
    extension_root.AppendRelativePath(full_path, &relative_path);

    if (!should_compute_hashes_for_resource.Run(relative_path)) {
      continue;
    }

    std::optional<std::vector<std::string>> hashes =
        ComputeAndCheckResourceHash(full_path, block_size);
    if (hashes) {
      data.Add(relative_path, block_size, std::move(hashes.value()));
    }
  }

  return data;
}

bool ComputedHashes::GetHashes(const base::FilePath& relative_path,
                               int* block_size,
                               std::vector<std::string>* hashes) const {
  const Data::HashInfo* hash_info = data_.GetItem(relative_path);
  if (!hash_info) {
    return false;
  }

  *block_size = hash_info->block_size;
  *hashes = hash_info->hashes;
  return true;
}

bool ComputedHashes::WriteToFile(const base::FilePath& path) const {
  // Make sure the directory exists.
  if (!base::CreateDirectoryAndGetError(path.DirName(), nullptr)) {
    return false;
  }

  base::Value::List file_list;
  for (const auto& resource_info : data_.items()) {
    const Data::HashInfo& hash_info = resource_info.second;
    int block_size = hash_info.block_size;
    const std::vector<std::string>& hashes = hash_info.hashes;

    base::Value::List block_hashes;
    block_hashes.reserve(hashes.size());
    for (const auto& hash : hashes) {
      block_hashes.Append(base::Base64Encode(hash));
    }

    base::Value::Dict dict;
    dict.Set(computed_hashes::kPathKey,
             hash_info.relative_unix_path.AsUTF8Unsafe());
    dict.Set(computed_hashes::kBlockSizeKey, block_size);
    dict.Set(computed_hashes::kBlockHashesKey, std::move(block_hashes));

    file_list.Append(std::move(dict));
  }

  std::string json;
  base::Value::Dict top_dictionary;
  top_dictionary.Set(computed_hashes::kVersionKey, computed_hashes::kVersion);
  top_dictionary.Set(computed_hashes::kFileHashesKey, std::move(file_list));

  if (!base::JSONWriter::Write(top_dictionary, &json)) {
    return false;
  }
  if (!base::WriteFile(path, json)) {
    LOG(ERROR) << "Error writing " << path.AsUTF8Unsafe();
    return false;
  }
  return true;
}

// static
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
    hash->Finish(std::data(buffer), buffer.size());
    hashes.push_back(std::move(buffer));

    // If |contents| is empty, then we want to just exit here.
    if (bytes_to_read == 0) {
      break;
    }

    offset += bytes_to_read;
  } while (offset < contents.size());

  return hashes;
}

// static
std::optional<std::vector<std::string>>
ComputedHashes::ComputeAndCheckResourceHash(const base::FilePath& full_path,
                                            int block_size) {
  std::string contents;
  if (!base::ReadFileToString(full_path, &contents)) {
    LOG(ERROR) << "Could not read " << full_path.MaybeAsASCII();
    return std::nullopt;
  }

  // Iterate through taking the hash of each block of size |block_size| of the
  // file.
  std::vector<std::string> hashes = GetHashesForContent(contents, block_size);

  return std::make_optional(std::move(hashes));
}

}  // namespace extensions
