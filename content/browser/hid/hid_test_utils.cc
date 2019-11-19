// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/hid/hid_test_utils.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"

#include <utility>

#include "base/callback.h"

namespace content {

MockHidDelegate::MockHidDelegate() = default;

MockHidDelegate::~MockHidDelegate() = default;

std::unique_ptr<HidChooser> MockHidDelegate::RunChooser(
    RenderFrameHost* frame,
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    HidChooser::Callback callback) {
  std::move(callback).Run(RunChooserInternal());
  return nullptr;
}

HidTestContentBrowserClient::HidTestContentBrowserClient() = default;

HidTestContentBrowserClient::~HidTestContentBrowserClient() = default;

HidDelegate* HidTestContentBrowserClient::GetHidDelegate() {
  return &delegate_;
}

}  // namespace content
