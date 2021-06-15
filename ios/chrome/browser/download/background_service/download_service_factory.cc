// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/background_service/download_service_factory.h"

#include <utility>

#include "components/download/internal/background_service/ios/download_service_impl.h"
#include "components/download/public/background_service/download_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

// static
download::DownloadService* DownloadServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<download::DownloadService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
DownloadServiceFactory* DownloadServiceFactory::GetInstance() {
  static base::NoDestructor<DownloadServiceFactory> instance;
  return instance.get();
}

DownloadServiceFactory::DownloadServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "DownloadService",
          BrowserStateDependencyManager::GetInstance()) {}

DownloadServiceFactory::~DownloadServiceFactory() = default;

std::unique_ptr<KeyedService> DownloadServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<download::DownloadServiceImpl>();
}
