// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/shell_main_delegate.h"

#import "ios/web/shell/shell_web_client.h"

namespace web {

ShellMainDelegate::ShellMainDelegate() {
}

ShellMainDelegate::~ShellMainDelegate() {
}

void ShellMainDelegate::BasicStartupComplete() {
  web_client_.reset(new ShellWebClient());
  web::SetWebClient(web_client_.get());
}

}  // namespace web
