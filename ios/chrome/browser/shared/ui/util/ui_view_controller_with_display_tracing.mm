// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/ui_view_controller_with_display_tracing.h"

#import <QuartzCore/QuartzCore.h>

#import <string>

#import "base/strings/sys_string_conversions.h"
#import "base/trace_event/trace_event.h"
#import "base/trace_event/typed_macros.h"

namespace {
enum class UIUpdatePhase {
  kAfterUpdateScheduled,
  kBeforeEventDispatch,
  kAfterEventDispatch,
  kBeforeCADisplayLinkDispatch,
  kAfterCADisplayLinkDispatch,
  kBeforeCATransactionCommit,
  kAfterCATransactionCommit,
  kBeforeLowLatencyEventDispatch,
  kAfterLowLatencyEventDispatch,
  kBeforeLowLatencyCATransactionCommit,
  kAfterLowLatencyCATransactionCommit,
  kAfterUpdateComplete,
};
}  // namespace

@implementation UIViewControllerWithDisplayTracing {
  std::string _className;
  BOOL _phaseActive;
  UIUpdateLink* _updateLink;
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
  [self maybeInitializeUpdateLink];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self maybeInitializeUpdateLink];
}

- (void)maybeInitializeUpdateLink {
  // UIUpdateLink requires the view to be attached to a window hierarchy to be
  // correctly configured. If `self.view.window` is nil (which is often the
  // case during the initial `viewWillAppear:`), defer initialization until
  // the window is available (e.g., in `viewDidAppear:`).
  if (!_updateLink && self.view.window) {
    _updateLink = [UIUpdateLink updateLinkForView:self.view];
    if (!_updateLink) {
      return;
    }

    UIUpdateLink* localUpdateLink = _updateLink;
    __weak __typeof(self) weakSelf = self;
    auto registerPhase = ^(UIUpdateActionPhase* phase, UIUpdatePhase phaseId) {
      [localUpdateLink
          addActionToPhase:phase
                   handler:^(UIUpdateLink* link, UIUpdateInfo* info) {
                     [weakSelf handleUpdatePhase:phaseId];
                   }];
    };

    registerPhase(UIUpdateActionPhase.afterUpdateScheduled,
                  UIUpdatePhase::kAfterUpdateScheduled);
    registerPhase(UIUpdateActionPhase.beforeEventDispatch,
                  UIUpdatePhase::kBeforeEventDispatch);
    registerPhase(UIUpdateActionPhase.afterEventDispatch,
                  UIUpdatePhase::kAfterEventDispatch);
    registerPhase(UIUpdateActionPhase.beforeCADisplayLinkDispatch,
                  UIUpdatePhase::kBeforeCADisplayLinkDispatch);
    registerPhase(UIUpdateActionPhase.afterCADisplayLinkDispatch,
                  UIUpdatePhase::kAfterCADisplayLinkDispatch);
    registerPhase(UIUpdateActionPhase.beforeCATransactionCommit,
                  UIUpdatePhase::kBeforeCATransactionCommit);
    registerPhase(UIUpdateActionPhase.afterCATransactionCommit,
                  UIUpdatePhase::kAfterCATransactionCommit);
    registerPhase(UIUpdateActionPhase.beforeLowLatencyEventDispatch,
                  UIUpdatePhase::kBeforeLowLatencyEventDispatch);
    registerPhase(UIUpdateActionPhase.afterLowLatencyEventDispatch,
                  UIUpdatePhase::kAfterLowLatencyEventDispatch);
    registerPhase(UIUpdateActionPhase.beforeLowLatencyCATransactionCommit,
                  UIUpdatePhase::kBeforeLowLatencyCATransactionCommit);
    registerPhase(UIUpdateActionPhase.afterLowLatencyCATransactionCommit,
                  UIUpdatePhase::kAfterLowLatencyCATransactionCommit);
    registerPhase(UIUpdateActionPhase.afterUpdateComplete,
                  UIUpdatePhase::kAfterUpdateComplete);

    _updateLink.enabled = YES;
  }
}

- (void)endCurrentPhaseIfNeeded {
  if (_phaseActive) {
    TRACE_EVENT_END("ui");
    _phaseActive = NO;
  }
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];

  if (_updateLink) {
    [self endCurrentPhaseIfNeeded];
    _updateLink.enabled = NO;
    _updateLink = nil;
  }
}

- (void)handleUpdatePhase:(UIUpdatePhase)phaseId {
  [self endCurrentPhaseIfNeeded];

  const char* phaseName = nullptr;
  switch (phaseId) {
    case UIUpdatePhase::kBeforeEventDispatch:
      phaseName = "EventDispatch";
      break;
    case UIUpdatePhase::kBeforeCADisplayLinkDispatch:
      phaseName = "CADisplayLinkDispatch";
      break;
    case UIUpdatePhase::kBeforeCATransactionCommit:
      phaseName = "CATransactionCommit";
      break;
    case UIUpdatePhase::kBeforeLowLatencyEventDispatch:
      phaseName = "LowLatencyEventDispatch";
      break;
    case UIUpdatePhase::kBeforeLowLatencyCATransactionCommit:
      phaseName = "LowLatencyCATransactionCommit";
      break;
    default:
      break;
  }

  if (phaseName) {
    TRACE_EVENT_BEGIN("ui", perfetto::DynamicString(phaseName), "controller",
                      _className);
    _phaseActive = YES;
  }
}

@end
