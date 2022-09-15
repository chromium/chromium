// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_badge_service.h"

#include <utility>

namespace content {

MockBadgeService::MockBadgeService() = default;

MockBadgeService::~MockBadgeService() = default;

void MockBadgeService::Bind(
    mojo::PendingReceiver<blink::mojom::BadgeService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MockBadgeService::Reset() {}

void MockBadgeService::SetBadge(blink::mojom::BadgeValuePtr value) {}

void MockBadgeService::ClearBadge() {}

}  // namespace content
