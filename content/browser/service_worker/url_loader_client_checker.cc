// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/url_loader_client_checker.h"

#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/notreached.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/mojom/early_hints.mojom.h"

namespace content {

URLLoaderClientCheckedRemote::URLLoaderClientCheckedRemote(
    mojo::PendingRemote<network::mojom::URLLoaderClient> client)
    : proxy_(std::move(client)) {}

URLLoaderClientCheckedRemote::Proxy::Proxy(
    mojo::PendingRemote<network::mojom::URLLoaderClient> client)
    : client_(std::move(client)) {}

URLLoaderClientCheckedRemote::Proxy::~Proxy() = default;

void URLLoaderClientCheckedRemote::Proxy::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  client_->OnReceiveEarlyHints(std::move(early_hints));
}

void URLLoaderClientCheckedRemote::Proxy::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kURLLoaderClientCheckedRemote);
  client_->OnTransferSizeUpdated(transfer_size_diff);
}

NOINLINE void URLLoaderClientCheckedRemote::Proxy::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (status.error_code == net::OK && !on_receive_response_called_) {
    NOTREACHED_IN_MIGRATION();
    base::debug::DumpWithoutCrashing();
    NO_CODE_FOLDING();
  }
  client_->OnComplete(status);
}

}  // namespace content
