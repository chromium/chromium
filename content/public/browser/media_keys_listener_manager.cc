// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_keys_listener_manager.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/feature_list.h"
#include "media/base/media_switches.h"
#endif

namespace content {

// static
bool MediaKeysListenerManager::IsMediaKeysListenerManagerEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  return false;
#else
  return base::FeatureList::IsEnabled(media::kHardwareMediaKeyHandling);
#endif
}

MediaKeysListenerManager::~MediaKeysListenerManager() = default;

}  // namespace content
