// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/init/web_main_runner.h"

#import "ios/web/init/web_main_runner_impl.h"

#import <memory>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// static
WebMainRunner* WebMainRunner::Create() {
  return new WebMainRunnerImpl();
}

}  // namespace web
