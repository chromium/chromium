// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_PUBLIC_SCOPED_FORCE_FULLSCREEN_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_PUBLIC_SCOPED_FORCE_FULLSCREEN_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"

// A helper object that forces fullscreen mode for `feature` over its entire
// lifetime using FullscreenCommands.
class ScopedForceFullscreen {
 public:
  ScopedForceFullscreen(id<FullscreenCommands> handler,
                        ForceFullscreenFeature feature);

  ScopedForceFullscreen(const ScopedForceFullscreen&) = delete;
  ScopedForceFullscreen& operator=(const ScopedForceFullscreen&) = delete;

  ~ScopedForceFullscreen();

 private:
  __weak id<FullscreenCommands> handler_ = nil;
  ForceFullscreenFeature feature_;
};

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_PUBLIC_SCOPED_FORCE_FULLSCREEN_H_
