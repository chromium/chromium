// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_KEY_SYSTEMS_SUPPORT_REGISTRATION_H_
#define MEDIA_BASE_KEY_SYSTEMS_SUPPORT_REGISTRATION_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "media/base/key_system_capability.h"
#include "media/base/media_export.h"

namespace media {

using KeySystemCapabilities =
    base::flat_map<std::string, media::KeySystemCapability>;
using KeySystemSupportCB = base::RepeatingCallback<void(KeySystemCapabilities)>;

// A class that is used to keep the KeySystemSupport and
// KeySystemSupportObserver mojo channel registered between renderer and
// browser alive and destructed properly.
class MEDIA_EXPORT KeySystemSupportRegistration {
 public:
  KeySystemSupportRegistration();
  KeySystemSupportRegistration(const KeySystemSupportRegistration&) = delete;
  KeySystemSupportRegistration& operator=(const KeySystemSupportRegistration&) =
      delete;
  virtual ~KeySystemSupportRegistration();
};

}  // namespace media

#endif  // MEDIA_BASE_KEY_SYSTEMS_SUPPORT_REGISTRATION_H_
