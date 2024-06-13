// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BLOB_READER_H_
#define EXTENSIONS_BROWSER_BLOB_READER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

// This class may only be used from the UI thread.
class BlobReader : public blink::mojom::BlobReaderClient,
                   public mojo::DataPipeDrainer::Client {
 public:
  // `blob_data` contains the portion of the Blob requested. `blob_total_size`
  // is the total size of the Blob, and may be larger than `blob_data->size()`.
  // `blob_total_size` is 0 if it cannot be determined.
  using BlobReadCallback =
      base::OnceCallback<void(std::string blob_data, int64_t blob_total_size)>;

  static void Read(mojo::PendingRemote<blink::mojom::Blob> blob,
                   BlobReadCallback callback,
                   uint64_t offset,
                   uint64_t length);

  static void Read(mojo::PendingRemote<blink::mojom::Blob> blob,
                   BlobReadCallback callback);

  BlobReader(const BlobReader&) = delete;
  BlobReader& operator=(const BlobReader&) = delete;

  ~BlobReader() override;

 private:
  struct Range {
    uint64_t offset;
    uint64_t length;
  };

  static void Read(mojo::PendingRemote<blink::mojom::Blob> blob,
                   BlobReadCallback callback,
                   std::optional<Range> range);

  BlobReader(mojo::PendingRemote<blink::mojom::Blob> blob,
             std::optional<Range> range);
  void Start(base::OnceClosure callback);

  // blink::mojom::BlobReaderClient:
  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override;
  void OnComplete(int32_t status, uint64_t data_length) override {}

  // mojo::DataPipeDrainer:
  void OnDataAvailable(base::span<const uint8_t> data) override;
  void OnDataComplete() override;

  void Failed();
  void Succeeded();

  base::OnceClosure callback_;
  mojo::Remote<blink::mojom::Blob> blob_;
  std::optional<Range> read_range_;

  mojo::Receiver<blink::mojom::BlobReaderClient> receiver_{this};
  std::unique_ptr<mojo::DataPipeDrainer> data_pipe_drainer_;

  std::optional<uint64_t> blob_length_;
  std::string blob_data_;
  bool data_complete_ = false;
};

#endif  // EXTENSIONS_BROWSER_BLOB_READER_H_
