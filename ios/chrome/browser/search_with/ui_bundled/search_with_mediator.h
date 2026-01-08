// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_WITH_UI_BUNDLED_SEARCH_WITH_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SEARCH_WITH_UI_BUNDLED_SEARCH_WITH_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/browser_content/model/edit_menu_builder.h"

@protocol SceneCommands;
class TemplateURLService;

// Mediator that mediates between the browser container views and the
// search with tab helpers.
@interface SearchWithMediator : NSObject <EditMenuBuilder>

// Initializer for a mediator.
- (instancetype)initWithTemplateURLService:
                    (TemplateURLService*)templateURLService
                                 incognito:(BOOL)incognito
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)shutdown;

// The handler for SceneCommands commands.
@property(nonatomic, weak) id<SceneCommands> sceneHandler;

@end

#endif  // IOS_CHROME_BROWSER_SEARCH_WITH_UI_BUNDLED_SEARCH_WITH_MEDIATOR_H_
