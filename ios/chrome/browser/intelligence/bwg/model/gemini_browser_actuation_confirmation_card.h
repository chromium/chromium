// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_ACTUATION_CONFIRMATION_CARD_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_ACTUATION_CONFIRMATION_CARD_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/bwg/model/gemini_block_data_types.h"

// The confirmation Gemini asks for before it starts an agent task. The task
// only starts once the user accepts, by sending a confirming message in the
// conversation.
@protocol GeminiBrowserActuationConfirmationCard <NSObject>

// Human-readable description of the task the agent proposes to perform (e.g.
// "Open rei.com and search for shoes"), or nil if not set.
@property(nonatomic, readonly, copy) NSString* task;

@end

// Adopted by block nodes that can carry a start-task confirmation card.
@protocol GeminiBrowserActuationConfirmationCardBlockNode <GeminiBlockNode>

// The card, or nil if this node holds another type of block.
@property(nonatomic, readonly) id<GeminiBrowserActuationConfirmationCard>
    browserActuationConfirmationCard;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_ACTUATION_CONFIRMATION_CARD_H_
