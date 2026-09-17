// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/extension/gpc_web_extension/gpc_web_extension.h"

#import "base/apple/bundle_locations.h"
#import "base/no_destructor.h"

namespace web {

// static
GPCWebExtension* GPCWebExtension::GetInstance() {
  static base::NoDestructor<GPCWebExtension> instance;
  return instance.get();
}

GPCWebExtension::GPCWebExtension() = default;

GPCWebExtension::~GPCWebExtension() = default;

NSURL* GPCWebExtension::GetExtensionURL() const {
  NSString* resource_path = [base::apple::FrameworkBundle() resourcePath];
  NSString* path = [resource_path
      stringByAppendingPathComponent:@"extensions/gpc_web_extension"];
  return [NSURL fileURLWithPath:path isDirectory:YES];
}

}  // namespace web
