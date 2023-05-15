// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_RENDERER_HOST_SHELL_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_RENDERER_HOST_SHELL_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_

#import <Cocoa/Cocoa.h>

#import "content/public/browser/render_widget_host_view_mac_delegate.h"

@interface ShellRenderWidgetHostViewMacDelegate
    : NSObject <RenderWidgetHostViewMacDelegate>

@end

#endif  // CONTENT_SHELL_BROWSER_RENDERER_HOST_SHELL_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_
