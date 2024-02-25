// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MENU_BROWSER_ACTION_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_MENU_BROWSER_ACTION_FACTORY_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_action_type.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"

class Browser;
class GURL;

namespace web {
class WebState;
}

// Factory providing methods to create UIActions that depends on the provided
// browser with consistent titles, images and metrics structure. When using any
// action from this class, an histogram will be recorded on
// Mobile.ContextMenu.<Scenario>.Action.
@interface BrowserActionFactory : ActionFactory

// Initializes a factory instance for the current `browser` to create action
// instances for the given `scenario`. `browser` can be nil, and in that case
// only actions that doesn't require browser will work, and other actions will
// return nil; `scenario` is used to choose the histogram in which to record the
// actions.
- (instancetype)initWithBrowser:(Browser*)browser
                       scenario:(MenuScenarioHistogram)scenario;

// Creates a UIAction instance configured for opening the `URL` in a new tab and
// which will invoke the given `completion` block after execution.
- (UIAction*)actionToOpenInNewTabWithURL:(const GURL)URL
                              completion:(ProceduralBlock)completion;

// Creates a UIAction instance configured for opening the `URL` in a new
// incognito tab and which will invoke the given `completion` block after
// execution.
- (UIAction*)actionToOpenInNewIncognitoTabWithURL:(const GURL)URL
                                       completion:(ProceduralBlock)completion;

// Creates a UIAction instance whose title and icon are configured for opening a
// URL in a new incognito tab. When triggered, the action will invoke the
// `block` which needs to open a URL in a new incognito tab.
- (UIAction*)actionToOpenInNewIncognitoTabWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance configured for opening the `URL` in a new window
// from `activityOrigin`.
- (UIAction*)actionToOpenInNewWindowWithURL:(const GURL)URL
                             activityOrigin:
                                 (WindowActivityOrigin)activityOrigin;

// Creates a UIAction instance configured for opening a new window using
// `activity`.
- (UIAction*)actionToOpenInNewWindowWithActivity:(NSUserActivity*)activity;

// Creates a UIAction instance for opening an image `URL` in current tab and
// invoke the given `completion` block after execution.
- (UIAction*)actionOpenImageWithURL:(const GURL)URL
                         completion:(ProceduralBlock)completion;

// Creates a UIAction instance for opening an image `params` in a new tab and
// invoke the given `completion` block after execution.
- (UIAction*)actionOpenImageInNewTabWithUrlLoadParams:(UrlLoadParams)params
                                           completion:
                                               (ProceduralBlock)completion;

// Creates a UIAction instance for searching with Lens.
- (UIAction*)actionToSearchWithLensWithEntryPoint:(LensEntrypoint)entryPoint;

// Creates a UIAction instance for saving an image to Photos. `block` will be
// executed if the action is triggered to perform any additional action before
// the Save to Photos UI is started.
- (UIAction*)actionToSaveToPhotosWithImageURL:(const GURL&)url
                                     referrer:(const web::Referrer&)referrer
                                     webState:(web::WebState*)webState
                                        block:(ProceduralBlock)block;

// Creates a UIAction instance for opening a new tab.
- (UIAction*)actionToOpenNewTab;

// Creates a UIAction instance for opening a new incognito tab.
- (UIAction*)actionToOpenNewIncognitoTab;

// Creates a UIAction instance for closing the current tab.
- (UIAction*)actionToCloseCurrentTab;

// Creates a UIAction instance for showing the QR Scanner.
- (UIAction*)actionToShowQRScanner;

// Creates a UIAction instance for starting a voice search.
- (UIAction*)actionToStartVoiceSearch;

// Creates a UIAction instance for opening a new search.
- (UIAction*)actionToStartNewSearch;

// Creates a UIAction instance for opening a new incognito search.
- (UIAction*)actionToStartNewIncognitoSearch;

// Creates a UIAction instance for searching for the image in the pasteboard.
- (UIAction*)actionToSearchCopiedImage;

// Creates a UIAction instance for searching for the URL in the pasteboard.
- (UIAction*)actionToSearchCopiedURL;

// Creates a UIAction instance for searching for the text in the pasteboard.
- (UIAction*)actionToSearchCopiedText;

@end

#endif  // IOS_CHROME_BROWSER_UI_MENU_BROWSER_ACTION_FACTORY_H_
