// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/ui_view_controller_with_display_tracing.h"

#import <QuartzCore/QuartzCore.h>

#import <string>

#import "base/strings/sys_string_conversions.h"
#import "base/trace_event/trace_event.h"
#import "base/trace_event/typed_macros.h"

@implementation UIViewControllerWithDisplayTracing {
  std::string _className;
  CADisplayLink* _displayLink;
}

- (instancetype)init {
  if ((self = [super init])) {
    [self commonInit];
  }
  return self;
}

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil {
  if ((self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil])) {
    [self commonInit];
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  if ((self = [super initWithCoder:coder])) {
    [self commonInit];
  }
  return self;
}

- (void)commonInit {
  _className = base::SysNSStringToUTF8(NSStringFromClass([self class]));
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  if (!_displayLink) {
    _displayLink = [CADisplayLink displayLinkWithTarget:self
                                               selector:@selector(didDisplay:)];
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop]
                       forMode:NSRunLoopCommonModes];
  }
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];

  [_displayLink invalidate];
  _displayLink = nil;
}

- (void)didDisplay:(CADisplayLink*)displayLink {
  // Note: Even though the class name is immutable, to avoid potential trace
  // data corruption if the view controller is destryed while a trace is being
  // captured, we must use perfetto::DynamicString.
  TRACE_EVENT_INSTANT("ui", perfetto::DynamicString(_className.c_str()),
                      perfetto::NamedTrack("Display"));
}

@end
