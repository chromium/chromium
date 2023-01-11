// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_PERMISSION_H_
#define MEDIA_BASE_MEDIA_PERMISSION_H_

#include "base/functional/callback_forward.h"
#include "media/base/media_export.h"

namespace media {

// Interface to handle media related permission checks and requests.
class MEDIA_EXPORT MediaPermission {
 public:
  using PermissionStatusCB = base::OnceCallback<void(bool)>;

  enum Type {
    PROTECTED_MEDIA_IDENTIFIER,
    AUDIO_CAPTURE,
    VIDEO_CAPTURE,
  };

  MediaPermission();

  MediaPermission(const MediaPermission&) = delete;
  MediaPermission& operator=(const MediaPermission&) = delete;

  virtual ~MediaPermission();

  // Checks whether |type| is permitted without triggering user interaction
  // (e.g. permission prompt). The status will be |false| if the permission
  // has never been set.
  virtual void HasPermission(Type type,
                             PermissionStatusCB permission_status_cb) = 0;

  // Requests |type| permission. This may trigger user interaction
  // (e.g. permission prompt) if the permission has never been set.
  virtual void RequestPermission(Type type,
                                 PermissionStatusCB permission_status_cb) = 0;

  // Whether to allow the use of Encrypted Media Extensions (EME), except for
  // the use of Clear Key key systems, which is always allowed as required by
  // the spec.
  virtual bool IsEncryptedMediaEnabled() = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_PERMISSION_H_
