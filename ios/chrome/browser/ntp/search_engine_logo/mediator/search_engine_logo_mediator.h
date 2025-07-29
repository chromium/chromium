// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_SEARCH_ENGINE_LOGO_MEDIATOR_SEARCH_ENGINE_LOGO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_NTP_SEARCH_ENGINE_LOGO_MEDIATOR_SEARCH_ENGINE_LOGO_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "url/gurl.h"

@protocol SearchEngineLogoConsumer;
enum class SearchEngineLogoState;

class Browser;
namespace web {
class WebState;
}

@interface SearchEngineLogoMediator : NSObject

// Whether the logo should be multicolor or monochrome.
@property(nonatomic, assign) BOOL usesMonochromeLogo;
@property(nonatomic, weak) id<SearchEngineLogoConsumer> consumer;

// View that shows a doodle or a search engine logo.
// TODO(crbug.com/423883582): Need to be removed.
@property(nonatomic, strong, readonly) UIView* view;
// Whether or not the logo should be shown. Defaults to
// SearchEngineLogoState::kLogo.
// TODO(crbug.com/423883582): Need to be removed: the consumer is supposed to
// rely on -[<SearchEngineLogoConsumer> searchEngineLogoStateDidChange:] to get
// the value.
@property(nonatomic, assign) SearchEngineLogoState logoState;

// Designated initializer.
- (instancetype)initWithBrowser:(Browser*)browser
                       webState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnect the instance.
- (void)disconnect;

// Checks for a new doodle.  Calling this method frequently will result in a
// query being issued at most once per hour.
// TODO(crbug.com/423883582): Need to be removed.
- (void)fetchDoodle;

// Updates the vendor's WebState.
- (void)setWebState:(web::WebState*)webState;

@end

@interface SearchEngineLogoMediator (VisibleForTesting)

// Simulates tapping on the doodle.
- (void)simulateDoodleTapped;
// Sets the destination URL for the doodle tap handler.
- (void)setClickURLText:(const GURL&)url;

@end

#endif  // IOS_CHROME_BROWSER_NTP_SEARCH_ENGINE_LOGO_MEDIATOR_SEARCH_ENGINE_LOGO_MEDIATOR_H_
