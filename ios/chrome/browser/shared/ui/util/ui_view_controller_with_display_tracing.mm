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
  UIViewControllerDisplayTracingOptions _displayTracingOptions;
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  UIUpdateLink* _updateLink;
#endif  // !BUILDFLAG(IS_IOS_MACCATALYST)
}

- (instancetype)init {
  return [self initWithNibName:nil
                        bundle:nil
         displayTracingOptions:UIViewControllerDisplayTracingOptionNone];
}

- (instancetype)initWithDisplayTracingOptions:
    (UIViewControllerDisplayTracingOptions)displayTracingOptions {
  return [self initWithNibName:nil
                        bundle:nil
         displayTracingOptions:displayTracingOptions];
}

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil {
  return [self initWithNibName:nibNameOrNil
                        bundle:nibBundleOrNil
         displayTracingOptions:UIViewControllerDisplayTracingOptionNone];
}

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil
          displayTracingOptions:
              (UIViewControllerDisplayTracingOptions)displayTracingOptions {
  if ((self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil])) {
    _displayTracingOptions = displayTracingOptions;
    [self commonInit];
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  return [self initWithCoder:coder
       displayTracingOptions:UIViewControllerDisplayTracingOptionNone];
}

- (instancetype)initWithCoder:(NSCoder*)coder
        displayTracingOptions:
            (UIViewControllerDisplayTracingOptions)displayTracingOptions {
  if ((self = [super initWithCoder:coder])) {
    _displayTracingOptions = displayTracingOptions;
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
  if (!_updateLink) {
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

    if (_displayTracingOptions &
        UIViewControllerDisplayTracingOptionEventDispatch) {
      registerPhase(UIUpdateActionPhase.beforeEventDispatch,
                    UIUpdatePhase::kBeforeEventDispatch);
      registerPhase(UIUpdateActionPhase.afterEventDispatch,
                    UIUpdatePhase::kAfterEventDispatch);
      registerPhase(UIUpdateActionPhase.beforeLowLatencyEventDispatch,
                    UIUpdatePhase::kBeforeLowLatencyEventDispatch);
      registerPhase(UIUpdateActionPhase.afterLowLatencyEventDispatch,
                    UIUpdatePhase::kAfterLowLatencyEventDispatch);
    }
    if (_displayTracingOptions &
        UIViewControllerDisplayTracingOptionCADisplayLinkDispatch) {
      registerPhase(UIUpdateActionPhase.beforeCADisplayLinkDispatch,
                    UIUpdatePhase::kBeforeCADisplayLinkDispatch);
      registerPhase(UIUpdateActionPhase.afterCADisplayLinkDispatch,
                    UIUpdatePhase::kAfterCADisplayLinkDispatch);
    }
    if (_displayTracingOptions &
        UIViewControllerDisplayTracingOptionCATransactionCommit) {
      registerPhase(UIUpdateActionPhase.beforeCATransactionCommit,
                    UIUpdatePhase::kBeforeCATransactionCommit);
      registerPhase(UIUpdateActionPhase.afterCATransactionCommit,
                    UIUpdatePhase::kAfterCATransactionCommit);
      registerPhase(UIUpdateActionPhase.beforeLowLatencyCATransactionCommit,
                    UIUpdatePhase::kBeforeLowLatencyCATransactionCommit);
      registerPhase(UIUpdateActionPhase.afterLowLatencyCATransactionCommit,
                    UIUpdatePhase::kAfterLowLatencyCATransactionCommit);
    }
    _updateLink.enabled = YES;
  }
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
