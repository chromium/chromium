// Copyright 2020 The Chromium Authors. All rights reserved.
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

class IdleTimeProvider;

// Provides an interface for handling mojo connections from the renderer,
// keeping track of clients that are monitoring the user's idle state.
class IdleManager {
 public:
  // Provides an interface for calculating a user's idle time and screen state.
  class IdleTimeProvider {
   public:
    IdleTimeProvider() = default;
    virtual ~IdleTimeProvider() = default;

    IdleTimeProvider(const IdleTimeProvider&) = delete;
    IdleTimeProvider& operator=(const IdleTimeProvider&) = delete;

    // See ui/base/idle/idle.h for the semantics of these methods.
    virtual base::TimeDelta CalculateIdleTime() = 0;
    virtual bool CheckIdleStateIsLocked() = 0;
  };

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
