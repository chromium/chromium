// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/distribution/app_distribution_provider.h"

#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

AppDistributionProvider::AppDistributionProvider() {}

AppDistributionProvider::~AppDistributionProvider() {}

std::string AppDistributionProvider::GetDistributionBrandCode() {
  return std::string();
}

void AppDistributionProvider::ScheduleDistributionNotifications(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    bool is_first_run) {}

void AppDistributionProvider::CancelDistributionNotifications() {}

void AppDistributionProvider::InitializeFirebase(base::Time install_date,
                                                 bool is_first_run) {}
