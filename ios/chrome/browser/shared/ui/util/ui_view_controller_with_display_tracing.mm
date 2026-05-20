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
  kBeforeLowLatencyEventDispatch,
  kAfterLowLatencyEventDispatch,
  kBeforeLowLatencyCATransactionCommit,
};
}  // namespace

@implementation UIViewControllerWithDisplayTracing {
  std::string _className;
  BOOL _phaseActive;
  UIViewControllerDisplayTracingOptions _displayTracingOptions;
  BOOL _appearing;
  uint64_t _flowID;
  uint64_t _flowCounter;
  NSTimeInterval _lastTargetPresentationTime;
  NSTimeInterval _currentFramePeriodEstimate;
  BOOL _isVariableRefreshRate;
  NSTimeInterval _minSupportedFramePeriod;
  int _lastDroppedFrames;
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

    if (_displayTracingOptions & UIViewControllerDisplayTracingOptionDisplay) {
      // Determine the target frame interval based on the display's maximum
      // frame rate. We use 60fps as a baseline. If the display supports a
      // higher refresh rate (e.g. ProMotion), we adjust accordingly. If the
      // actual refresh rate is lower (e.g. due to power saving), that will be
      // detected empirically
      double maxFramesPerSecond = 60.0;
      UIScreen* screen = self.view.window.screen;
      if (screen && screen.maximumFramesPerSecond > 0) {
        maxFramesPerSecond = screen.maximumFramesPerSecond;
        // Determine if we're on a ProMotion (variable refresh rate) display.
        // This is important for the accuracy of our frame drop calculations.
        // This logic assumes that any iOS device with a max frame rate >60fps
        // is a ProMotion device.
        _isVariableRefreshRate = (maxFramesPerSecond > 60.0);
        _minSupportedFramePeriod = 1.0 / maxFramesPerSecond;
      }
      _currentFramePeriodEstimate = 1.0 / maxFramesPerSecond;
    }

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
    [localUpdateLink
        addActionToPhase:UIUpdateActionPhase.afterCATransactionCommit
                 handler:^(UIUpdateLink* link, UIUpdateInfo* info) {
                   [weakSelf handleCATransactionCommitEndWithLink:link
                                                             info:info
                                                     isLowLatency:NO];
                 }];
    [localUpdateLink
        addActionToPhase:UIUpdateActionPhase.afterLowLatencyCATransactionCommit
                 handler:^(UIUpdateLink* link, UIUpdateInfo* info) {
                   [weakSelf handleCATransactionCommitEndWithLink:link
                                                             info:info
                                                     isLowLatency:YES];
                 }];

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

- (NSTimeInterval)currentMediaTime {
  return CACurrentMediaTime();
}

- (void)handleCATransactionCommitEndWithLink:(UIUpdateLink*)link
                                        info:(UIUpdateInfo*)info
                                isLowLatency:(BOOL)isLowLatency {
  [self endCurrentPhaseIfNeeded];
  if ([self displayTracingOptions] &
      UIViewControllerDisplayTracingOptionDisplay) {
    base::TimeTicks nowInTicks = base::TimeTicks::Now();
    NSTimeInterval nowInSeconds = [self currentMediaTime];

    NSTimeInterval targetTime = info.estimatedPresentationTime;
    NSTimeInterval scanoutDelay = targetTime - nowInSeconds;

    // The following logic computes an optimistic estimate of pipeline latency,
    // based on the assumption that the downstream OS compositing pipeline is
    // jank free, which is usually the case. The expected number of dropped
    // frames is also estimated based on the same assumptions. The calculations
    // require knowledge of the display device's refresh rate, which also needs
    // to be estimated if the device has a variable refresh rate (ProMotion).
    // A retained estimate of the Vsync interval is used. This estimate is
    // updated every time we get two consecutive commits that may not have
    // dropped frames.

    // This accounts for arithmetic precision, clock precision, and clock
    // de-sync between `base::TimeTicks::Now()` and `CACurrentMediaTime()`.
    // Real world error shouldn't exceed 1 microsecond, 1e-5 (10
    // microseconds) is a comfortably safe margin.
    constexpr NSTimeInterval kMaxError = 1e-5;
    constexpr NSTimeInterval kMaxSupportedFramePeriod = 1.0 / 10.0;

    // If variable refresh rate is supported, keep the persistent frame period
    // estimate up to date before computing deadlines to avoid miscalculating
    // dropped frames.
    if (_isVariableRefreshRate) {
      // Because _currentFramePeriodEstimate may be wrong, we can't use
      // it to determine with absolute certainty whether the current
      // commit is dropping frames. In cases where the actual frame rate
      // is faster than the current frame rate estimate, we may get a
      // dropped frame estimate that is a false positive, and we don't want
      // that to prevent us from re-estimating the frame rate. To solve this,
      // we use an optimistic presentation deadline estimate that is based on
      // the knowledge that the actual frame period is greater or equal to
      // the device's minimum supported frame period.
      NSTimeInterval optimisticEarliestPresentation =
          isLowLatency ? nowInSeconds : nowInSeconds + _minSupportedFramePeriod;
      if (targetTime < optimisticEarliestPresentation) {
        // We know for sure that frames are being dropped.
        _lastTargetPresentationTime = 0;
      } else {
        if (_lastTargetPresentationTime > 0) {
          // Current commit and previous commit possibly did not drop frames.
          NSTimeInterval delta = targetTime - _lastTargetPresentationTime;
          // Outliers are possible since we don't know with absolute certainty
          // that we're not currently dropping frames. We know for sure that
          // delta is an outlier if it is above kMaxSupportedFramePeriod.
          if (delta < kMaxSupportedFramePeriod + kMaxError) {
            _currentFramePeriodEstimate = delta;
          }
        }
        _lastTargetPresentationTime = targetTime;
      }
    }

    // Establish pipeline latency: Standard rendering cycles are double
    // buffered; whereas low-latency views bypass the OS's rendering queue,
    // reducing latency by one frame.
    NSTimeInterval pipelineMinLatency =
        isLowLatency ? 0 : _currentFramePeriodEstimate;
    NSTimeInterval earliestPresentation = nowInSeconds + pipelineMinLatency;

    // Calculate missed deadlines. If the earliest possible presentation time
    // exceeds the target time, the main thread was delayed and missed the
    // window.
    NSTimeInterval missedBy = earliestPresentation - targetTime;
    int droppedFrames =
        ceil((missedBy + kMaxError) / _currentFramePeriodEstimate);
    droppedFrames = std::max(droppedFrames, 0);
    _lastDroppedFrames = droppedFrames;
    scanoutDelay += droppedFrames * _currentFramePeriodEstimate;

    base::TimeTicks presentationTime = nowInTicks + base::Seconds(scanoutDelay);
    TRACE_EVENT_INSTANT("ui", "Display", perfetto::NamedTrack("Display"),
                        presentationTime,
                        perfetto::Flow::ProcessScoped(_flowID), "controller",
                        _className, "dropped_frames", droppedFrames);
  }

  _flowID = 0;
}
@end
