// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_CRW_DATA_CONTROLS_DELEGATE_H_
#define IOS_WEB_WEB_STATE_CRW_DATA_CONTROLS_DELEGATE_H_

#import <Foundation/Foundation.h>

// Protocol to provide data controls for web view.
@protocol CRWDataControlsDelegate <NSObject>

// Checks if copy is allowed.
- (void)shouldAllowCopyWithDecisionHandler:(void (^)(BOOL))decisionHandler;

// Checks if paste is allowed.
- (void)shouldAllowPasteWithDecisionHandler:(void (^)(BOOL))decisionHandler;

// Checks if cut is allowed.
- (void)shouldAllowCutWithDecisionHandler:(void (^)(BOOL))decisionHandler;

@end

#endif  // IOS_WEB_WEB_STATE_CRW_DATA_CONTROLS_DELEGATE_H_
