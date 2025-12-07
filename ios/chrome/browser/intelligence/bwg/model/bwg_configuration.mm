// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_configuration.h"

#import "base/check.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"

@implementation BWGConfiguration {
  // The current configuration's PageContext.
  std::unique_ptr<optimization_guide::proto::PageContext> _uniquePageContext;
}

- (std::unique_ptr<optimization_guide::proto::PageContext>)uniquePageContext {
  return std::move(_uniquePageContext);
}

- (void)setUniquePageContext:
    (std::unique_ptr<optimization_guide::proto::PageContext>)uniquePageContext {
  _uniquePageContext = std::move(uniquePageContext);
}

@end
