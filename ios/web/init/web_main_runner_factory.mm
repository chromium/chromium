// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/init/web_main_runner.h"

#import "build/blink_buildflags.h"
#import "ios/web/init/web_main_runner_impl.h"

#if BUILDFLAG(USE_BLINK)
#import "ios/web/content/init/ios_content_main_runner.h"
#endif  // USE_BLINK

#import <memory>

namespace web {

// static
WebMainRunner* WebMainRunner::Create() {
#if BUILDFLAG(USE_BLINK)
  return new IOSContentMainRunner();
#else
  return new WebMainRunnerImpl();
#endif  // USE_BLINK
}

}  // namespace web
