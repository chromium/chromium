// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sessions/core/tab_restore_service_impl.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_client.h"

namespace {

std::unique_ptr<KeyedService> BuildTabRestoreService(
    web::BrowserState* context) {
  DCHECK(!context->IsOffTheRecord());

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<sessions::TabRestoreServiceImpl>(
      base::WrapUnique(new IOSChromeTabRestoreServiceClient(browser_state)),
      browser_state->GetPrefs(), nullptr);
}

}  // namespace

// static
sessions::TabRestoreService*
IOSChromeTabRestoreServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<sessions::TabRestoreService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
IOSChromeTabRestoreServiceFactory*
IOSChromeTabRestoreServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeTabRestoreServiceFactory> instance;
  return instance.get();
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
