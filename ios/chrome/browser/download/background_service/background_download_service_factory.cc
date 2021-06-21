// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/background_service/background_download_service_factory.h"

#include <utility>

#include "components/download/internal/background_service/ios/background_download_service_impl.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

// static
download::BackgroundDownloadService*
BackgroundDownloadServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<download::BackgroundDownloadService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
BackgroundDownloadServiceFactory*
BackgroundDownloadServiceFactory::GetInstance() {
  static base::NoDestructor<BackgroundDownloadServiceFactory> instance;
  return instance.get();
}

BackgroundDownloadServiceFactory::BackgroundDownloadServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "BackgroundDownloadService",
          BrowserStateDependencyManager::GetInstance()) {}

BackgroundDownloadServiceFactory::~BackgroundDownloadServiceFactory() = default;

std::unique_ptr<KeyedService>
BackgroundDownloadServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<download::BackgroundDownloadServiceImpl>();
}
