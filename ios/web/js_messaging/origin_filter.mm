// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/js_messaging/origin_filter.h"

namespace web {

NSArray<NSString*>* GetOriginList(web::OriginFilter filter) {
  switch (filter) {
    case web::OriginFilter::kPublic:
      return nil;
    case web::OriginFilter::kValidTestOriginForTesting:
      return @[ @"https://test.test" ];
  }
}

}  // namespace web
