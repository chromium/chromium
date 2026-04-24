// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_COORDINATOR_COMPOSEBOX_MENU_MEDIATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_COORDINATOR_COMPOSEBOX_MENU_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_mutator.h"
#import "ios/chrome/browser/composebox/public/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/public/composebox_focus_params.h"

@class ComposeboxMenuMediator;

// Delegate for the menu mediator.
@protocol ComposeboxMenuMediatorDelegate <NSObject>

// Called when the mediator yields a new bundle of focus params to start the
// composebox with.
- (void)composeboxMenuMediatorDidProduceFocusParams:
    (ComposeboxFocusParams*)focusParams;

@end

// Mediator for the composebox menu.
@interface ComposeboxMenuMediator : NSObject <ComposeboxMenuMutator>

// Delegate for this mediator.
@property(nonatomic, weak) id<ComposeboxMenuMediatorDelegate> delegate;

// Creates a new instance with an entrypoint.
- (instancetype)initWithEntrypoint:(ComposeboxEntrypoint)entrypoint;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_COORDINATOR_COMPOSEBOX_MENU_MEDIATOR_H_
