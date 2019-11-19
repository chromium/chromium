// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_FAKE_BROWSER_OBSERVER_H_
#define IOS_CHROME_BROWSER_MAIN_FAKE_BROWSER_OBSERVER_H_

#import "ios/chrome/browser/main/browser_observer.h"

class Browser;

// Fake browser observer.
class FakeBrowserObserver : public BrowserObserver {
 public:
  // Constructor for a fake observer that observes |browser| upon construction
  // until the BrowserDestroyed() signal is received.
  explicit FakeBrowserObserver(Browser* browser);
  ~FakeBrowserObserver() override;

  // Whether the BrowserDestroyed() signal has been received.
  bool browser_destroyed() const { return browser_destroyed_; }

  // BrowserObserver:
  void BrowserDestroyed(Browser* browser) override;

 private:
  bool browser_destroyed_ = false;
};

#endif  // IOS_CHROME_BROWSER_MAIN_FAKE_BROWSER_OBSERVER_H_
