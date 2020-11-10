// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_keys_listener_manager.h"

#if !defined(OS_CHROMEOS)
#include "base/feature_list.h"
#include "media/base/media_switches.h"
#endif

namespace content {

// static
bool MediaKeysListenerManager::IsMediaKeysListenerManagerEnabled() {
#if defined(OS_CHROMEOS)
  return false;
#else
  return base::FeatureList::IsEnabled(media::kHardwareMediaKeyHandling);
#endif
}

MediaKeysListenerManager::~MediaKeysListenerManager() = default;

}  // namespace content
