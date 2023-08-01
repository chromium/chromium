// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/shell/browser/renderer_host/shell_render_widget_host_view_mac_delegate.h"

#include "base/command_line.h"
#include "content/shell/common/shell_switches.h"

@implementation ShellRenderWidgetHostViewMacDelegate {
  BOOL _drop_events;
}

- (id)init {
  if ((self = [super init])) {
    // Throw out all native input events if we are running with web test
    // enabled.
    _drop_events = switches::IsRunWebTestsSwitchPresent();
  }
  return self;
}

- (BOOL)handleEvent:(NSEvent*)event {
  return _drop_events;
}

- (void)beginGestureWithEvent:(NSEvent*)event {
}

- (void)endGestureWithEvent:(NSEvent*)event {
}

- (void)touchesMovedWithEvent:(NSEvent*)event {
}

- (void)touchesBeganWithEvent:(NSEvent*)event {
}

- (void)touchesCancelledWithEvent:(NSEvent*)event {
}

- (void)touchesEndedWithEvent:(NSEvent*)event {
}

- (void)rendererHandledWheelEvent:(const blink::WebMouseWheelEvent&)event
                         consumed:(BOOL)consumed {
}

- (void)rendererHandledGestureScrollEvent:(const blink::WebGestureEvent&)event
                                 consumed:(BOOL)consumed {
}

- (void)rendererHandledOverscrollEvent:(const ui::DidOverscrollParams&)params {
}

@end
