// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_INIT_WEB_MAIN_H_
#define IOS_WEB_PUBLIC_INIT_WEB_MAIN_H_

#include <memory>

#include "base/macros.h"
#include "ios/web/public/init/web_main_delegate.h"

namespace web {
class WebMainRunner;

// Contains parameters passed to WebMain.
struct WebMainParams {
  WebMainParams();
  explicit WebMainParams(WebMainDelegate* delegate);
  ~WebMainParams();

  // WebMainParams is moveable.
  WebMainParams(WebMainParams&& other);
  WebMainParams& operator=(WebMainParams&& other);

  WebMainDelegate* delegate;

  bool register_exit_manager;

  int argc;
  const char** argv;

  DISALLOW_COPY_AND_ASSIGN(WebMainParams);
};

// Encapsulates any setup and initialization that is needed by common
// web/ code.  A single instance of this object should be created during app
// startup (or shortly after launch), and clients must ensure that this object
// is not destroyed while web/ code is still on the stack.
//
// Clients can add custom code to the startup flow by implementing the methods
// in WebMainDelegate and WebMainParts.
class WebMain {
 public:
  explicit WebMain(WebMainParams params);
  ~WebMain();

 private:
  std::unique_ptr<WebMainRunner> web_main_runner_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_INIT_WEB_MAIN_H_
