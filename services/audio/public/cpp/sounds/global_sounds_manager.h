// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_GLOBAL_SOUNDS_MANAGER_H_
#define SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_GLOBAL_SOUNDS_MANAGER_H_

#include <memory>

#include "base/component_export.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"

namespace audio {

class COMPONENT_EXPORT(AUDIO_PUBLIC_CPP) GlobalSoundsManager {
 public:
  // Creates a singleton instance of the `SoundsManager`.
  static void Create(SoundsManager::StreamFactoryBinder stream_factory_binder);

  // Removes a singleton instance of the `SoundsManager`.
  static void Shutdown();

  // Returns a reference to a singleton instance of the `SoundsManager`.
  // `GlobalSoundsManager::Create` must be called before this method.
  static SoundsManager& Get();

  // Initializes `SoundsManager` for testing. The `manager` will be owned
  // by the internal pointer and will be deleted by
  // `GlobalSoundsManager::Shutdown`.
  static void InitializeForTesting(std::unique_ptr<SoundsManager> manager);

  GlobalSoundsManager() = delete;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_GLOBAL_SOUNDS_MANAGER_H_
