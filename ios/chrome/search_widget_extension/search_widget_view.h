// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_SEARCH_WIDGET_EXTENSION_SEARCH_WIDGET_VIEW_H_
#define IOS_CHROME_SEARCH_WIDGET_EXTENSION_SEARCH_WIDGET_VIEW_H_

#import <UIKit/UIKit.h>

typedef NS_ENUM(NSInteger, CopiedContentType);

// Protocol to be implemented by targets for user actions coming from the search
// widget view.
@protocol SearchWidgetViewActionTarget

// Called when the user taps the Search button.
- (void)openSearch:(id)sender;
// Called when the user taps the Incognito Search button.
- (void)openIncognito:(id)sender;
// Called when the user taps the Voice Search button.
- (void)openVoice:(id)sender;
// Called when the user taps the QR Code button.
- (void)openQRCode:(id)sender;
// Called when the user taps the Open Copied URL section.
- (void)openCopiedContent:(id)sender;

@end

// View for the search widget, shows two sections. The first section is a row of
// ways to launch the app. The second section displays the current copied URL.
@interface SearchWidgetView : UIView

// Designated initializer, creates the widget view with a `target` for user
// actions. `compactHeight` indicates the size to use in compact display.
- (instancetype)initWithActionTarget:(id<SearchWidgetViewActionTarget>)target
                       compactHeight:(CGFloat)compactHeight
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Changes the display mode of the view to compact of expanded.
- (void)showMode:(BOOL)compact;

// Gets the height of the widget.
- (CGFloat)widgetHeight;

// Sets the copied content type.
- (void)setCopiedContentType:(CopiedContentType)type;

@end

#endif  // IOS_CHROME_SEARCH_WIDGET_EXTENSION_SEARCH_WIDGET_VIEW_H_
