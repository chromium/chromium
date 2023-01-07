// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_SHELL_SHELL_RISK_DATA_LOADER_H_
#define IOS_WEB_VIEW_SHELL_SHELL_RISK_DATA_LOADER_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Risk data loader for ios_web_view_shell.
@interface ShellRiskDataLoader : NSObject

// Risk data needed for 1P payments integration.
// See go/risk-eng.g3doc for more details.
@property(nonatomic, readonly) NSString* riskData;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_SHELL_SHELL_RISK_DATA_LOADER_H_
