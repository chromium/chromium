// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_MUTATOR_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_MUTATOR_H_

#import <Foundation/Foundation.h>

#import "components/optimization_guide/optimization_guide_buildflags.h"

namespace optimization_guide::proto {

enum BlingPrototypingRequest_ModelEnum : int;
enum TabOrganizationRequest_TabOrganizationModelStrategy : int;
}  // namespace optimization_guide::proto

// Mutator protocol for the UI layer to communicate to the
// AIPrototypingMediator.
@protocol AIPrototypingMutator

// Executes a freeform prototyping request to a server-hosted model.
- (void)executeFreeformServerQuery:(NSString*)query
                systemInstructions:(NSString*)systemInstructions
                includePageContext:(BOOL)includePageContext
                       temperature:(float)temperature
                             model:(optimization_guide::proto::
                                        BlingPrototypingRequest_ModelEnum)model;

// Executes a tab organization request with a given organization `strategy`.
- (void)executeGroupTabsWithStrategy:
    (optimization_guide::proto::
         TabOrganizationRequest_TabOrganizationModelStrategy)strategy;

// Executes an enhanced calendar request with a given (optional) prompt and
// selected text.
- (void)executeEnhancedCalendarQueryWithPrompt:(NSString*)prompt
                                  selectedText:(NSString*)selectedText;
;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_MUTATOR_H_
