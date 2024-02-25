// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_BROWSER_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_BROWSER_OBSERVER_H_

#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_list_observer.h"

// A BrowserObserver that observes the BrowserDestroyed callback.
class FullscreenBrowserObserver : public BrowserObserver {
 public:
  FullscreenBrowserObserver(
      FullscreenWebStateListObserver* web_state_list_observer,
      Browser* browser);
  ~FullscreenBrowserObserver() override;

 private:
  // BrowserObserver
  void BrowserDestroyed(Browser* browser) override;

  // The FullscreenWebStateListObserver passed on construction.
  raw_ptr<FullscreenWebStateListObserver> web_state_list_observer_;
  // Scoped observer that facilitates observing an BrowserObserver.
  base::ScopedObservation<Browser, BrowserObserver> scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_BROWSER_OBSERVER_H_
