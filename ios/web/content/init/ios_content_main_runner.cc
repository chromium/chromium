// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/init/ios_content_main_runner.h"

#import "content/public/app/content_main.h"
#import "content/public/app/content_main_runner.h"
#import "ios/web/content/init/ios_main_delegate.h"

namespace web {

IOSContentMainRunner::IOSContentMainRunner() {}

IOSContentMainRunner::~IOSContentMainRunner() {}

int IOSContentMainRunner::Initialize(WebMainParams params) {
  content_main_delegate_ = std::make_unique<IOSMainDelegate>();
  content::ContentMainParams content_params(content_main_delegate_.get());
  content_params.argc = params.argc;
  content_params.argv = params.argv;
  content_main_runner_ = content::ContentMainRunner::Create();
  return RunContentProcess(std::move(content_params),
                           content_main_runner_.get());
}

void IOSContentMainRunner::ShutDown() {
  content_main_runner_->Shutdown();
}

}  // namespace web
