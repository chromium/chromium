// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_utils.h"

#import <AppKit/AppKit.h>

#include "content/browser/web_contents/web_contents_impl.h"
#import "content/browser/web_contents/web_contents_view_mac.h"
#import "content/browser/web_contents/web_drag_dest_mac.h"
#include "content/public/browser/web_contents.h"

namespace content {

namespace {

WebDragDest* GetWebDragDest(WebContents* web_contents) {
  if (!web_contents) {
    return nil;
  }
  auto* outermost_contents =
      static_cast<WebContentsImpl*>(web_contents->GetOutermostWebContents());
  if (!outermost_contents || !outermost_contents->GetView()) {
    return nil;
  }
  return static_cast<WebContentsViewMac*>(outermost_contents->GetView())
      ->drag_dest();
}

}  // namespace

bool EnableNativeWindowActivation() {
  // Do not downgrade the activation policy.
  if (NSApp.activationPolicy > NSApplicationActivationPolicyProhibited) {
    return true;
  }

  // NSApplicationActivationPolicyAccessory is the least permissive policy that
  // still allows for programmatic activation.
  return [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory]
             ? true
             : false;
}

bool IsDragCanceledForTesting(WebContents* web_contents) {
  WebDragDest* drag_dest = GetWebDragDest(web_contents);
  return !drag_dest || [drag_dest isDragCanceledForTesting];
}

void CleanupDragStateForTesting(WebContents* web_contents) {
  if (WebDragDest* drag_dest = GetWebDragDest(web_contents)) {
    [drag_dest cleanupDragState];
  }
}

}  // namespace content
