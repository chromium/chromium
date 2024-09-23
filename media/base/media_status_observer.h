// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_STATUS_OBSERVER_H_
#define MEDIA_BASE_MEDIA_STATUS_OBSERVER_H_

#include "media/base/media_status.h"

namespace media {

// Describes the current state of media being controlled via the
// FlingingController interface. This is a copy of
// media_router.mojom.MediaStatus interface, without the cast specific portions.
// TODO(crbug.com/41375562): Deduplicate media_router::MediaStatus.
class MediaStatusObserver {
 public:
  virtual ~MediaStatusObserver() = default;

  virtual void OnMediaStatusUpdated(const MediaStatus& status) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_STATUS_OBSERVER_H_
