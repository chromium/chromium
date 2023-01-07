// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_IDLE_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_IDLE_MANAGER_H_

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom.h"

namespace url {
class Origin;
}

namespace content {

// Provides an interface for handling mojo connections from the renderer,
// keeping track of clients that are monitoring the user's idle state.
class IdleManager {
 public:
  IdleManager() = default;
  virtual ~IdleManager() = default;

  IdleManager(const IdleManager&) = delete;
  IdleManager& operator=(const IdleManager&) = delete;

  virtual void CreateService(
      mojo::PendingReceiver<blink::mojom::IdleManager> receiver,
      const url::Origin& origin) = 0;

  virtual void SetIdleOverride(blink::mojom::UserIdleState user_state,
                               blink::mojom::ScreenIdleState screen_state) = 0;
  virtual void ClearIdleOverride() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_IDLE_MANAGER_H_
