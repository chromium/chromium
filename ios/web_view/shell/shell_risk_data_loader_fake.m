// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_risk_data_loader.h"

// Fake implementation of ShellRiskDataLoader.
@implementation ShellRiskDataLoader

- (NSString*)riskData {
  return @"dummy-risk-data";
}

@end
