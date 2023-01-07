// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/url_loader_client_checker.h"

namespace content {

URLLoaderClientCheckedRemote::URLLoaderClientCheckedRemote(
    mojo::PendingRemote<network::mojom::URLLoaderClient> client)
    : proxy_(std::move(client)) {}

URLLoaderClientCheckedRemote::Proxy::Proxy(
    mojo::PendingRemote<network::mojom::URLLoaderClient> client)
    : client_(std::move(client)) {}

URLLoaderClientCheckedRemote::Proxy::~Proxy() = default;

}  // namespace content
