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
  data_pipe_getter_receivers_.Add(this, std::move(receiver));
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
  // This method is used for the File API. Technically IDB can store Files, but
  // when it does so, the size and last modification date should always be known
  // and propagated to the renderer through IndexedDBExternalObject's metadata.
  // This path is likely only reached when the file modification date and/or
  // size is somehow unknown, but reproducing this scenario has proven
  // difficult. See crbug.com/390586616
  // Note that we don't stat the underlying file because it's just a copy of
  // whatever the original File was, and hence would have the wrong modification
  // date.
  std::move(callback).Run(blob_length_, std::nullopt);
}

void BlobReader::GetInternalUUID(GetInternalUUIDCallback callback) {
  std::move(callback).Run(uuid_);
}

void BlobReader::Clone(
    mojo::PendingReceiver<network::mojom::DataPipeGetter> receiver) {
  data_pipe_getter_receivers_.Add(this, std::move(receiver));
}

void BlobReader::Read(mojo::ScopedDataPipeProducerHandle pipe,
                      network::mojom::DataPipeGetter::ReadCallback callback) {
  std::move(callback).Run(net::OK, blob_length_);
  Read(0, std::numeric_limits<uint64_t>::max(), std::move(pipe),
       base::DoNothing());
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
                       base::OnceClosure on_last_receiver_disconnected)
    : uuid_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      blob_length_(blob_info.size()),
      content_type_(base::UTF16ToUTF8(blob_info.type())),
      file_path_(blob_info.indexed_db_file_path()),
      on_last_receiver_disconnected_(std::move(on_last_receiver_disconnected)) {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &BlobReader::OnMojoDisconnect, base::Unretained(this)));
  data_pipe_getter_receivers_.set_disconnect_handler(base::BindRepeating(
      &BlobReader::OnMojoDisconnect, base::Unretained(this)));
  readers_.set_disconnect_handler(base::BindRepeating(
      &BlobReader::OnMojoDisconnect, base::Unretained(this)));
}

BlobReader::~BlobReader() = default;

void BlobReader::BindRegistryBlob(
    storage::mojom::BlobStorageContext& blob_registry) {
  CHECK(!registry_blob_.is_bound());
  auto element = storage::mojom::BlobDataItem::New();
  element->size = blob_length_;
  element->side_data_size = 0;
  element->content_type = content_type_;
  element->type = storage::mojom::BlobDataItemType::kIndexedDB;
  readers_.Add(this, element->reader.InitWithNewPipeAndPassReceiver());
  blob_registry.RegisterFromDataItem(
      registry_blob_.BindNewPipeAndPassReceiver(), uuid_, std::move(element));
}

void BlobReader::AddReceiver(
    mojo::PendingReceiver<blink::mojom::Blob> receiver,
    storage::mojom::BlobStorageContext& blob_registry) {
  if (!registry_blob_.is_bound()) {
    CHECK(receivers_.empty());
    BindRegistryBlob(blob_registry);
  }
  Clone(std::move(receiver));
}

void BlobReader::OnMojoDisconnect() {
  if (!receivers_.empty() || !data_pipe_getter_receivers_.empty()) {
    return;
  }

  // Unregistering the blob will drop its reference to the `BlobDataItem`
  // associated with `this` as a `BlobDataItemReader`, which will often lead to
  // `readers_` receiving a disconnect. But there may still be other references
  // to the `BlobDataItem`, such as another blob, which means that `this` can go
  // on living indefinitely. See crbug.com/392376370
  registry_blob_.reset();

  if (readers_.empty()) {
    std::move(on_last_receiver_disconnected_).Run();
    // `this` is deleted.
  }
}

}  // namespace content::indexed_db
