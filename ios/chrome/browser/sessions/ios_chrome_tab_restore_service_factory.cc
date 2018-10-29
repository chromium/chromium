// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sessions/core/persistent_tab_restore_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_client.h"

namespace {

std::unique_ptr<KeyedService> BuildTabRestoreService(
    web::BrowserState* context) {
  DCHECK(!context->IsOffTheRecord());

  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<sessions::PersistentTabRestoreService>(
      base::WrapUnique(new IOSChromeTabRestoreServiceClient(browser_state)),
      nullptr);
}

}  // namespace

// static
sessions::TabRestoreService*
IOSChromeTabRestoreServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<sessions::TabRestoreService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
IOSChromeTabRestoreServiceFactory*
IOSChromeTabRestoreServiceFactory::GetInstance() {
  return base::Singleton<IOSChromeTabRestoreServiceFactory>::get();
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
IOSChromeTabRestoreServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildTabRestoreService);
}

IOSChromeTabRestoreServiceFactory::IOSChromeTabRestoreServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "TabRestoreService",
          BrowserStateDependencyManager::GetInstance()) {}

IOSChromeTabRestoreServiceFactory::~IOSChromeTabRestoreServiceFactory() {}

bool IOSChromeTabRestoreServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

std::unique_ptr<KeyedService>
IOSChromeTabRestoreServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildTabRestoreService(context);
}
