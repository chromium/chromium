// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_DEBUGGER_UI_AIM_DEBUGGER_MUTATOR_H_
#define IOS_CHROME_BROWSER_AIM_DEBUGGER_UI_AIM_DEBUGGER_MUTATOR_H_

#import <Foundation/Foundation.h>

// Mutator for AimDebuggerViewController actions.
@protocol AimDebuggerMutator <NSObject>

// Called when "Request Server Eligibility" is tapped.
- (void)didTapRequestServerEligibility;

// Called when "Save Response" is tapped.
- (void)didTapApplyResponse:(NSString*)base64Response;

// Called when "Copy View Link" is tapped.
- (void)didTapCopyViewLink:(NSString*)base64Response;

// Called when "Copy Draft Link" is tapped.
- (void)didTapCopyDraftLink;

// Called when "Copy Response" is tapped.
- (void)didTapCopyResponse:(NSString*)base64Response;

@end

#endif  // IOS_CHROME_BROWSER_AIM_DEBUGGER_UI_AIM_DEBUGGER_MUTATOR_H_
