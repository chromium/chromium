// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/ui_view_controller_with_display_tracing.h"

#import <QuartzCore/QuartzCore.h>

#import <string>

#import "base/metrics/histogram_functions.h"
#import "base/rand_util.h"
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
  kMaybeGesture,
};

// Shared event dispatch timestamp, set by the top-level view controller in the
// view controller hierarchy that has the gesture tracing option enabled.
base::TimeTicks g_possibleGestureTimestamp;
}  // namespace

// A private delegate that handles the tracing gesture recognizers' callbacks to
// ensure they are completely passive and do not interfere with other gesture
// recognizers or UI controls in the view hierarchy.
@interface DisplayTracingGestureDelegate
    : NSObject <UIGestureRecognizerDelegate>
@end

@implementation DisplayTracingGestureDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  // Always recognize simultaneously to be completely transparent and passive.
  return YES;
}

@end

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
  const char* _currentGesture;
  DisplayTracingGestureDelegate* _gestureDelegate;
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
  _gestureDelegate = [[DisplayTracingGestureDelegate alloc] init];
}

#pragma mark - UIViewController overrides

- (void)viewDidLoad {
  [super viewDidLoad];
  if (_displayTracingOptions & UIViewControllerDisplayTracingOptionGesture) {
    UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(handleTapGesture:)];
    tapRecognizer.cancelsTouchesInView = NO;
    tapRecognizer.delaysTouchesBegan = NO;
    tapRecognizer.delaysTouchesEnded = NO;
    tapRecognizer.delegate = _gestureDelegate;
    [self.view addGestureRecognizer:tapRecognizer];

    UIPanGestureRecognizer* panRecognizer = [[UIPanGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(handlePanGesture:)];
    panRecognizer.cancelsTouchesInView = NO;
    panRecognizer.delaysTouchesBegan = NO;
    panRecognizer.delaysTouchesEnded = NO;
    panRecognizer.delegate = _gestureDelegate;
    [self.view addGestureRecognizer:panRecognizer];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  if (_displayTracingOptions & UIViewControllerDisplayTracingOptionAppear) {
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
  if (_displayTracingOptions & UIViewControllerDisplayTracingOptionLayout) {
    TRACE_EVENT_BEGIN("ui", "UIViewControllerWithDisplayTracing LayoutSubviews",
                      perfetto::Flow::ProcessScoped([self ensureFlowID]),
                      "controller", _className);
  }
  [super viewWillLayoutSubviews];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if (_displayTracingOptions & UIViewControllerDisplayTracingOptionLayout) {
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

    // Detect whether this view controller is top-level in the display tracing
    // hierarchy.
    BOOL isTopLevel = YES;
    UIViewController* parent = self.parentViewController;
    while (parent) {
      if ([parent isKindOfClass:[UIViewControllerWithDisplayTracing class]]) {
        isTopLevel = NO;
        break;
      }
      parent = parent.parentViewController;
    }

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

    if (isTopLevel && (_displayTracingOptions &
                       UIViewControllerDisplayTracingOptionGesture)) {
      registerPhase(UIUpdateActionPhase.beforeEventDispatch,
                    UIUpdatePhase::kMaybeGesture);
      registerPhase(UIUpdateActionPhase.beforeLowLatencyEventDispatch,
                    UIUpdatePhase::kMaybeGesture);
    }

    if (isTopLevel && (_displayTracingOptions &
                       UIViewControllerDisplayTracingOptionEventDispatch)) {
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

#pragma mark - Gesture Handlers

- (void)handleTapGesture:(UITapGestureRecognizer*)sender {
  CGPoint location = [sender locationInView:sender.view];
  UIView* hitView = [sender.view hitTest:location withEvent:nil];

  // 1. Check if the hit view is an interactive control (like a UIButton).
  if ([hitView isKindOfClass:[UIControl class]]) {
    _currentGesture = "Tap";
    return;
  }

  // 2. Traverse up the view hierarchy to check if any parent of hitView's tap
  // gesture recognizer has recognized the gesture simultaneously.
  UIView* view = hitView;
  while (view && view != sender.view) {
    for (UIGestureRecognizer* recognizer in view.gestureRecognizers) {
      if (recognizer != sender &&
          [recognizer isKindOfClass:[UITapGestureRecognizer class]] &&
          (recognizer.state == UIGestureRecognizerStateRecognized)) {
        _currentGesture = "Tap";
        return;
      }
    }
    view = view.superview;
  }
}

- (void)handlePanGesture:(UIPanGestureRecognizer*)sender {
  CGPoint location = [sender locationInView:sender.view];
  UIView* hitView = [sender.view hitTest:location withEvent:nil];

  // Traverse up the view hierarchy to check if any parent of the hitView's pan
  // or swipe gesture recognizer has recognized the gesture simultaneously.
  UIView* view = hitView;
  while (view && view != sender.view) {
    for (UIGestureRecognizer* recognizer in view.gestureRecognizers) {
      if (recognizer != sender &&
          ([recognizer isKindOfClass:[UIPanGestureRecognizer class]] ||
           [recognizer isKindOfClass:[UISwipeGestureRecognizer class]]) &&
          (recognizer.state == UIGestureRecognizerStateBegan ||
           recognizer.state == UIGestureRecognizerStateChanged ||
           recognizer.state == UIGestureRecognizerStateRecognized)) {
        _currentGesture = "Pan";
        return;
      }
    }
    view = view.superview;
  }
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
    case UIUpdatePhase::kMaybeGesture:
      // At this time we know an event is queued, but we don't yet know whether
      // it will be recognized as a gesture we care about.
      g_possibleGestureTimestamp = base::TimeTicks::Now();
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

- (const char*)currentGestureForTesting {
  return _currentGesture;
}

#if !BUILDFLAG(IS_IOS_MACCATALYST)
- (void)handleCATransactionCommitEndWithLink:(UIUpdateLink*)link
                                        info:(UIUpdateInfo*)info
                                isLowLatency:(BOOL)isLowLatency {
  [self endCurrentPhaseIfNeeded];
  bool shouldRecordGestureLatency =
      _currentGesture && !g_possibleGestureTimestamp.is_null() /*&&
                         base::ShouldRecordSubsampledMetric(0.01)*/
      ;

  if ((_displayTracingOptions & UIViewControllerDisplayTracingOptionDisplay) ||
      shouldRecordGestureLatency) {
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
    base::TimeDelta scanoutDelayTimeDelta = base::Seconds(scanoutDelay);
    base::TimeTicks presentationTime = nowInTicks + scanoutDelayTimeDelta;
    if (_displayTracingOptions & UIViewControllerDisplayTracingOptionDisplay) {
      TRACE_EVENT_INSTANT(
          "ui", "Display", perfetto::NamedTrack("Display"), presentationTime,
          perfetto::Flow::ProcessScoped(_flowID), "controller", _className,
          "dropped_frames", droppedFrames, "gesture",
          _currentGesture ? _currentGesture : "none/unknown", "is_low_latency",
          isLowLatency);
    }
    if (shouldRecordGestureLatency) {
      std::string histogram_name =
          "IOS.InputLatency." + _className + "." + _currentGesture;
      base::TimeDelta inputToCommitTimeDelta =
          nowInTicks - g_possibleGestureTimestamp;
      base::UmaHistogramTimes(histogram_name + ".InputToCommit",
                              inputToCommitTimeDelta);
      base::UmaHistogramTimes(histogram_name + ".CommitToDisplay",
                              scanoutDelayTimeDelta);
      // Due to probable covariance between the individual component metrics,
      // the distribution of their sum cannot be reliably estimated from the
      // distributions of the components. Therefore the sum metric must also be
      // recorded.
      base::UmaHistogramTimes(histogram_name + ".InputToDisplay",
                              inputToCommitTimeDelta + scanoutDelayTimeDelta);

      base::UmaHistogramCounts1000(histogram_name + ".DroppedFrames",
                                   droppedFrames);
    }
  }

  _flowID = 0;
  _currentGesture = nullptr;
}
#endif  // !BUILDFLAG(IS_IOS_MACCATALYST)
@end
