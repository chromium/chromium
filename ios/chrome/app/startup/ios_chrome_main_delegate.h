// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_IOS_CHROME_MAIN_DELEGATE_H_
#define IOS_CHROME_APP_STARTUP_IOS_CHROME_MAIN_DELEGATE_H_

#include "ios/web/public/init/web_main_delegate.h"

// Implementation of WebMainDelegate for Chrome on iOS.
class IOSChromeMainDelegate : public web::WebMainDelegate {
 public:
  IOSChromeMainDelegate();

  IOSChromeMainDelegate(const IOSChromeMainDelegate&) = delete;
  IOSChromeMainDelegate& operator=(const IOSChromeMainDelegate&) = delete;

  ~IOSChromeMainDelegate() override;

 protected:
  // web::WebMainDelegate implementation:
  void BasicStartupComplete() override;
};

#endif  // IOS_CHROME_APP_STARTUP_IOS_CHROME_MAIN_DELEGATE_H_
