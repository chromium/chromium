// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_data_item.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/data_pipe_to_source_stream.h"
#include "storage/browser/file_system/file_system_context.h"
#include "third_party/blink/public/common/blob/blob_utils.h"

namespace storage {

namespace {
const base::FilePath::CharType kFutureFileName[] =
    FILE_PATH_LITERAL("_future_name_");
}

class MojoDataItem : public BlobDataItem::DataHandle {
 public:
  explicit MojoDataItem(mojom::BlobDataItemPtr element)
      : item_(std::move(element)) {
    reader_.Bind(std::move(item_->reader));
  }

  // BlobDataItem::DataHandle implementation.
  uint64_t GetSize() const override { return item_->size; }

  void Read(mojo::ScopedDataPipeProducerHandle producer,
            uint64_t src_offset,
            uint64_t bytes_to_read,
            base::OnceCallback<void(int)> callback) override {
    reader_->Read(src_offset, bytes_to_read, std::move(producer),
                  std::move(callback));
  }

  uint64_t GetSideDataSize() const override { return item_->side_data_size; }

  void ReadSideData(
      base::OnceCallback<void(int, mojo_base::BigBuffer)> callback) override {
    reader_->ReadSideData(std::move(callback));
  }

  void PrintTo(::std::ostream* os) const override {
    // TODO(enne): this is tricky to implement, as it's synchronous.
    // PrintTo should ideally be asynchronous.  See: http://crbug.com/809821
    *os << "<MojoDataItem>";
  }

 protected:
  ~MojoDataItem() override = default;

  mojom::BlobDataItemPtr item_;
  mojo::Remote<mojom::BlobDataItemReader> reader_;
};

BlobDataItem::DataHandle::~DataHandle() = default;

// static
scoped_refptr<BlobDataItem> BlobDataItem::CreateBytes(
    base::span<const uint8_t> bytes) {
  auto item =
      base::WrapRefCounted(new BlobDataItem(Type::kBytes, 0, bytes.size()));
  item->bytes_.assign(bytes.begin(), bytes.end());
  return item;
}

// static
scoped_refptr<BlobDataItem> BlobDataItem::CreateBytesDescription(
    size_t length) {
  return base::WrapRefCounted(
      new BlobDataItem(Type::kBytesDescription, 0, length));
}

// static
scoped_refptr<BlobDataItem> BlobDataItem::CreateFile(
    base::FilePath path,
    file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
        file_access) {
  return CreateFile(path, 0, blink::BlobUtils::kUnknownSize, base::Time(),
                    nullptr, std::move(file_access));
}

// static
scoped_refptr<BlobDataItem> BlobDataItem::CreateFile(
    base::FilePath path,
    uint64_t offset,
    uint64_t length,
    base::Time expected_modification_time,
    scoped_refptr<ShareableFileReference> file_ref,
    file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
        file_access) {
  auto item =
      base::WrapRefCounted(new BlobDataItem(Type::kFile, offset, length));
  item->path_ = std::move(path);
  item->expected_modification_time_ = std::move(expected_modification_time);
  item->file_ref_ = std::move(file_ref);
  item->file_access_ = std::move(file_access);
  // TODO(mek): DCHECK(!item->IsFutureFileItem()) when BlobDataBuilder has some
  // other way of slicing a future file.
  return item;
}

// static
scoped_refptr<BlobDataItem> BlobDataItem::CreateFutureFile(uint64_t offset,
                                                           uint64_t length,
                                                           uint64_t file_id) {
  auto item =
      base::WrapRefCounted(new BlobDataItem(Type::kFile, offset, length));
  std::string file_id_str = base::NumberToString(file_id);
  item->path_ = base::FilePath(kFutureFileName)
                    .AddExtension(base::FilePath::StringType(
                        file_id_str.begin(), file_id_str.end()));
  return item;
}

// static
scoped_refptr<BlobDataItem> BlobDataItem::CreateFileFilesystem(
    const FileSystemURL& url,
    uint64_t offset,
    uint64_t length,
    base::Time expected_modification_time,
    scoped_refptr<FileSystemContext> file_system_context,
    file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
        file_access) {
  auto item = base::WrapRefCounted(
      new BlobDataItem(Type::kFileFilesystem, offset, length));
  item->filesystem_url_ = url;
  item->expected_modification_time_ = std::move(expected_modification_time);
  item->file_system_context_ = std::move(file_system_context);
  item->file_access_ = std::move(file_access);
  return item;
}

// static
scoped_refptr<BlobDataItem> BlobDataItem::CreateReadableDataHandle(
    scoped_refptr<DataHandle> data_handle,
    uint64_t offset,
    uint64_t length) {
  DCHECK(data_handle);
  DCHECK_LE(offset, data_handle->GetSize());
  DCHECK_LE(length, (data_handle->GetSize() - offset));
  auto item = base::WrapRefCounted(
      new BlobDataItem(Type::kReadableDataHandle, offset, length));
  item->data_handle_ = std::move(data_handle);
  return item;
}

// static
scoped_refptr<BlobDataItem> BlobDataItem::CreateMojoDataItem(
    mojom::BlobDataItemPtr item) {
  auto handle = base::MakeRefCounted<MojoDataItem>(std::move(item));
  auto data_item = base::WrapRefCounted(
      new BlobDataItem(Type::kReadableDataHandle, 0, handle->GetSize()));
  DCHECK_GT(handle->GetSize(), 0u);
  data_item->data_handle_ = std::move(handle);
  return data_item;
}

bool BlobDataItem::IsFutureFileItem() const {
  if (type_ != Type::kFile)
    return false;
  const base::FilePath::StringType prefix(kFutureFileName);
  // The prefix shouldn't occur unless the user used "AppendFutureFile". We
  // DCHECK on AppendFile to make sure no one appends a future file.
  return base::StartsWith(path().value(), prefix, base::CompareCase::SENSITIVE);
}

uint64_t BlobDataItem::GetFutureFileID() const {
  DCHECK(IsFutureFileItem());
  uint64_t id = 0;
  bool success = base::StringToUint64(path().Extension().substr(1), &id);
  DCHECK(success) << path().Extension();
  return id;
}

BlobDataItem::BlobDataItem(Type type, uint64_t offset, uint64_t length)
    : type_(type), offset_(offset), length_(length) {}

BlobDataItem::~BlobDataItem() = default;

void BlobDataItem::AllocateBytes() {
  DCHECK_EQ(type_, Type::kBytesDescription);
  bytes_.resize(length_);
  type_ = Type::kBytes;
}

void BlobDataItem::PopulateBytes(base::span<const uint8_t> data) {
  DCHECK_EQ(type_, Type::kBytesDescription);
  DCHECK_EQ(length_, data.size());
  type_ = Type::kBytes;
  bytes_.assign(data.begin(), data.end());
}

void BlobDataItem::ShrinkBytes(size_t new_length) {
  DCHECK_EQ(type_, Type::kBytes);
  length_ = new_length;
  bytes_.resize(length_);
  bytes_.shrink_to_fit();
}

void BlobDataItem::PopulateFile(
    base::FilePath path,
    base::Time expected_modification_time,
    scoped_refptr<ShareableFileReference> file_ref) {
  DCHECK_EQ(type_, Type::kFile);
  DCHECK(IsFutureFileItem());
  path_ = std::move(path);
  expected_modification_time_ = std::move(expected_modification_time);
  file_ref_ = std::move(file_ref);
}

void BlobDataItem::ShrinkFile(uint64_t new_length) {
  DCHECK_EQ(type_, Type::kFile);
  DCHECK_LE(new_length, length_);
  length_ = new_length;
}

void BlobDataItem::GrowFile(uint64_t new_length) {
  DCHECK_EQ(type_, Type::kFile);
  DCHECK_GE(new_length, length_);
  length_ = new_length;
}

// static
void BlobDataItem::SetFileModificationTimes(
    std::vector<scoped_refptr<BlobDataItem>> items,
    std::vector<base::Time> times) {
  DCHECK_EQ(items.size(), times.size());
  for (size_t i = 0; i < items.size(); ++i) {
    items[i]->expected_modification_time_ = times[i];
  }
}

void PrintTo(const BlobDataItem& x, ::std::ostream* os) {
  const uint64_t kMaxDataPrintLength = 40;
  DCHECK(os);
  *os << "<BlobDataItem>{type: ";
  switch (x.type()) {
    case BlobDataItem::Type::kBytes: {
      uint64_t length = std::min(x.length(), kMaxDataPrintLength);
      *os << "kBytes, data: ["
          << base::HexEncode(x.bytes().data(), static_cast<size_t>(length));
      if (length < x.length()) {
        *os << "<...truncated due to length...>";
      }
      *os << "]";
      break;
    }
    case BlobDataItem::Type::kBytesDescription:
      *os << "kBytesDescription";
      break;
    case BlobDataItem::Type::kFile:
      *os << "kFile, path: " << x.path().AsUTF8Unsafe()
          << ", expected_modification_time: " << x.expected_modification_time();
      break;
    case BlobDataItem::Type::kFileFilesystem:
      *os << "kFileFilesystem, url: " << x.filesystem_url().DebugString();
      break;
    case BlobDataItem::Type::kReadableDataHandle:
      *os << "kReadableDataHandle"
          << ", data_handle_: ";
      x.data_handle()->PrintTo(os);
      break;
  }
  *os << ", length: " << x.length() << ", offset: " << x.offset()
      << ", has_data_handle: " << (x.data_handle_.get() ? "true" : "false");
}

bool operator==(const BlobDataItem& a, const BlobDataItem& b) {
  if (a.type() != b.type() || a.offset() != b.offset() ||
      a.length() != b.length())
    return false;
  switch (a.type()) {
    case BlobDataItem::Type::kBytes:
      return base::ranges::equal(a.bytes(), b.bytes());
    case BlobDataItem::Type::kBytesDescription:
      return true;
    case BlobDataItem::Type::kFile:
      return a.path() == b.path() &&
             a.expected_modification_time() == b.expected_modification_time();
    case BlobDataItem::Type::kFileFilesystem:
      return a.filesystem_url() == b.filesystem_url();
    case BlobDataItem::Type::kReadableDataHandle:
      return a.data_handle() == b.data_handle();
  }
  NOTREACHED();
}

bool operator!=(const BlobDataItem& a, const BlobDataItem& b) {
  return !(a == b);
}

}  // namespace storage
