// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_INIT_WEB_MAIN_RUNNER_H_
#define IOS_WEB_PUBLIC_INIT_WEB_MAIN_RUNNER_H_

#include "ios/web/public/init/web_main.h"

namespace web {

// This class is responsible for web initialization and shutdown.
class WebMainRunner {
 public:
  virtual ~WebMainRunner() {}

  // Create a new WebMainRunner object.
  static WebMainRunner* Create();

  // Initialize all necessary web state.
  virtual int Initialize(WebMainParams params) = 0;

  // Shut down the web state.
  virtual void ShutDown() = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_INIT_WEB_MAIN_RUNNER_H_
