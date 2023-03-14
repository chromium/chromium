// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_OPEN_NEW_TAB_COMMAND_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_OPEN_NEW_TAB_COMMAND_H_

#import <UIKit/UIKit.h>

class GURL;

namespace web {
struct Referrer;
}

// Describes the intended position for a new tab.
enum class OpenPosition {
  kCurrentTab,     // Relative to currently selected tab.
  kLastTab,        // Always at end of tab model.
  kSpecifiedIndex  // Index is specified elsewhere.
};

// Command sent to open a new tab, optionally including a point (in UIWindow
// coordinates).
@interface OpenNewTabCommand : NSObject

- (instancetype)initWithURL:(const GURL&)URL
                 virtualURL:(const GURL&)virtualURL
                   referrer:(const web::Referrer&)referrer
                inIncognito:(BOOL)inIncognito
               inBackground:(BOOL)inBackground
                   appendTo:(OpenPosition)append;

- (instancetype)initWithURL:(const GURL&)URL
                   referrer:(const web::Referrer&)referrer
                inIncognito:(BOOL)inIncognito
               inBackground:(BOOL)inBackground
                   appendTo:(OpenPosition)append;

// Initializes a command intended to open a new page.
- (instancetype)initInIncognito:(BOOL)inIncognito
                   inBackground:(BOOL)inBackground NS_DESIGNATED_INITIALIZER;

// Mark inherited initializer as unavailable to prevent calling it by mistake.
- (instancetype)init NS_UNAVAILABLE;

// Convenience initializers

// Initializes a command intended to open a URL from browser chrome (e.g.,
// settings).
+ (instancetype)commandWithURLFromChrome:(const GURL&)URL
                             inIncognito:(BOOL)inIncognito;

// Initializes a command intended to open a URL from browser chrome (e.g.,
// settings). This will always open in a new foreground tab in non-incognito
// mode.
+ (instancetype)commandWithURLFromChrome:(const GURL&)URL;

+ (instancetype)commandWithIncognito:(BOOL)incognito
                         originPoint:(CGPoint)origin;

// Creates an OpenTabCommand with `incognito` and an `originPoint` of
// CGPointZero.
+ (instancetype)commandWithIncognito:(BOOL)incognito;

// Creates an OpenTabCommand with `incognito` NO and an `originPoint` of
// CGPointZero.
+ (instancetype)command;

// Creates an OpenTabCommand with `incognito` YES and an `originPoint` of
// CGPointZero.
+ (instancetype)incognitoTabCommand;

#pragma mark - ReadWrite properties

// Whether the new tab command was initiated by the user (e.g. by tapping the
// new tab button in the tools menu) or not (e.g. opening a new tab via a
// Javascript action). Defaults to `YES`. Only used when the `URL` isn't valid.
@property(nonatomic, assign, getter=isUserInitiated) BOOL userInitiated;

// Whether the new tab command should also trigger the omnibox to be focused.
// Only used when the `URL` isn't valid.
@property(nonatomic, assign) BOOL shouldFocusOmnibox;

// Origin point of the action triggering this command.
@property(nonatomic, assign) CGPoint originPoint;

#pragma mark - ReadOnly properties

// Whether this URL command requests opening in incognito or not.
@property(nonatomic, readonly, assign) BOOL inIncognito;

// Whether this URL command requests opening in background or not.
@property(nonatomic, readonly, assign) BOOL inBackground;

// ---- Properties only used when `URL` is valid.

// URL to open.
@property(nonatomic, readonly, assign) const GURL& URL;

// VirtualURL to display.
@property(nonatomic, readonly, assign) const GURL& virtualURL;

// Referrer for the URL.
@property(nonatomic, readonly, assign) const web::Referrer& referrer;

// Whether or not this URL command comes from a chrome context (e.g., settings),
// as opposed to a web page context.
@property(nonatomic, readonly, assign) BOOL fromChrome;

// Location where the new tab should be opened.
@property(nonatomic, assign) OpenPosition appendTo;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_OPEN_NEW_TAB_COMMAND_H_
