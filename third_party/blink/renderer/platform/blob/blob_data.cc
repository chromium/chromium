/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/blob/blob_data.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/data_pipe_getter.mojom-blink.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom-blink.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/blob/blob_bytes_provider.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/line_ending.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using mojom::blink::BytesProvider;
using mojom::blink::DataElement;
using mojom::blink::DataElementBlob;
using mojom::blink::DataElementBytes;
using mojom::blink::DataElementBytesPtr;
using mojom::blink::DataElementFile;
using mojom::blink::DataElementPtr;

namespace {

// http://dev.w3.org/2006/webapi/FileAPI/#constructorBlob
bool IsValidBlobType(const String& type) {
  for (unsigned i = 0; i < type.length(); ++i) {
    UChar c = type[i];
    if (c < 0x20 || c > 0x7E)
      return false;
  }
  return true;
}

mojom::blink::BlobRegistry* g_blob_registry_for_testing = nullptr;

mojom::blink::BlobRegistry* GetThreadSpecificRegistry() {
  if (UNLIKELY(g_blob_registry_for_testing))
    return g_blob_registry_for_testing;

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<mojo::Remote<mojom::blink::BlobRegistry>>, registry, ());
  if (UNLIKELY(!registry.IsSet())) {
    // TODO(mek): Going through BrowserInterfaceBroker to get a
    // mojom::blink::BlobRegistry ends up going through the main thread. Ideally
    // workers wouldn't need to do that.
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        (*registry).BindNewPipeAndPassReceiver());
  }
  return registry->get();
}

}  // namespace

constexpr int64_t BlobData::kToEndOfFile;

RawData::RawData() = default;

BlobData::BlobData(FileCompositionStatus composition)
    : file_composition_(composition) {}

BlobData::~BlobData() = default;

Vector<mojom::blink::DataElementPtr> BlobData::ReleaseElements() {
  if (last_bytes_provider_) {
    DCHECK(last_bytes_provider_receiver_);
    BlobBytesProvider::Bind(std::move(last_bytes_provider_),
                            std::move(last_bytes_provider_receiver_));
  }

  return std::move(elements_);
}

std::unique_ptr<BlobData> BlobData::CreateForFileWithUnknownSize(
    const String& path) {
  std::unique_ptr<BlobData> data = base::WrapUnique(
      new BlobData(FileCompositionStatus::kSingleUnknownSizeFile));
  data->elements_.push_back(DataElement::NewFile(DataElementFile::New(
      WebStringToFilePath(path), 0, BlobData::kToEndOfFile, base::Time())));
  return data;
}

std::unique_ptr<BlobData> BlobData::CreateForFileWithUnknownSize(
    const String& path,
    const absl::optional<base::Time>& expected_modification_time) {
  std::unique_ptr<BlobData> data = base::WrapUnique(
      new BlobData(FileCompositionStatus::kSingleUnknownSizeFile));
  data->elements_.push_back(DataElement::NewFile(
      DataElementFile::New(WebStringToFilePath(path), 0, BlobData::kToEndOfFile,
                           expected_modification_time)));
  return data;
}

void BlobData::SetContentType(const String& content_type) {
  if (IsValidBlobType(content_type))
    content_type_ = content_type;
  else
    content_type_ = "";
}

void BlobData::AppendData(scoped_refptr<RawData> data) {
  AppendDataInternal(base::make_span(data->data(), data->length()), data);
}

void BlobData::AppendFile(
    const String& path,
    int64_t offset,
    int64_t length,
    const absl::optional<base::Time>& expected_modification_time) {
  DCHECK_EQ(file_composition_, FileCompositionStatus::kNoUnknownSizeFiles)
      << "Blobs with a unknown-size file cannot have other items.";
  DCHECK_NE(length, BlobData::kToEndOfFile)
      << "It is illegal to append file items that have an unknown size. To "
         "create a blob with a single file with unknown size, use "
         "BlobData::createForFileWithUnknownSize. Otherwise please provide the "
         "file size.";
  DCHECK_GE(length, 0);
  // Skip zero-byte items, as they don't matter for the contents of the blob.
  if (length == 0)
    return;
  elements_.push_back(DataElement::NewFile(DataElementFile::New(
      WebStringToFilePath(path), offset, length, expected_modification_time)));
}

void BlobData::AppendBlob(scoped_refptr<BlobDataHandle> data_handle,
                          int64_t offset,
                          int64_t length) {
  DCHECK_EQ(file_composition_, FileCompositionStatus::kNoUnknownSizeFiles)
      << "Blobs with a unknown-size file cannot have other items.";
  DCHECK(!data_handle->IsSingleUnknownSizeFile())
      << "It is illegal to append an unknown size file blob.";
  // Skip zero-byte items, as they don't matter for the contents of the blob.
  if (length == 0)
    return;
  elements_.push_back(DataElement::NewBlob(
      DataElementBlob::New(data_handle->CloneBlobRemote(), offset, length)));
}

void BlobData::AppendText(const String& text,
                          bool do_normalize_line_endings_to_native) {
  DCHECK_EQ(file_composition_, FileCompositionStatus::kNoUnknownSizeFiles)
      << "Blobs with a unknown-size file cannot have other items.";
  std::string utf8_text = UTF8Encoding().Encode(text, WTF::kNoUnencodables);

  if (do_normalize_line_endings_to_native) {
    if (utf8_text.length() >
        BlobBytesProvider::kMaxConsolidatedItemSizeInBytes) {
      auto raw_data = RawData::Create();
      NormalizeLineEndingsToNative(utf8_text, *raw_data->MutableData());
      AppendDataInternal(base::make_span(raw_data->data(), raw_data->length()),
                         raw_data);
    } else {
      Vector<char> buffer;
      NormalizeLineEndingsToNative(utf8_text, buffer);
      AppendDataInternal(base::make_span(buffer));
    }
  } else {
    AppendDataInternal(base::make_span(utf8_text.data(), utf8_text.length()));
  }
}

void BlobData::AppendBytes(const void* bytes, size_t length) {
  AppendDataInternal(
      base::make_span(reinterpret_cast<const char*>(bytes), length));
}

uint64_t BlobData::length() const {
  uint64_t length = 0;

  for (const auto& element : elements_) {
    switch (element->which()) {
      case DataElement::Tag::kBytes:
        length += element->get_bytes()->length;
        break;
      case DataElement::Tag::kFile:
        length += element->get_file()->length;
        break;
      case DataElement::Tag::kBlob:
        length += element->get_blob()->length;
        break;
    }
  }
  return length;
}

void BlobData::AppendDataInternal(base::span<const char> data,
                                  scoped_refptr<RawData> raw_data) {
  DCHECK_EQ(file_composition_, FileCompositionStatus::kNoUnknownSizeFiles)
      << "Blobs with a unknown-size file cannot have other items.";
  // Skip zero-byte items, as they don't matter for the contents of the blob.
  if (data.empty())
    return;
  bool should_embed_bytes = current_memory_population_ + data.size() <=
                            DataElementBytes::kMaximumEmbeddedDataSize;
  if (!elements_.empty() && elements_.back()->is_bytes()) {
    // Append bytes to previous element.
    DCHECK(last_bytes_provider_);
    DCHECK(last_bytes_provider_receiver_);
    const auto& bytes_element = elements_.back()->get_bytes();
    bytes_element->length += data.size();
    if (should_embed_bytes && bytes_element->embedded_data) {
      bytes_element->embedded_data->Append(
          data.data(), base::checked_cast<wtf_size_t>(data.size()));
      current_memory_population_ += data.size();
    } else if (bytes_element->embedded_data) {
      current_memory_population_ -= bytes_element->embedded_data->size();
      bytes_element->embedded_data = absl::nullopt;
    }
  } else {
    if (last_bytes_provider_) {
      // If `last_bytes_provider_` is set, but the previous element is not a
      // bytes element, a new BytesProvider will be created and we need to
      // make sure to bind the previous one first.
      DCHECK(last_bytes_provider_receiver_);
      BlobBytesProvider::Bind(std::move(last_bytes_provider_),
                              std::move(last_bytes_provider_receiver_));
    }
    mojo::PendingRemote<BytesProvider> bytes_provider_remote;
    last_bytes_provider_ = std::make_unique<BlobBytesProvider>();
    last_bytes_provider_receiver_ =
        bytes_provider_remote.InitWithNewPipeAndPassReceiver();

    auto bytes_element = DataElementBytes::New(
        data.size(), absl::nullopt, std::move(bytes_provider_remote));
    if (should_embed_bytes) {
      bytes_element->embedded_data = Vector<uint8_t>();
      bytes_element->embedded_data->Append(
          data.data(), base::checked_cast<wtf_size_t>(data.size()));
      current_memory_population_ += data.size();
    }
    elements_.push_back(DataElement::NewBytes(std::move(bytes_element)));
  }
  if (raw_data)
    last_bytes_provider_->AppendData(std::move(raw_data));
  else
    last_bytes_provider_->AppendData(std::move(data));
}

// static
scoped_refptr<BlobDataHandle> BlobDataHandle::Create(
    const String& uuid,
    const String& type,
    uint64_t size,
    mojo::PendingRemote<mojom::blink::Blob> blob_remote) {
  if (blob_remote.is_valid()) {
    return base::AdoptRef(
        new BlobDataHandle(uuid, type, size, std::move(blob_remote)));
  }
  return base::AdoptRef(new BlobDataHandle(uuid, type, size));
}

BlobDataHandle::BlobDataHandle()
    : uuid_(WTF::CreateCanonicalUUIDString()),
      size_(0),
      is_single_unknown_size_file_(false) {
  GetThreadSpecificRegistry()->Register(
      blob_remote_.InitWithNewPipeAndPassReceiver(), uuid_, "", "", {});
}

BlobDataHandle::BlobDataHandle(std::unique_ptr<BlobData> data, uint64_t size)
    : uuid_(WTF::CreateCanonicalUUIDString()),
      type_(data->ContentType()),
      size_(size),
      is_single_unknown_size_file_(data->IsSingleUnknownSizeFile()) {
  auto elements = data->ReleaseElements();
  TRACE_EVENT0("Blob", "Registry::RegisterBlob");
  GetThreadSpecificRegistry()->Register(
      blob_remote_.InitWithNewPipeAndPassReceiver(), uuid_,
      type_.IsNull() ? "" : type_, "", std::move(elements));
}

BlobDataHandle::BlobDataHandle(const String& uuid,
                               const String& type,
                               uint64_t size)
    : uuid_(uuid),
      type_(IsValidBlobType(type) ? type : ""),
      size_(size),
      is_single_unknown_size_file_(false) {
  GetThreadSpecificRegistry()->GetBlobFromUUID(
      blob_remote_.InitWithNewPipeAndPassReceiver(), uuid_);
}

BlobDataHandle::BlobDataHandle(
    const String& uuid,
    const String& type,
    uint64_t size,
    mojo::PendingRemote<mojom::blink::Blob> blob_remote)
    : uuid_(uuid),
      type_(IsValidBlobType(type) ? type : ""),
      size_(size),
      is_single_unknown_size_file_(false),
      blob_remote_(std::move(blob_remote)) {
  DCHECK(blob_remote_.is_valid());
}

BlobDataHandle::~BlobDataHandle() = default;

mojo::PendingRemote<mojom::blink::Blob> BlobDataHandle::CloneBlobRemote() {
  base::AutoLock locker(blob_remote_lock_);
  if (!blob_remote_.is_valid())
    return mojo::NullRemote();
  mojo::Remote<mojom::blink::Blob> blob(std::move(blob_remote_));
  mojo::PendingRemote<mojom::blink::Blob> blob_clone;
  blob->Clone(blob_clone.InitWithNewPipeAndPassReceiver());
  blob_remote_ = blob.Unbind();
  return blob_clone;
}

void BlobDataHandle::CloneBlobRemote(
    mojo::PendingReceiver<mojom::blink::Blob> receiver) {
  base::AutoLock locker(blob_remote_lock_);
  if (!blob_remote_.is_valid())
    return;
  mojo::Remote<mojom::blink::Blob> blob(std::move(blob_remote_));
  blob->Clone(std::move(receiver));
  blob_remote_ = blob.Unbind();
}

mojo::PendingRemote<network::mojom::blink::DataPipeGetter>
BlobDataHandle::AsDataPipeGetter() {
  base::AutoLock locker(blob_remote_lock_);
  if (!blob_remote_.is_valid())
    return mojo::NullRemote();
  mojo::PendingRemote<network::mojom::blink::DataPipeGetter> result;
  mojo::Remote<mojom::blink::Blob> blob(std::move(blob_remote_));
  blob->AsDataPipeGetter(result.InitWithNewPipeAndPassReceiver());
  blob_remote_ = blob.Unbind();
  return result;
}

void BlobDataHandle::ReadAll(
    mojo::ScopedDataPipeProducerHandle pipe,
    mojo::PendingRemote<mojom::blink::BlobReaderClient> client) {
  base::AutoLock locker(blob_remote_lock_);
  mojo::Remote<mojom::blink::Blob> blob(std::move(blob_remote_));
  blob->ReadAll(std::move(pipe), std::move(client));
  blob_remote_ = blob.Unbind();
}

void BlobDataHandle::ReadRange(
    uint64_t offset,
    uint64_t length,
    mojo::ScopedDataPipeProducerHandle pipe,
    mojo::PendingRemote<mojom::blink::BlobReaderClient> client) {
  base::AutoLock locker(blob_remote_lock_);
  mojo::Remote<mojom::blink::Blob> blob(std::move(blob_remote_));
  blob->ReadRange(offset, length, std::move(pipe), std::move(client));
  blob_remote_ = blob.Unbind();
}

bool BlobDataHandle::CaptureSnapshot(
    uint64_t* snapshot_size,
    absl::optional<base::Time>* snapshot_modification_time) {
  // This method operates on a cloned blob remote; this lets us avoid holding
  // the |blob_remote_lock_| locked during the duration of the (synchronous)
  // CaptureSnapshot call.
  mojo::Remote<mojom::blink::Blob> remote(CloneBlobRemote());
  return remote->CaptureSnapshot(snapshot_size, snapshot_modification_time);
}

// static
mojom::blink::BlobRegistry* BlobDataHandle::GetBlobRegistry() {
  return GetThreadSpecificRegistry();
}

// static
void BlobDataHandle::SetBlobRegistryForTesting(
    mojom::blink::BlobRegistry* registry) {
  g_blob_registry_for_testing = registry;
}

}  // namespace blink
