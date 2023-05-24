// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_data_builder.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "storage/browser/blob/blob_entry.h"
#include "storage/browser/blob/blob_storage_registry.h"
#include "storage/browser/blob/shareable_blob_data_item.h"
#include "third_party/blink/public/common/blob/blob_utils.h"

using base::FilePath;

namespace storage {

BlobDataBuilder::FutureData::FutureData(FutureData&&) = default;
BlobDataBuilder::FutureData& BlobDataBuilder::FutureData::operator=(
    FutureData&&) = default;
BlobDataBuilder::FutureData::~FutureData() = default;

bool BlobDataBuilder::FutureData::Populate(base::span<const uint8_t> data,
                                           size_t offset) const {
  DCHECK(data.data());
  base::span<uint8_t> target = GetDataToPopulate(offset, data.size());
  if (!target.data())
    return false;
  DCHECK_EQ(target.size(), data.size());
  std::memcpy(target.data(), data.data(), data.size());
  return true;
}

base::span<uint8_t> BlobDataBuilder::FutureData::GetDataToPopulate(
    size_t offset,
    size_t length) const {
  // We lazily allocate our data buffer by waiting until the first
  // PopulateFutureData call.
  // Why? The reason we have the AppendFutureData  method is to create our Blob
  // record when the Renderer tells us about the blob without actually
  // allocating the memory yet, as we might not have the quota yet. So we don't
  // want to allocate the memory until we're actually receiving the data (which
  // the browser process only does when it has quota).
  if (item_->type() == BlobDataItem::Type::kBytesDescription) {
    item_->AllocateBytes();
  }
  DCHECK_EQ(item_->type(), BlobDataItem::Type::kBytes);
  base::CheckedNumeric<size_t> checked_end = offset;
  checked_end += length;
  if (!checked_end.IsValid() || checked_end.ValueOrDie() > item_->length()) {
    DVLOG(1) << "Invalid offset or length.";
    return base::span<uint8_t>();
  }
  return item_->mutable_bytes().subspan(offset, length);
}

BlobDataBuilder::FutureData::FutureData(scoped_refptr<BlobDataItem> item)
    : item_(item) {}

BlobDataBuilder::FutureFile::FutureFile(FutureFile&&) = default;
BlobDataBuilder::FutureFile& BlobDataBuilder::FutureFile::operator=(
    FutureFile&&) = default;
BlobDataBuilder::FutureFile::~FutureFile() = default;

bool BlobDataBuilder::FutureFile::Populate(
    scoped_refptr<ShareableFileReference> file_reference,
    const base::Time& expected_modification_time) {
  if (!item_) {
    DVLOG(1) << "File item already populated";
    return false;
  }
  DCHECK_EQ(item_->type(), BlobDataItem::Type::kFile);
  item_->PopulateFile(file_reference->path(), expected_modification_time,
                      file_reference);
  item_ = nullptr;
  return true;
}

BlobDataBuilder::FutureFile::FutureFile(scoped_refptr<BlobDataItem> item)
    : item_(item) {}

BlobDataBuilder::BlobDataBuilder(const std::string& uuid) : uuid_(uuid) {}
BlobDataBuilder::~BlobDataBuilder() = default;

void BlobDataBuilder::AppendData(base::span<const uint8_t> data) {
  if (!data.size())
    return;
  auto item = BlobDataItem::CreateBytes(data);
  auto shareable_item = base::MakeRefCounted<ShareableBlobDataItem>(
      std::move(item), ShareableBlobDataItem::QUOTA_NEEDED);
  // Even though we already prepopulate this data, we treat it as needing
  // transport anyway since we do need to allocate memory quota.
  pending_transport_items_.push_back(shareable_item);
  items_.push_back(std::move(shareable_item));

  total_size_ += data.size();
  total_memory_size_ += data.size();
  transport_quota_needed_ += data.size();
  found_memory_transport_ = true;
}

BlobDataBuilder::FutureData BlobDataBuilder::AppendFutureData(size_t length) {
  CHECK_NE(length, 0u);
  auto item = BlobDataItem::CreateBytesDescription(length);
  auto shareable_item = base::MakeRefCounted<ShareableBlobDataItem>(
      item, ShareableBlobDataItem::QUOTA_NEEDED);
  pending_transport_items_.push_back(shareable_item);
  items_.push_back(std::move(shareable_item));

  total_size_ += length;
  total_memory_size_ += length;
  transport_quota_needed_ += length;
  found_memory_transport_ = true;

  return FutureData(std::move(item));
}

BlobDataBuilder::FutureFile BlobDataBuilder::AppendFutureFile(
    uint64_t offset,
    uint64_t length,
    uint64_t file_id) {
  CHECK_NE(length, 0ull);
  DCHECK_NE(length, blink::BlobUtils::kUnknownSize);
  auto item = BlobDataItem::CreateFutureFile(offset, length, file_id);

  auto shareable_item = base::MakeRefCounted<ShareableBlobDataItem>(
      item, ShareableBlobDataItem::QUOTA_NEEDED);
  pending_transport_items_.push_back(shareable_item);
  items_.push_back(std::move(shareable_item));

  total_size_ += length;
  transport_quota_needed_ += length;
  found_file_transport_ = true;

  return FutureFile(std::move(item));
}

void BlobDataBuilder::AppendFile(
    const FilePath& file_path,
    uint64_t offset,
    uint64_t length,
    const base::Time& expected_modification_time,
    file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
        file_access) {
  auto item = BlobDataItem::CreateFile(
      file_path, offset, length, expected_modification_time,
      ShareableFileReference::Get(file_path), std::move(file_access));
  DCHECK(!item->IsFutureFileItem()) << file_path.value();

  auto shareable_item = base::MakeRefCounted<ShareableBlobDataItem>(
      std::move(item), ShareableBlobDataItem::POPULATED_WITHOUT_QUOTA);
  items_.push_back(std::move(shareable_item));

  total_size_ += length;
}

void BlobDataBuilder::AppendBlob(const std::string& uuid,
                                 uint64_t offset,
                                 uint64_t length,
                                 const BlobStorageRegistry& blob_registry) {
  DCHECK_GT(length, 0ul);
  const BlobEntry* ref_entry = blob_registry.GetEntry(uuid);

  // Self-references or non-existing blob references are invalid.
  if (!ref_entry || uuid == uuid_) {
    has_blob_errors_ = true;
    return;
  }

  dependent_blob_uuids_.insert(uuid);

  if (BlobStatusIsError(ref_entry->status())) {
    has_blob_errors_ = true;
    return;
  }

  // If we're referencing a blob with unknown size, the caller needs to provide
  // an explicit length for how much of the blob to reference.
  if (ref_entry->total_size() == blink::BlobUtils::kUnknownSize &&
      length == blink::BlobUtils::kUnknownSize) {
    has_blob_errors_ = true;
    return;
  }

  if (length == blink::BlobUtils::kUnknownSize) {
    length = ref_entry->total_size() - offset;
  }

  total_size_ += length;

  // If we're referencing the whole blob, then we don't need to slice.
  if (offset == 0 && length == ref_entry->total_size()) {
    for (const auto& shareable_item : ref_entry->items()) {
      if (shareable_item->item()->type() == BlobDataItem::Type::kBytes ||
          shareable_item->item()->type() ==
              BlobDataItem::Type::kBytesDescription) {
        total_memory_size_ += shareable_item->item()->length();
      }
      items_.push_back(shareable_item);
    }
    return;
  }

  // Validate our reference has good offset & length.
  uint64_t end_byte;
  if (!base::CheckAdd(offset, length).AssignIfValid(&end_byte) ||
      end_byte > ref_entry->total_size()) {
    has_blob_errors_ = true;
    return;
  }

  // If `ref_entry` is a blob with unknown size it must always consist of a
  // single file item, and as such we can just add a reference to the same file.
  if (ref_entry->total_size() == blink::BlobUtils::kUnknownSize) {
    CHECK_EQ(ref_entry->items().size(), 1u);
    const scoped_refptr<BlobDataItem>& source_item =
        ref_entry->items()[0]->item();
    CHECK_EQ(source_item->type(), BlobDataItem::Type::kFile);
    CHECK(!source_item->IsFutureFileItem());

    items_.push_back(base::MakeRefCounted<ShareableBlobDataItem>(
        BlobDataItem::CreateFile(
            source_item->path(), source_item->offset() + offset, length,
            source_item->expected_modification_time(), source_item->file_ref_,
            source_item->file_access_),
        ShareableBlobDataItem::POPULATED_WITHOUT_QUOTA));
    return;
  }

  SliceBlob(ref_entry, offset, length);
}

void BlobDataBuilder::SliceBlob(const BlobEntry* source,
                                uint64_t slice_offset,
                                uint64_t slice_size) {
  const auto& source_items = source->items();
  const auto& offsets = source->offsets();
  DCHECK_LE(slice_offset + slice_size, source->total_size());
  size_t item_index =
      std::upper_bound(offsets.begin(), offsets.end(), slice_offset) -
      offsets.begin();
  uint64_t item_offset =
      item_index == 0 ? slice_offset : slice_offset - offsets[item_index - 1];
  size_t num_items = source_items.size();

  // Read starting from 'first_item_index' and 'item_offset'.
  for (uint64_t total_sliced = 0;
       item_index < num_items && total_sliced < slice_size; item_index++) {
    const scoped_refptr<BlobDataItem>& source_item =
        source_items[item_index]->item();
    uint64_t source_length = source_item->length();
    BlobDataItem::Type type = source_item->type();
    DCHECK_NE(source_length, blink::BlobUtils::kUnknownSize);
    DCHECK_NE(source_length, 0ull);

    uint64_t read_size =
        std::min(source_length - item_offset, slice_size - total_sliced);
    total_sliced += read_size;

    bool reusing_blob_item = (read_size == source_length);
    if (reusing_blob_item) {
      // We can share the entire item.
      items_.push_back(source_items[item_index]);
      if (type == BlobDataItem::Type::kBytes ||
          type == BlobDataItem::Type::kBytesDescription) {
        total_memory_size_ += source_length;
      }
      continue;
    }

    bool need_copy = false;
    scoped_refptr<BlobDataItem> data_item;
    ShareableBlobDataItem::State state =
        ShareableBlobDataItem::POPULATED_WITHOUT_QUOTA;
    switch (type) {
      case BlobDataItem::Type::kBytesDescription:
      case BlobDataItem::Type::kBytes: {
        need_copy = true;
        copy_quota_needed_ += read_size;
        total_memory_size_ += read_size;
        // Since we don't have quota yet for memory, we create temporary items
        // for this data. When our blob is finished constructing, all dependent
        // blobs are done, and we have enough memory quota, we'll copy the data
        // over.
        data_item = BlobDataItem::CreateBytesDescription(
            base::checked_cast<size_t>(read_size));
        state = ShareableBlobDataItem::QUOTA_NEEDED;
        break;
      }
      case BlobDataItem::Type::kFile: {
        data_item = BlobDataItem::CreateFile(
            source_item->path(), source_item->offset() + item_offset, read_size,
            source_item->expected_modification_time(), source_item->file_ref_,
            source_item->file_access_);

        if (source_item->IsFutureFileItem()) {
          // The source file isn't a real file yet (path is fake), so store the
          // items we need to copy from later.
          need_copy = true;
        }
        break;
      }
      case BlobDataItem::Type::kFileFilesystem: {
        data_item = BlobDataItem::CreateFileFilesystem(
            source_item->filesystem_url(), source_item->offset() + item_offset,
            read_size, source_item->expected_modification_time(),
            source_item->file_system_context(), source_item->file_access_);
        break;
      }
      case BlobDataItem::Type::kReadableDataHandle: {
        data_item = BlobDataItem::CreateReadableDataHandle(
            source_item->data_handle_, source_item->offset() + item_offset,
            read_size);
        break;
      }
    }

    items_.push_back(new ShareableBlobDataItem(std::move(data_item), state));
    if (need_copy) {
      copies_.push_back(
          ItemCopyEntry(source_items[item_index], item_offset, items_.back()));
    }
    item_offset = 0;
  }
}

void BlobDataBuilder::AppendBlob(const std::string& uuid,
                                 const BlobStorageRegistry& blob_registry) {
  AppendBlob(uuid, 0, blink::BlobUtils::kUnknownSize, blob_registry);
}

void BlobDataBuilder::AppendFileSystemFile(
    const FileSystemURL& url,
    uint64_t offset,
    uint64_t length,
    const base::Time& expected_modification_time,
    scoped_refptr<FileSystemContext> file_system_context,
    file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
        file_access) {
  DCHECK_GT(length, 0ul);
  auto item = BlobDataItem::CreateFileFilesystem(
      url, offset, length, expected_modification_time,
      std::move(file_system_context), std::move(file_access));

  auto shareable_item = base::MakeRefCounted<ShareableBlobDataItem>(
      std::move(item), ShareableBlobDataItem::POPULATED_WITHOUT_QUOTA);
  items_.push_back(std::move(shareable_item));

  total_size_ += length;
}

void BlobDataBuilder::AppendReadableDataHandle(
    scoped_refptr<DataHandle> data_handle,
    uint64_t offset,
    uint64_t length) {
  if (length == 0ul)
    return;
  auto item = BlobDataItem::CreateReadableDataHandle(std::move(data_handle),
                                                     offset, length);

  total_size_ += item->length();
  auto shareable_item = base::MakeRefCounted<ShareableBlobDataItem>(
      std::move(item), ShareableBlobDataItem::POPULATED_WITHOUT_QUOTA);
  items_.push_back(std::move(shareable_item));
}

void BlobDataBuilder::AppendMojoDataItem(mojom::BlobDataItemPtr item_ptr) {
  if (item_ptr->size == 0ul)
    return;
  auto item = BlobDataItem::CreateMojoDataItem(std::move(item_ptr));

  total_size_ += item->length();
  auto shareable_item = base::MakeRefCounted<ShareableBlobDataItem>(
      std::move(item), ShareableBlobDataItem::POPULATED_WITHOUT_QUOTA);
  items_.push_back(std::move(shareable_item));
}

std::unique_ptr<BlobDataSnapshot> BlobDataBuilder::CreateSnapshot() const {
  std::vector<scoped_refptr<BlobDataItem>> items;
  items.reserve(items_.size());
  for (const auto& item : items_)
    items.push_back(item->item());
  return base::WrapUnique(new BlobDataSnapshot(
      uuid_, content_type_, content_disposition_, std::move(items)));
}

void PrintTo(const BlobDataBuilder& x, std::ostream* os) {
  DCHECK(os);
  *os << "<BlobDataBuilder>{uuid: " << x.uuid()
      << ", content_type: " << x.content_type_
      << ", content_disposition: " << x.content_disposition_ << ", items: [";
  for (const auto& item : x.items_) {
    PrintTo(*item->item(), os);
    *os << ", ";
  }
  *os << "]}";
}

}  // namespace storage
