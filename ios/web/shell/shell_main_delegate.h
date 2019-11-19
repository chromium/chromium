// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_SHELL_MAIN_DELEGATE_H_
#define IOS_WEB_SHELL_SHELL_MAIN_DELEGATE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "ios/web/public/init/web_main_delegate.h"

namespace web {
class ShellWebClient;

class ShellMainDelegate : public WebMainDelegate {
 public:
  ShellMainDelegate();
  ~ShellMainDelegate() override;

  void BasicStartupComplete() override;

 private:
  std::unique_ptr<ShellWebClient> web_client_;
};

}  // namespace web

#endif  // IOS_WEB_SHELL_SHELL_MAIN_DELEGATE_H_
