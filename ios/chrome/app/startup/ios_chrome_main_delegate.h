// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_IOS_CHROME_MAIN_DELEGATE_H_
#define IOS_CHROME_APP_STARTUP_IOS_CHROME_MAIN_DELEGATE_H_

#include "base/macros.h"
#include "ios/web/public/init/web_main_delegate.h"

// Implementation of WebMainDelegate for Chrome on iOS.
class IOSChromeMainDelegate : public web::WebMainDelegate {
 public:
  IOSChromeMainDelegate();
  ~IOSChromeMainDelegate() override;

 protected:
  // web::WebMainDelegate implementation:
  void BasicStartupComplete() override;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeMainDelegate);
};

#endif  // IOS_CHROME_APP_STARTUP_IOS_CHROME_MAIN_DELEGATE_H_
