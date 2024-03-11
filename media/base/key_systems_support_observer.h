// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_KEY_SYSTEMS_SUPPORT_OBSERVER_H_
#define MEDIA_BASE_KEY_SYSTEMS_SUPPORT_OBSERVER_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "media/base/key_system_capability.h"
#include "media/base/media_export.h"

namespace media {

using KeySystemCapabilities =
    base::flat_map<std::string, media::KeySystemCapability>;
using KeySystemSupportCB = base::RepeatingCallback<void(KeySystemCapabilities)>;

class MEDIA_EXPORT KeySystemSupportObserver {
 public:
  KeySystemSupportObserver();
  KeySystemSupportObserver(const KeySystemSupportObserver&) = delete;
  KeySystemSupportObserver& operator=(const KeySystemSupportObserver&) = delete;
  virtual ~KeySystemSupportObserver();

  virtual void OnKeySystemSupportUpdated(
      const KeySystemCapabilities& key_system_capabilities) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_KEY_SYSTEMS_SUPPORT_OBSERVER_H_
