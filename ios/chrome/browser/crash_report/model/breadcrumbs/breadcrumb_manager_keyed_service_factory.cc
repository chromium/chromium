// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/model/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"

#include <utility>

#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

BreadcrumbManagerKeyedServiceFactory::BreadcrumbManagerKeyedServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "BreadcrumbManagerService",
          ProfileSelection::kOwnInstanceInIncognito) {}

std::unique_ptr<KeyedService>
BreadcrumbManagerKeyedServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<breadcrumbs::BreadcrumbManagerKeyedService>(
      profile->IsOffTheRecord());
}
