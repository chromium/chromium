// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DATA_URL_BLOB_READER_H_
#define CONTENT_BROWSER_DOWNLOAD_DATA_URL_BLOB_READER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "url/gurl.h"

namespace storage {
class BlobDataHandle;
}  // namespace storage

namespace content {

// Helper class to read a data url from a BlobDataHandle.
class CONTENT_EXPORT DataURLBlobReader : public mojo::DataPipeDrainer::Client {
 public:
  using ReadCompletionCallback = base::OnceCallback<void(GURL)>;

  // Read the data URL from |blob_data_handle|, and call
  // |read_completion_callback| once it completes. If the data URL cannot be
  // retrieved, |read_completion_callback| will be called with an empty URL.
  // This method must be called on the UI thread.
  static void ReadDataURLFromBlob(
      mojo::PendingRemote<blink::mojom::Blob> data_url_blob,
      ReadCompletionCallback read_completion_callback);

  ~DataURLBlobReader() override;

 private:
  DataURLBlobReader(mojo::PendingRemote<blink::mojom::Blob> data_url_blob);

  // Starts reading from the |data_url_blob_| and calls |callback| once
  // completes.
  void Start(base::OnceClosure callback);

  // mojo::DataPipeDrainer:
  void OnDataAvailable(const void* data, size_t num_bytes) override;
  void OnDataComplete() override;

  // Called when failed to read from blob.
  void OnFailed();

  std::unique_ptr<mojo::DataPipeDrainer> data_pipe_drainer_;

  mojo::Remote<blink::mojom::Blob> data_url_blob_;

  // Data URL retrieved from the blob.
  std::string url_data_;

  // Callback to run once blob data is read.
  base::OnceClosure callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(DataURLBlobReader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DATA_URL_BLOB_READER_H_
