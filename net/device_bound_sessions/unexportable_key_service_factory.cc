// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/unexportable_key_service_factory.h"

#include "base/logging.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/unexportable_key.h"

namespace {

#if BUILDFLAG(IS_MAC)
constexpr char kKeychainAccessGroup[] = MAC_TEAM_IDENTIFIER_STRING
    "." MAC_BUNDLE_IDENTIFIER_STRING ".unexportable-keys";
constexpr char kKeychainAccessGroup[] =
    ".org.chromium.Chromium.unexportable-keys";
#endif  // BUILDFLAG(IS_MAC)

// Returns a newly created task manager instance, or nullptr if unexportable
// keys are not available.
std::unique_ptr<unexportable_keys::UnexportableKeyTaskManager>
CreateTaskManagerInstance() {
  crypto::UnexportableKeyProvider::Config config{
#if BUILDFLAG(IS_MAC)
      .keychain_access_group = kKeychainAccessGroup,
#endif  // BUILDFLAG(IS_MAC)
  };
  if (!unexportable_keys::UnexportableKeyServiceImpl::
          IsUnexportableKeyProviderSupported(config)) {
    return nullptr;
  }
  return std::make_unique<unexportable_keys::UnexportableKeyTaskManager>(
      std::move(config));
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
              *task_manager);
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
