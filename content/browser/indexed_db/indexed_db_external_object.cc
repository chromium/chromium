// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_external_object.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content::indexed_db {

const int64_t IndexedDBExternalObject::kUnknownSize;

// static
void IndexedDBExternalObject::ConvertToMojo(
    const std::vector<IndexedDBExternalObject>& objects,
    std::vector<blink::mojom::IDBExternalObjectPtr>* mojo_objects) {
  mojo_objects->reserve(objects.size());
  for (const auto& iter : objects) {
    if (!iter.mark_used_callback().is_null())
      iter.mark_used_callback().Run();

    switch (iter.object_type()) {
      case ObjectType::kBlob:
      case ObjectType::kFile: {
        auto info = blink::mojom::IDBBlobInfo::New();
        info->mime_type = iter.type();
        info->size = iter.size();
        if (iter.object_type() == ObjectType::kFile) {
          info->file = blink::mojom::IDBFileInfo::New();
          info->file->name = iter.file_name();
          info->file->last_modified = iter.last_modified();
        }
        mojo_objects->push_back(
            blink::mojom::IDBExternalObject::NewBlobOrFile(std::move(info)));
        break;
      }
      case ObjectType::kFileSystemAccessHandle:
        // Contents of token will be filled in later by
        // BucketContext::CreateAllExternalObjects.
        mojo_objects->push_back(
            blink::mojom::IDBExternalObject::NewFileSystemAccessToken(
                mojo::NullRemote()));
        break;
    }
  }
}

IndexedDBExternalObject::IndexedDBExternalObject()
    : object_type_(ObjectType::kBlob) {}

IndexedDBExternalObject::IndexedDBExternalObject(
    mojo::PendingRemote<blink::mojom::Blob> blob_remote,
    const std::u16string& type,
    int64_t size)
    : object_type_(ObjectType::kBlob),
      blob_remote_(std::move(blob_remote)),
      type_(type),
      size_(size) {}

IndexedDBExternalObject::IndexedDBExternalObject(const std::u16string& type,
                                                 int64_t size,
                                                 int64_t blob_number)
    : object_type_(ObjectType::kBlob),
      type_(type),
      size_(size),
      blob_number_(blob_number) {}

IndexedDBExternalObject::IndexedDBExternalObject(
    mojo::PendingRemote<blink::mojom::Blob> blob_remote,
    const std::u16string& file_name,
    const std::u16string& type,
    const base::Time& last_modified,
    const int64_t size)
    : object_type_(ObjectType::kFile),
      blob_remote_(std::move(blob_remote)),
      type_(type),
      size_(size),
      file_name_(file_name),
      last_modified_(last_modified) {}

IndexedDBExternalObject::IndexedDBExternalObject(
    int64_t blob_number,
    const std::u16string& type,
    const std::u16string& file_name,
    const base::Time& last_modified,
    const int64_t size)
    : object_type_(ObjectType::kFile),
      type_(type),
      size_(size),
      file_name_(file_name),
      last_modified_(last_modified),
      blob_number_(blob_number) {}

IndexedDBExternalObject::IndexedDBExternalObject(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
        token_remote)
    : object_type_(ObjectType::kFileSystemAccessHandle),
      token_remote_(std::move(token_remote)) {}

IndexedDBExternalObject::IndexedDBExternalObject(
    std::vector<uint8_t> serialized_file_system_access_handle)
    : object_type_(ObjectType::kFileSystemAccessHandle),
      serialized_file_system_access_handle_(
          std::move(serialized_file_system_access_handle)) {}

IndexedDBExternalObject::IndexedDBExternalObject(
    const IndexedDBExternalObject& other) = default;

IndexedDBExternalObject::~IndexedDBExternalObject() = default;

IndexedDBExternalObject& IndexedDBExternalObject::operator=(
    const IndexedDBExternalObject& other) = default;

void IndexedDBExternalObject::Clone(
    mojo::PendingReceiver<blink::mojom::Blob> receiver) const {
  DCHECK(is_remote_valid());
  blob_remote_->Clone(std::move(receiver));
}

void IndexedDBExternalObject::set_size(int64_t size) {
  DCHECK_EQ(-1, size_);
  size_ = size;
}

void IndexedDBExternalObject::set_indexed_db_file_path(
    const base::FilePath& file_path) {
  indexed_db_file_path_ = file_path;
}

void IndexedDBExternalObject::set_last_modified(const base::Time& time) {
  DCHECK(base::Time().is_null());
  DCHECK_EQ(object_type_, ObjectType::kFile);
  last_modified_ = time;
}

void IndexedDBExternalObject::set_serialized_file_system_access_handle(
    std::vector<uint8_t> token) {
  DCHECK_EQ(object_type_, ObjectType::kFileSystemAccessHandle);
  serialized_file_system_access_handle_ = std::move(token);
}

void IndexedDBExternalObject::set_blob_number(int64_t blob_number) {
  DCHECK_EQ(DatabaseMetaDataKey::kInvalidBlobNumber, blob_number_);
  blob_number_ = blob_number;
}

void IndexedDBExternalObject::set_mark_used_callback(
    base::RepeatingClosure mark_used_callback) {
  DCHECK(!mark_used_callback_);
  mark_used_callback_ = std::move(mark_used_callback);
}

void IndexedDBExternalObject::set_release_callback(
    base::RepeatingClosure release_callback) {
  DCHECK(!release_callback_);
  release_callback_ = std::move(release_callback);
}

}  // namespace content::indexed_db
