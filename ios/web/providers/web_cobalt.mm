// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/web/cobalt_api.h"

namespace web::provider {

void InitializeCobaltInWKWebViewConfiguration(
    WKWebViewConfiguration* configuration,
    bool is_off_the_record,
    web::CobaltController* cobalt_controller) {
  // Nothing to do.
}

NSArray<NSString*>* GetCobaltOriginList() {
  return nil;
}

}  // namespace web::provider
