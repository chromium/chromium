// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_OVERLAY_INFO_H_
#define MEDIA_BASE_OVERLAY_INFO_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/unguessable_token.h"
#include "media/base/media_export.h"

namespace media {

struct MEDIA_EXPORT OverlayInfo {
  // An unset routing token indicates "do not use any routing token".  A null
  // routing token isn't serializable, else we'd probably use that instead.
  using RoutingToken = std::optional<base::UnguessableToken>;

  OverlayInfo();
  OverlayInfo(const OverlayInfo&);
  OverlayInfo& operator=(const OverlayInfo&);

  // Convenience functions to return true if and only if this specifies a
  // surface ID / routing token that is not kNoSurfaceID / empty.  I.e., if we
  // provide enough info to create an overlay.
  bool HasValidRoutingToken() const;

  // Whether |other| refers to the same (surface_id, routing_token) pair as
  // |this|.
  bool RefersToSameOverlayAs(const OverlayInfo& other);

  // The routing token for AndroidOverlay, if any.
  RoutingToken routing_token;

  // Is the player in fullscreen?
  bool is_fullscreen = false;

  // Is the player persistent video (PiP)?
  bool is_persistent_video = false;
};

// Used by the WebMediaPlayer to provide overlay information to the decoder,
// which can ask for that information repeatedly (see
// WebMediaPlayerImpl::OnOverlayInfoRequested).
using ProvideOverlayInfoCB = base::RepeatingCallback<void(const OverlayInfo&)>;
using RequestOverlayInfoCB =
    base::RepeatingCallback<void(bool, ProvideOverlayInfoCB)>;

}  // namespace media

#endif  // MEDIA_BASE_OVERLAY_INFO_H_
