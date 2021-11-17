// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/mac_scrollbar_animator_impl.h"

#import <AppKit/AppKit.h>

#include "base/mac/scoped_nsobject.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/scroll/ns_scroller_imp_details.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_mac.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/geometry/cg_conversions.h"
#include "third_party/blink/renderer/platform/mac/block_exceptions.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {
namespace {

typedef HeapHashMap<WeakMember<const Scrollbar>, MacScrollbarImpl*>
    ScrollbarToAnimatorMap;

ScrollbarToAnimatorMap& GetScrollbarToAnimatorMap() {
  DEFINE_STATIC_LOCAL(Persistent<ScrollbarToAnimatorMap>, map,
                      (MakeGarbageCollected<ScrollbarToAnimatorMap>()));
  return *map;
}

}  // namespace
}  // namespace blink

namespace {
bool SupportsUIStateTransitionProgress() {
  // FIXME: This is temporary until all platforms that support ScrollbarPainter
  // support this part of the API.
  static bool global_supports_ui_state_transition_progress =
      [NSClassFromString(@"NSScrollerImp")
          instancesRespondToSelector:@selector(mouseEnteredScroller)];
  return global_supports_ui_state_transition_progress;
}

bool SupportsExpansionTransitionProgress() {
  static bool global_supports_expansion_transition_progress =
      [NSClassFromString(@"NSScrollerImp")
          instancesRespondToSelector:@selector(expansionTransitionProgress)];
  return global_supports_expansion_transition_progress;
}

bool SupportsContentAreaScrolledInDirection() {
  static bool global_supports_content_area_scrolled_in_direction =
      [NSClassFromString(@"NSScrollerImpPair")
          instancesRespondToSelector:@selector
          (contentAreaScrolledInDirection:)];
  return global_supports_content_area_scrolled_in_direction;
}

blink::ScrollbarThemeMac* MacOverlayScrollbarTheme(
    blink::ScrollbarTheme& scrollbar_theme) {
  return !scrollbar_theme.IsMockTheme()
             ? static_cast<blink::ScrollbarThemeMac*>(&scrollbar_theme)
             : nil;
}

bool IsScrollbarRegistered(blink::Scrollbar& scrollbar) {
  if (blink::ScrollbarThemeMac* scrollbar_theme =
          MacOverlayScrollbarTheme(scrollbar.GetTheme())) {
    return scrollbar_theme->IsScrollbarRegistered(scrollbar);
  }
  return false;
}

ScrollbarPainter ScrollbarPainterForScrollbar(
    const blink::Scrollbar& scrollbar) {
  if (auto* mac_scrollbar_impl =
          blink::MacScrollbarImpl::GetForScrollbar(scrollbar)) {
    return mac_scrollbar_impl->painter();
  }
  return nil;
}

}  // namespace

// This class is a delegator of ScrollbarPainterController to ScrollableArea
// that has the scrollbars of a ScrollbarPainter.
@interface BlinkScrollbarPainterControllerDelegate : NSObject {
  blink::ScrollableArea* _scrollableArea;
}
- (instancetype)initWithScrollableArea:(blink::ScrollableArea*)scrollableArea;
@end

@implementation BlinkScrollbarPainterControllerDelegate

- (instancetype)initWithScrollableArea:(blink::ScrollableArea*)scrollableArea {
  self = [super init];
  if (!self)
    return nil;

  _scrollableArea = scrollableArea;
  return self;
}

- (void)invalidate {
  _scrollableArea = 0;
}

- (NSRect)contentAreaRectForScrollerImpPair:(id)scrollerImpPair {
  if (!_scrollableArea)
    return NSZeroRect;

  blink::IntSize contentsSize = _scrollableArea->ContentsSize();
  return NSMakeRect(0, 0, contentsSize.width(), contentsSize.height());
}

- (BOOL)inLiveResizeForScrollerImpPair:(id)scrollerImpPair {
  return NO;
}

- (NSPoint)mouseLocationInContentAreaForScrollerImpPair:(id)scrollerImpPair {
  if (!_scrollableArea)
    return NSZeroPoint;

  return blink::PointToCGPoint(_scrollableArea->LastKnownMousePosition());
}

- (NSPoint)scrollerImpPair:(id)scrollerImpPair
       convertContentPoint:(NSPoint)pointInContentArea
             toScrollerImp:(id)scrollerImp {
  if (!_scrollableArea || !scrollerImp)
    return NSZeroPoint;

  blink::Scrollbar* scrollbar = nil;
  if ([scrollerImp isHorizontal])
    scrollbar = _scrollableArea->HorizontalScrollbar();
  else
    scrollbar = _scrollableArea->VerticalScrollbar();

  // It is possible to have a null scrollbar here since it is possible for this
  // delegate method to be called between the moment when a scrollbar has been
  // set to 0 and the moment when its destructor has been called.
  if (!scrollbar)
    return NSZeroPoint;

  DCHECK_EQ(scrollerImp, ScrollbarPainterForScrollbar(*scrollbar));

  return blink::PointToCGPoint(
      scrollbar->ConvertFromContainingEmbeddedContentView(
          gfx::Point(pointInContentArea)));
}

- (void)scrollerImpPair:(id)scrollerImpPair
    setContentAreaNeedsDisplayInRect:(NSRect)rect {
  if (!_scrollableArea)
    return;

  if (!_scrollableArea->ScrollbarsCanBeActive())
    return;

  _scrollableArea->ContentAreaWillPaint();
}

- (void)scrollerImpPair:(id)scrollerImpPair
    updateScrollerStyleForNewRecommendedScrollerStyle:
        (NSScrollerStyle)newRecommendedScrollerStyle {
  // Chrome has a single process mode which is used for testing on Mac. In that
  // mode, WebKit runs on a thread in the
  // browser process. This notification is called by the OS on the main thread
  // in the browser process, and not on the
  // the WebKit thread. Better to not update the style than crash.
  // http://crbug.com/126514
  if (!IsMainThread())
    return;

  if (!_scrollableArea || !_scrollableArea->GetMacScrollbarAnimator())
    return;

  [scrollerImpPair setScrollerStyle:newRecommendedScrollerStyle];

  _scrollableArea->GetMacScrollbarAnimator()->UpdateScrollerStyle();
}

@end

enum FeatureToAnimate {
  ThumbAlpha,
  TrackAlpha,
  UIStateTransition,
  ExpansionTransition
};

@class BlinkScrollbarPartAnimation;
namespace blink {
// This class is used to drive the animation timer for
// BlinkScrollbarPartAnimation
// objects. This is used instead of NSAnimation because CoreAnimation
// establishes connections to the WindowServer, which should not be done in a
// sandboxed renderer process.
class BlinkScrollbarPartAnimationTimer {
 public:
  BlinkScrollbarPartAnimationTimer(
      BlinkScrollbarPartAnimation* animation,
      CFTimeInterval duration,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : timer_(std::move(task_runner),
               this,
               &BlinkScrollbarPartAnimationTimer::TimerFired),
        start_time_(0.0),
        duration_(duration),
        animation_(animation),
        timing_function_(CubicBezierTimingFunction::Preset(
            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)) {}

  ~BlinkScrollbarPartAnimationTimer() {}

  void Start() {
    start_time_ = base::Time::Now().ToDoubleT();
    // Set the framerate of the animation. NSAnimation uses a default
    // framerate of 60 Hz, so use that here.
    timer_.StartRepeating(base::Seconds(1.0 / 60.0), FROM_HERE);
  }

  void Stop() { timer_.Stop(); }

  void SetDuration(CFTimeInterval duration) { duration_ = duration; }

 private:
  void TimerFired(TimerBase*) {
    double current_time = base::Time::Now().ToDoubleT();
    double delta = current_time - start_time_;

    if (delta >= duration_)
      timer_.Stop();

    double fraction = delta / duration_;
    fraction = ClampTo(fraction, 0.0, 1.0);
    double progress = timing_function_->Evaluate(fraction);
    // In some scenarios, animation_ gets released during the call to
    // setCurrentProgress. Because BlinkScrollbarPartAnimationTimer is a
    // member variable of BlinkScrollbarPartAnimation animation_ the timer
    // gets freed at the same time with animation_. In that case, it will
    // not be safe to call any other code after animation_ setCurrentProgress.
    [animation_ setCurrentProgress:progress];
  }

  TaskRunnerTimer<BlinkScrollbarPartAnimationTimer> timer_;
  double start_time_;                       // In seconds.
  double duration_;                         // In seconds.
  BlinkScrollbarPartAnimation* animation_;  // Weak, owns this.
  scoped_refptr<CubicBezierTimingFunction> timing_function_;
};

}  // namespace blink

// This class handles the animation of a |_featureToAnimate| part of
// |_scrollbar|.
@interface BlinkScrollbarPartAnimation : NSObject {
  blink::Scrollbar* _scrollbar;
  std::unique_ptr<blink::BlinkScrollbarPartAnimationTimer> _timer;
  base::scoped_nsobject<ScrollbarPainter> _scrollbarPainter;
  FeatureToAnimate _featureToAnimate;
  CGFloat _startValue;
  CGFloat _endValue;
}
- (instancetype)initWithScrollbar:(blink::Scrollbar*)scrollbar
                 featureToAnimate:(FeatureToAnimate)featureToAnimate
                      animateFrom:(CGFloat)startValue
                        animateTo:(CGFloat)endValue
                         duration:(NSTimeInterval)duration
                       taskRunner:(scoped_refptr<base::SingleThreadTaskRunner>)
                                      taskRunner;
@end

@implementation BlinkScrollbarPartAnimation

- (instancetype)initWithScrollbar:(blink::Scrollbar*)scrollbar
                 featureToAnimate:(FeatureToAnimate)featureToAnimate
                      animateFrom:(CGFloat)startValue
                        animateTo:(CGFloat)endValue
                         duration:(NSTimeInterval)duration
                       taskRunner:(scoped_refptr<base::SingleThreadTaskRunner>)
                                      taskRunner {
  self = [super init];
  if (!self)
    return nil;

  _timer = std::make_unique<blink::BlinkScrollbarPartAnimationTimer>(
      self, duration, std::move(taskRunner));
  _scrollbar = scrollbar;
  _featureToAnimate = featureToAnimate;
  _startValue = startValue;
  _endValue = endValue;

  return self;
}

- (void)startAnimation {
  DCHECK(_scrollbar);
  _scrollbarPainter.reset(ScrollbarPainterForScrollbar(*_scrollbar),
                          base::scoped_policy::RETAIN);
  _timer->Start();
}

- (void)stopAnimation {
  _timer->Stop();
}

- (void)setDuration:(CFTimeInterval)duration {
  _timer->SetDuration(duration);
}

- (void)setStartValue:(CGFloat)startValue {
  _startValue = startValue;
}

- (void)setEndValue:(CGFloat)endValue {
  _endValue = endValue;
}

- (void)setCurrentProgress:(NSAnimationProgress)progress {
  DCHECK(_scrollbar);
  // In some scenarios, BlinkScrollbarPartAnimation is released in the middle
  // of this method by _scrollbarPainter. This is why we have to retain the self
  // pointer when we run this method.
  [self retain];

  CGFloat currentValue;
  if (_startValue > _endValue)
    currentValue = 1 - progress;
  else
    currentValue = progress;

  blink::ScrollbarPart invalidParts = blink::kNoPart;
  switch (_featureToAnimate) {
    case ThumbAlpha:
      [_scrollbarPainter setKnobAlpha:currentValue];
      break;
    case TrackAlpha:
      [_scrollbarPainter setTrackAlpha:currentValue];
      invalidParts = static_cast<blink::ScrollbarPart>(~blink::kThumbPart);
      break;
    case UIStateTransition:
      [_scrollbarPainter setUiStateTransitionProgress:currentValue];
      invalidParts = blink::kAllParts;
      break;
    case ExpansionTransition:
      [_scrollbarPainter setExpansionTransitionProgress:currentValue];
      invalidParts = blink::kThumbPart;
      break;
  }

  // Before BlinkScrollbarPartAnimation is released by _scrollbarPainter,
  // invalidate is called and _scrollbar is set to nullptr. Check to see
  // if _scrollbar is non-null before calling SetNeedsPaintInvalidation.
  if (_scrollbar)
    _scrollbar->SetNeedsPaintInvalidation(invalidParts);

  [self release];
}

- (void)invalidate {
  BEGIN_BLOCK_OBJC_EXCEPTIONS;
  [self stopAnimation];
  END_BLOCK_OBJC_EXCEPTIONS;
  _scrollbar = nullptr;
}
@end

// This class is a delegator of ScrollbarPainter to the 4 types of animation
// it can run. The animations are run through BlinkScrollbarPartAnimation.
@interface BlinkScrollbarPainterDelegate : NSObject <NSAnimationDelegate> {
  blink::Scrollbar* _scrollbar;
  scoped_refptr<base::SingleThreadTaskRunner> _taskRunner;

  base::scoped_nsobject<BlinkScrollbarPartAnimation> _knobAlphaAnimation;
  base::scoped_nsobject<BlinkScrollbarPartAnimation> _trackAlphaAnimation;
  base::scoped_nsobject<BlinkScrollbarPartAnimation>
      _uiStateTransitionAnimation;
  base::scoped_nsobject<BlinkScrollbarPartAnimation>
      _expansionTransitionAnimation;
  BOOL _hasExpandedSinceInvisible;
}
- (instancetype)initWithScrollbar:(blink::Scrollbar*)scrollbar
                       taskRunner:(scoped_refptr<base::SingleThreadTaskRunner>)
                                      taskRunner;
- (void)updateVisibilityImmediately:(bool)show;
- (void)cancelAnimations;
@end

@implementation BlinkScrollbarPainterDelegate

- (instancetype)initWithScrollbar:(blink::Scrollbar*)scrollbar
                       taskRunner:(scoped_refptr<base::SingleThreadTaskRunner>)
                                      taskRunner {
  self = [super init];
  if (!self)
    return nil;

  _scrollbar = scrollbar;
  _taskRunner = taskRunner;
  return self;
}

- (void)updateVisibilityImmediately:(bool)show {
  [self cancelAnimations];
  [ScrollbarPainterForScrollbar(*_scrollbar) setKnobAlpha:(show ? 1.0 : 0.0)];
}

- (void)cancelAnimations {
  BEGIN_BLOCK_OBJC_EXCEPTIONS;
  [_knobAlphaAnimation stopAnimation];
  [_trackAlphaAnimation stopAnimation];
  [_uiStateTransitionAnimation stopAnimation];
  [_expansionTransitionAnimation stopAnimation];
  END_BLOCK_OBJC_EXCEPTIONS;
}

- (blink::ScrollAnimator&)scrollAnimator {
  return static_cast<blink::ScrollAnimator&>(
      _scrollbar->GetScrollableArea()->GetScrollAnimator());
}

- (NSRect)convertRectToBacking:(NSRect)aRect {
  return aRect;
}

- (NSRect)convertRectFromBacking:(NSRect)aRect {
  return aRect;
}

- (NSPoint)mouseLocationInScrollerForScrollerImp:(id)scrollerImp {
  if (!_scrollbar)
    return NSZeroPoint;

  DCHECK_EQ(scrollerImp, ScrollbarPainterForScrollbar(*_scrollbar));

  return blink::PointToCGPoint(
      _scrollbar->ConvertFromContainingEmbeddedContentView(
          _scrollbar->GetScrollableArea()->LastKnownMousePosition()));
}

- (void)setUpAlphaAnimation:
            (base::scoped_nsobject<BlinkScrollbarPartAnimation>&)
                scrollbarPartAnimation
            scrollerPainter:(ScrollbarPainter)scrollerPainter
                       part:(blink::ScrollbarPart)part
             animateAlphaTo:(CGFloat)newAlpha
                   duration:(NSTimeInterval)duration {
  blink::MacScrollbarAnimator* scrollbar_animator =
      _scrollbar->GetScrollableArea()->GetMacScrollbarAnimator();
  DCHECK(scrollbar_animator);

  // If the user has scrolled the page, then the scrollbars must be animated
  // here.
  // This overrides the early returns.
  bool mustAnimate = [self scrollAnimator].HaveScrolledSincePageLoad();

  if (scrollbar_animator->ScrollbarPaintTimerIsActive() && !mustAnimate)
    return;

  if (_scrollbar->GetScrollableArea()->ShouldSuspendScrollAnimations() &&
      !mustAnimate) {
    scrollbar_animator->StartScrollbarPaintTimer();
    return;
  }

  // At this point, we are definitely going to animate now, so stop the timer.
  scrollbar_animator->StopScrollbarPaintTimer();

  // If we are currently animating, stop
  if (scrollbarPartAnimation) {
    [scrollbarPartAnimation invalidate];
    scrollbarPartAnimation.reset();
  }

  scrollbarPartAnimation.reset([[BlinkScrollbarPartAnimation alloc]
      initWithScrollbar:_scrollbar
       featureToAnimate:part == blink::kThumbPart ? ThumbAlpha : TrackAlpha
            animateFrom:part == blink::kThumbPart ? [scrollerPainter knobAlpha]
                                                  : [scrollerPainter trackAlpha]
              animateTo:newAlpha
               duration:duration
             taskRunner:_taskRunner]);
  [scrollbarPartAnimation startAnimation];
}

- (void)scrollerImp:(id)scrollerImp
    animateKnobAlphaTo:(CGFloat)newKnobAlpha
              duration:(NSTimeInterval)duration {
  if (!_scrollbar)
    return;

  DCHECK_EQ(scrollerImp, ScrollbarPainterForScrollbar(*_scrollbar));

  ScrollbarPainter scrollerPainter = (ScrollbarPainter)scrollerImp;
  [self setUpAlphaAnimation:_knobAlphaAnimation
            scrollerPainter:scrollerPainter
                       part:blink::kThumbPart
             animateAlphaTo:newKnobAlpha
                   duration:duration];
}

- (void)scrollerImp:(id)scrollerImp
    animateTrackAlphaTo:(CGFloat)newTrackAlpha
               duration:(NSTimeInterval)duration {
  if (!_scrollbar)
    return;

  DCHECK_EQ(scrollerImp, ScrollbarPainterForScrollbar(*_scrollbar));

  ScrollbarPainter scrollerPainter = (ScrollbarPainter)scrollerImp;
  [self setUpAlphaAnimation:_trackAlphaAnimation
            scrollerPainter:scrollerPainter
                       part:blink::kBackTrackPart
             animateAlphaTo:newTrackAlpha
                   duration:duration];
}

- (void)scrollerImp:(id)scrollerImp
    animateUIStateTransitionWithDuration:(NSTimeInterval)duration {
  if (!_scrollbar)
    return;

  if (!SupportsUIStateTransitionProgress())
    return;

  DCHECK_EQ(scrollerImp, ScrollbarPainterForScrollbar(*_scrollbar));

  ScrollbarPainter scrollbarPainter = (ScrollbarPainter)scrollerImp;

  // UIStateTransition always animates to 1. In case an animation is in progress
  // this avoids a hard transition.
  [scrollbarPainter
      setUiStateTransitionProgress:1 - [scrollerImp uiStateTransitionProgress]];

  if (!_uiStateTransitionAnimation)
    _uiStateTransitionAnimation.reset([[BlinkScrollbarPartAnimation alloc]
        initWithScrollbar:_scrollbar
         featureToAnimate:UIStateTransition
              animateFrom:[scrollbarPainter uiStateTransitionProgress]
                animateTo:1.0
                 duration:duration
               taskRunner:_taskRunner]);
  else {
    // If we don't need to initialize the animation, just reset the values in
    // case they have changed.
    [_uiStateTransitionAnimation
        setStartValue:[scrollbarPainter uiStateTransitionProgress]];
    [_uiStateTransitionAnimation setEndValue:1.0];
    [_uiStateTransitionAnimation setDuration:duration];
  }
  [_uiStateTransitionAnimation startAnimation];
}

- (void)scrollerImp:(id)scrollerImp
    animateExpansionTransitionWithDuration:(NSTimeInterval)duration {
  if (!_scrollbar)
    return;

  if (!SupportsExpansionTransitionProgress())
    return;

  DCHECK_EQ(scrollerImp, ScrollbarPainterForScrollbar(*_scrollbar));

  ScrollbarPainter scrollbarPainter = (ScrollbarPainter)scrollerImp;

  // ExpansionTransition always animates to 1. In case an animation is in
  // progress this avoids a hard transition.
  [scrollbarPainter
      setExpansionTransitionProgress:1 -
                                     [scrollerImp expansionTransitionProgress]];

  if (!_expansionTransitionAnimation) {
    _expansionTransitionAnimation.reset([[BlinkScrollbarPartAnimation alloc]
        initWithScrollbar:_scrollbar
         featureToAnimate:ExpansionTransition
              animateFrom:[scrollbarPainter expansionTransitionProgress]
                animateTo:1.0
                 duration:duration
               taskRunner:_taskRunner]);
  } else {
    // If we don't need to initialize the animation, just reset the values in
    // case they have changed.
    [_expansionTransitionAnimation
        setStartValue:[scrollbarPainter uiStateTransitionProgress]];
    [_expansionTransitionAnimation setEndValue:1.0];
    [_expansionTransitionAnimation setDuration:duration];
  }
  [_expansionTransitionAnimation startAnimation];
}

- (void)scrollerImp:(id)scrollerImp
    overlayScrollerStateChangedTo:(NSUInteger)newOverlayScrollerState {
  // The names of these states are based on their observed behavior, and are not
  // based on documentation.
  enum {
    NSScrollerStateInvisible = 0,
    NSScrollerStateKnob = 1,
    NSScrollerStateExpanded = 2
  };
  // We do not receive notifications about the thumb un-expanding when the
  // scrollbar fades away. Ensure
  // that we re-paint the thumb the next time that we transition away from being
  // invisible, so that
  // the thumb doesn't stick in an expanded state.
  if (newOverlayScrollerState == NSScrollerStateExpanded) {
    _hasExpandedSinceInvisible = YES;
  } else if (newOverlayScrollerState != NSScrollerStateInvisible &&
             _hasExpandedSinceInvisible) {
    _scrollbar->SetNeedsPaintInvalidation(blink::kThumbPart);
    _hasExpandedSinceInvisible = NO;
  }
}

- (void)invalidate {
  _scrollbar = 0;
  BEGIN_BLOCK_OBJC_EXCEPTIONS;
  [_knobAlphaAnimation invalidate];
  [_trackAlphaAnimation invalidate];
  [_uiStateTransitionAnimation invalidate];
  [_expansionTransitionAnimation invalidate];
  END_BLOCK_OBJC_EXCEPTIONS;
}
@end

namespace blink {

MacScrollbarImpl::MacScrollbarImpl(
    Scrollbar& scrollbar,
    base::scoped_nsobject<ScrollbarPainterController> painter_controller,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ScrollbarThemeMac* mac_theme,
    std::unique_ptr<MacScrollbarImpl> old_scrollbar)

    : is_horizontal_(scrollbar.Orientation() == kHorizontalScrollbar),
      painter_controller_(painter_controller) {
  painter_delegate_.reset([[BlinkScrollbarPainterDelegate alloc]
      initWithScrollbar:&scrollbar
             taskRunner:task_runner]);

  const NSScrollerStyle style = [painter_controller_ scrollerStyle];
  const NSControlSize size =
      (scrollbar.CSSScrollbarWidth() == EScrollbarWidth::kThin)
          ? NSControlSizeSmall
          : NSControlSizeRegular;

  ScrollbarPainter old_painter = nil;
  if (old_scrollbar) {
    old_painter = old_scrollbar->painter();
    if (is_horizontal_)
      DCHECK_EQ(old_painter, [painter_controller_ horizontalScrollerImp]);
    else
      DCHECK_EQ(old_painter, [painter_controller_ verticalScrollerImp]);
  }

  base::scoped_nsobject<ScrollbarPainter> new_painter(
      [NSClassFromString(@"NSScrollerImp") scrollerImpWithStyle:style
                                                    controlSize:size
                                                     horizontal:is_horizontal_
                                           replacingScrollerImp:old_painter],
      base::scoped_policy::RETAIN);
  old_scrollbar.reset();

  if (is_horizontal_)
    [painter_controller_ setHorizontalScrollerImp:new_painter];
  else
    [painter_controller_ setVerticalScrollerImp:new_painter];

  [new_painter setDelegate:painter_delegate_];
  observer_.reset([[BlinkScrollbarObserver alloc]
      initWithScrollbar:&scrollbar
                painter:new_painter]);

  GetScrollbarToAnimatorMap().Set(&scrollbar, this);

  mac_theme->SetNewPainterForScrollbar(scrollbar, painter());
}

MacScrollbarImpl::~MacScrollbarImpl() {
  auto it = GetScrollbarToAnimatorMap().find([observer_ scrollbar]);
  DCHECK(it != GetScrollbarToAnimatorMap().end());
  DCHECK_EQ(it->value, this);

  if (is_horizontal_)
    DCHECK_EQ([painter_controller_ horizontalScrollerImp], painter());
  else
    DCHECK_EQ([painter_controller_ verticalScrollerImp], painter());

  [painter() setDelegate:nil];
  [painter_delegate_ invalidate];
  painter_delegate_.reset();

  if (is_horizontal_)
    [painter_controller_ setHorizontalScrollerImp:nil];
  else
    [painter_controller_ setVerticalScrollerImp:nil];
  observer_.reset();

  GetScrollbarToAnimatorMap().erase(it);
}

// static
MacScrollbarImpl* MacScrollbarImpl::GetForScrollbar(
    const Scrollbar& scrollbar) {
  auto it = GetScrollbarToAnimatorMap().find(&scrollbar);
  if (it != GetScrollbarToAnimatorMap().end())
    return it->value;
  return nullptr;
}

ScrollbarPainter MacScrollbarImpl::painter() {
  return [observer_ painter];
}

MacScrollbarAnimatorImpl::MacScrollbarAnimatorImpl(
    ScrollableArea* scrollable_area)
    : task_runner_(scrollable_area->GetCompositorTaskRunner()),
      scrollable_area_(scrollable_area) {
  scrollbar_painter_controller_delegate_.reset(
      [[BlinkScrollbarPainterControllerDelegate alloc]
          initWithScrollableArea:scrollable_area]);
  scrollbar_painter_controller_.reset(
      [[[NSClassFromString(@"NSScrollerImpPair") alloc] init] autorelease],
      base::scoped_policy::RETAIN);
  [scrollbar_painter_controller_
      performSelector:@selector(setDelegate:)
           withObject:scrollbar_painter_controller_delegate_];
  [scrollbar_painter_controller_
      setScrollerStyle:ScrollbarThemeMac::RecommendedScrollerStyle()];
}

void MacScrollbarAnimatorImpl::Dispose() {
  BEGIN_BLOCK_OBJC_EXCEPTIONS;

  horizontal_scrollbar_.reset();
  vertical_scrollbar_.reset();
  [scrollbar_painter_controller_delegate_ invalidate];
  [scrollbar_painter_controller_ setDelegate:nil];
  END_BLOCK_OBJC_EXCEPTIONS;

  initial_scrollbar_paint_task_handle_.Cancel();
  send_content_area_scrolled_task_handle_.Cancel();
}

void MacScrollbarAnimatorImpl::ContentAreaWillPaint() const {
  if (!scrollable_area_->ScrollbarsCanBeActive())
    return;
  [scrollbar_painter_controller_ contentAreaWillDraw];
}
void MacScrollbarAnimatorImpl::MouseEnteredContentArea() const {
  if (!scrollable_area_->ScrollbarsCanBeActive())
    return;
  [scrollbar_painter_controller_ mouseEnteredContentArea];
}
void MacScrollbarAnimatorImpl::MouseExitedContentArea() const {
  if (!scrollable_area_->ScrollbarsCanBeActive())
    return;
  [scrollbar_painter_controller_ mouseExitedContentArea];
}
void MacScrollbarAnimatorImpl::MouseMovedInContentArea() const {
  if (!scrollable_area_->ScrollbarsCanBeActive())
    return;
  [scrollbar_painter_controller_ mouseMovedInContentArea];
}

void MacScrollbarAnimatorImpl::MouseEnteredScrollbar(
    Scrollbar& scrollbar) const {
  if (!scrollable_area_->ScrollbarsCanBeActive())
    return;

  if (!SupportsUIStateTransitionProgress())
    return;
  if (ScrollbarPainter painter = ScrollbarPainterForScrollbar(scrollbar))
    [painter mouseEnteredScroller];
}

void MacScrollbarAnimatorImpl::MouseExitedScrollbar(
    Scrollbar& scrollbar) const {
  if (!scrollable_area_->ScrollbarsCanBeActive())
    return;

  if (!SupportsUIStateTransitionProgress())
    return;
  if (ScrollbarPainter painter = ScrollbarPainterForScrollbar(scrollbar))
    [painter mouseExitedScroller];
}

void MacScrollbarAnimatorImpl::ContentsResized() const {
  if (!scrollable_area_->ScrollbarsCanBeActive())
    return;
  [scrollbar_painter_controller_ contentAreaDidResize];
}

void MacScrollbarAnimatorImpl::DidAddVerticalScrollbar(Scrollbar& scrollbar) {
  if (!IsScrollbarRegistered(scrollbar))
    return;

  DCHECK(!vertical_scrollbar_);
  vertical_scrollbar_ = std::make_unique<MacScrollbarImpl>(
      scrollbar, scrollbar_painter_controller_, task_runner_,
      MacOverlayScrollbarTheme(scrollable_area_->GetPageScrollbarTheme()),
      nullptr);
}

void MacScrollbarAnimatorImpl::WillRemoveVerticalScrollbar(
    Scrollbar& scrollbar) {
  vertical_scrollbar_.reset();
}

void MacScrollbarAnimatorImpl::DidAddHorizontalScrollbar(Scrollbar& scrollbar) {
  if (!IsScrollbarRegistered(scrollbar))
    return;

  DCHECK(!horizontal_scrollbar_);
  horizontal_scrollbar_ = std::make_unique<MacScrollbarImpl>(
      scrollbar, scrollbar_painter_controller_, task_runner_,
      MacOverlayScrollbarTheme(scrollable_area_->GetPageScrollbarTheme()),
      nullptr);
}

void MacScrollbarAnimatorImpl::WillRemoveHorizontalScrollbar(
    Scrollbar& scrollbar) {
  horizontal_scrollbar_.reset();
}

bool MacScrollbarAnimatorImpl::SetScrollbarsVisibleForTesting(bool show) {
  if (show)
    [scrollbar_painter_controller_ flashScrollers];
  else
    [scrollbar_painter_controller_ hideOverlayScrollers];

  if (vertical_scrollbar_) {
    [vertical_scrollbar_->painter_delegate() updateVisibilityImmediately:show];
  }
  if (horizontal_scrollbar_) {
    [horizontal_scrollbar_->painter_delegate()
        updateVisibilityImmediately:show];
  }
  return true;
}

void MacScrollbarAnimatorImpl::UpdateScrollerStyle() {
  if (!scrollable_area_->ScrollbarsCanBeActive())
    return;

  blink::ScrollbarThemeMac* mac_theme =
      MacOverlayScrollbarTheme(scrollable_area_->GetPageScrollbarTheme());
  if (!mac_theme)
    return;

  if (Scrollbar* vertical_scrollbar = scrollable_area_->VerticalScrollbar()) {
    vertical_scrollbar->SetNeedsPaintInvalidation(kAllParts);

    vertical_scrollbar_ = std::make_unique<MacScrollbarImpl>(
        *vertical_scrollbar, scrollbar_painter_controller_, task_runner_,
        mac_theme, std::move(vertical_scrollbar_));

    // The different scrollbar styles have different thicknesses, so we must
    // re-set the frameRect to the new thickness, and the re-layout below will
    // ensure the offset and length are properly updated.
    int thickness =
        mac_theme->ScrollbarThickness(vertical_scrollbar->ScaleFromDIP(),
                                      vertical_scrollbar->CSSScrollbarWidth());
    vertical_scrollbar->SetFrameRect(IntRect(0, 0, thickness, thickness));
  }

  if (Scrollbar* horizontal_scrollbar =
          scrollable_area_->HorizontalScrollbar()) {
    horizontal_scrollbar->SetNeedsPaintInvalidation(kAllParts);

    horizontal_scrollbar_ = std::make_unique<MacScrollbarImpl>(
        *horizontal_scrollbar, scrollbar_painter_controller_, task_runner_,
        mac_theme, std::move(horizontal_scrollbar_));

    // The different scrollbar styles have different thicknesses, so we must
    // re-set the
    // frameRect to the new thickness, and the re-layout below will ensure the
    // offset
    // and length are properly updated.
    int thickness = mac_theme->ScrollbarThickness(
        horizontal_scrollbar->ScaleFromDIP(),
        horizontal_scrollbar->CSSScrollbarWidth());
    horizontal_scrollbar->SetFrameRect(IntRect(0, 0, thickness, thickness));
  }
}

void MacScrollbarAnimatorImpl::StartScrollbarPaintTimer() {
  initial_scrollbar_paint_task_handle_ = PostDelayedCancellableTask(
      *task_runner_, FROM_HERE,
      WTF::Bind(&MacScrollbarAnimatorImpl::InitialScrollbarPaintTask,
                WrapWeakPersistent(this)),
      base::Milliseconds(100));
}

bool MacScrollbarAnimatorImpl::ScrollbarPaintTimerIsActive() const {
  return initial_scrollbar_paint_task_handle_.IsActive();
}

void MacScrollbarAnimatorImpl::StopScrollbarPaintTimer() {
  initial_scrollbar_paint_task_handle_.Cancel();
}

void MacScrollbarAnimatorImpl::InitialScrollbarPaintTask() {
  // To force the scrollbars to flash, we have to call hide first. Otherwise,
  // the ScrollbarPainterController
  // might think that the scrollbars are already showing and bail early.
  [scrollbar_painter_controller_ hideOverlayScrollers];
  [scrollbar_painter_controller_ flashScrollers];
}

void MacScrollbarAnimatorImpl::DidChangeUserVisibleScrollOffset(
    const ScrollOffset& delta) {
  content_area_scrolled_timer_scroll_delta_ = delta;

  if (send_content_area_scrolled_task_handle_.IsActive())
    return;
  send_content_area_scrolled_task_handle_ = PostCancellableTask(
      *task_runner_, FROM_HERE,
      WTF::Bind(&MacScrollbarAnimatorImpl::SendContentAreaScrolledTask,
                WrapWeakPersistent(this)));
}

void MacScrollbarAnimatorImpl::SendContentAreaScrolledTask() {
  if (SupportsContentAreaScrolledInDirection()) {
    [scrollbar_painter_controller_
        contentAreaScrolledInDirection:
            NSMakePoint(content_area_scrolled_timer_scroll_delta_.width(),
                        content_area_scrolled_timer_scroll_delta_.height())];
    content_area_scrolled_timer_scroll_delta_ = ScrollOffset();
  } else
    [scrollbar_painter_controller_ contentAreaScrolled];
}

MacScrollbarAnimator* MacScrollbarAnimator::Create(
    ScrollableArea* scrollable_area) {
  return MakeGarbageCollected<MacScrollbarAnimatorImpl>(
      const_cast<ScrollableArea*>(scrollable_area));
}
}  // namespace blink
