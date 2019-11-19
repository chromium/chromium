// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_blob_info.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {

// static
void IndexedDBBlobInfo::ConvertBlobInfo(
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<blink::mojom::IDBBlobInfoPtr>* blob_or_file_info) {
  blob_or_file_info->reserve(blob_info.size());
  for (const auto& iter : blob_info) {
    if (!iter.mark_used_callback().is_null())
      iter.mark_used_callback().Run();

    auto info = blink::mojom::IDBBlobInfo::New();
    info->mime_type = iter.type();
    info->size = iter.size();
    if (iter.is_file()) {
      info->file = blink::mojom::IDBFileInfo::New();
      info->file->name = iter.file_name();
      info->file->path = iter.file_path();
      info->file->last_modified = iter.last_modified();
    }
    blob_or_file_info->push_back(std::move(info));
  }
}

IndexedDBBlobInfo::IndexedDBBlobInfo()
    : is_file_(false), size_(-1), key_(DatabaseMetaDataKey::kInvalidBlobKey) {
}

IndexedDBBlobInfo::IndexedDBBlobInfo(
    std::unique_ptr<storage::BlobDataHandle> blob_handle,
    const base::string16& type,
    int64_t size)
    : is_file_(false),
      blob_handle_(*blob_handle),
      type_(type),
      size_(size),
      key_(DatabaseMetaDataKey::kInvalidBlobKey) {}

IndexedDBBlobInfo::IndexedDBBlobInfo(const base::string16& type,
                                     int64_t size,
                                     int64_t key)
    : is_file_(false), type_(type), size_(size), key_(key) {}

IndexedDBBlobInfo::IndexedDBBlobInfo(
    std::unique_ptr<storage::BlobDataHandle> blob_handle,
    const base::FilePath& file_path,
    const base::string16& file_name,
    const base::string16& type)
    : is_file_(true),
      blob_handle_(*blob_handle),
      type_(type),
      size_(-1),
      file_name_(file_name),
      file_path_(file_path),
      key_(DatabaseMetaDataKey::kInvalidBlobKey) {}

IndexedDBBlobInfo::IndexedDBBlobInfo(int64_t key,
                                     const base::string16& type,
                                     const base::string16& file_name)
    : is_file_(true),
      type_(type),
      size_(-1),
      file_name_(file_name),
      key_(key) {}

IndexedDBBlobInfo::IndexedDBBlobInfo(const IndexedDBBlobInfo& other) = default;

IndexedDBBlobInfo::~IndexedDBBlobInfo() = default;

IndexedDBBlobInfo& IndexedDBBlobInfo::operator=(
    const IndexedDBBlobInfo& other) = default;

void IndexedDBBlobInfo::set_size(int64_t size) {
  DCHECK_EQ(-1, size_);
  size_ = size;
}

void IndexedDBBlobInfo::set_file_path(const base::FilePath& file_path) {
  DCHECK(file_path_.empty());
  file_path_ = file_path;
}

void IndexedDBBlobInfo::set_last_modified(const base::Time& time) {
  DCHECK(base::Time().is_null());
  DCHECK(is_file_);
  last_modified_ = time;
}

void IndexedDBBlobInfo::set_key(int64_t key) {
  DCHECK_EQ(DatabaseMetaDataKey::kInvalidBlobKey, key_);
  key_ = key;
}

void IndexedDBBlobInfo::set_mark_used_callback(
    const base::Closure& mark_used_callback) {
  DCHECK(mark_used_callback_.is_null());
  mark_used_callback_ = mark_used_callback;
}

void IndexedDBBlobInfo::set_release_callback(
    const ReleaseCallback& release_callback) {
  DCHECK(release_callback_.is_null());
  release_callback_ = release_callback;
}

}  // namespace content
