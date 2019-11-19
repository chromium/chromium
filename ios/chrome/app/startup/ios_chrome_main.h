// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_IOS_CHROME_MAIN_H_
#define IOS_CHROME_APP_STARTUP_IOS_CHROME_MAIN_H_

#include <memory>

#include "ios/chrome/app/startup/ios_chrome_main_delegate.h"

namespace base {
class TimeTicks;
}

namespace web {
class WebMainRunner;
}

// Encapsulates any setup and initialization that is needed by common
// Chrome code.  A single instance of this object should be created during app
// startup (or shortly after launch), and clients must ensure that this object
// is not destroyed while Chrome code is still on the stack.
class IOSChromeMain {
 public:
  IOSChromeMain();
  ~IOSChromeMain();

  // The time main() starts.  Only call from main().
  static void InitStartTime();

  // Returns the time that main() started.  Used for performance tests.
  // InitStartTime() must has been called before.
  static const base::TimeTicks& StartTime();

 private:
  IOSChromeMainDelegate main_delegate_;
  std::unique_ptr<web::WebMainRunner> web_main_runner_;
};

#endif  // IOS_CHROME_APP_STARTUP_IOS_CHROME_MAIN_H_
