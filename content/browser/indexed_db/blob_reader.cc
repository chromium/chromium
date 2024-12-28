// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/blob_reader.h"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "content/browser/indexed_db/file_stream_reader_to_data_pipe.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"

namespace content::indexed_db {

void BlobReader::Clone(mojo::PendingReceiver<blink::mojom::Blob> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void BlobReader::AsDataPipeGetter(
    mojo::PendingReceiver<network::mojom::DataPipeGetter> receiver) {
  NOTREACHED();
}

void BlobReader::ReadRange(
    uint64_t offset,
    uint64_t length,
    mojo::ScopedDataPipeProducerHandle handle,
    mojo::PendingRemote<blink::mojom::BlobReaderClient> client) {
  OpenFileAndReadIntoPipe(file_path_, blob_length_, offset, length,
                          std::move(handle), std::move(client));
}

void BlobReader::ReadAll(
    mojo::ScopedDataPipeProducerHandle handle,
    mojo::PendingRemote<blink::mojom::BlobReaderClient> client) {
  ReadRange(0, std::numeric_limits<uint64_t>::max(), std::move(handle),
            std::move(client));
}

void BlobReader::Load(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  // Bounce back to the registry so that we can avoid reimplementing
  // `BlobUrlLoader`. This is used for Object URLs. It's not clear how often
  // this is used or how important it is to make it super efficient.
  registry_blob_->Load(std::move(loader), method, headers, std::move(client));
}

void BlobReader::ReadSideData(
    blink::mojom::Blob::ReadSideDataCallback callback) {
  std::move(callback).Run(std::nullopt);
}

void BlobReader::CaptureSnapshot(CaptureSnapshotCallback callback) {
  // Only used for `File`.
  NOTREACHED();
}

void BlobReader::GetInternalUUID(GetInternalUUIDCallback callback) {
  std::move(callback).Run(uuid_);
}

void BlobReader::Read(
    uint64_t offset,
    uint64_t length,
    mojo::ScopedDataPipeProducerHandle pipe,
    storage::mojom::BlobDataItemReader::ReadCallback callback) {
  OpenFileAndReadIntoPipe(file_path_, offset, length, std::move(pipe),
                          std::move(callback));
}

void BlobReader::ReadSideData(
    storage::mojom::BlobDataItemReader::ReadSideDataCallback callback) {
  // This type should never have side data.
  std::move(callback).Run(net::ERR_NOT_IMPLEMENTED, mojo_base::BigBuffer());
}

BlobReader::BlobReader(const IndexedDBExternalObject& blob_info,
                       storage::mojom::BlobStorageContext& blob_registry,
                       base::OnceClosure on_last_receiver_disconnected)
    : uuid_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      blob_length_(blob_info.size()),
      file_path_(blob_info.indexed_db_file_path()),
      on_last_receiver_disconnected_(std::move(on_last_receiver_disconnected)) {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &BlobReader::OnMojoDisconnect, base::Unretained(this)));

  auto element = storage::mojom::BlobDataItem::New();
  element->size = blob_info.size();
  element->side_data_size = 0;
  element->content_type = base::UTF16ToUTF8(blob_info.type());
  element->type = storage::mojom::BlobDataItemType::kIndexedDB;
  reader_.Bind(element->reader.InitWithNewPipeAndPassReceiver());
  reader_.set_disconnect_handler(base::BindRepeating(
      &BlobReader::OnReaderDisconnect, base::Unretained(this)));
  blob_registry.RegisterFromDataItem(
      registry_blob_.BindNewPipeAndPassReceiver(), uuid_, std::move(element));
}

BlobReader::~BlobReader() = default;

void BlobReader::OnReaderDisconnect() {
  reader_.reset();
  OnMojoDisconnect();
}

void BlobReader::OnMojoDisconnect() {
  if (receivers_.empty()) {
    registry_blob_.reset();
  }

  if (receivers_.empty() && !reader_.is_bound()) {
    std::move(on_last_receiver_disconnected_).Run();
    // `this` is deleted.
  }
}

}  // namespace content::indexed_db
