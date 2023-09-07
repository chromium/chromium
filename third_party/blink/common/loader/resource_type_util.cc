// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/resource_type_util.h"

#include "services/network/public/cpp/request_destination.h"

namespace blink {

bool IsRequestDestinationFrame(network::mojom::RequestDestination destination) {
  // kObject and kEmbed can also be destinations for a frame navigation.
  return destination == network::mojom::RequestDestination::kDocument ||
         destination == network::mojom::RequestDestination::kObject ||
         destination == network::mojom::RequestDestination::kEmbed ||
         network::IsRequestDestinationEmbeddedFrame(destination);
}

}  // namespace blink
