// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_OBSERVER_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_OBSERVER_H_

#include "base/macros.h"
#include "base/observer_list_types.h"

class Browser;

// Observer interface for objects interested in Browser events.
class BrowserObserver : public base::CheckedObserver {
 public:
  // Invoked when the Browser is being destroyed. Gives subclasses a chance
  // to cleanup.
  virtual void BrowserDestroyed(Browser* browser) {}

 protected:
  BrowserObserver() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserObserver);
};

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_OBSERVER_H_
