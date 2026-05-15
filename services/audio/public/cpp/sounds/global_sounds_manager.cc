// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/sounds/global_sounds_manager.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"

namespace audio {
namespace {

SoundsManager* g_instance = nullptr;
bool g_initialized_for_testing = false;

}  // namespace

// static
void GlobalSoundsManager::Create(
    SoundsManager::StreamFactoryBinder stream_factory_binder) {
  CHECK(!g_instance || g_initialized_for_testing)
      << "`GlobalSoundsManager::Create` is called twice";
  if (g_initialized_for_testing) {
    CHECK_IS_TEST();
    return;
  }
  g_instance =
      SoundsManager::Create(std::move(stream_factory_binder)).release();
}

// static
void GlobalSoundsManager::Shutdown() {
  CHECK(g_instance) << "`GlobalSoundsManager::Shutdown` is called "
                       "without previous call to `GlobalSoundsManager::Create`";
  delete g_instance;
  g_instance = nullptr;
  g_initialized_for_testing = false;
}

// static
SoundsManager& GlobalSoundsManager::Get() {
  return CHECK_DEREF(g_instance);
}

// static
void GlobalSoundsManager::InitializeForTesting(
    std::unique_ptr<SoundsManager> manager) {
  CHECK_IS_TEST();
  CHECK(!g_instance) << "`GlobalSoundsManager` is already initialized.";
  CHECK(manager);
  g_instance = manager.release();
  g_initialized_for_testing = true;
}

}  // namespace audio
