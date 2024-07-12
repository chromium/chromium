// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_mac.h"

#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <stddef.h>
#include <stdint.h>
#include <tuple>

#include "base/apple/scoped_cftyperef.h"
#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/child_local_surface_id_allocator.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/browser/site_instance_group.h"
#include "content/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_view_mac_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/mock_render_input_router.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/mock_widget_input_handler.h"
#include "content/test/stub_render_widget_host_owner_delegate.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_render_widget_host.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/ocmock_extensions.h"
#include "ui/base/cocoa/secure_password_input.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"
#import "ui/base/test/cocoa_helper.h"
#import "ui/base/test/scoped_fake_nswindow_focus.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/recyclable_compositor_mac.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/test/cocoa_test_event_utils.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/latency/latency_info.h"

using testing::_;

// Helper class with methods used to mock -[NSEvent phase], used by
// |MockScrollWheelEventWithPhase()|.
@interface MockPhaseMethods : NSObject {
}

- (NSEventPhase)phaseNone;
- (NSEventPhase)phaseBegan;
- (NSEventPhase)phaseChanged;
- (NSEventPhase)phaseEnded;
@end

@implementation MockPhaseMethods

- (NSEventPhase)phaseNone {
  return NSEventPhaseNone;
}
- (NSEventPhase)phaseBegan {
  return NSEventPhaseBegan;
}
- (NSEventPhase)phaseChanged {
  return NSEventPhaseChanged;
}
- (NSEventPhase)phaseEnded {
  return NSEventPhaseEnded;
}

@end

@interface MockRenderWidgetHostViewMacDelegate
    : NSObject<RenderWidgetHostViewMacDelegate> {
  BOOL _unhandledWheelEventReceived;
}

@property(nonatomic) BOOL unhandledWheelEventReceived;

@end

@implementation MockRenderWidgetHostViewMacDelegate

@synthesize unhandledWheelEventReceived = _unhandledWheelEventReceived;

- (void)rendererHandledWheelEvent:(const blink::WebMouseWheelEvent&)event
                         consumed:(BOOL)consumed {
  if (!consumed)
    _unhandledWheelEventReceived = true;
}

- (void)rendererHandledGestureScrollEvent:(const blink::WebGestureEvent&)event
                                 consumed:(BOOL)consumed {
  if (!consumed &&
      event.GetType() == blink::WebInputEvent::Type::kGestureScrollUpdate)
    _unhandledWheelEventReceived = true;
}

- (void)touchesBeganWithEvent:(NSEvent*)event {}
- (void)touchesMovedWithEvent:(NSEvent*)event {}
- (void)touchesCancelledWithEvent:(NSEvent*)event {}
- (void)touchesEndedWithEvent:(NSEvent*)event {}
- (void)beginGestureWithEvent:(NSEvent*)event {}
- (void)endGestureWithEvent:(NSEvent*)event {}
- (void)rendererHandledOverscrollEvent:(const ui::DidOverscrollParams&)params {
}

@end

@interface FakeTextCheckingResult : NSObject<NSCopying>
@property(readonly) NSRange range;
@property(readonly) NSString* replacementString;
@end

@implementation FakeTextCheckingResult

@synthesize range = _range;
@synthesize replacementString = _replacementString;

+ (FakeTextCheckingResult*)resultWithRange:(NSRange)range
                         replacementString:(NSString*)replacementString {
  FakeTextCheckingResult* result = [[FakeTextCheckingResult alloc] init];
  result->_range = range;
  result->_replacementString = replacementString;
  return result;
}

- (id)copyWithZone:(NSZone*)zone {
  return [FakeTextCheckingResult resultWithRange:self.range
                               replacementString:self.replacementString];
}

- (NSString*)replacementString {
  return _replacementString;
}
@end

using SpellCheckerCompletionHandlerType = void (
    ^)(NSInteger sequenceNumber, NSArray<NSTextCheckingResult*>* candidates);

@interface FakeSpellChecker : NSObject
@property(readonly) NSInteger lastAssignedSequenceNumber;
@property(readonly) NSDictionary<NSNumber*, SpellCheckerCompletionHandlerType>*
    completionHandlers;
@end

@implementation FakeSpellChecker {
  NSMutableDictionary* __strong _completionHandlers;
}

@synthesize lastAssignedSequenceNumber = _lastAssignedSequenceNumber;
@synthesize completionHandlers = _completionHandlers;

- (instancetype)init {
  if (self = [super init]) {
    _completionHandlers = [NSMutableDictionary dictionary];
  }
  return self;
}

- (NSInteger)
    requestCandidatesForSelectedRange:(NSRange)selectedRange
                             inString:(NSString*)stringToCheck
                                types:(NSTextCheckingTypes)checkingTypes
                              options:(nullable
                                           NSDictionary<NSTextCheckingOptionKey,
                                                        id>*)options
               inSpellDocumentWithTag:(NSInteger)tag
                    completionHandler:
                        (void (^__nullable)(NSInteger sequenceNumber,
                                            NSArray<NSTextCheckingResult*>*
                                                candidates))completionHandler {
  _lastAssignedSequenceNumber += 1;
  _completionHandlers[@(_lastAssignedSequenceNumber)] = completionHandler;
  return _lastAssignedSequenceNumber;
}

@end

namespace content {

namespace {

std::string GetMessageNames(
    const MockWidgetInputHandler::MessageVector& events) {
  std::vector<std::string> result;
  for (auto& event : events)
    result.push_back(event->name());
  return base::JoinString(result, " ");
}

blink::WebPointerProperties::PointerType GetPointerType(
    const MockWidgetInputHandler::MessageVector& events) {
  EXPECT_EQ(events.size(), 1U);
  MockWidgetInputHandler::DispatchedEventMessage* event = events[0]->ToEvent();
  if (!event)
    return blink::WebPointerProperties::PointerType::kUnknown;

  if (blink::WebInputEvent::IsMouseEventType(
          event->Event()->Event().GetType())) {
    return static_cast<const blink::WebMouseEvent&>(event->Event()->Event())
        .pointer_type;
  }

  if (blink::WebInputEvent::IsTouchEventType(
          event->Event()->Event().GetType())) {
    return static_cast<const blink::WebTouchEvent&>(event->Event()->Event())
        .touches[0]
        .pointer_type;
  }
  return blink::WebPointerProperties::PointerType::kUnknown;
}

NSEvent* MockTabletEventWithParams(CGEventType type,
                                   bool is_entering_proximity,
                                   NSPointingDeviceType device_type) {
  base::apple::ScopedCFTypeRef<CGEventRef> cg_event(
      CGEventCreate(/*source=*/nullptr));
  CGEventSetType(cg_event.get(), type);
  CGEventSetIntegerValueField(cg_event.get(),
                              kCGTabletProximityEventEnterProximity,
                              is_entering_proximity);
  CGEventSetIntegerValueField(cg_event.get(),
                              kCGTabletProximityEventPointerType, device_type);
  NSEvent* event = [NSEvent eventWithCGEvent:cg_event.get()];
  return event;
}

NSEvent* MockMouseEventWithParams(CGEventType mouse_type,
                                  CGPoint location,
                                  CGMouseButton button,
                                  CGEventMouseSubtype subtype,
                                  bool is_entering_proximity = false,
                                  bool is_pen_tip = false) {
  // CGEvents have their origin at the *top* left screen corner. Converting to
  // an NSEvent, below, flips the location back to bottom left origin.
  CGPoint cg_location =
      CGPointMake(location.x, NSHeight(NSScreen.screens[0].frame) - location.y);
  base::apple::ScopedCFTypeRef<CGEventRef> cg_event(CGEventCreateMouseEvent(
      /*source=*/nullptr, mouse_type, cg_location, button));
  CGEventSetIntegerValueField(cg_event.get(), kCGMouseEventSubtype, subtype);
  CGEventSetIntegerValueField(cg_event.get(),
                              kCGTabletProximityEventEnterProximity,
                              is_entering_proximity);
  CGEventSetIntegerValueField(cg_event.get(), kCGTabletEventRotation, 300);
  if (is_pen_tip)
    CGEventSetIntegerValueField(cg_event.get(), kCGTabletEventPointButtons, 1);
  CGEventTimestamp timestamp =
      (ui::EventTimeForNow() - base::TimeTicks()).InMicroseconds() *
      base::Time::kNanosecondsPerMicrosecond;
  CGEventSetTimestamp(cg_event.get(), timestamp);
  NSEvent* event = [NSEvent eventWithCGEvent:cg_event.get()];
  return event;
}

id MockPinchEvent(NSEventPhase phase, double magnification) {
  id event = [OCMockObject mockForClass:[NSEvent class]];
  NSEventType type = NSEventTypeMagnify;
  NSPoint locationInWindow = NSMakePoint(0, 0);
  CGFloat deltaX = 0;
  CGFloat deltaY = 0;
  NSTimeInterval timestamp = 1;
  NSUInteger modifierFlags = 0;

  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(type)] type];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(phase)] phase];
  [(NSEvent*)[[event stub]
      andReturnValue:OCMOCK_VALUE(locationInWindow)] locationInWindow];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(deltaX)] deltaX];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(deltaY)] deltaY];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(timestamp)] timestamp];
  [(NSEvent*)[[event stub]
      andReturnValue:OCMOCK_VALUE(modifierFlags)] modifierFlags];
  [(NSEvent*)[[event stub]
      andReturnValue:OCMOCK_VALUE(magnification)] magnification];
  return event;
}

id MockSmartMagnifyEvent() {
  id event = [OCMockObject mockForClass:[NSEvent class]];
  NSEventType type = NSEventTypeSmartMagnify;
  NSPoint locationInWindow = NSMakePoint(0, 0);
  CGFloat deltaX = 0;
  CGFloat deltaY = 0;
  NSTimeInterval timestamp = 1;
  NSUInteger modifierFlags = 0;

  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(type)] type];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(locationInWindow)]
      locationInWindow];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(deltaX)] deltaX];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(deltaY)] deltaY];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(timestamp)] timestamp];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(modifierFlags)]
      modifierFlags];
  return event;
}

// Generates the |length| of composition rectangle vector and save them to
// |output|. It starts from |origin| and each rectangle contains |unit_size|.
void GenerateCompositionRectArray(const gfx::Point& origin,
                                  const gfx::Size& unit_size,
                                  size_t length,
                                  const std::vector<size_t>& break_points,
                                  std::vector<gfx::Rect>* output) {
  DCHECK(output);
  output->clear();

  base::queue<int> break_point_queue;
  for (unsigned long break_point : break_points) {
    break_point_queue.push(break_point);
  }
  break_point_queue.push(length);
  size_t next_break_point = break_point_queue.front();
  break_point_queue.pop();

  gfx::Rect current_rect(origin, unit_size);
  for (size_t i = 0; i < length; ++i) {
    if (i == next_break_point) {
      current_rect.set_x(origin.x());
      current_rect.set_y(current_rect.y() + current_rect.height());
      next_break_point = break_point_queue.front();
      break_point_queue.pop();
    }
    output->push_back(current_rect);
    current_rect.set_x(current_rect.right());
  }
}

gfx::Rect GetExpectedRect(const gfx::Point& origin,
                          const gfx::Size& size,
                          const gfx::Range& range,
                          int line_no) {
  return gfx::Rect(
      origin.x() + range.start() * size.width(),
      origin.y() + line_no * size.height(),
      range.length() * size.width(),
      size.height());
}

// Returns NSEventTypeScrollWheel event that mocks -phase. |mockPhaseSelector|
// should correspond to a method in |MockPhaseMethods| that returns the desired
// phase.
NSEvent* MockScrollWheelEventWithPhase(SEL mockPhaseSelector, int32_t delta) {
  base::apple::ScopedCFTypeRef<CGEventRef> cg_event(
      CGEventCreateScrollWheelEvent(
          /*source=*/nullptr, kCGScrollEventUnitLine, 1, delta, 0));
  CGEventTimestamp timestamp = 0;
  CGEventSetTimestamp(cg_event.get(), timestamp);
  NSEvent* event = [NSEvent eventWithCGEvent:cg_event.get()];
  method_setImplementation(
      class_getInstanceMethod([NSEvent class], @selector(phase)),
      [MockPhaseMethods instanceMethodForSelector:mockPhaseSelector]);
  return event;
}

NSEvent* MockScrollWheelEventWithMomentumPhase(SEL mockPhaseSelector,
                                               int32_t delta) {
  // Create a fake event with phaseNone. This is for resetting the phase info
  // of CGEventRef.
  MockScrollWheelEventWithPhase(@selector(phaseNone), 0);
  base::apple::ScopedCFTypeRef<CGEventRef> cg_event(
      CGEventCreateScrollWheelEvent(
          /*source=*/nullptr, kCGScrollEventUnitLine, 1, delta, 0));
  CGEventTimestamp timestamp = 0;
  CGEventSetTimestamp(cg_event.get(), timestamp);
  NSEvent* event = [NSEvent eventWithCGEvent:cg_event.get()];
  method_setImplementation(
      class_getInstanceMethod([NSEvent class], @selector(momentumPhase)),
      [MockPhaseMethods instanceMethodForSelector:mockPhaseSelector]);
  return event;
}

NSEvent* MockScrollWheelEventWithoutPhase(int32_t delta) {
  return MockScrollWheelEventWithMomentumPhase(@selector(phaseNone), delta);
}

class MockRenderWidgetHostOwnerDelegate
    : public StubRenderWidgetHostOwnerDelegate {
 public:
  MOCK_METHOD1(SetBackgroundOpaque, void(bool opaque));
  MOCK_METHOD(void, RenderWidgetGotFocus, (), (override));
  MOCK_METHOD(void, RenderWidgetLostFocus, (), (override));
};

}  // namespace

class MockRenderWidgetHostImpl : public RenderWidgetHostImpl {
 public:
  using RenderWidgetHostImpl::render_input_router_;

  MockRenderWidgetHostImpl(RenderWidgetHostDelegate* delegate,
                           base::SafeRef<SiteInstanceGroup> site_instance_group,
                           int32_t routing_id,
                           bool for_frame_widget)
      : RenderWidgetHostImpl(
            /*frame_tree=*/nullptr,
            /*self_owned=*/false,
            DefaultFrameSinkId(*site_instance_group, routing_id),
            delegate,
            std::move(site_instance_group),
            routing_id,
            /*hidden=*/false,
            /*renderer_initiated_creation=*/false,
            std::make_unique<FrameTokenMessageQueue>()) {
    SetupMockRenderInputRouter();

    mojo::AssociatedRemote<blink::mojom::WidgetHost> widget_host;
    BindWidgetInterfaces(widget_host.BindNewEndpointAndPassDedicatedReceiver(),
                         TestRenderWidgetHost::CreateStubWidgetRemote());
    if (for_frame_widget) {
      mojo::AssociatedRemote<blink::mojom::FrameWidgetHost> frame_widget_host;
      BindFrameWidgetInterfaces(
          frame_widget_host.BindNewEndpointAndPassDedicatedReceiver(),
          TestRenderWidgetHost::CreateStubFrameWidgetRemote());
    }
    RendererWidgetCreated(for_frame_widget);

    ON_CALL(*this, Focus())
        .WillByDefault(
            testing::Invoke(this, &MockRenderWidgetHostImpl::FocusImpl));
    ON_CALL(*this, Blur())
        .WillByDefault(
            testing::Invoke(this, &MockRenderWidgetHostImpl::BlurImpl));
  }

  MockRenderWidgetHostImpl(const MockRenderWidgetHostImpl&) = delete;
  MockRenderWidgetHostImpl& operator=(const MockRenderWidgetHostImpl&) = delete;

  ~MockRenderWidgetHostImpl() override = default;

  // Extracts |latency_info| and stores it in |last_wheel_event_latency_info_|.
  void ForwardWheelEventWithLatencyInfo(
      const blink::WebMouseWheelEvent& wheel_event,
      const ui::LatencyInfo& ui_latency) override {
    RenderWidgetHostImpl::ForwardWheelEventWithLatencyInfo(wheel_event,
                                                           ui_latency);
    last_wheel_event_latency_info_ = ui::LatencyInfo(ui_latency);
  }

  MOCK_METHOD0(Focus, void());
  MOCK_METHOD0(Blur, void());

  MockRenderInputRouter* mock_render_input_router() {
    return static_cast<MockRenderInputRouter*>(render_input_router_.get());
  }

  MockWidgetInputHandler* input_handler() {
    return mock_render_input_router()->mock_widget_input_handler_.get();
  }

  MockWidgetInputHandler::MessageVector GetAndResetDispatchedMessages() {
    return input_handler()->GetAndResetDispatchedMessages();
  }

  input::RenderInputRouter* GetRenderInputRouter() override {
    return render_input_router_.get();
  }

  const ui::LatencyInfo& LastWheelEventLatencyInfo() const {
    return last_wheel_event_latency_info_;
  }

 private:
  void FocusImpl() { RenderWidgetHostImpl::Focus(); }
  void BlurImpl() { RenderWidgetHostImpl::Blur(); }

  void SetupMockRenderInputRouter() {
    render_input_router_ = std::make_unique<MockRenderInputRouter>(
        this, MakeFlingScheduler(), this,
        base::SingleThreadTaskRunner::GetCurrentDefault());
    SetupInputRouter();
  }

  ui::LatencyInfo last_wheel_event_latency_info_;
};

class RenderWidgetHostViewMacTest : public RenderViewHostImplTestHarness {
 public:
  RenderWidgetHostViewMacTest() : rwhv_mac_(nullptr) {
  }

  RenderWidgetHostViewMacTest(const RenderWidgetHostViewMacTest&) = delete;
  RenderWidgetHostViewMacTest& operator=(const RenderWidgetHostViewMacTest&) =
      delete;

  void SetUp() override {
    mock_clock_.Advance(base::Milliseconds(100));
    ui::SetEventTickClockForTesting(&mock_clock_);
    RenderViewHostImplTestHarness::SetUp();

    browser_context_ = std::make_unique<TestBrowserContext>();
    process_host_ =
        std::make_unique<MockRenderProcessHost>(browser_context_.get());
    process_host_->Init();
    site_instance_group_ =
        base::WrapRefCounted(SiteInstanceGroup::CreateForTesting(
            browser_context_.get(), process_host_.get()));
    host_ = std::make_unique<MockRenderWidgetHostImpl>(
        &delegate_, site_instance_group_->GetSafeRef(),
        process_host_->GetNextRoutingID(),
        /*for_frame_widget=*/true);
    host_->set_owner_delegate(&mock_owner_delegate_);
    rwhv_mac_ = new RenderWidgetHostViewMac(host_.get());
    rwhv_cocoa_ = rwhv_mac_->GetInProcessNSView();

    window_ = [[CocoaTestHelperWindow alloc] init];
    window_.releasedWhenClosed = NO;
    window_.pretendIsKeyWindow = YES;
    [window_.contentView addSubview:rwhv_cocoa_];
    [rwhv_cocoa_ setFrame:window_.contentView.bounds];
    rwhv_mac_->Show();

    base::RunLoop().RunUntilIdle();
    process_host_->sink().ClearMessages();
  }

  void TearDown() override {
    ui::SetEventTickClockForTesting(nullptr);
    rwhv_cocoa_ = nil;
    // RenderWidgetHostImpls with an owner delegate are not expected to be self-
    // deleting.
    host_->ShutdownAndDestroyWidget(/*also_delete=*/false);
    host_.reset();
    process_host_->Cleanup();
    site_instance_group_.reset();
    process_host_.reset();
    browser_context_.reset();
    RecycleAndWait();
    RenderViewHostImplTestHarness::TearDown();
  }

  void RecycleAndWait() {
    pool_.Recycle();
    base::RunLoop().RunUntilIdle();
    pool_.Recycle();
  }

  void ActivateViewWithTextInputManager(RenderWidgetHostViewBase* view,
                                        ui::TextInputType type) {
    ui::mojom::TextInputState state;
    state.type = type;
    view->TextInputStateChanged(state);
  }

 protected:
  std::string selected_text() const {
    return base::UTF16ToUTF8(rwhv_mac_->GetTextSelection()->selected_text());
  }

  MockRenderWidgetHostDelegate delegate_;

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<MockRenderProcessHost> process_host_;
  scoped_refptr<SiteInstanceGroup> site_instance_group_;
  testing::NiceMock<MockRenderWidgetHostOwnerDelegate> mock_owner_delegate_;
  std::unique_ptr<MockRenderWidgetHostImpl> host_;
  raw_ptr<RenderWidgetHostViewMac, DanglingUntriaged> rwhv_mac_ = nullptr;
  RenderWidgetHostViewCocoa* __strong rwhv_cocoa_;
  CocoaTestHelperWindow* __strong window_;

 private:
  // This class isn't derived from PlatformTest.
  STACK_ALLOCATED_IGNORE("https://crbug.com/1424190")
  base::apple::ScopedNSAutoreleasePool pool_;

  base::SimpleTestTickClock mock_clock_;
};

TEST_F(RenderWidgetHostViewMacTest, Basic) {
}

TEST_F(RenderWidgetHostViewMacTest, AcceptsFirstResponder) {
  // The RWHVCocoa should normally accept first responder status.
  EXPECT_TRUE(rwhv_cocoa_.acceptsFirstResponder);
}

// This test verifies that RenderWidgetHostViewCocoa's implementation of
// NSTextInputClientConformance conforms to requirements.
TEST_F(RenderWidgetHostViewMacTest, NSTextInputClientConformance) {
  EXPECT_NSEQ(NSMakeRange(0, 0), rwhv_cocoa_.selectedRange);

  rwhv_mac_->SelectionChanged(u"llo, world!", 2, gfx::Range(5, 10));
  EXPECT_NSEQ(NSMakeRange(5, 5), rwhv_cocoa_.selectedRange);

  NSRange actualRange = NSMakeRange(1u, 2u);
  NSAttributedString* actualString = [rwhv_cocoa_
      attributedSubstringForProposedRange:NSMakeRange(NSNotFound, 0u)
                              actualRange:&actualRange];
  EXPECT_EQ(nil, actualString);
  EXPECT_EQ(static_cast<NSUInteger>(NSNotFound), actualRange.location);
  EXPECT_EQ(0u, actualRange.length);

  actualString = [rwhv_cocoa_
      attributedSubstringForProposedRange:NSMakeRange(NSNotFound, 15u)
                              actualRange:&actualRange];
  EXPECT_EQ(nil, actualString);
  EXPECT_EQ(static_cast<NSUInteger>(NSNotFound), actualRange.location);
  EXPECT_EQ(0u, actualRange.length);
}

// Test that NSEvent of private use character won't generate keypress event
// http://crbug.com/459089
TEST_F(RenderWidgetHostViewMacTest, FilterNonPrintableCharacter) {
  // Simulate ctrl+F12, will produce a private use character but shouldn't
  // fire keypress event
  MockWidgetInputHandler::MessageVector events =
      host_->GetAndResetDispatchedMessages();

  EXPECT_EQ(0U, events.size());
  [rwhv_mac_->GetInProcessNSView()
      keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                   0x7B, 0xF70F, NSEventTypeKeyDown,
                   NSEventModifierFlagControl)];
  base::RunLoop().RunUntilIdle();
  events = host_->GetAndResetDispatchedMessages();

  EXPECT_EQ("RawKeyDown", GetMessageNames(events));

  // Simulate ctrl+delete, will produce a private use character but shouldn't
  // fire keypress event
  process_host_->sink().ClearMessages();
  EXPECT_EQ(0U, process_host_->sink().message_count());
  [rwhv_mac_->GetInProcessNSView()
      keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                   0x2E, 0xF728, NSEventTypeKeyDown,
                   NSEventModifierFlagControl)];
  base::RunLoop().RunUntilIdle();
  events = host_->GetAndResetDispatchedMessages();
  EXPECT_EQ("RawKeyDown", GetMessageNames(events));

  // Simulate a printable char, should generate keypress event
  [rwhv_mac_->GetInProcessNSView()
      keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                   0x58, 'x', NSEventTypeKeyDown, NSEventModifierFlagControl)];
  base::RunLoop().RunUntilIdle();
  events = host_->GetAndResetDispatchedMessages();
  EXPECT_EQ("RawKeyDown Char", GetMessageNames(events));
}

// Test that invalid |keyCode| shouldn't generate key events.
// https://crbug.com/601964
TEST_F(RenderWidgetHostViewMacTest, InvalidKeyCode) {
  // Simulate "Convert" key on JIS PC keyboard, will generate a
  // |NSEventTypeFlagsChanged| NSEvent with |keyCode| == 0xFF.
  [rwhv_mac_->GetInProcessNSView()
      keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                   0xFF, 0, NSEventTypeFlagsChanged, 0)];
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0U, host_->GetAndResetDispatchedMessages().size());
}

TEST_F(RenderWidgetHostViewMacTest, GetFirstRectForCharacterRangeCaretCase) {
  const std::u16string kDummyString = u"hogehoge";
  const size_t kDummyOffset = 0;

  gfx::Rect caret_rect(10, 11, 0, 10);
  gfx::Range caret_range(0, 0);

  gfx::Rect rect;
  gfx::Range actual_range;
  rwhv_mac_->SelectionChanged(kDummyString, kDummyOffset, caret_range);
  rwhv_mac_->SelectionBoundsChanged(caret_rect, base::i18n::LEFT_TO_RIGHT,
                                    caret_rect, base::i18n::LEFT_TO_RIGHT,
                                    gfx::Rect(), false);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(caret_range, &rect,
                                                             &actual_range));
  EXPECT_EQ(caret_rect, rect);
  EXPECT_EQ(caret_range, gfx::Range(actual_range));

  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(0, 1), &rect, &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 1), &rect, &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(2, 3), &rect, &actual_range));

  // Caret moved.
  caret_rect = gfx::Rect(20, 11, 0, 10);
  caret_range = gfx::Range(1, 1);
  rwhv_mac_->SelectionChanged(kDummyString, kDummyOffset, caret_range);
  rwhv_mac_->SelectionBoundsChanged(caret_rect, base::i18n::LEFT_TO_RIGHT,
                                    caret_rect, base::i18n::LEFT_TO_RIGHT,
                                    gfx::Rect(), false);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(caret_range, &rect,
                                                             &actual_range));
  EXPECT_EQ(caret_rect, rect);
  EXPECT_EQ(caret_range, actual_range);

  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(0, 0), &rect, &actual_range));
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 2), &rect, &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(2, 3), &rect, &actual_range));

  // No caret.
  caret_range = gfx::Range(1, 2);
  rwhv_mac_->SelectionChanged(kDummyString, kDummyOffset, caret_range);
  rwhv_mac_->SelectionBoundsChanged(
      caret_rect, base::i18n::LEFT_TO_RIGHT, gfx::Rect(30, 11, 0, 10),
      base::i18n::LEFT_TO_RIGHT, gfx::Rect(), false);
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(0, 0), &rect, &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(0, 1), &rect, &actual_range));
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 1), &rect, &actual_range));
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 2), &rect, &actual_range));
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(2, 2), &rect, &actual_range));
}

TEST_F(RenderWidgetHostViewMacTest, UpdateCompositionSinglelineCase) {
  ActivateViewWithTextInputManager(rwhv_mac_, ui::TEXT_INPUT_TYPE_TEXT);
  const gfx::Point kOrigin(10, 11);
  const gfx::Size kBoundsUnit(10, 20);

  gfx::Rect rect;
  // Make sure not crashing by passing nullptr pointer instead of
  // |actual_range|.
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(gfx::Range(0, 0),
                                                              &rect, nullptr));

  // If there are no update from renderer, always returned caret position.
  gfx::Range actual_range;
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(0, 0), &rect, &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(0, 1), &rect, &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 0), &rect, &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 1), &rect, &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 2), &rect, &actual_range));

  // If the firstRectForCharacterRange is failed in renderer, empty rect vector
  // is sent. Make sure this does not crash.
  rwhv_mac_->ImeCompositionRangeChanged(gfx::Range(10, 12),
                                        std::vector<gfx::Rect>(), std::nullopt);
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(10, 11), &rect, nullptr));

  const int kCompositionLength = 10;
  std::vector<gfx::Rect> composition_bounds;
  const int kCompositionStart = 3;
  const gfx::Range kCompositionRange(kCompositionStart,
                                    kCompositionStart + kCompositionLength);
  GenerateCompositionRectArray(kOrigin,
                               kBoundsUnit,
                               kCompositionLength,
                               std::vector<size_t>(),
                               &composition_bounds);
  rwhv_mac_->ImeCompositionRangeChanged(kCompositionRange, composition_bounds,
                                        std::nullopt);

  // Out of range requests will return caret position.
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(0, 0), &rect, &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 1), &rect, &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 2), &rect, &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(2, 2), &rect, &actual_range));
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(13, 14), &rect, &actual_range));
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(14, 15), &rect, &actual_range));

  for (int i = 0; i <= kCompositionLength; ++i) {
    for (int j = 0; j <= kCompositionLength - i; ++j) {
      const gfx::Range range(i, i + j);
      const gfx::Rect expected_rect = GetExpectedRect(kOrigin,
                                                      kBoundsUnit,
                                                      range,
                                                      0);
      const gfx::Range request_range = gfx::Range(
          kCompositionStart + range.start(), kCompositionStart + range.end());
      EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
            request_range,
            &rect,
            &actual_range));
      EXPECT_EQ(request_range, actual_range);
      EXPECT_EQ(expected_rect, rect);

      // Make sure not crashing by passing nullptr pointer instead of
      // |actual_range|.
      EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
          request_range, &rect, nullptr));
    }
  }
}

TEST_F(RenderWidgetHostViewMacTest, UpdateCompositionMultilineCase) {
  ActivateViewWithTextInputManager(rwhv_mac_, ui::TEXT_INPUT_TYPE_TEXT);
  const gfx::Point kOrigin(10, 11);
  const gfx::Size kBoundsUnit(10, 20);
  gfx::Rect rect;

  const int kCompositionLength = 30;
  std::vector<gfx::Rect> composition_bounds;
  const gfx::Range kCompositionRange(0, kCompositionLength);
  // Set breaking point at 10 and 20.
  std::vector<size_t> break_points;
  break_points.push_back(10);
  break_points.push_back(20);
  GenerateCompositionRectArray(kOrigin,
                               kBoundsUnit,
                               kCompositionLength,
                               break_points,
                               &composition_bounds);
  rwhv_mac_->ImeCompositionRangeChanged(kCompositionRange, composition_bounds,
                                        std::nullopt);

  // Range doesn't contain line breaking point.
  gfx::Range range;
  range = gfx::Range(5, 8);
  gfx::Range actual_range;
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range, &rect,
                                                             &actual_range));
  EXPECT_EQ(range, actual_range);
  EXPECT_EQ(GetExpectedRect(kOrigin, kBoundsUnit, range, 0), rect);
  range = gfx::Range(15, 18);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range, &rect,
                                                             &actual_range));
  EXPECT_EQ(range, actual_range);
  EXPECT_EQ(GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(5, 8), 1), rect);
  range = gfx::Range(25, 28);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range, &rect,
                                                             &actual_range));
  EXPECT_EQ(range, actual_range);
  EXPECT_EQ(GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(5, 8), 2), rect);

  // Range contains line breaking point.
  range = gfx::Range(8, 12);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range, &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(8, 10), actual_range);
  EXPECT_EQ(GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(8, 10), 0), rect);
  range = gfx::Range(18, 22);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range, &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(18, 20), actual_range);
  EXPECT_EQ(GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(8, 10), 1), rect);

  // Start point is line breaking point.
  range = gfx::Range(10, 12);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range, &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(10, 12), actual_range);
  EXPECT_EQ(GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(0, 2), 1), rect);
  range = gfx::Range(20, 22);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range, &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(20, 22), actual_range);
  EXPECT_EQ(GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(0, 2), 2), rect);

  // End point is line breaking point.
  range = gfx::Range(5, 10);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range, &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(5, 10), actual_range);
  EXPECT_EQ(GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(5, 10), 0), rect);
  range = gfx::Range(15, 20);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range, &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(15, 20), actual_range);
  EXPECT_EQ(GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(5, 10), 1), rect);

  // Start and end point are same line breaking point.
  range = gfx::Range(10, 10);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range, &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(10, 10), actual_range);
  EXPECT_EQ(GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(0, 0), 1), rect);
  range = gfx::Range(20, 20);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range, &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(20, 20), actual_range);
  EXPECT_EQ(GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(0, 0), 2), rect);

  // Start and end point are different line breaking point.
  range = gfx::Range(10, 20);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range, &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(10, 20), actual_range);
  EXPECT_EQ(GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(0, 10), 1), rect);
}

// Check that events coming from AppKit via -[NSTextInputClient
// firstRectForCharacterRange:actualRange] are handled in a sane manner if they
// arrive after the C++ RenderWidgetHostView is destroyed.
TEST_F(RenderWidgetHostViewMacTest, CompositionEventAfterDestroy) {
  ActivateViewWithTextInputManager(rwhv_mac_, ui::TEXT_INPUT_TYPE_TEXT);
  const gfx::Rect composition_bounds(0, 0, 30, 40);
  const gfx::Range range(0, 1);
  rwhv_mac_->ImeCompositionRangeChanged(
      range, std::vector<gfx::Rect>(1, composition_bounds), std::nullopt);

  NSRange actual_range = NSMakeRange(0, 0);

  NSRect rect = [rwhv_cocoa_ firstRectForCharacterRange:range.ToNSRange()
                                            actualRange:&actual_range];
  EXPECT_EQ(30, rect.size.width);
  EXPECT_EQ(40, rect.size.height);
  EXPECT_EQ(range, gfx::Range(actual_range));

  rwhv_mac_->Destroy();
  actual_range = NSMakeRange(0, 0);
  rect = [rwhv_cocoa_ firstRectForCharacterRange:range.ToNSRange()
                                     actualRange:&actual_range];
  EXPECT_NSEQ(NSZeroRect, rect);
  EXPECT_EQ(gfx::Range(), gfx::Range(actual_range));
}

// Verify that |SetActive()| calls |RenderWidgetHostImpl::LostFocus()| and
// |RenderWidgetHostImp::GotFocus()|.
TEST_F(RenderWidgetHostViewMacTest, LostFocusAndGotFocusOnSetActive) {
  EXPECT_CALL(*host_, Focus());
  EXPECT_CALL(mock_owner_delegate_, RenderWidgetGotFocus());
  [window_ makeFirstResponder:rwhv_mac_->GetInProcessNSView()];
  testing::Mock::VerifyAndClearExpectations(host_.get());

  EXPECT_CALL(*host_, Blur());
  EXPECT_CALL(mock_owner_delegate_, RenderWidgetLostFocus());
  rwhv_mac_->SetActive(false);
  testing::Mock::VerifyAndClearExpectations(host_.get());

  EXPECT_CALL(*host_, Focus());
  EXPECT_CALL(mock_owner_delegate_, RenderWidgetGotFocus());
  rwhv_mac_->SetActive(true);
  testing::Mock::VerifyAndClearExpectations(host_.get());

  // Unsetting first responder should blur.
  EXPECT_CALL(*host_, Blur());
  EXPECT_CALL(mock_owner_delegate_, RenderWidgetLostFocus());
  [window_ makeFirstResponder:nil];
  testing::Mock::VerifyAndClearExpectations(host_.get());

  // |SetActive()| should not focus if view is not first responder.
  EXPECT_CALL(*host_, Focus()).Times(0);
  EXPECT_CALL(mock_owner_delegate_, RenderWidgetGotFocus()).Times(0);
  rwhv_mac_->SetActive(true);
  testing::Mock::VerifyAndClearExpectations(host_.get());
}

TEST_F(RenderWidgetHostViewMacTest, LastWheelEventLatencyInfoExists) {
  process_host_->sink().ClearMessages();

  // Send an initial wheel event for scrolling by 3 lines.
  // Verifies that ui::INPUT_EVENT_LATENCY_UI_COMPONENT is added
  // properly in scrollWheel function.
  NSEvent* wheelEvent1 = MockScrollWheelEventWithPhase(@selector(phaseBegan),3);
  [rwhv_mac_->GetInProcessNSView() scrollWheel:wheelEvent1];
  ASSERT_TRUE(host_->LastWheelEventLatencyInfo().FindLatency(
      ui::INPUT_EVENT_LATENCY_UI_COMPONENT, nullptr));

  MockWidgetInputHandler::MessageVector events =
      host_->GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  // Send a wheel event with phaseEnded.
  // Verifies that ui::INPUT_EVENT_LATENCY_UI_COMPONENT is added
  // properly in shortCircuitScrollWheelEvent function which is called
  // in scrollWheel.
  NSEvent* wheelEvent2 = MockScrollWheelEventWithPhase(@selector(phaseEnded),0);
  [rwhv_mac_->GetInProcessNSView() scrollWheel:wheelEvent2];
  ASSERT_TRUE(host_->LastWheelEventLatencyInfo().FindLatency(
      ui::INPUT_EVENT_LATENCY_UI_COMPONENT, nullptr));

  events = host_->GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollBegin GestureScrollUpdate", GetMessageNames(events));
}

TEST_F(RenderWidgetHostViewMacTest, ScrollWheelEndEventDelivery) {
  // Send an initial wheel event with NSEventPhaseBegan to the view.
  NSEvent* event1 = MockScrollWheelEventWithPhase(@selector(phaseBegan), 0);
  [rwhv_mac_->GetInProcessNSView() scrollWheel:event1];

  // Flush and clear other messages (e.g. begin frames) the RWHVMac also sends.
  base::RunLoop().RunUntilIdle();

  MockWidgetInputHandler::MessageVector events =
      host_->GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  // Send an ACK for the first wheel event, so that the queue will be flushed.
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Post the NSEventPhaseEnded wheel event to NSApp and check whether the
  // render view receives it.
  NSEvent* event2 = MockScrollWheelEventWithPhase(@selector(phaseEnded), 0);
  [NSApp postEvent:event2 atStart:NO];
  base::RunLoop().RunUntilIdle();
  events = host_->GetAndResetDispatchedMessages();
  // The wheel event with phaseEnded won't be sent to the render view
  // immediately, instead the mouse_wheel_phase_handler will wait for 100ms
  // to see if a wheel event with momentumPhase began arrives or not.
  ASSERT_EQ(0U, events.size());
}

TEST_F(RenderWidgetHostViewMacTest, PointerEventWithEraserType) {
  // Send a NSEvent of NSEventTypeTabletProximity type which has a device type
  // of eraser.
  NSEvent* event = MockTabletEventWithParams(kCGEventTabletProximity, true,
                                             NSPointingDeviceTypeEraser);
  [rwhv_mac_->GetInProcessNSView() tabletEvent:event];
  // Flush and clear other messages (e.g. begin frames) the RWHVMac also sends.
  base::RunLoop().RunUntilIdle();

  event =
      MockMouseEventWithParams(kCGEventMouseMoved, {6, 9}, kCGMouseButtonLeft,
                               kCGEventMouseSubtypeTabletPoint);
  [rwhv_mac_->GetInProcessNSView() mouseEvent:event];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host_->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseMove", GetMessageNames(events));
  EXPECT_EQ(blink::WebPointerProperties::PointerType::kEraser,
            GetPointerType(events));
}

TEST_F(RenderWidgetHostViewMacTest, PointerEventWithPenType) {
  // Send a NSEvent of NSEventTypeTabletProximity type which has a device type
  // of pen.
  NSEvent* event = MockTabletEventWithParams(kCGEventTabletProximity, true,
                                             NSPointingDeviceTypePen);
  [rwhv_mac_->GetInProcessNSView() tabletEvent:event];
  // Flush and clear other messages (e.g. begin frames) the RWHVMac also sends.
  base::RunLoop().RunUntilIdle();

  event =
      MockMouseEventWithParams(kCGEventMouseMoved, {6, 9}, kCGMouseButtonLeft,
                               kCGEventMouseSubtypeTabletPoint);
  [rwhv_mac_->GetInProcessNSView() mouseEvent:event];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host_->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseMove", GetMessageNames(events));
  EXPECT_EQ(blink::WebPointerProperties::PointerType::kPen,
            GetPointerType(events));
}

TEST_F(RenderWidgetHostViewMacTest, PointerEventWithPenTypeNoTabletEvent) {
  // Send a NSEvent of a mouse type with a subtype of
  // NSEventSubtypeTabletProximity, which indicates the input device is a pen.
  NSEvent* event =
      MockMouseEventWithParams(kCGEventMouseMoved, {6, 9}, kCGMouseButtonLeft,
                               kCGEventMouseSubtypeTabletProximity, true);
  [rwhv_mac_->GetInProcessNSView() mouseEvent:event];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host_->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseMove", GetMessageNames(events));
  EXPECT_EQ(blink::WebPointerProperties::PointerType::kPen,
            GetPointerType(events));
  events.clear();

  event = cocoa_test_event_utils::EnterEvent({1, 1}, window_);
  [rwhv_mac_->GetInProcessNSView() mouseEntered:event];
  base::RunLoop().RunUntilIdle();
  events = host_->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseMove", GetMessageNames(events));
  EXPECT_EQ(blink::WebPointerProperties::PointerType::kPen,
            static_cast<const blink::WebMouseEvent&>(
                events[0]->ToEvent()->Event()->Event())
                .pointer_type);
}

TEST_F(RenderWidgetHostViewMacTest, PointerEventWithMouseType) {
  // Send a NSEvent of a mouse type.
  NSEvent* event =
      MockMouseEventWithParams(kCGEventMouseMoved, {6, 9}, kCGMouseButtonLeft,
                               kCGEventMouseSubtypeDefault);
  [rwhv_mac_->GetInProcessNSView() mouseEvent:event];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host_->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseMove", GetMessageNames(events));
  EXPECT_EQ(blink::WebPointerProperties::PointerType::kMouse,
            GetPointerType(events));
}

TEST_F(RenderWidgetHostViewMacTest, SendMouseMoveOnShowingContextMenu) {
  rwhv_mac_->SetShowingContextMenu(true);
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host_->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseMove", GetMessageNames(events));

  events.clear();

  rwhv_mac_->SetShowingContextMenu(false);
  base::RunLoop().RunUntilIdle();
  events = host_->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseMove", GetMessageNames(events));
}

TEST_F(RenderWidgetHostViewMacTest,
       IgnoreEmptyUnhandledWheelEventWithWheelGestures) {
  // Add a delegate to the view.
  MockRenderWidgetHostViewMacDelegate* view_delegate =
      [[MockRenderWidgetHostViewMacDelegate alloc] init];
  rwhv_mac_->SetDelegate(view_delegate);

  // Send an initial wheel event for scrolling by 3 lines.
  NSEvent* event1 = MockScrollWheelEventWithPhase(@selector(phaseBegan), 3);
  [rwhv_mac_->GetInProcessNSView() scrollWheel:event1];
  base::RunLoop().RunUntilIdle();

  MockWidgetInputHandler::MessageVector events =
      host_->GetAndResetDispatchedMessages();

  ASSERT_EQ("MouseWheel", GetMessageNames(events));

  // Indicate that the wheel event was unhandled.
  events.clear();

  base::RunLoop().RunUntilIdle();
  events = host_->GetAndResetDispatchedMessages();

  // GestureEventQueue allows multiple in-flight events.
  ASSERT_EQ("GestureScrollBegin GestureScrollUpdate", GetMessageNames(events));
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  events.clear();

  // Check that the view delegate got an unhandled wheel event.
  ASSERT_EQ(YES, view_delegate.unhandledWheelEventReceived);
  view_delegate.unhandledWheelEventReceived = NO;

  // Send another wheel event, this time for scrolling by 0 lines (empty event).
  NSEvent* event2 = MockScrollWheelEventWithPhase(@selector(phaseChanged), 0);
  [rwhv_mac_->GetInProcessNSView() scrollWheel:event2];
  base::RunLoop().RunUntilIdle();
  events = host_->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseWheel", GetMessageNames(events));

  events.clear();

  // Check that the view delegate ignored the empty unhandled wheel event.
  ASSERT_EQ(NO, view_delegate.unhandledWheelEventReceived);

  // Delete the view while |view_delegate| is still in scope.
  rwhv_cocoa_ = nil;
}

// Tests setting background transparency. See also (disabled on Mac)
// RenderWidgetHostTest.Background. This test has some additional checks for
// Mac.
TEST_F(RenderWidgetHostViewMacTest, Background) {
  // If no color has been specified then background_color is not set yet.
  ASSERT_FALSE(rwhv_mac_->GetBackgroundColor());

  {
    // Set the color to red. The background is initially assumed to be opaque,
    // so no opacity message change should be sent.
    EXPECT_CALL(mock_owner_delegate_, SetBackgroundOpaque(_)).Times(0);
    rwhv_mac_->SetBackgroundColor(SK_ColorRED);
    EXPECT_EQ(unsigned{SK_ColorRED}, *rwhv_mac_->GetBackgroundColor());
  }
  {
    // Set the color to blue. This should not send an opacity message.
    EXPECT_CALL(mock_owner_delegate_, SetBackgroundOpaque(_)).Times(0);
    rwhv_mac_->SetBackgroundColor(SK_ColorBLUE);
    EXPECT_EQ(unsigned{SK_ColorBLUE}, *rwhv_mac_->GetBackgroundColor());
  }
  {
    // Set the color back to transparent. The background color should now be
    // reported as the default (white), and a transparency change message should
    // be sent.
    EXPECT_CALL(mock_owner_delegate_, SetBackgroundOpaque(false));
    rwhv_mac_->SetBackgroundColor(SK_ColorTRANSPARENT);
    EXPECT_EQ(unsigned{SK_ColorWHITE}, *rwhv_mac_->GetBackgroundColor());
  }
  {
    // Set the color to blue. This should send an opacity message.
    EXPECT_CALL(mock_owner_delegate_, SetBackgroundOpaque(true));
    rwhv_mac_->SetBackgroundColor(SK_ColorBLUE);
    EXPECT_EQ(unsigned{SK_ColorBLUE}, *rwhv_mac_->GetBackgroundColor());
  }
}

// Scrolling with a mouse wheel device on Mac won't give phase information.
// MouseWheelPhaseHandler adds timer based phase information to wheel events
// generated from this type of devices.
TEST_F(RenderWidgetHostViewMacTest, TimerBasedPhaseInfo) {
  rwhv_mac_->set_mouse_wheel_wheel_phase_handler_timeout(
      base::Milliseconds(100));

  // Send a wheel event without phase information for scrolling by 3 lines.
  NSEvent* wheelEvent = MockScrollWheelEventWithoutPhase(3);
  [rwhv_mac_->GetInProcessNSView() scrollWheel:wheelEvent];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host_->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseWheel", GetMessageNames(events));

  events.clear();
  events = host_->GetAndResetDispatchedMessages();
  // Both GSB and GSU will be sent since GestureEventQueue allows multiple
  // in-flight events.
  ASSERT_EQ("GestureScrollBegin GestureScrollUpdate", GetMessageNames(events));
  ASSERT_TRUE(static_cast<const blink::WebGestureEvent&>(
                  events[0]->ToEvent()->Event()->Event())
                  .data.scroll_begin.synthetic);
  events.clear();

  // Wait for the mouse_wheel_end_dispatch_timer_ to expire, the pending wheel
  // event gets dispatched.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitWhenIdleClosure(), base::Milliseconds(100));
  run_loop.Run();

  events = host_->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseWheel GestureScrollEnd", GetMessageNames(events));
  ASSERT_TRUE(static_cast<const blink::WebGestureEvent&>(
                  events[1]->ToEvent()->Event()->Event())
                  .data.scroll_end.synthetic);
}

// With wheel scroll latching wheel end events are not sent immediately, instead
// we start a timer to see if momentum phase of the scroll starts or not.
TEST_F(RenderWidgetHostViewMacTest,
       WheelWithPhaseEndedIsNotForwardedImmediately) {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost process_host(&browser_context);
  process_host.Init();
  scoped_refptr<SiteInstanceGroup> site_instance_group = base::WrapRefCounted(
      SiteInstanceGroup::CreateForTesting(&browser_context, &process_host));
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host.GetNextRoutingID();
  auto host = std::make_unique<MockRenderWidgetHostImpl>(
      &delegate, site_instance_group->GetSafeRef(), routing_id,
      /*for_frame_widget=*/false);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host.get());
  base::RunLoop().RunUntilIdle();

  // Send an initial wheel event for scrolling by 3 lines.
  NSEvent* wheelEvent1 =
      MockScrollWheelEventWithPhase(@selector(phaseBegan), 3);
  [view->GetInProcessNSView() scrollWheel:wheelEvent1];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseWheel", GetMessageNames(events));

  events.clear();
  events = host->GetAndResetDispatchedMessages();
  // Both GSB and GSU will be sent since GestureEventQueue allows multiple
  // in-flight events.
  ASSERT_EQ("GestureScrollBegin GestureScrollUpdate", GetMessageNames(events));

  // Send a wheel event with phaseEnded. The event will be dropped and the
  // mouse_wheel_end_dispatch_timer_ will start.
  NSEvent* wheelEvent2 =
      MockScrollWheelEventWithPhase(@selector(phaseEnded), 0);
  [view->GetInProcessNSView() scrollWheel:wheelEvent2];
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();
  ASSERT_EQ(0U, events.size());
  DCHECK(view->HasPendingWheelEndEventForTesting());

  // Get the max time here before |view| is destroyed in the
  // ShutdownAndDestroyWidget call below.
  const base::TimeDelta max_time_between_phase_ended_and_momentum_phase_began =
      view->max_time_between_phase_ended_and_momentum_phase_began_for_test();

  host->ShutdownAndDestroyWidget(false);
  host.reset();

  // Wait for the mouse_wheel_end_dispatch_timer_ to expire after host is
  // destroyed. The pending wheel end event won't get dispatched since the
  // render_widget_host_ is null. This waiting confirms that no crash happens
  // because of an attempt to send the pending wheel end event.
  // https://crbug.com/770057
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      max_time_between_phase_ended_and_momentum_phase_began);
  run_loop.Run();
  process_host.Cleanup();
}

TEST_F(RenderWidgetHostViewMacTest,
       WheelWithMomentumPhaseBeganStopsTheWheelEndDispatchTimer) {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost process_host(&browser_context);
  process_host.Init();
  scoped_refptr<SiteInstanceGroup> site_instance_group = base::WrapRefCounted(
      SiteInstanceGroup::CreateForTesting(&browser_context, &process_host));
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host.GetNextRoutingID();
  auto host = std::make_unique<MockRenderWidgetHostImpl>(
      &delegate, site_instance_group->GetSafeRef(), routing_id,
      /*for_frame_widget=*/false);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host.get());
  base::RunLoop().RunUntilIdle();

  // Send an initial wheel event for scrolling by 3 lines.
  NSEvent* wheelEvent1 =
      MockScrollWheelEventWithPhase(@selector(phaseBegan), 3);
  [view->GetInProcessNSView() scrollWheel:wheelEvent1];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseWheel", GetMessageNames(events));

  // Indicate that the wheel event was unhandled.
  events.clear();
  events = host->GetAndResetDispatchedMessages();
  // Both GSB and GSU will be sent since GestureEventQueue allows multiple
  // in-flight events.
  ASSERT_EQ("GestureScrollBegin GestureScrollUpdate", GetMessageNames(events));

  // Send a wheel event with phaseEnded. The event will be dropped and the
  // mouse_wheel_end_dispatch_timer_ will start.
  NSEvent* wheelEvent2 =
      MockScrollWheelEventWithPhase(@selector(phaseEnded), 0);
  [view->GetInProcessNSView() scrollWheel:wheelEvent2];
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();
  ASSERT_EQ(0U, events.size());
  DCHECK(view->HasPendingWheelEndEventForTesting());

  // Send a wheel event with momentum phase started, this should stop the wheel
  // end dispatch timer.
  NSEvent* wheelEvent3 =
      MockScrollWheelEventWithMomentumPhase(@selector(phaseBegan), 3);
  ASSERT_TRUE(wheelEvent3);
  [view->GetInProcessNSView() scrollWheel:wheelEvent3];
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseWheel GestureScrollUpdate", GetMessageNames(events));
  DCHECK(!view->HasPendingWheelEndEventForTesting());

  host->ShutdownAndDestroyWidget(false);
  host.reset();
  process_host.Cleanup();
}

TEST_F(RenderWidgetHostViewMacTest,
       WheelWithPhaseBeganDispatchesThePendingWheelEnd) {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost process_host(&browser_context);
  process_host.Init();
  MockRenderWidgetHostDelegate delegate;
  scoped_refptr<SiteInstanceGroup> site_instance_group = base::WrapRefCounted(
      SiteInstanceGroup::CreateForTesting(&browser_context, &process_host));
  int32_t routing_id = process_host.GetNextRoutingID();
  auto host = std::make_unique<MockRenderWidgetHostImpl>(
      &delegate, site_instance_group->GetSafeRef(), routing_id,
      /*for_frame_widget=*/false);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host.get());
  base::RunLoop().RunUntilIdle();

  // Send an initial wheel event for scrolling by 3 lines.
  NSEvent* wheelEvent1 =
      MockScrollWheelEventWithPhase(@selector(phaseBegan), 3);
  [view->GetInProcessNSView() scrollWheel:wheelEvent1];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseWheel", GetMessageNames(events));

  // Indicate that the wheel event was unhandled.
  events.clear();
  // Both GSB and GSU will be sent since GestureEventQueue allows multiple
  // in-flight events.
  events = host->GetAndResetDispatchedMessages();
  ASSERT_EQ("GestureScrollBegin GestureScrollUpdate", GetMessageNames(events));

  // Send a wheel event with phaseEnded. The event will be dropped and the
  // mouse_wheel_end_dispatch_timer_ will start.
  NSEvent* wheelEvent2 =
      MockScrollWheelEventWithPhase(@selector(phaseEnded), 0);
  [view->GetInProcessNSView() scrollWheel:wheelEvent2];
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();
  ASSERT_EQ(0U, events.size());
  DCHECK(view->HasPendingWheelEndEventForTesting());

  // Send a wheel event with phase started, this should stop the wheel end
  // dispatch timer and dispatch the pending wheel end event for the previous
  // scroll sequence.
  NSEvent* wheelEvent3 =
      MockScrollWheelEventWithPhase(@selector(phaseBegan), 3);
  ASSERT_TRUE(wheelEvent3);
  [view->GetInProcessNSView() scrollWheel:wheelEvent3];
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseWheel GestureScrollEnd MouseWheel", GetMessageNames(events));
  DCHECK(!view->HasPendingWheelEndEventForTesting());

  host->ShutdownAndDestroyWidget(false);
  host.reset();
  process_host.Cleanup();
}

class RenderWidgetHostViewMacPinchTest : public RenderWidgetHostViewMacTest {
 public:
  RenderWidgetHostViewMacPinchTest() = default;

  RenderWidgetHostViewMacPinchTest(const RenderWidgetHostViewMacPinchTest&) =
      delete;
  RenderWidgetHostViewMacPinchTest& operator=(
      const RenderWidgetHostViewMacPinchTest&) = delete;

  void SendBeginPinchEvent() {
    NSEvent* pinchBeginEvent = MockPinchEvent(NSEventPhaseBegan, 0);
    [rwhv_cocoa_ magnifyWithEvent:pinchBeginEvent];
  }

  void SendEndPinchEvent() {
    NSEvent* pinchEndEvent = MockPinchEvent(NSEventPhaseEnded, 0);
    [rwhv_cocoa_ magnifyWithEvent:pinchEndEvent];
  }
};

TEST_F(RenderWidgetHostViewMacPinchTest, PinchThresholding) {
  // Do a gesture that crosses the threshold.
  {
    NSEvent* pinchUpdateEvents[3] = {
        MockPinchEvent(NSEventPhaseChanged, 0.25),
        MockPinchEvent(NSEventPhaseChanged, 0.25),
        MockPinchEvent(NSEventPhaseChanged, 0.25),
    };

    SendBeginPinchEvent();
    base::RunLoop().RunUntilIdle();
    MockWidgetInputHandler::MessageVector events =
        host_->GetAndResetDispatchedMessages();

    EXPECT_EQ(0U, events.size());

    // No zoom is sent for the first update event.
    [rwhv_cocoa_ magnifyWithEvent:pinchUpdateEvents[0]];
    base::RunLoop().RunUntilIdle();
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ("MouseWheel", GetMessageNames(events));

    // After acking the synthetic mouse wheel, no GesturePinch events are
    // produced.
    events[0]->ToEvent()->CallCallback(
        blink::mojom::InputEventResultState::kNoConsumerExists);
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ(0U, events.size());

    // The second update event crosses the threshold of 0.4, and so zoom is no
    // longer disabled.
    [rwhv_cocoa_ magnifyWithEvent:pinchUpdateEvents[1]];
    base::RunLoop().RunUntilIdle();
    events = host_->GetAndResetDispatchedMessages();

    EXPECT_EQ("MouseWheel GesturePinchBegin GesturePinchUpdate",
              GetMessageNames(events));

    // The third update still has zoom enabled.
    [rwhv_cocoa_ magnifyWithEvent:pinchUpdateEvents[2]];
    base::RunLoop().RunUntilIdle();
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ("MouseWheel GesturePinchUpdate", GetMessageNames(events));

    SendEndPinchEvent();
    base::RunLoop().RunUntilIdle();
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ("MouseWheel GesturePinchEnd", GetMessageNames(events));
  }

  // Do a gesture that doesn't cross the threshold, but happens when we're not
  // at page scale factor one, so it should be sent to the renderer.
  {
    NSEvent* pinchUpdateEvent = MockPinchEvent(NSEventPhaseChanged, 0.25);

    rwhv_mac_->page_at_minimum_scale_ = false;

    SendBeginPinchEvent();
    base::RunLoop().RunUntilIdle();
    MockWidgetInputHandler::MessageVector events =
        host_->GetAndResetDispatchedMessages();
    EXPECT_EQ(0U, events.size());

    // Expect that a zoom happen because the time threshold has not passed.
    [rwhv_cocoa_ magnifyWithEvent:pinchUpdateEvent];
    base::RunLoop().RunUntilIdle();
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ("MouseWheel", GetMessageNames(events));
    events[0]->ToEvent()->CallCallback(
        blink::mojom::InputEventResultState::kNoConsumerExists);
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ("GesturePinchBegin GesturePinchUpdate", GetMessageNames(events));

    SendEndPinchEvent();
    base::RunLoop().RunUntilIdle();
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ("MouseWheel GesturePinchEnd", GetMessageNames(events));
  }

  // Do a gesture again, after the page scale is no longer at one, and ensure
  // that it is thresholded again.
  {
    NSEvent* pinchUpdateEvent = MockPinchEvent(NSEventTypeMagnify, 0.25);

    rwhv_mac_->page_at_minimum_scale_ = true;

    SendBeginPinchEvent();
    base::RunLoop().RunUntilIdle();
    MockWidgetInputHandler::MessageVector events =
        host_->GetAndResetDispatchedMessages();
    EXPECT_EQ(0U, events.size());

    // Get back to zoom one right after the begin event. This should still keep
    // the thresholding in place (it is latched at the begin event).
    rwhv_mac_->page_at_minimum_scale_ = false;

    // Expect that zoom be disabled because the time threshold has passed.
    [rwhv_cocoa_ magnifyWithEvent:pinchUpdateEvent];
    base::RunLoop().RunUntilIdle();
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ("MouseWheel", GetMessageNames(events));

    events[0]->ToEvent()->CallCallback(
        blink::mojom::InputEventResultState::kNoConsumerExists);
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ(0U, events.size());

    // Since no GesturePinchBegin was sent by the time we reach the pinch end,
    // the GesturePinchBegin and GesturePinchEnd are elided.
    SendEndPinchEvent();
    base::RunLoop().RunUntilIdle();
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ("MouseWheel", GetMessageNames(events));
  }
}

// Tests that the NSEventTypeSmartMagnify event is first offered as a mouse
// wheel event and is then sent as a GestureDoubleTap to invoke the double-tap
// to zoom logic.
TEST_F(RenderWidgetHostViewMacTest, DoubleTapZoom) {
  NSEvent* smartMagnifyEvent = MockSmartMagnifyEvent();
  [rwhv_cocoa_ smartMagnifyWithEvent:smartMagnifyEvent];
  base::RunLoop().RunUntilIdle();

  MockWidgetInputHandler::MessageVector events =
      host_->GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));

  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNoConsumerExists);

  events = host_->GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureDoubleTap", GetMessageNames(events));
}

// Tests that the NSEventTypeSmartMagnify event may be consumed by a wheel
// listener to prevent the scale change.
TEST_F(RenderWidgetHostViewMacTest, DoubleTapZoomConsumed) {
  NSEvent* smartMagnifyEvent = MockSmartMagnifyEvent();
  [rwhv_cocoa_ smartMagnifyWithEvent:smartMagnifyEvent];
  base::RunLoop().RunUntilIdle();

  MockWidgetInputHandler::MessageVector events =
      host_->GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));

  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  events = host_->GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());
}

TEST_F(RenderWidgetHostViewMacTest, EventLatencyOSMouseWheelHistogram) {
  base::HistogramTester histogram_tester;

  // Send an initial wheel event for scrolling by 3 lines.
  // Verify that Event.Latency.OS2.MOUSE_WHEEL histogram is computed properly.
  NSEvent* wheelEvent = MockScrollWheelEventWithPhase(@selector(phaseBegan),3);
  [rwhv_mac_->GetInProcessNSView() scrollWheel:wheelEvent];

  MockWidgetInputHandler::MessageVector events =
      host_->GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  histogram_tester.ExpectTotalCount("Event.Latency.OS2.MOUSE_WHEEL", 1);
}

// This test verifies that |selected_text_| is updated accordingly with
// different variations of RWHVMac::SelectChanged updates.
TEST_F(RenderWidgetHostViewMacTest, SelectedText) {
  std::u16string sample_text;
  base::UTF8ToUTF16("hello world!", 12, &sample_text);
  gfx::Range range(6, 11);

  // Send a valid selection for the word 'World'.
  rwhv_mac_->SelectionChanged(sample_text, 0U, range);
  EXPECT_EQ("world", selected_text());

  // Make the range cover some of the text and extend more.
  range.set_end(100);
  rwhv_mac_->SelectionChanged(sample_text, 0U, range);
  EXPECT_EQ("world!", selected_text());

  // Finally, send an empty range. This should clear the selected text.
  range.set_start(100);
  rwhv_mac_->SelectionChanged(sample_text, 0U, range);
  EXPECT_EQ("", selected_text());
}

// This class is used for IME-related unit tests which verify correctness of IME
// for pages with multiple RWHVs.
class InputMethodMacTest : public RenderWidgetHostViewMacTest {
 public:
  InputMethodMacTest() = default;

  InputMethodMacTest(const InputMethodMacTest&) = delete;
  InputMethodMacTest& operator=(const InputMethodMacTest&) = delete;

  ~InputMethodMacTest() override = default;

  void SetUp() override {
    RenderWidgetHostViewMacTest::SetUp();

    // Initializing a child frame's view.
    child_browser_context_ = std::make_unique<TestBrowserContext>();
    child_process_host_ =
        std::make_unique<MockRenderProcessHost>(child_browser_context_.get());
    child_process_host_->Init();
    child_site_instance_group_ =
        base::WrapRefCounted(SiteInstanceGroup::CreateForTesting(
            site_instance_group_.get(), child_process_host_.get()));
    child_widget_ = std::make_unique<MockRenderWidgetHostImpl>(
        &delegate_, child_site_instance_group_->GetSafeRef(),
        child_process_host_->GetNextRoutingID(), /*for_frame_widget=*/false);
    child_view_ = new TestRenderWidgetHostView(child_widget_.get());
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    child_widget_->ShutdownAndDestroyWidget(false);
    child_widget_.reset();
    child_process_host_->Cleanup();
    child_site_instance_group_.reset();
    child_process_host_.reset();
    child_browser_context_.reset();
    RenderWidgetHostViewMacTest::TearDown();
  }

  void SetTextInputType(RenderWidgetHostViewBase* view,
                        ui::TextInputType type) {
    ui::mojom::TextInputState state;
    state.type = type;
    view->TextInputStateChanged(state);
  }

  IPC::TestSink& tab_sink() { return process()->sink(); }
  IPC::TestSink& child_sink() { return child_process_host_->sink(); }
  TextInputManager* text_input_manager() {
    return delegate_.GetTextInputManager();
  }
  RenderWidgetHostViewMac* tab_view() { return rwhv_mac_; }
  RenderWidgetHostImpl* tab_widget() { return host_.get(); }
  RenderWidgetHostViewCocoa* tab_GetInProcessNSView() { return rwhv_cocoa_; }

  NSCandidateListTouchBarItem* candidate_list_item() {
    return [tab_GetInProcessNSView().touchBar
        itemForIdentifier:NSTouchBarItemIdentifierCandidateList];
  }

 protected:
  std::unique_ptr<MockRenderProcessHost> child_process_host_;
  scoped_refptr<SiteInstanceGroup> child_site_instance_group_;
  std::unique_ptr<MockRenderWidgetHostImpl> child_widget_;
  raw_ptr<TestRenderWidgetHostView, DanglingUntriaged> child_view_;

 private:
  std::unique_ptr<TestBrowserContext> child_browser_context_;
};

// This test will verify that calling unmarkText on the cocoa view will lead to
// a finish composing text IPC for the corresponding active widget.
TEST_F(InputMethodMacTest, UnmarkText) {
  // Make the child view active and then call unmarkText on the view (Note that
  // |RenderWidgetHostViewCocoa::handlingKeyDown_| is false so calling
  // unmarkText would lead to an IPC. This assumption is made in other similar
  // tests as well). We should observe an IPC being sent to the |child_widget_|.
  SetTextInputType(child_view_, ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(child_widget_.get(), text_input_manager()->GetActiveWidget());
  [tab_GetInProcessNSView() unmarkText];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      child_widget_->GetAndResetDispatchedMessages();
  EXPECT_EQ("FinishComposingText", GetMessageNames(events));

  // Repeat the same steps for the tab's view .
  SetTextInputType(tab_view(), ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(tab_widget(), text_input_manager()->GetActiveWidget());
  [tab_GetInProcessNSView() unmarkText];
  base::RunLoop().RunUntilIdle();
  events = host_->GetAndResetDispatchedMessages();
  EXPECT_EQ("FinishComposingText", GetMessageNames(events));
}

// This test makes sure that calling setMarkedText on the cocoa view will lead
// to a set composition IPC for the corresponding active widget.
TEST_F(InputMethodMacTest, SetMarkedText) {
  // Some values for the call to setMarkedText.
  NSString* text = @"sample text";
  NSRange selectedRange = NSMakeRange(0, 4);
  NSRange replacementRange = NSMakeRange(0, 1);

  // Make the child view active and then call setMarkedText with some values. We
  // should observe an IPC being sent to the |child_widget_|.
  SetTextInputType(child_view_, ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(child_widget_.get(), text_input_manager()->GetActiveWidget());
  [tab_GetInProcessNSView() setMarkedText:text
                            selectedRange:selectedRange
                         replacementRange:replacementRange];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      child_widget_->GetAndResetDispatchedMessages();
  EXPECT_EQ("SetComposition", GetMessageNames(events));

  // Repeat the same steps for the tab's view.
  SetTextInputType(tab_view(), ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(tab_widget(), text_input_manager()->GetActiveWidget());
  [tab_GetInProcessNSView() setMarkedText:text
                            selectedRange:selectedRange
                         replacementRange:replacementRange];
  base::RunLoop().RunUntilIdle();
  events = host_->GetAndResetDispatchedMessages();
  EXPECT_EQ("SetComposition", GetMessageNames(events));
}

// This test makes sure that selectedRange and markedRange are updated correctly
// in various scenarios.
TEST_F(InputMethodMacTest, MarkedRangeSelectedRange) {
  if (!base::FeatureList::IsEnabled(features::kMacImeLiveConversionFix)) {
    return;
  }
  // If the replacement range is valid, the range should be replaced with the
  // new text.
  {
    NSString* text = @"sample text";
    NSRange selectedRange = NSMakeRange(2, 4);
    NSRange replacementRange = NSMakeRange(1, 1);

    SetTextInputType(tab_view(), ui::TEXT_INPUT_TYPE_TEXT);
    EXPECT_EQ(tab_widget(), text_input_manager()->GetActiveWidget());
    [tab_GetInProcessNSView() setMarkedText:text
                              selectedRange:selectedRange
                           replacementRange:replacementRange];

    NSRange actualSelectedRange = [tab_GetInProcessNSView() selectedRange];
    NSRange actualMarkedRange = [tab_GetInProcessNSView() markedRange];

    EXPECT_EQ((signed)actualMarkedRange.location, 1);
    EXPECT_EQ((signed)actualMarkedRange.length, 11);
    EXPECT_EQ((signed)actualSelectedRange.location, 3);
    EXPECT_EQ((signed)actualSelectedRange.length, 4);
  }

  // If the text is empty, the marked range should be reset and the selection
  // should be collapsed to the begining of the old marked range.
  {
    NSString* text = @"";
    NSRange selectedRange = NSMakeRange(0, 0);
    NSRange replacementRange = NSMakeRange(NSNotFound, 0);

    EXPECT_EQ(tab_widget(), text_input_manager()->GetActiveWidget());
    [tab_GetInProcessNSView() setMarkedText:text
                              selectedRange:selectedRange
                           replacementRange:replacementRange];

    NSRange actualSelectedRange = [tab_GetInProcessNSView() selectedRange];
    NSRange actualMarkedRange = [tab_GetInProcessNSView() markedRange];

    EXPECT_EQ((signed)actualMarkedRange.location, (signed)NSNotFound);
    EXPECT_EQ((signed)actualMarkedRange.length, 0);
    EXPECT_EQ((signed)actualSelectedRange.location, 1);
    EXPECT_EQ((signed)actualSelectedRange.length, 0);
  }

  // If no marked range and no replacement range, the current selection should
  // be replaced.
  {
    NSString* text = @"sample2";
    NSRange selectedRange = NSMakeRange(3, 2);
    NSRange replacementRange = NSMakeRange(NSNotFound, 0);

    SetTextInputType(tab_view(), ui::TEXT_INPUT_TYPE_TEXT);
    EXPECT_EQ(tab_widget(), text_input_manager()->GetActiveWidget());
    [tab_GetInProcessNSView() setMarkedText:text
                              selectedRange:selectedRange
                           replacementRange:replacementRange];

    NSRange actualSelectedRange = [tab_GetInProcessNSView() selectedRange];
    NSRange actualMarkedRange = [tab_GetInProcessNSView() markedRange];

    EXPECT_EQ((signed)actualMarkedRange.location, 1);
    EXPECT_EQ((signed)actualMarkedRange.length, 7);
    EXPECT_EQ((signed)actualSelectedRange.location, 4);
    EXPECT_EQ((signed)actualSelectedRange.length, 2);
  }

  // If the marked range is valid and there is no replacement range, the current
  // marked range should be replaced.
  {
    NSString* text = @"new";
    NSRange selectedRange = NSMakeRange(2, 1);
    NSRange replacementRange = NSMakeRange(NSNotFound, 0);

    SetTextInputType(tab_view(), ui::TEXT_INPUT_TYPE_TEXT);
    EXPECT_EQ(tab_widget(), text_input_manager()->GetActiveWidget());
    [tab_GetInProcessNSView() setMarkedText:text
                              selectedRange:selectedRange
                           replacementRange:replacementRange];

    NSRange actualSelectedRange = [tab_GetInProcessNSView() selectedRange];
    NSRange actualMarkedRange = [tab_GetInProcessNSView() markedRange];

    EXPECT_EQ((signed)actualMarkedRange.location, 1);
    EXPECT_EQ((signed)actualMarkedRange.length, 3);
    EXPECT_EQ((signed)actualSelectedRange.location, 3);
    EXPECT_EQ((signed)actualSelectedRange.length, 1);
  }
}

// This test verifies that calling insertText on the cocoa view will lead to a
// commit text IPC sent to the active widget.
TEST_F(InputMethodMacTest, InsertText) {
  // Some values for the call to insertText.
  NSString* text = @"sample text";
  NSRange replacementRange = NSMakeRange(0, 1);

  // Make the child view active and then call insertText with some values. We
  // should observe an IPC being sent to the |child_widget_|.
  SetTextInputType(child_view_, ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(child_widget_.get(), text_input_manager()->GetActiveWidget());
  [tab_GetInProcessNSView() insertText:text replacementRange:replacementRange];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      child_widget_->GetAndResetDispatchedMessages();
  EXPECT_EQ("CommitText", GetMessageNames(events));

  // Repeat the same steps for the tab's view.
  SetTextInputType(tab_view(), ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(tab_widget(), text_input_manager()->GetActiveWidget());
  [tab_GetInProcessNSView() insertText:text replacementRange:replacementRange];
  base::RunLoop().RunUntilIdle();
  events = host_->GetAndResetDispatchedMessages();
  EXPECT_EQ("CommitText", GetMessageNames(events));
}

// This test makes sure that calling finishComposingText on the cocoa view will
// lead to a finish composing text IPC for a the corresponding active widget.
TEST_F(InputMethodMacTest, FinishComposingText) {
  // Some values for the call to setMarkedText.
  NSString* text = @"sample text";
  NSRange selectedRange = NSMakeRange(0, 4);
  NSRange replacementRange = NSMakeRange(0, 1);

  // Make child view active and then call finishComposingText. We should observe
  // an IPC being sent to the |child_widget_|.
  SetTextInputType(child_view_, ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(child_widget_.get(), text_input_manager()->GetActiveWidget());
  // In order to finish composing text, we must first have some marked text. So,
  // we will first call setMarkedText on cocoa view. This would lead to a set
  // composition IPC in the sink, but it doesn't matter since we will be looking
  // for a finish composing text IPC for this test.
  [tab_GetInProcessNSView() setMarkedText:text
                            selectedRange:selectedRange
                         replacementRange:replacementRange];
  [tab_GetInProcessNSView() finishComposingText];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      child_widget_->GetAndResetDispatchedMessages();
  EXPECT_EQ("SetComposition FinishComposingText", GetMessageNames(events));

  // Repeat the same steps for the tab's view.
  SetTextInputType(tab_view(), ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(tab_widget(), text_input_manager()->GetActiveWidget());
  [tab_GetInProcessNSView() setMarkedText:text
                            selectedRange:selectedRange
                         replacementRange:replacementRange];
  [tab_GetInProcessNSView() finishComposingText];
  base::RunLoop().RunUntilIdle();
  events = host_->GetAndResetDispatchedMessages();
  EXPECT_EQ("SetComposition FinishComposingText", GetMessageNames(events));
}

TEST_F(InputMethodMacTest, SecurePasswordInput) {
  ASSERT_FALSE(ui::ScopedPasswordInputEnabler::IsPasswordInputEnabled());
  ASSERT_EQ(text_input_manager(), tab_view()->GetTextInputManager());

  // RenderWidgetHostViewMacTest.LostFocusAndGotFocusOnSetActive checks the
  // GotFocus()/LostFocus() rules, just silence the warnings here.
  EXPECT_CALL(*host_, Focus()).Times(::testing::AnyNumber());
  EXPECT_CALL(*host_, Blur()).Times(::testing::AnyNumber());

  [window_ makeFirstResponder:tab_view()->GetInProcessNSView()];

  // Shouldn't enable secure input if it's not a password textfield.
  tab_view()->SetActive(true);
  EXPECT_FALSE(ui::ScopedPasswordInputEnabler::IsPasswordInputEnabled());

  SetTextInputType(child_view_, ui::TEXT_INPUT_TYPE_PASSWORD);
  ASSERT_EQ(child_widget_.get(), text_input_manager()->GetActiveWidget());
  ASSERT_EQ(text_input_manager(), tab_view()->GetTextInputManager());
  ASSERT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, tab_view()->GetTextInputType());

  // Single matched calls immediately update IsPasswordInputEnabled().
  tab_view()->SetActive(true);
  EXPECT_TRUE(ui::ScopedPasswordInputEnabler::IsPasswordInputEnabled());

  tab_view()->SetActive(false);
  EXPECT_FALSE(ui::ScopedPasswordInputEnabler::IsPasswordInputEnabled());
}

// This test creates a test view to mimic a child frame's view and verifies that
// calling ImeCancelComposition on either the child view or the tab's view will
// always lead to a call to cancelComposition on the cocoa view.
TEST_F(InputMethodMacTest, ImeCancelCompositionForAllViews) {
  // Some values for the call to setMarkedText.
  NSString* text = @"sample text";
  NSRange selectedRange = NSMakeRange(0, 1);
  NSRange replacementRange = NSMakeRange(0, 1);

  // Make Cocoa view assume there is marked text.
  [tab_GetInProcessNSView() setMarkedText:text
                            selectedRange:selectedRange
                         replacementRange:replacementRange];
  EXPECT_TRUE(tab_GetInProcessNSView().hasMarkedText);
  child_view_->ImeCancelComposition();
  EXPECT_FALSE(tab_GetInProcessNSView().hasMarkedText);

  // Repeat for the tab's view.
  [tab_GetInProcessNSView() setMarkedText:text
                            selectedRange:selectedRange
                         replacementRange:replacementRange];
  EXPECT_TRUE(tab_GetInProcessNSView().hasMarkedText);
  tab_view()->ImeCancelComposition();
  EXPECT_FALSE(tab_GetInProcessNSView().hasMarkedText);
}

// This test verifies that calling FocusedNodeChanged() on
// RenderWidgetHostViewMac calls cancelComposition on the Cocoa view.
TEST_F(InputMethodMacTest, FocusedNodeChanged) {
  // Some values for the call to setMarkedText.
  NSString* text = @"sample text";
  NSRange selectedRange = NSMakeRange(0, 1);
  NSRange replacementRange = NSMakeRange(0, 1);

  [tab_GetInProcessNSView() setMarkedText:text
                            selectedRange:selectedRange
                         replacementRange:replacementRange];
  EXPECT_TRUE(tab_GetInProcessNSView().hasMarkedText);
  tab_view()->FocusedNodeChanged(true, gfx::Rect());
  EXPECT_FALSE(tab_GetInProcessNSView().hasMarkedText);
}

// This test verifies that when a RenderWidgetHostView changes its
// TextInputState to NONE we send the IPC to stop monitor composition info and,
// conversely, when its state is set to non-NONE, we start monitoring the
// composition info.
TEST_F(InputMethodMacTest, MonitorCompositionRangeForActiveWidget) {
  // First, we need to make the cocoa view the first responder so that the
  // method RWHVMac::HasFocus() returns true. Then we can make sure that as long
  // as there is some TextInputState of non-NONE, the corresponding widget will
  // be asked to start monitoring composition info.
  [window_ makeFirstResponder:tab_GetInProcessNSView()];
  EXPECT_TRUE(tab_view()->HasFocus());

  ui::mojom::TextInputState state;
  state.type = ui::TEXT_INPUT_TYPE_TEXT;

  // Make the tab's widget active.
  tab_view()->TextInputStateChanged(state);

  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host_->GetAndResetDispatchedMessages();
  // The tab's widget must have received an IPC regarding composition updates.
  EXPECT_EQ("SetFocus RequestCompositionUpdates", GetMessageNames(events));

  // The message should ask for monitoring updates, but no immediate update.
  MockWidgetInputHandler::DispatchedRequestCompositionUpdatesMessage* message =
      events.at(1)->ToRequestCompositionUpdates();
  EXPECT_FALSE(message->immediate_request());
  EXPECT_TRUE(message->monitor_request());

  // Now make the child view active.
  child_view_->TextInputStateChanged(state);

  // The tab should receive another IPC for composition updates.
  base::RunLoop().RunUntilIdle();
  events = host_->GetAndResetDispatchedMessages();
  // The tab's widget must have received an IPC regarding composition updates.
  EXPECT_EQ("RequestCompositionUpdates", GetMessageNames(events));
  // This time, the tab should have been asked to stop monitoring (and no
  // immediate updates).
  message = events.at(0)->ToRequestCompositionUpdates();
  EXPECT_FALSE(message->immediate_request());
  EXPECT_FALSE(message->monitor_request());

  // The child too must have received an IPC for composition updates.
  events = child_widget_->GetAndResetDispatchedMessages();
  EXPECT_EQ("RequestCompositionUpdates", GetMessageNames(events));

  // Verify that the message is asking for monitoring to start; but no immediate
  // updates.
  message = events.at(0)->ToRequestCompositionUpdates();
  EXPECT_FALSE(message->immediate_request());
  EXPECT_TRUE(message->monitor_request());

  // Make the tab view active again.
  tab_view()->TextInputStateChanged(state);

  base::RunLoop().RunUntilIdle();
  events = child_widget_->GetAndResetDispatchedMessages();

  // Verify that the child received another IPC for composition updates.
  EXPECT_EQ("RequestCompositionUpdates", GetMessageNames(events));

  // Verify that this IPC is asking for no monitoring or immediate updates.
  message = events.at(0)->ToRequestCompositionUpdates();
  EXPECT_FALSE(message->immediate_request());
  EXPECT_FALSE(message->monitor_request());
}

TEST_F(InputMethodMacTest, TouchBarTextSuggestionsPresence) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_NSEQ(nil, candidate_list_item());
  SetTextInputType(tab_view(), ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_NSNE(nil, candidate_list_item());
  SetTextInputType(tab_view(), ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_NSNE(nil, candidate_list_item());
}

TEST_F(InputMethodMacTest, TouchBarTextSuggestionsReplacement) {
  base::test::ScopedFeatureList feature_list;
  FakeSpellChecker* spellChecker = [[FakeSpellChecker alloc] init];
  tab_GetInProcessNSView().spellCheckerForTesting =
      static_cast<NSSpellChecker*>(spellChecker);

  SetTextInputType(tab_view(), ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_NSNE(nil, candidate_list_item());
  candidate_list_item().allowsCollapsing = NO;

  FakeTextCheckingResult* fakeResult =
      [FakeTextCheckingResult resultWithRange:NSMakeRange(0, 3)
                            replacementString:@"foo"];

  const std::u16string kOriginalString = u"abcxxxghi";

  // Change the selection once; requests completions from the spell checker.
  tab_view()->SelectionChanged(kOriginalString, 3, gfx::Range(3, 3));

  NSInteger firstSequenceNumber = spellChecker.lastAssignedSequenceNumber;
  SpellCheckerCompletionHandlerType firstCompletionHandler =
      spellChecker.completionHandlers[@(firstSequenceNumber)];

  EXPECT_NE(nil, firstCompletionHandler);
  EXPECT_EQ(0U, candidate_list_item().candidates.count);

  // Instead of replying right away, change the selection again!
  tab_view()->SelectionChanged(kOriginalString, 3, gfx::Range(5, 5));

  NSInteger secondSequenceNumber = spellChecker.lastAssignedSequenceNumber;
  SpellCheckerCompletionHandlerType secondCompletionHandler =
      spellChecker.completionHandlers[@(secondSequenceNumber)];

  EXPECT_NE(firstSequenceNumber, secondSequenceNumber);

  // Make sure that calling the stale completion handler is a no-op.
  firstCompletionHandler(firstSequenceNumber,
                         @[ static_cast<NSTextCheckingResult*>(fakeResult) ]);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0U, candidate_list_item().candidates.count);

  // But calling the current handler should work.
  secondCompletionHandler(secondSequenceNumber,
                          @[ static_cast<NSTextCheckingResult*>(fakeResult) ]);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, candidate_list_item().candidates.count);

  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host_->GetAndResetDispatchedMessages();
  ASSERT_EQ("", GetMessageNames(events));

  // Now, select that result.
  [tab_GetInProcessNSView() candidateListTouchBarItem:candidate_list_item()
                         endSelectingCandidateAtIndex:0];
  base::RunLoop().RunUntilIdle();
  events = host_->GetAndResetDispatchedMessages();
  ASSERT_EQ("CommitText", GetMessageNames(events));
}

TEST_F(InputMethodMacTest, TouchBarTextSuggestionsNotRequestedForPasswords) {
  base::test::ScopedFeatureList feature_list;
  FakeSpellChecker* spellChecker = [[FakeSpellChecker alloc] init];
  tab_GetInProcessNSView().spellCheckerForTesting =
      static_cast<NSSpellChecker*>(spellChecker);

  SetTextInputType(tab_view(), ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_NSNE(nil, candidate_list_item());
  candidate_list_item().allowsCollapsing = NO;

  const std::u16string kOriginalString = u"abcxxxghi";

  // Change the selection once; completions should *not* be requested.
  tab_view()->SelectionChanged(kOriginalString, 3, gfx::Range(3, 3));

  EXPECT_EQ(0U, spellChecker.lastAssignedSequenceNumber);
}

// https://crbug.com/893038: There exist code paths which set the selection
// range to gfx::Range::InvalidRange(). I'm not sure how to exercise them in
// practice, but this has caused crashes in the field.
TEST_F(InputMethodMacTest, TouchBarTextSuggestionsInvalidSelection) {
  base::test::ScopedFeatureList feature_list;
  FakeSpellChecker* spellChecker = [[FakeSpellChecker alloc] init];
  tab_GetInProcessNSView().spellCheckerForTesting =
      static_cast<NSSpellChecker*>(spellChecker);

  SetTextInputType(tab_view(), ui::TEXT_INPUT_TYPE_TEXT);

  NSInteger firstSequenceNumber = spellChecker.lastAssignedSequenceNumber;
  SpellCheckerCompletionHandlerType firstCompletionHandler =
      spellChecker.completionHandlers[@(firstSequenceNumber)];

  if (firstSequenceNumber) {
    firstCompletionHandler(firstSequenceNumber, @[]);
    base::RunLoop().RunUntilIdle();
  }

  const std::u16string kOriginalString = u"abcxxxghi";

  tab_view()->SelectionChanged(kOriginalString, 3, gfx::Range::InvalidRange());

  NSInteger secondSequenceNumber = spellChecker.lastAssignedSequenceNumber;
  // If the selection changed to a bogus value, expect the machinery to have
  // bailed out early and not ended up requesting suggestions.
  EXPECT_EQ(firstSequenceNumber, secondSequenceNumber);
}

// This test verifies that in AutoResize mode a child-allocated
// viz::LocalSurfaceId will be properly routed and stored in the parent.
TEST_F(RenderWidgetHostViewMacTest, ChildAllocationAcceptedInParent) {
  viz::LocalSurfaceId local_surface_id1(rwhv_mac_->GetLocalSurfaceId());
  EXPECT_TRUE(local_surface_id1.is_valid());

  host_->SetAutoResize(true, gfx::Size(50, 50), gfx::Size(100, 100));

  viz::ChildLocalSurfaceIdAllocator child_allocator;
  child_allocator.UpdateFromParent(rwhv_mac_->GetLocalSurfaceId());
  child_allocator.GenerateId();
  viz::LocalSurfaceId local_surface_id2 =
      child_allocator.GetCurrentLocalSurfaceId();
  cc::RenderFrameMetadata metadata;
  metadata.viewport_size_in_pixels = gfx::Size(75, 75);
  metadata.local_surface_id = child_allocator.GetCurrentLocalSurfaceId();
  static_cast<RenderFrameMetadataProvider::Observer&>(*host_)
      .OnLocalSurfaceIdChanged(metadata);

  viz::LocalSurfaceId local_surface_id3(rwhv_mac_->GetLocalSurfaceId());
  EXPECT_NE(local_surface_id1, local_surface_id3);
  EXPECT_EQ(local_surface_id2, local_surface_id3);
}

// This test verifies that when the child and parent both allocate their own
// viz::LocalSurfaceId the resulting conflict is resolved.
TEST_F(RenderWidgetHostViewMacTest, ConflictingAllocationsResolve) {
  viz::LocalSurfaceId local_surface_id1(rwhv_mac_->GetLocalSurfaceId());
  EXPECT_TRUE(local_surface_id1.is_valid());

  host_->SetAutoResize(true, gfx::Size(50, 50), gfx::Size(100, 100));
  viz::ChildLocalSurfaceIdAllocator child_allocator;
  child_allocator.UpdateFromParent(rwhv_mac_->GetLocalSurfaceId());
  child_allocator.GenerateId();
  viz::LocalSurfaceId local_surface_id2 =
      child_allocator.GetCurrentLocalSurfaceId();
  cc::RenderFrameMetadata metadata;
  metadata.viewport_size_in_pixels = gfx::Size(75, 75);
  metadata.local_surface_id = child_allocator.GetCurrentLocalSurfaceId();
  static_cast<RenderFrameMetadataProvider::Observer&>(*host_)
      .OnLocalSurfaceIdChanged(metadata);

  // Cause a conflicting viz::LocalSurfaceId allocation
  BrowserCompositorMac* browser_compositor = rwhv_mac_->BrowserCompositor();
  browser_compositor->ForceNewSurfaceForTesting();
  viz::LocalSurfaceId local_surface_id3(rwhv_mac_->GetLocalSurfaceId());
  EXPECT_NE(local_surface_id1, local_surface_id3);

  // RenderWidgetHostImpl has delayed auto-resize processing. Yield here to
  // let it complete.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  viz::LocalSurfaceId local_surface_id4(rwhv_mac_->GetLocalSurfaceId());
  EXPECT_NE(local_surface_id1, local_surface_id4);
  EXPECT_NE(local_surface_id2, local_surface_id4);
  viz::LocalSurfaceId merged_local_surface_id(
      local_surface_id2.parent_sequence_number() + 1,
      local_surface_id2.child_sequence_number(),
      local_surface_id2.embed_token());
  EXPECT_EQ(local_surface_id4, merged_local_surface_id);
}

TEST_F(RenderWidgetHostViewMacTest, TransformToRootNoParentLayer) {
  gfx::PointF point(10, 20);
  rwhv_mac_->TransformPointToRootSurface(&point);
  EXPECT_EQ(point, gfx::PointF(10, 20));
}

TEST_F(RenderWidgetHostViewMacTest, TransformToRootWithParentLayer) {
  std::unique_ptr<ui::RecyclableCompositorMac> compositor =
      std::make_unique<ui::RecyclableCompositorMac>(
          ImageTransportFactory::GetInstance()->GetContextFactory());
  std::unique_ptr<ui::Layer> root_surface_layer =
      std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  std::unique_ptr<ui::Layer> parent_layer =
      std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);

  compositor->compositor()->SetRootLayer(root_surface_layer.get());
  root_surface_layer->SetBounds(gfx::Rect(-5, -10, 1000, 2000));
  parent_layer->SetBounds(gfx::Rect(100, 300, 500, 400));
  root_surface_layer->Add(parent_layer.get());
  gfx::PointF point(10, 20);
  rwhv_mac_->SetParentUiLayer(parent_layer.get());
  rwhv_mac_->TransformPointToRootSurface(&point);
  EXPECT_EQ(point, gfx::PointF(105, 310));
}

TEST_F(RenderWidgetHostViewMacTest, AccessibilityParentTest) {
  NSView* view = rwhv_mac_->GetInProcessNSView();

  // NSBox so it participates in the a11y hierarchy.
  NSView* parent_view = [[NSBox alloc] init];
  NSView* accessibility_parent = [[NSView alloc] init];
  NSWindow* window = [[NSWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, 512, 512)
                styleMask:NSWindowStyleMaskResizable | NSWindowStyleMaskTitled
                  backing:NSBackingStoreBuffered
                    defer:NO];
  window.releasedWhenClosed = NO;
  [window.contentView addSubview:accessibility_parent];

  [parent_view addSubview:view];
  EXPECT_NSEQ([view accessibilityParent], parent_view);

  rwhv_mac_->SetParentAccessibilityElement(accessibility_parent);
  EXPECT_NSEQ([view accessibilityParent],
              NSAccessibilityUnignoredAncestor(accessibility_parent));
  EXPECT_NSNE(nil, [view accessibilityParent]);

  rwhv_mac_->SetParentAccessibilityElement(nil);
  EXPECT_NSEQ([view accessibilityParent], parent_view);
}

}  // namespace content
