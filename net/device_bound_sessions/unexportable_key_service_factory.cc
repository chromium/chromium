// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/unexportable_key_service_factory.h"

#include "base/logging.h"
#include "build/branding_buildflags.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/unexportable_key.h"

namespace {

// TODO: crbug.com/443932320 - Replace all usages of this class with //chrome's
// `UnexportableKeyServiceFactory`. This already uses the version constants.

// Returns the config to use for creating unexportable key service.
crypto::UnexportableKeyProvider::Config GetConfig() {
  return {
#if BUILDFLAG(IS_MAC)
      .keychain_access_group =
// Ideally we'd just use `MAC_TEAM_IDENTIFIER_STRING` and
// `MAC_BUNDLE_IDENTIFIER_STRING`, but we can't depend on //chrome here.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          "EQHXZ8M8AV.com.google.Chrome"
#else
          ".org.chromium.Chromium"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
          ".unexportable-keys",
#endif  // BUILDFLAG(IS_MAC)
  };
}

std::unique_ptr<unexportable_keys::UnexportableKeyTaskManager>
CreateTaskManagerInstance() {
  if (!unexportable_keys::UnexportableKeyServiceImpl::
          IsUnexportableKeyProviderSupported(GetConfig())) {
    return nullptr;
  }
  return std::make_unique<unexportable_keys::UnexportableKeyTaskManager>();
}

// Returns an `UnexportableKeyTaskManager` instance that is shared across the
// process hosting the network service, or nullptr if unexportable keys are not
//  available. This function caches availability, so any flags that may change
// it must be set before the first call.
//
// Note: this instance is currently accessible only to
// `UnexportableKeyServiceFactory`. The getter can be moved to some common place
// if there is a need.
unexportable_keys::UnexportableKeyTaskManager* GetSharedTaskManagerInstance() {
  static base::NoDestructor<
      std::unique_ptr<unexportable_keys::UnexportableKeyTaskManager>>
      instance(CreateTaskManagerInstance());
  return instance->get();
}

unexportable_keys::UnexportableKeyService* (*g_mock_factory)() = nullptr;

}  // namespace

namespace net::device_bound_sessions {

// Currently there is another UnexportableKeyServiceFactory in the
// chrome/browser/signin code in the browser process. They do not share code,
// currently code for other factory is here:
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.cc
// It is not an issue if both factories are hosted in the browser process.
// static
UnexportableKeyServiceFactory* UnexportableKeyServiceFactory::GetInstance() {
  static base::NoDestructor<UnexportableKeyServiceFactory> instance;
  return instance.get();
}

void UnexportableKeyServiceFactory::SetUnexportableKeyFactoryForTesting(
    unexportable_keys::UnexportableKeyService* (*func)()) {
  if (g_mock_factory) {
    CHECK(!func);
    g_mock_factory = nullptr;
  } else {
    g_mock_factory = func;
  }
}

unexportable_keys::UnexportableKeyService*
UnexportableKeyServiceFactory::GetShared() {
  if (g_mock_factory) {
    return g_mock_factory();
  }

  if (!has_created_service_) {
    has_created_service_ = true;
    unexportable_keys::UnexportableKeyTaskManager* task_manager =
        GetSharedTaskManagerInstance();
    if (task_manager) {
      unexportable_key_service_ =
          std::make_unique<unexportable_keys::UnexportableKeyServiceImpl>(
              *task_manager, GetConfig());
    }
  }

  return unexportable_key_service_.get();
}

UnexportableKeyServiceFactory*
UnexportableKeyServiceFactory::GetInstanceForTesting() {
  return new UnexportableKeyServiceFactory();
}

UnexportableKeyServiceFactory::UnexportableKeyServiceFactory() = default;
UnexportableKeyServiceFactory::~UnexportableKeyServiceFactory() = default;

}  // namespace net::device_bound_sessions
