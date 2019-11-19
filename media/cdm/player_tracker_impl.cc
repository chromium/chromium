// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/player_tracker_impl.h"

#include <utility>

#include "base/stl_util.h"

namespace media {

PlayerTrackerImpl::PlayerCallbacks::PlayerCallbacks(
    const base::Closure& new_key_cb,
    const base::Closure& cdm_unset_cb)
    : new_key_cb(new_key_cb), cdm_unset_cb(cdm_unset_cb) {}

PlayerTrackerImpl::PlayerCallbacks::PlayerCallbacks(
    const PlayerCallbacks& other) = default;

PlayerTrackerImpl::PlayerCallbacks::~PlayerCallbacks() = default;

PlayerTrackerImpl::PlayerTrackerImpl() : next_registration_id_(1) {
}

PlayerTrackerImpl::~PlayerTrackerImpl() = default;

int PlayerTrackerImpl::RegisterPlayer(const base::Closure& new_key_cb,
                                      const base::Closure& cdm_unset_cb) {
  base::AutoLock lock(lock_);
  int registration_id = next_registration_id_++;
  DCHECK(!base::Contains(player_callbacks_map_, registration_id));
  player_callbacks_map_.insert(std::make_pair(
      registration_id, PlayerCallbacks(new_key_cb, cdm_unset_cb)));
  return registration_id;
}

void PlayerTrackerImpl::UnregisterPlayer(int registration_id) {
  base::AutoLock lock(lock_);
  DCHECK(base::Contains(player_callbacks_map_, registration_id))
      << registration_id;
  player_callbacks_map_.erase(registration_id);
}

void PlayerTrackerImpl::NotifyNewKey() {
  std::vector<base::Closure> new_key_callbacks;

  {
    base::AutoLock lock(lock_);
    for (const auto& entry : player_callbacks_map_)
      new_key_callbacks.push_back(entry.second.new_key_cb);
  }

  for (const auto& cb : new_key_callbacks)
    cb.Run();
}

void PlayerTrackerImpl::NotifyCdmUnset() {
  std::vector<base::Closure> cdm_unset_callbacks;

  {
    base::AutoLock lock(lock_);
    for (const auto& entry : player_callbacks_map_)
      cdm_unset_callbacks.push_back(entry.second.cdm_unset_cb);
  }

  for (const auto& cb : cdm_unset_callbacks)
    cb.Run();
}

}  // namespace media
