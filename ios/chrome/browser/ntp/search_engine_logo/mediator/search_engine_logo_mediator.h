// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_SEARCH_ENGINE_LOGO_MEDIATOR_SEARCH_ENGINE_LOGO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_NTP_SEARCH_ENGINE_LOGO_MEDIATOR_SEARCH_ENGINE_LOGO_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/memory/scoped_refptr.h"
#import "url/gurl.h"

class GoogleLogoService;
@protocol SearchEngineLogoConsumer;
enum class SearchEngineLogoState;
class TemplateURLService;
class UrlLoadingBrowserAgent;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network
namespace web {
class WebState;
}  // namespace web

@interface SearchEngineLogoMediator : NSObject

// Whether the logo should be multicolor or monochrome. If the logo is a doodle,
// that will supercede a potential monochrome logo.
@property(nonatomic, assign) BOOL usesMonochromeLogo;
@property(nonatomic, weak) id<SearchEngineLogoConsumer> consumer;

// View that shows a doodle or a search engine logo.
// TODO(crbug.com/436228514): Need to be removed.
@property(nonatomic, strong, readonly) UIView* view;

// Designated initializer.
- (instancetype)initWithWebState:(web::WebState*)webState
              templateURLService:(TemplateURLService*)templateURLService
                     logoService:(GoogleLogoService*)logoService
          URLLoadingBrowserAgent:(UrlLoadingBrowserAgent*)URLLoadingBrowserAgent
          sharedURLLoaderFactory:
              (scoped_refptr<network::SharedURLLoaderFactory>)
                  sharedURLLoaderFactory
                    offTheRecord:(BOOL)offTheRecord NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnect the instance.
- (void)disconnect;

// Updates the vendor's WebState.
- (void)setWebState:(web::WebState*)webState;

@end

@interface SearchEngineLogoMediator (VisibleForTesting)

// Simulates tapping on the doodle.
- (void)simulateDoodleTapped;
// Sets the destination URL for the doodle tap handler.
- (void)setClickURLText:(const GURL&)url;
// Called when the search engine has changed.
- (void)searchEngineChanged;

@end

#endif  // IOS_CHROME_BROWSER_NTP_SEARCH_ENGINE_LOGO_MEDIATOR_SEARCH_ENGINE_LOGO_MEDIATOR_H_
