// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TEXT_ZOOM_TEXT_ZOOM_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TEXT_ZOOM_TEXT_ZOOM_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/text_zoom/text_zoom_view_controller.h"

@protocol TextZoomCommands;
@protocol TextZoomConsumer;
class WebStateList;

@interface TextZoomMediator : NSObject <TextZoomHandler>

@property(nonatomic, weak) id<TextZoomConsumer> consumer;

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                      commandHandler:(id<TextZoomCommands>)commandHandler
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Stops observing all objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TEXT_ZOOM_TEXT_ZOOM_MEDIATOR_H_
