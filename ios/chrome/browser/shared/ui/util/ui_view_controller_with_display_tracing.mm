// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/ui_view_controller_with_display_tracing.h"

#import <QuartzCore/QuartzCore.h>

#import <string>

#import "base/strings/sys_string_conversions.h"
#import "base/trace_event/trace_event.h"
#import "base/trace_event/typed_macros.h"
#import "build/build_config.h"

namespace {
enum class UIUpdatePhase {
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
};
}  // namespace

@implementation UIViewControllerWithDisplayTracing {
  std::string _className;
  BOOL _phaseActive;
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  UIUpdateLink* _updateLink;
#endif  // !BUILDFLAG(IS_IOS_MACCATALYST)
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

- (void)viewIsAppearing:(BOOL)animated {
  [super viewIsAppearing:animated];
#if !BUILDFLAG(IS_IOS_MACCATALYST)
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

  _updateLink.enabled = YES;
#endif  // !BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)endCurrentPhaseIfNeeded {
  if (_phaseActive) {
    TRACE_EVENT_END("ui");
    _phaseActive = NO;
  }
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];

#if !BUILDFLAG(IS_IOS_MACCATALYST)
  if (_updateLink) {
    [self endCurrentPhaseIfNeeded];
    _updateLink.enabled = NO;
    _updateLink = nil;
  }
#endif  // !BUILDFLAG(IS_IOS_MACCATALYST)
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
