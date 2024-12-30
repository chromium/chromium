// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/blob_reader.h"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "content/browser/indexed_db/file_stream_reader_to_data_pipe.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"

namespace content::indexed_db {

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

BlobReader::BlobReader(const base::FilePath& file_path,
                       base::OnceClosure on_last_receiver_disconnected)
    : file_path_(file_path),
      on_last_receiver_disconnected_(std::move(on_last_receiver_disconnected)) {
  readers_.set_disconnect_handler(base::BindRepeating(
      &BlobReader::OnMojoDisconnect, base::Unretained(this)));
}

void BlobReader::AddReader(mojo::PendingReceiver<BlobDataItemReader> receiver) {
  readers_.Add(this, std::move(receiver));
}

BlobReader::~BlobReader() = default;

void BlobReader::OnMojoDisconnect() {
  if (readers_.empty()) {
    std::move(on_last_receiver_disconnected_).Run();
    // `this` is deleted.
  }
}

}  // namespace content::indexed_db
