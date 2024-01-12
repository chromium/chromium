// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/url_loading/model/url_loading_observer.h"

// Objective-C equivalent of the UrlLoadingObserverBridge class.
@protocol URLLoadingObserving <NSObject>
@optional

// The loader will load `URL` in the current tab. Next state will be
// one of: tabFailedToLoadURL, tabDidPrerenderURL,
// tabDidReloadURL or tabDidLoadURL.
// Invoked by UrlLoadingObserverBridge::TabWillLoadUrl.
- (void)tabWillLoadURL:(const GURL&)URL
        transitionType:(ui::PageTransition)transitionType;

// The loader didn't succeed loading the requested `URL`. Reason
// can, for example be an incognito mismatch or an induced crash.
// It is possible that the url was loaded, but in another tab.
// Invoked by UrlLoadingObserverBridge::TabFailedToLoadUrl.
- (void)tabFailedToLoadURL:(const GURL&)URL
            transitionType:(ui::PageTransition)transitionType;

// The loader replaced the load with a prerendering.
// Invoked by UrlLoadingObserverBridge::TabDidPrerenderUrl.
- (void)tabDidPrerenderURL:(const GURL&)URL
            transitionType:(ui::PageTransition)transitionType;

// The loader reloaded the `URL` in the current tab.
// Invoked by UrlLoadingObserverBridge::TabDidReloadUrl.
- (void)tabDidReloadURL:(const GURL&)URL
         transitionType:(ui::PageTransition)transitionType;

// The loader initiated the `url` loading successfully.
// Invoked by UrlLoadingObserverBridge::TabDidLoadUrl.
- (void)tabDidLoadURL:(const GURL&)URL
       transitionType:(ui::PageTransition)transitionType;

// The loader will load `URL` in a new tab. Next state will be:
// newTabDidLoadURL.
// Invoked by UrlLoadingObserverBridge::NewTabWillLoadUrl.
- (void)newTabWillLoadURL:(const GURL&)URL
          isUserInitiated:(BOOL)isUserInitiated;

// The loader initiated the `URL` loading in a new tab successfully.
// Invoked by UrlLoadingObserverBridge::NewTabDidLoadUrl.
- (void)newTabDidLoadURL:(const GURL&)URL isUserInitiated:(BOOL)isUserInitiated;

// The loader will switch to an existing tab with `URL` instead of loading it.
// Next state will be: didSwitchToTabWithURL. Invoked by
// UrlLoadingObserverBridge::NewTabWillLoadUrl.
- (void)willSwitchToTabWithURL:(const GURL&)URL
              newWebStateIndex:(NSInteger)newWebStateIndex;

// The loader switched to an existing tab with `URL`.
// Invoked by UrlLoadingObserverBridge::NewTabDidLoadUrl.
- (void)didSwitchToTabWithURL:(const GURL&)URL
             newWebStateIndex:(NSInteger)newWebStateIndex;

@end

// Observes url loading events from Objective-C. Used to update listeners of
// change of state in url loading.
class UrlLoadingObserverBridge : public UrlLoadingObserver {
 public:
  UrlLoadingObserverBridge(id<URLLoadingObserving> owner);
  ~UrlLoadingObserverBridge() override;

  // UrlLoadingObserver
  void TabWillLoadUrl(const GURL& url,
                      ui::PageTransition transition_type) override;
  void TabFailedToLoadUrl(const GURL& url,
                          ui::PageTransition transition_type) override;
  void TabDidPrerenderUrl(const GURL& url,
                          ui::PageTransition transition_type) override;
  void TabDidReloadUrl(const GURL& url,
                       ui::PageTransition transition_type) override;
  void TabDidLoadUrl(const GURL& url,
                     ui::PageTransition transition_type) override;

  void NewTabWillLoadUrl(const GURL& url, bool user_initiated) override;
  void NewTabDidLoadUrl(const GURL& url, bool user_initiated) override;

  void WillSwitchToTabWithUrl(const GURL& url,
                              int new_web_state_index) override;
  void DidSwitchToTabWithUrl(const GURL& url, int new_web_state_index) override;

 private:
  __weak id<URLLoadingObserving> owner_;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_OBSERVER_BRIDGE_H_
