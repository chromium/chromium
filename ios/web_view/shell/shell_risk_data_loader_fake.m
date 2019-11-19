// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_risk_data_loader.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Fake implementation of ShellRiskDataLoader.
@implementation ShellRiskDataLoader

- (NSString*)riskData {
  return @"dummy-risk-data";
}

@end
