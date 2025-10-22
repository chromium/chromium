// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_FAKE_BLOB_H_
#define STORAGE_BROWSER_TEST_FAKE_BLOB_H_

#include <optional>
#include <string>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace storage {

// Implementation of the blink::mojom::Blob interface. Only supports the Clone
// and GetInternalUUID methods.
class FakeBlob : public blink::mojom::Blob {
 public:
  explicit FakeBlob(const std::string& uuid);
  ~FakeBlob() override;

  // If no body is set, then reading will return an error through the
  // BlobReaderClient interface. It's valid to set an empty body.
  void set_body(std::string_view body) { body_ = std::string(body); }

  mojo::PendingRemote<blink::mojom::Blob> Clone();
  void Clone(mojo::PendingReceiver<blink::mojom::Blob> receiver) override;
  void AsDataPipeGetter(
      mojo::PendingReceiver<network::mojom::DataPipeGetter>) override;
  void ReadRange(uint64_t offset,
                 uint64_t size,
                 mojo::ScopedDataPipeProducerHandle,
                 mojo::PendingRemote<blink::mojom::BlobReaderClient>) override;
  void ReadAll(mojo::ScopedDataPipeProducerHandle,
               mojo::PendingRemote<blink::mojom::BlobReaderClient>) override;
  void Load(mojo::PendingReceiver<network::mojom::URLLoader>,
            const std::string& method,
            const net::HttpRequestHeaders&,
            mojo::PendingRemote<network::mojom::URLLoaderClient>) override;
  void ReadSideData(ReadSideDataCallback) override;
  void CaptureSnapshot(CaptureSnapshotCallback) override;
  void GetInternalUUID(GetInternalUUIDCallback callback) override;

 private:
  const std::string uuid_;
  std::optional<std::string> body_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_FAKE_BLOB_H_
