// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TEXT_ZOOM_UI_BUNDLED_TEXT_ZOOM_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TEXT_ZOOM_UI_BUNDLED_TEXT_ZOOM_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/text_zoom/ui_bundled/text_zoom_consumer.h"

@protocol TextZoomCommands;
@class TextZoomViewController;

@protocol TextZoomHandler <NSObject>

// Asks the handler to zoom in.
- (void)zoomIn;
// Asks the handler to zoom out.
- (void)zoomOut;
// Asks the handler to reset the zoom level to the default.
- (void)resetZoom;

@end

@interface TextZoomViewController : UIViewController <TextZoomConsumer>

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@property(nonatomic, weak) id<TextZoomCommands> commandHandler;
@property(nonatomic, weak) id<TextZoomHandler> zoomHandler;

@end

#endif  // IOS_CHROME_BROWSER_TEXT_ZOOM_UI_BUNDLED_TEXT_ZOOM_VIEW_CONTROLLER_H_
