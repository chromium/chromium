// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/init/web_main.h"
#include "ios/web/public/init/web_main_runner.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
