// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_context.h"

#import "base/check.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"

@implementation GeminiPageContext {
  // A pointer to the page context proto.
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
