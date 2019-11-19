// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/external_files/external_file_remover_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/external_files/external_file_remover_impl.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
ExternalFileRemover* ExternalFileRemoverFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<ExternalFileRemover*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
ExternalFileRemoverFactory* ExternalFileRemoverFactory::GetInstance() {
  static base::NoDestructor<ExternalFileRemoverFactory> instance;
  return instance.get();
}

ExternalFileRemoverFactory::ExternalFileRemoverFactory()
    : BrowserStateKeyedServiceFactory(
          "ExternalFileRemoverService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IOSChromeTabRestoreServiceFactory::GetInstance());
}

ExternalFileRemoverFactory::~ExternalFileRemoverFactory() {}

std::unique_ptr<KeyedService>
ExternalFileRemoverFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<ExternalFileRemoverImpl>(
      browser_state,
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(browser_state));
}
