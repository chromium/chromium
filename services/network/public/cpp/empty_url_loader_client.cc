// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/empty_url_loader_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {

// static
void EmptyURLLoaderClientWrapper::DrainURLRequest(
    mojo::PendingReceiver<mojom::URLLoaderClient> client_receiver,
    mojo::PendingRemote<mojom::URLLoader> url_loader) {
  // Raw |new| is okay, because the object will delete itself.
  new EmptyURLLoaderClientWrapper(std::move(client_receiver),
                                  std::move(url_loader));
}

EmptyURLLoaderClientWrapper::EmptyURLLoaderClientWrapper(
    mojo::PendingReceiver<mojom::URLLoaderClient> receiver,
    mojo::PendingRemote<mojom::URLLoader> url_loader)
    : receiver_(&client_, std::move(receiver)),
      url_loader_(std::move(url_loader)) {
  client_.Drain(base::BindOnce(&EmptyURLLoaderClientWrapper::DidDrain,
                               base::Unretained(this)));
  receiver_.set_disconnect_handler(base::BindOnce(
      &EmptyURLLoaderClientWrapper::DeleteSelf, base::Unretained(this)));
}

EmptyURLLoaderClientWrapper::~EmptyURLLoaderClientWrapper() = default;

void EmptyURLLoaderClientWrapper::DidDrain(
    const network::URLLoaderCompletionStatus& status) {
  DeleteSelf();
}

void EmptyURLLoaderClientWrapper::DeleteSelf() {
  delete this;
}

EmptyURLLoaderClient::EmptyURLLoaderClient() = default;
EmptyURLLoaderClient::~EmptyURLLoaderClient() = default;

void EmptyURLLoaderClient::Drain(
    base::OnceCallback<void(const URLLoaderCompletionStatus&)> callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);
  MaybeDone();
}

void EmptyURLLoaderClient::MaybeDone() {
  if (done_status_ && !response_body_drainer_ && callback_)
    std::move(callback_).Run(*done_status_);
}

void EmptyURLLoaderClient::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {}

void EmptyURLLoaderClient::OnReceiveResponse(
    const mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  if (!body)
    return;

  // TODO(bashi): Consider failing the request rather than DCHECK in case a
  // URLLoader is misbehaved.
  DCHECK(!response_body_drainer_);
  response_body_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(body));
}

void EmptyURLLoaderClient::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    mojom::URLResponseHeadPtr head) {}

void EmptyURLLoaderClient::OnUploadProgress(int64_t current_position,
                                            int64_t total_size,
                                            OnUploadProgressCallback callback) {
  std::move(callback).Run();
}

void EmptyURLLoaderClient::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kEmptyURLLoaderClient);
}

void EmptyURLLoaderClient::OnComplete(const URLLoaderCompletionStatus& status) {
  done_status_ = status;
  MaybeDone();
}

void EmptyURLLoaderClient::OnDataAvailable(base::span<const uint8_t> data) {}

void EmptyURLLoaderClient::OnDataComplete() {
  response_body_drainer_.reset();
  MaybeDone();
}

}  // namespace network
