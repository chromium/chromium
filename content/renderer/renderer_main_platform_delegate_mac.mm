// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "sandbox/mac/seatbelt.h"
#include "sandbox/mac/system_services.h"

extern "C" {
CGError CGSSetDenyWindowServerConnections(bool);
}

namespace content {

namespace {

// This tells Core Graphics not to attempt to connect to the WindowServer (and
// verifies there are no existing open connections), and then indicates that
// Chrome should continue execution without access to launchservicesd.
void DisableSystemServices() {
  // Tell the WindowServer that we don't want to make any future connections.
  // This will return Success as long as there are no open connections, which
  // is what we want.
  CGError result = CGSSetDenyWindowServerConnections(true);
  CHECK_EQ(result, kCGErrorSuccess);

  sandbox::DisableLaunchServices();
}

}  // namespace

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters) {}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

// TODO(mac-port): Any code needed to initialize a process for purposes of
// running a renderer needs to also be reflected in chrome_main.cc for
// --single-process support.
void RendererMainPlatformDelegate::PlatformInitialize() {
  if (![NSThread isMultiThreaded]) {
    NSString* string = @"";
    [NSThread detachNewThreadSelector:@selector(length)
                             toTarget:string
                           withObject:nil];
  }
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  // The sandbox is enabled as part of process launching, so assert that it has
  // been initialized.
  CHECK(sandbox::Seatbelt::IsSandboxed());

  // Inform various system services that they should not attempt to acquire
  // resources that will be blocked by the sandbox.
  DisableSystemServices();

  // Make sure that the renderer has not connected itself to Cocoa.
  CHECK(NSApp == nil);

  return true;
}

}  // namespace content
