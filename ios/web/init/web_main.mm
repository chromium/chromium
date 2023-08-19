// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/init/web_main.h"

#import "ios/web/public/init/web_main_runner.h"

namespace web {

WebMainParams::WebMainParams() : WebMainParams(nullptr) {}

WebMainParams::WebMainParams(WebMainDelegate* delegate)
    : delegate(delegate), register_exit_manager(true), argc(0), argv(nullptr) {}

WebMainParams::~WebMainParams() = default;

WebMainParams::WebMainParams(WebMainParams&& other) = default;

WebMainParams& WebMainParams::operator=(web::WebMainParams&& other) = default;

WebMain::WebMain(WebMainParams params) {
  web_main_runner_.reset(WebMainRunner::Create());
  web_main_runner_->Initialize(std::move(params));
}

WebMain::~WebMain() {
  web_main_runner_->ShutDown();
}

}  // namespace web
