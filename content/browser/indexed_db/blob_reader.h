// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_BLOB_READER_H_
#define CONTENT_BROWSER_INDEXED_DB_BLOB_READER_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"

namespace content::indexed_db {

class BlobReader : public storage::mojom::BlobDataItemReader {
 public:
  BlobReader(const base::FilePath& file_path,
             base::OnceClosure on_last_receiver_disconnected);
  ~BlobReader() override;

  BlobReader(const BlobReader&) = delete;
  BlobReader& operator=(const BlobReader&) = delete;

  void AddReader(mojo::PendingReceiver<BlobDataItemReader> receiver);

  // storage::mojom::BlobDataItemReader:
  void Read(uint64_t offset,
            uint64_t length,
            mojo::ScopedDataPipeProducerHandle pipe,
            storage::mojom::BlobDataItemReader::ReadCallback callback) override;
  void ReadSideData(storage::mojom::BlobDataItemReader::ReadSideDataCallback
                        callback) override;

 private:
  void OnMojoDisconnect();

  const base::FilePath file_path_;

  mojo::ReceiverSet<storage::mojom::BlobDataItemReader> readers_;

  base::OnceClosure on_last_receiver_disconnected_;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_BLOB_READER_H_
