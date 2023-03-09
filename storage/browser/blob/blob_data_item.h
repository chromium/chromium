// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_DATA_ITEM_H_
#define STORAGE_BROWSER_BLOB_BLOB_DATA_ITEM_H_

#include <stdint.h>

#include <memory>
#include <ostream>
#include <string>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "net/base/io_buffer.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

namespace storage {
class BlobDataBuilder;
class BlobStorageContext;
class FileSystemContext;

// Ref counted blob item. This class owns the backing data of the blob item. The
// backing data is immutable, and cannot change after creation. The purpose of
// this class is to allow the resource to stick around in the snapshot even
// after the resource was swapped in the blob (either to disk or to memory) by
// the BlobStorageContext.
class COMPONENT_EXPORT(STORAGE_BROWSER) BlobDataItem
    : public base::RefCounted<BlobDataItem> {
 public:
  enum class Type {
    kBytes,
    kBytesDescription,
    kFile,
    kFileFilesystem,
    kReadableDataHandle,
  };

  // The DataHandle class is used to persist an interface and resources for
  // reading this BlobDataItem. This object will stay around while any reads are
  // pending. If all blobs with this item are deleted or the item is swapped for
  // a different backend version (mem-to-disk or the reverse), then the item
  // will be destructed after all pending reads are complete.
  class COMPONENT_EXPORT(STORAGE_BROWSER) DataHandle
      : public base::RefCounted<DataHandle> {
   public:
    // Returns the size of the main blob data.
    virtual uint64_t GetSize() const = 0;

    // Reads the given data range into the given |producer|.
    // Returns the net::Error from the read operation to the callback.
    virtual void Read(mojo::ScopedDataPipeProducerHandle producer,
                      uint64_t src_offset,
                      uint64_t bytes_to_read,
                      base::OnceCallback<void(int)> callback) = 0;

    // Returns the side data size.  If there is no side data, then 0 should
    // be returned.
    virtual uint64_t GetSideDataSize() const = 0;

    // Returns the entire side data as a BigBuffer and the net::Error from
    // reading.  The number of bytes read is the size of the BigBuffer.
    virtual void ReadSideData(
        base::OnceCallback<void(int, mojo_base::BigBuffer)> callback) = 0;

    // Print a description of the readable DataHandle for debugging.
    virtual void PrintTo(::std::ostream* os) const = 0;

   protected:
    virtual ~DataHandle();

   private:
    friend class base::RefCounted<DataHandle>;
  };

  static scoped_refptr<BlobDataItem> CreateBytes(
      base::span<const uint8_t> bytes);
  static scoped_refptr<BlobDataItem> CreateBytesDescription(size_t length);
  static scoped_refptr<BlobDataItem> CreateFile(
      base::FilePath path,
      file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
          file_access = base::NullCallback());
  static scoped_refptr<BlobDataItem> CreateFile(
      base::FilePath path,
      uint64_t offset,
      uint64_t length,
      base::Time expected_modification_time = base::Time(),
      scoped_refptr<ShareableFileReference> file_ref = nullptr,
      file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
          file_access = base::NullCallback());
  static scoped_refptr<BlobDataItem> CreateFutureFile(uint64_t offset,
                                                      uint64_t length,
                                                      uint64_t file_id);
  static scoped_refptr<BlobDataItem> CreateFileFilesystem(
      const FileSystemURL& url,
      uint64_t offset,
      uint64_t length,
      base::Time expected_modification_time,
      scoped_refptr<FileSystemContext> file_system_context,
      file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
          file_access = base::NullCallback());
  static scoped_refptr<BlobDataItem> CreateReadableDataHandle(
      scoped_refptr<DataHandle> data_handle,
      uint64_t offset,
      uint64_t length);
  static scoped_refptr<BlobDataItem> CreateMojoDataItem(
      mojom::BlobDataItemPtr item);

  Type type() const { return type_; }
  uint64_t offset() const { return offset_; }
  uint64_t length() const { return length_; }

  base::span<const uint8_t> bytes() const {
    DCHECK_EQ(type_, Type::kBytes);
    return base::make_span(bytes_);
  }

  const base::FilePath& path() const {
    DCHECK_EQ(type_, Type::kFile);
    return path_;
  }

  const FileSystemURL& filesystem_url() const {
    DCHECK_EQ(type_, Type::kFileFilesystem);
    return filesystem_url_;
  }

  FileSystemContext* file_system_context() const {
    DCHECK_EQ(type_, Type::kFileFilesystem);
    return file_system_context_.get();
  }

  const base::Time& expected_modification_time() const {
    DCHECK(type_ == Type::kFile || type_ == Type::kFileFilesystem)
        << static_cast<int>(type_);
    return expected_modification_time_;
  }

  DataHandle* data_handle() const {
    DCHECK_EQ(type_, Type::kReadableDataHandle) << static_cast<int>(type_);
    return data_handle_.get();
  }

  // Returns true if this item was created by CreateFutureFile.
  bool IsFutureFileItem() const;
  // Returns |file_id| given to CreateFutureFile.
  uint64_t GetFutureFileID() const;

  file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
  file_access() const {
    DCHECK(type_ == Type::kFile || type_ == Type::kFileFilesystem);
    return file_access_;
  }

 private:
  friend class BlobBuilderFromStream;
  friend class BlobDataBuilder;
  friend class BlobStorageContext;
  friend class base::RefCounted<BlobDataItem>;
  friend COMPONENT_EXPORT(STORAGE_BROWSER) void PrintTo(const BlobDataItem& x,
                                                        ::std::ostream* os);

  BlobDataItem(Type type, uint64_t offset, uint64_t length);
  virtual ~BlobDataItem();

  base::span<uint8_t> mutable_bytes() {
    DCHECK_EQ(type_, Type::kBytes);
    return base::make_span(bytes_);
  }

  void AllocateBytes();
  void PopulateBytes(base::span<const uint8_t> data);
  void ShrinkBytes(size_t new_length);

  void PopulateFile(base::FilePath path,
                    base::Time expected_modification_time,
                    scoped_refptr<ShareableFileReference> file_ref);
  void ShrinkFile(uint64_t new_length);
  void GrowFile(uint64_t new_length);
  void SetFileModificationTime(base::Time time) {
    DCHECK_EQ(type_, Type::kFile);
    expected_modification_time_ = time;
  }

  static void SetFileModificationTimes(
      std::vector<scoped_refptr<BlobDataItem>> items,
      std::vector<base::Time> times);

  Type type_;
  uint64_t offset_;
  uint64_t length_;

  std::vector<uint8_t> bytes_;    // For Type::kBytes.
  base::FilePath path_;           // For Type::kFile.
  FileSystemURL filesystem_url_;  // For Type::kFileFilesystem.
  base::Time
      expected_modification_time_;  // For Type::kFile and kFileFilesystem.

  scoped_refptr<DataHandle> data_handle_;           // For kReadableDataHandle.
  scoped_refptr<ShareableFileReference> file_ref_;  // For Type::kFile

  scoped_refptr<FileSystemContext>
      file_system_context_;  // For Type::kFileFilesystem.

  file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
      file_access_;  // For Type::kFile and kFileFilesystem.
};

COMPONENT_EXPORT(STORAGE_BROWSER)
bool operator==(const BlobDataItem& a, const BlobDataItem& b);
COMPONENT_EXPORT(STORAGE_BROWSER)
bool operator!=(const BlobDataItem& a, const BlobDataItem& b);

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_DATA_ITEM_H_
