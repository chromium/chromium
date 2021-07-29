// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/omaha/omaha_service_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OmahaServiceProvider::OmahaServiceProvider() {}

OmahaServiceProvider::~OmahaServiceProvider() {}

void OmahaServiceProvider::Start() {}

void OmahaServiceProvider::Stop() {}

GURL OmahaServiceProvider::GetUpdateServerURL() const {
  return GURL();
}

std::string OmahaServiceProvider::GetApplicationID() const {
  return std::string();
}

std::string OmahaServiceProvider::GetBrandCode() const {
  return std::string();
}

void OmahaServiceProvider::AppendExtraAttributes(const std::string& tag,
                                                 OmahaXmlWriter* writer) const {
}
