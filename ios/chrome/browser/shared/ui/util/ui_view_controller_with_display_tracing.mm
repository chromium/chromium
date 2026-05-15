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
  BOOL _appearing;
  uint64_t _flowID;
  uint64_t _flowCounter;
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

- (UIViewControllerDisplayTracingOptions)displayTracingOptions {
  return UIViewControllerDisplayTracingOptionEssentialTraces;
}

- (void)viewWillAppear:(BOOL)animated {
  if ([self displayTracingOptions] &
      UIViewControllerDisplayTracingOptionAppear) {
    // Note: `viewWillAppear:` and `viewDidAppear:` execute across different
    // runloop spins, so using TRACE_EVENT_BEGIN/END here intentionally leaves
    // the trace slice open across runloop boundaries on the main thread. All
    // main thread tasks executed during the transition will be nested inside
    // this "Appear" event. This is intended: the UI thread is busy during this
    // transition period, and since CoreFoundation does not provide hooks to
    // bracket individual CF runloop tasks, this slice gives visibility into the
    // burst of UI work occurring while the view is appearing.
    TRACE_EVENT_BEGIN("ui", "UIViewControllerWithDisplayTracing Appear",
                      perfetto::Flow::ProcessScoped([self ensureFlowID]),
                      "controller", _className);
    _appearing = YES;
  }
  [super viewWillAppear:animated];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  if (_appearing) {
    TRACE_EVENT_END("ui");
    _appearing = NO;
  }
}

- (void)viewWillLayoutSubviews {
  if ([self displayTracingOptions] &
      UIViewControllerDisplayTracingOptionLayout) {
    TRACE_EVENT_BEGIN("ui", "UIViewControllerWithDisplayTracing LayoutSubviews",
                      perfetto::Flow::ProcessScoped([self ensureFlowID]),
                      "controller", _className);
  }
  [super viewWillLayoutSubviews];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if ([self displayTracingOptions] &
      UIViewControllerDisplayTracingOptionLayout) {
    TRACE_EVENT_END("ui");
  }
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
      registerPhase(UIUpdateActionPhase.beforeLowLatencyCATransactionCommit,
                    UIUpdatePhase::kBeforeLowLatencyCATransactionCommit);
    }

    // Always handle the after phases of transaction commit to reset the flow
    // id.
    registerPhase(UIUpdateActionPhase.afterCATransactionCommit,
                  UIUpdatePhase::kAfterCATransactionCommit);
    registerPhase(UIUpdateActionPhase.afterLowLatencyCATransactionCommit,
                  UIUpdatePhase::kAfterLowLatencyCATransactionCommit);
    _updateLink.enabled = YES;
  }
#endif  // !BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];

  if (_appearing) {
    TRACE_EVENT_END("ui");
    _appearing = NO;
  }

#if !BUILDFLAG(IS_IOS_MACCATALYST)
  if (_updateLink) {
    [self endCurrentPhaseIfNeeded];
    _updateLink.enabled = NO;
    _updateLink = nil;
  }
#endif  // !BUILDFLAG(IS_IOS_MACCATALYST)
}

#pragma mark - Private

// Ends the current phase's tracing slice, if there is one.
- (void)endCurrentPhaseIfNeeded {
  if (_phaseActive) {
    TRACE_EVENT_END("ui");
    _phaseActive = NO;
  }
}

// Returns a flow id that is unique to this view controller and its current
// update pass.
- (uint64_t)ensureFlowID {
  if (_flowID == 0) {
    _flowID = reinterpret_cast<uint64_t>(self) ^ ++_flowCounter;
  }
  return _flowID;
}

// Handles the beginning and end of the different update phases.
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
    case UIUpdatePhase::kAfterCATransactionCommit:
    case UIUpdatePhase::kAfterLowLatencyCATransactionCommit:
      _flowID = 0;
      break;
    default:
      break;
  }

  if (phaseName) {
    TRACE_EVENT_BEGIN("ui", perfetto::DynamicString(phaseName),
                      perfetto::Flow::ProcessScoped([self ensureFlowID]),
                      "controller", _className);
    _phaseActive = YES;
  }
}

@end
