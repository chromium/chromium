// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_BADGE_SERVICE_H_
#define CONTENT_TEST_MOCK_BADGE_SERVICE_H_

#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/badging/badging.mojom.h"

namespace content {

// Implements a mock BadgeService. This implementation does nothing, but is
// required because inbound Mojo messages which do not have a registered
// handler are considered an error, and the render process is terminated.
//
// This is to support Web Platform Tests. Blink web tests do not require
// this as they use a JavaScript implementation of the BadgeService that
// intercepts the outbound Mojo calls before they leave the renderer.
class MockBadgeService : public blink::mojom::BadgeService {
 public:
  MockBadgeService();
  MockBadgeService(const MockBadgeService&) = delete;
  MockBadgeService& operator=(const MockBadgeService&) = delete;
  ~MockBadgeService() override;

  void Bind(mojo::PendingReceiver<blink::mojom::BadgeService> receiver);
  void Reset();

  // blink::mojom::BadgeService:
  void SetBadge(blink::mojom::BadgeValuePtr value) override;
  void ClearBadge() override;

 private:
  mojo::ReceiverSet<blink::mojom::BadgeService> receivers_;
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_BADGE_SERVICE_H_
