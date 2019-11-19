// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BLOB_READER_H_
#define EXTENSIONS_BROWSER_BLOB_READER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

// This class may only be used from the UI thread.
class BlobReader : public blink::mojom::BlobReaderClient,
                   public mojo::DataPipeDrainer::Client {
 public:
  // |blob_data| contains the portion of the Blob requested. |blob_total_size|
  // is the total size of the Blob, and may be larger than |blob_data->size()|.
  // |blob_total_size| is -1 if it cannot be determined.
  typedef base::OnceCallback<void(std::unique_ptr<std::string> blob_data,
                                  int64_t blob_total_size)>
      BlobReadCallback;

  static void Read(content::BrowserContext* browser_context,
                   const std::string& blob_uuid,
                   BlobReadCallback callback,
                   int64_t offset,
                   int64_t length);
  static void Read(content::BrowserContext* browser_context,
                   const std::string& blob_uuid,
                   BlobReadCallback callback);

  ~BlobReader() override;

 private:
  struct Range {
    uint64_t offset;
    uint64_t length;
  };

  static void Read(content::BrowserContext* browser_context,
                   const std::string& blob_uuid,
                   BlobReadCallback callback,
                   base::Optional<BlobReader::Range> range);

  BlobReader(mojo::PendingRemote<blink::mojom::Blob> blob,
             base::Optional<Range> range);
  void Start(base::OnceClosure callback);

  // blink::mojom::BlobReaderClient:
  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override;
  void OnComplete(int32_t status, uint64_t data_length) override {}

  // mojo::DataPipeDrainer:
  void OnDataAvailable(const void* data, size_t num_bytes) override;
  void OnDataComplete() override;

  void Failed();
  void Succeeded();

  base::OnceClosure callback_;
  mojo::Remote<blink::mojom::Blob> blob_;
  base::Optional<Range> read_range_;

  mojo::Receiver<blink::mojom::BlobReaderClient> receiver_{this};
  std::unique_ptr<mojo::DataPipeDrainer> data_pipe_drainer_;

  base::Optional<uint64_t> blob_length_;
  std::unique_ptr<std::string> blob_data_;
  bool data_complete_ = false;

  DISALLOW_COPY_AND_ASSIGN(BlobReader);
};

#endif  // EXTENSIONS_BROWSER_BLOB_READER_H_
