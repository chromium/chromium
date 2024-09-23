// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/testing/fake_url_loader_factory_for_background_thread.h"

namespace blink {

class FakeURLLoaderFactoryForBackgroundThread::PendingFactory
    : public network::PendingSharedURLLoaderFactory {
 public:
  explicit PendingFactory(LoadStartCallback load_start_callback)
      : load_start_callback_(std::move(load_start_callback)) {}
  PendingFactory(const PendingFactory&) = delete;
  PendingFactory& operator=(const PendingFactory&) = delete;
  ~PendingFactory() override = default;

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override {
    CHECK(load_start_callback_);
    return base::MakeRefCounted<FakeURLLoaderFactoryForBackgroundThread>(
        std::move(load_start_callback_));
  }

 private:
  LoadStartCallback load_start_callback_;
};

FakeURLLoaderFactoryForBackgroundThread::
    FakeURLLoaderFactoryForBackgroundThread(
        LoadStartCallback load_start_callback)
    : load_start_callback_(std::move(load_start_callback)) {}

FakeURLLoaderFactoryForBackgroundThread::
    ~FakeURLLoaderFactoryForBackgroundThread() = default;

void FakeURLLoaderFactoryForBackgroundThread::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  CHECK(load_start_callback_);
  std::move(load_start_callback_).Run(std::move(loader), std::move(client));
}

void FakeURLLoaderFactoryForBackgroundThread::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  // Pass |this| as the receiver context to make sure this object stays alive
  // while it still has receivers.
  receivers_.Add(this, std::move(receiver), this);
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
FakeURLLoaderFactoryForBackgroundThread::Clone() {
  CHECK(load_start_callback_);
  return std::make_unique<PendingFactory>(std::move(load_start_callback_));
}

}  // namespace blink
