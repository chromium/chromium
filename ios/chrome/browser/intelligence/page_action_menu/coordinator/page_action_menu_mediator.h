// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_COORDINATOR_PAGE_ACTION_MENU_MEDIATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_COORDINATOR_PAGE_ACTION_MENU_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_consumer.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_mutator.h"

class BwgService;
class PrefService;
class ReaderModeTabHelper;
class TemplateURLService;
class HostContentSettingsMap;

@protocol PageActionMenuCommands;
@protocol ContextualSheetCommands;

namespace web {
class WebState;
}

// The mediator for the page action menu.
@interface PageActionMenuMediator : NSObject <PageActionMenuMutator>

// Designated initializer for the mediator.
- (instancetype)initWithWebState:(web::WebState*)webState
              profilePrefService:(PrefService*)profilePrefs
              templateURLService:(TemplateURLService*)templateURLService
                      BWGService:(BwgService*)BWGService
             readerModeTabHelper:(ReaderModeTabHelper*)readerModeTabHelper
          hostContentSettingsMap:(HostContentSettingsMap*)hostContentSettingsMap
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

// Returns whether the Lens overlay is available for the profile. It may still
// be unavailable for the current web state.
- (BOOL)isLensAvailableForProfile;

// Consumer for the Page Action Menu mediator.
@property(nonatomic, weak) id<PageActionMenuConsumer> consumer;

// The handler for sending page action menu commands.
@property(nonatomic, weak) id<PageActionMenuCommands> pageActionMenuHandler;

// Command handler for contextual sheet commands.
@property(nonatomic, weak) id<ContextualSheetCommands> contextualSheetHandler;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_COORDINATOR_PAGE_ACTION_MENU_MEDIATOR_H_
