// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/testing/fake_resource_load_info_notifier.h"

#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

namespace blink {

FakeResourceLoadInfoNotifier::FakeResourceLoadInfoNotifier() = default;
FakeResourceLoadInfoNotifier::~FakeResourceLoadInfoNotifier() = default;

void FakeResourceLoadInfoNotifier::NotifyResourceLoadCompleted(
    blink::mojom::ResourceLoadInfoPtr resource_load_info,
    const ::network::URLLoaderCompletionStatus& status) {
  resource_load_info_ = std::move(resource_load_info);
}

std::string FakeResourceLoadInfoNotifier::GetMimeType() {
  return resource_load_info_->mime_type;
}

}  // namespace blink
