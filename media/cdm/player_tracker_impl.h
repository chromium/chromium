// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PLAYER_TRACKER_IMPL_H_
#define MEDIA_CDM_PLAYER_TRACKER_IMPL_H_

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "media/base/media_export.h"
#include "media/base/player_tracker.h"

namespace media {

// A common implementation that can be shared by different PlayerTracker
// implementations. This class is thread safe and can be called on any thread.
class MEDIA_EXPORT PlayerTrackerImpl : public PlayerTracker {
 public:
  PlayerTrackerImpl();
  ~PlayerTrackerImpl() override;

  // PlayerTracker implementation.
  int RegisterPlayer(const base::Closure& new_key_cb,
                     const base::Closure& cdm_unset_cb) override;
  void UnregisterPlayer(int registration_id) override;

  // Helpers methods to fire registered callbacks.
  void NotifyNewKey();
  void NotifyCdmUnset();

 private:
  struct PlayerCallbacks {
    PlayerCallbacks(const base::Closure& new_key_cb,
                    const base::Closure& cdm_unset_cb);
    PlayerCallbacks(const PlayerCallbacks& other);
    ~PlayerCallbacks();

    base::Closure new_key_cb;
    base::Closure cdm_unset_cb;
  };

  base::Lock lock_;
  int next_registration_id_ GUARDED_BY(lock_);
  std::map<int, PlayerCallbacks> player_callbacks_map_ GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(PlayerTrackerImpl);
};

}  // namespace media

#endif  // MEDIA_CDM_PLAYER_TRACKER_IMPL_H_
