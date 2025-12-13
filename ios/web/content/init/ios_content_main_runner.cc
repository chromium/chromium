// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/init/ios_content_main_runner.h"

#import "base/types/fixed_array.h"
#import "components/crash/core/common/crash_key.h"
#import "content/public/app/content_main.h"
#import "content/public/app/content_main_runner.h"
#import "ios/web/content/init/ios_main_delegate.h"

namespace web {

IOSContentMainRunner::IOSContentMainRunner() {}

IOSContentMainRunner::~IOSContentMainRunner() {}

void IOSContentMainRunner::Initialize(WebMainParams params) {
  static crash_reporter::CrashKeyString<4> key("blink");
  key.Set("yes");
  argv_ = std::move(params.args);
}

int IOSContentMainRunner::Startup() {
  content_main_delegate_ = std::make_unique<IOSMainDelegate>();
  content::ContentMainParams content_params(content_main_delegate_.get());
  size_t argc = argv_.size();
  base::FixedArray<const char*> argv(argc);
  for (size_t i = 0; i < argc; ++i) {
    argv[i] = argv_[i].c_str();
  }
  content_params.argc = argc;
  content_params.argv = argv.data();
  content_main_runner_ = content::ContentMainRunner::Create();
  return RunContentProcess(std::move(content_params),
                           content_main_runner_.get());
}

void IOSContentMainRunner::ShutDown() {
  content_main_runner_->Shutdown();
}

}  // namespace web
