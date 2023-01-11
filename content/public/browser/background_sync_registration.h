// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_REGISTRATION_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_REGISTRATION_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"
#include "url/origin.h"

namespace content {

class CONTENT_EXPORT BackgroundSyncRegistration {
 public:
  BackgroundSyncRegistration();
  BackgroundSyncRegistration(const BackgroundSyncRegistration& other);
  BackgroundSyncRegistration& operator=(
      const BackgroundSyncRegistration& other);
  ~BackgroundSyncRegistration();

  bool Equals(const BackgroundSyncRegistration& other) const;
  bool IsFiring() const;

  const blink::mojom::SyncRegistrationOptions* options() const {
    return &options_;
  }
  blink::mojom::SyncRegistrationOptions* options() { return &options_; }
  blink::mojom::BackgroundSyncState sync_state() const { return sync_state_; }
  void set_sync_state(blink::mojom::BackgroundSyncState state) {
    sync_state_ = state;
  }

  int num_attempts() const { return num_attempts_; }
  void set_num_attempts(int num_attempts) { num_attempts_ = num_attempts; }
  int max_attempts() const { return max_attempts_; }
  void set_max_attempts(int max_attempts) { max_attempts_ = max_attempts; }

  base::Time delay_until() const { return delay_until_; }
  void set_delay_until(base::Time delay_until) { delay_until_ = delay_until; }

  // By default, new registrations will not fire until set_resolved is called
  // after the registration resolves.
  bool resolved() const { return resolved_; }
  void set_resolved() { resolved_ = true; }

  // Whether the registration is periodic or one-shot.
  blink::mojom::BackgroundSyncType sync_type() const {
    return options_.min_interval >= 0
               ? blink::mojom::BackgroundSyncType::PERIODIC
               : blink::mojom::BackgroundSyncType::ONE_SHOT;
  }

  const url::Origin& origin() const { return origin_; }

  void set_origin(const url::Origin& origin) { origin_ = origin; }

  bool is_suspended() const {
    if (sync_type() == blink::mojom::BackgroundSyncType::ONE_SHOT)
      return false;
    return delay_until_.is_max();
  }

 private:
  blink::mojom::SyncRegistrationOptions options_;
  blink::mojom::BackgroundSyncState sync_state_ =
      blink::mojom::BackgroundSyncState::PENDING;
  int num_attempts_ = 0;
  int max_attempts_ = 0;
  base::Time delay_until_;
  url::Origin origin_;

  // This member is not persisted to disk. It should be false until the client
  // has acknowledged that it has resolved its registration promise.
  bool resolved_ = false;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_REGISTRATION_H_
