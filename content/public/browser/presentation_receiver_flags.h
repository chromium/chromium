// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRESENTATION_RECEIVER_FLAGS_H_
#define CONTENT_PUBLIC_BROWSER_PRESENTATION_RECEIVER_FLAGS_H_

#include "services/network/public/cpp/web_sandbox_flags.h"

namespace content {

// Disable popups, modals, and top-level navigation for presentation receivers.
// See:
// https://w3c.github.io/presentation-api/#creating-a-receiving-browsing-context
constexpr network::mojom::WebSandboxFlags kPresentationReceiverSandboxFlags =
    network::mojom::WebSandboxFlags::kModals |
    network::mojom::WebSandboxFlags::kPopups |
    network::mojom::WebSandboxFlags::kTopNavigation;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRESENTATION_RECEIVER_FLAGS_H_
