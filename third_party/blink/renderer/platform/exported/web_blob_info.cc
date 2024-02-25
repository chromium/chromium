// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_blob_info.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"

namespace blink {

WebBlobInfo::WebBlobInfo(const WebString& uuid,
                         const WebString& type,
                         uint64_t size,
                         CrossVariantMojoRemote<mojom::BlobInterfaceBase> blob)
    : WebBlobInfo(BlobDataHandle::Create(
          uuid,
          type,
          size,
          mojo::PendingRemote<mojom::blink::Blob>(std::move(blob)))) {}

WebBlobInfo::WebBlobInfo(const WebString& uuid,
                         const WebString& file_name,
                         const WebString& type,
                         const std::optional<base::Time>& last_modified,
                         uint64_t size,
                         CrossVariantMojoRemote<mojom::BlobInterfaceBase> blob)
    : WebBlobInfo(BlobDataHandle::Create(
                      uuid,
                      type,
                      size,
                      mojo::PendingRemote<mojom::blink::Blob>(std::move(blob))),
                  file_name,
                  last_modified) {}

// static
WebBlobInfo WebBlobInfo::BlobForTesting(const WebString& uuid,
                                        const WebString& type,
                                        uint64_t size) {
  return WebBlobInfo(uuid, type, size, mojo::NullRemote());
}

// static
WebBlobInfo WebBlobInfo::FileForTesting(const WebString& uuid,
                                        const WebString& file_name,
                                        const WebString& type) {
  return WebBlobInfo(uuid, file_name, type, std::nullopt,
                     std::numeric_limits<uint64_t>::max(), mojo::NullRemote());
}

WebBlobInfo::~WebBlobInfo() {
  blob_handle_.Reset();
}

WebBlobInfo::WebBlobInfo(const WebBlobInfo& other) {
  *this = other;
}

WebBlobInfo& WebBlobInfo::operator=(const WebBlobInfo& other) = default;

CrossVariantMojoRemote<mojom::BlobInterfaceBase> WebBlobInfo::CloneBlobRemote()
    const {
  if (!blob_handle_)
    return mojo::NullRemote();
  return blob_handle_->CloneBlobRemote();
}

WebBlobInfo::WebBlobInfo(scoped_refptr<BlobDataHandle> handle)
    : WebBlobInfo(handle, handle->GetType(), handle->size()) {}

WebBlobInfo::WebBlobInfo(scoped_refptr<BlobDataHandle> handle,
                         const WebString& file_name,
                         const std::optional<base::Time>& last_modified)
    : WebBlobInfo(handle,
                  file_name,
                  handle->GetType(),
                  last_modified,
                  handle->size()) {}

WebBlobInfo::WebBlobInfo(scoped_refptr<BlobDataHandle> handle,
                         const WebString& type,
                         uint64_t size)
    : is_file_(false),
      uuid_(handle->Uuid()),
      type_(type),
      size_(size),
      blob_handle_(std::move(handle)) {}

WebBlobInfo::WebBlobInfo(scoped_refptr<BlobDataHandle> handle,
                         const WebString& file_name,
                         const WebString& type,
                         const std::optional<base::Time>& last_modified,
                         uint64_t size)
    : is_file_(true),
      uuid_(handle->Uuid()),
      type_(type),
      size_(size),
      blob_handle_(std::move(handle)),
      file_name_(file_name),
      last_modified_(last_modified) {}

scoped_refptr<BlobDataHandle> WebBlobInfo::GetBlobHandle() const {
  return blob_handle_.Get();
}

}  // namespace blink
