// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/direct_manipulation_helper_win.h"

#include <objbase.h>

#include "base/test/scoped_feature_list.h"
#include "base/win/windows_version.h"
#include "content/browser/renderer_host/direct_manipulation_test_helper_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/win/window_event_target.h"

namespace content {

namespace {

class MockDirectManipulationViewport
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::RuntimeClassType::ClassicCom>,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<
                  Microsoft::WRL::RuntimeClassType::ClassicCom>,
              Microsoft::WRL::FtmBase,
              IDirectManipulationViewport>> {
 public:
  MockDirectManipulationViewport() {}

  ~MockDirectManipulationViewport() override {}

  bool WasZoomToRectCalled() {
    bool called = zoom_to_rect_called_;
    zoom_to_rect_called_ = false;
    return called;
  }

  HRESULT STDMETHODCALLTYPE Enable() override { return S_OK; }

  HRESULT STDMETHODCALLTYPE Disable() override { return S_OK; }

  HRESULT STDMETHODCALLTYPE SetContact(_In_ UINT32 pointerId) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE ReleaseContact(_In_ UINT32 pointerId) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE ReleaseAllContacts() override { return S_OK; }

  HRESULT STDMETHODCALLTYPE
  GetStatus(_Out_ DIRECTMANIPULATION_STATUS* status) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetTag(_In_ REFIID riid,
                                   _COM_Outptr_opt_ void** object,
                                   _Out_opt_ UINT32* id) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE SetTag(_In_opt_ IUnknown* object,
                                   _In_ UINT32 id) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetViewportRect(_Out_ RECT* viewport) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  SetViewportRect(_In_ const RECT* viewport) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE ZoomToRect(_In_ const float left,
                                       _In_ const float top,
                                       _In_ const float right,
                                       _In_ const float bottom,
                                       _In_ BOOL animate) override {
    zoom_to_rect_called_ = true;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  SetViewportTransform(_In_reads_(point_count) const float* matrix,
                       _In_ DWORD point_count) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  SyncDisplayTransform(_In_reads_(point_count) const float* matrix,
                       _In_ DWORD point_count) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  GetPrimaryContent(_In_ REFIID riid, _COM_Outptr_ void** object) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  AddContent(_In_ IDirectManipulationContent* content) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  RemoveContent(_In_ IDirectManipulationContent* content) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE SetViewportOptions(
      _In_ DIRECTMANIPULATION_VIEWPORT_OPTIONS options) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE AddConfiguration(
      _In_ DIRECTMANIPULATION_CONFIGURATION configuration) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE RemoveConfiguration(
      _In_ DIRECTMANIPULATION_CONFIGURATION configuration) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE ActivateConfiguration(
      _In_ DIRECTMANIPULATION_CONFIGURATION configuration) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE SetManualGesture(
      _In_ DIRECTMANIPULATION_GESTURE_CONFIGURATION configuration) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  SetChaining(_In_ DIRECTMANIPULATION_MOTION_TYPES enabledTypes) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  AddEventHandler(_In_opt_ HWND window,
                  _In_ IDirectManipulationViewportEventHandler* eventHandler,
                  _Out_ DWORD* cookie) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE RemoveEventHandler(_In_ DWORD cookie) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  SetInputMode(_In_ DIRECTMANIPULATION_INPUT_MODE mode) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  SetUpdateMode(_In_ DIRECTMANIPULATION_INPUT_MODE mode) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE Stop() override { return S_OK; }

  HRESULT STDMETHODCALLTYPE Abandon() override { return S_OK; }

 private:
  bool zoom_to_rect_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockDirectManipulationViewport);
};

enum class EventGesture {
  kScrollBegin,
  kScroll,
  kScrollEnd,
  kFlingBegin,
  kFling,
  kFlingEnd,
  kScaleBegin,
  kScale,
  kScaleEnd,
};

struct Event {
  explicit Event(float scale) : gesture_(EventGesture::kScale), scale_(scale) {}

  Event(EventGesture gesture, float scroll_x, float scroll_y)
      : gesture_(gesture), scroll_x_(scroll_x), scroll_y_(scroll_y) {}

  explicit Event(EventGesture gesture) : gesture_(gesture) {}

  EventGesture gesture_;
  float scale_ = 0;
  float scroll_x_ = 0;
  float scroll_y_ = 0;
};

class MockWindowEventTarget : public ui::WindowEventTarget {
 public:
  MockWindowEventTarget() {}

  ~MockWindowEventTarget() override {}

  void ApplyPinchZoomScale(float scale) override {
    events_.push_back(Event(scale));
  }

  void ApplyPinchZoomBegin() override {
    events_.push_back(Event(EventGesture::kScaleBegin));
  }

  void ApplyPinchZoomEnd() override {
    events_.push_back(Event(EventGesture::kScaleEnd));
  }

  void ApplyPanGestureScroll(int scroll_x, int scroll_y) override {
    events_.push_back(Event(EventGesture::kScroll, scroll_x, scroll_y));
  }

  void ApplyPanGestureFling(int scroll_x, int scroll_y) override {
    events_.push_back(Event(EventGesture::kFling, scroll_x, scroll_y));
  }

  void ApplyPanGestureScrollBegin(int scroll_x, int scroll_y) override {
    events_.push_back(Event(EventGesture::kScrollBegin, scroll_x, scroll_y));
  }

  void ApplyPanGestureFlingBegin() override {
    events_.push_back(Event(EventGesture::kFlingBegin));
  }

  void ApplyPanGestureFlingEnd() override {
    events_.push_back(Event(EventGesture::kFlingEnd));
  }

  void ApplyPanGestureScrollEnd(bool tranisitioning_to_pinch) override {
    events_.push_back(Event(EventGesture::kScrollEnd));
  }

  std::vector<Event> GetEvents() {
    std::vector<Event> result = events_;
    events_.clear();
    return result;
  }

  // Other Overrides
  LRESULT HandleMouseMessage(unsigned int message,
                             WPARAM w_param,
                             LPARAM l_param,
                             bool* handled) override {
    return S_OK;
  }

  LRESULT HandlePointerMessage(unsigned int message,
                               WPARAM w_param,
                               LPARAM l_param,
                               bool* handled) override {
    return S_OK;
  }

  LRESULT HandleKeyboardMessage(unsigned int message,
                                WPARAM w_param,
                                LPARAM l_param,
                                bool* handled) override {
    return S_OK;
  }

  LRESULT HandleTouchMessage(unsigned int message,
                             WPARAM w_param,
                             LPARAM l_param,
                             bool* handled) override {
    return S_OK;
  }

  LRESULT HandleInputMessage(unsigned int message,
                             WPARAM w_param,
                             LPARAM l_param,
                             bool* handled) override {
    return S_OK;
  }

  LRESULT HandleScrollMessage(unsigned int message,
                              WPARAM w_param,
                              LPARAM l_param,
                              bool* handled) override {
    return S_OK;
  }

  LRESULT HandleNcHitTestMessage(unsigned int message,
                                 WPARAM w_param,
                                 LPARAM l_param,
                                 bool* handled) override {
    return S_OK;
  }

  void HandleParentChanged() override {}

 private:
  std::vector<Event> events_;

  DISALLOW_COPY_AND_ASSIGN(MockWindowEventTarget);
};

}  //  namespace

class DirectManipulationUnitTest : public testing::Test {
 public:
  DirectManipulationUnitTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kPrecisionTouchpad);

    viewport_ = Microsoft::WRL::Make<MockDirectManipulationViewport>();
    content_ = Microsoft::WRL::Make<MockDirectManipulationContent>();
    direct_manipulation_helper_ =
        DirectManipulationHelper::CreateInstanceForTesting(&event_target_,
                                                           viewport_);
  }

  ~DirectManipulationUnitTest() override {}

  DirectManipulationHelper* GetDirectManipulationHelper() {
    return direct_manipulation_helper_.get();
  }

  std::vector<Event> GetEvents() { return event_target_.GetEvents(); }

  void ViewportStatusChanged(DIRECTMANIPULATION_STATUS current,
                             DIRECTMANIPULATION_STATUS previous) {
    direct_manipulation_helper_->event_handler_->OnViewportStatusChanged(
        viewport_.Get(), current, previous);
  }

  void ContentUpdated(float scale, float scroll_x, float scroll_y) {
    content_->SetContentTransform(scale, scroll_x, scroll_y);
    direct_manipulation_helper_->event_handler_->OnContentUpdated(
        viewport_.Get(), content_.Get());
  }

  bool WasZoomToRectCalled() { return viewport_->WasZoomToRectCalled(); }

  void SetDeviceScaleFactor(float factor) {
    direct_manipulation_helper_->SetDeviceScaleFactorForTesting(factor);
  }

 private:
  std::unique_ptr<DirectManipulationHelper> direct_manipulation_helper_;
  Microsoft::WRL::ComPtr<MockDirectManipulationViewport> viewport_;
  Microsoft::WRL::ComPtr<MockDirectManipulationContent> content_;
  MockWindowEventTarget event_target_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DirectManipulationUnitTest);
};

TEST_F(DirectManipulationUnitTest, HelperShouldCreateForWin10) {
  // We should create DirectManipulationHelper instance when win version >= 10.
  EXPECT_EQ(GetDirectManipulationHelper() != nullptr,
            base::win::GetVersion() >= base::win::Version::WIN10);
}

TEST_F(DirectManipulationUnitTest, ReceiveSimplePanTransform) {
  if (!GetDirectManipulationHelper())
    return;

  ViewportStatusChanged(DIRECTMANIPULATION_RUNNING, DIRECTMANIPULATION_READY);
  ContentUpdated(1, 10, 0);

  std::vector<Event> events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kScrollBegin, events[0].gesture_);
  EXPECT_EQ(10, events[0].scroll_x_);
  EXPECT_EQ(0, events[0].scroll_y_);

  // For next update, should only apply the difference.
  ContentUpdated(1, 15, 0);

  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kScroll, events[0].gesture_);
  EXPECT_EQ(5, events[0].scroll_x_);
  EXPECT_EQ(0, events[0].scroll_y_);

  ViewportStatusChanged(DIRECTMANIPULATION_READY, DIRECTMANIPULATION_RUNNING);

  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kScrollEnd, events[0].gesture_);
}

TEST_F(DirectManipulationUnitTest, ReceivePanFling) {
  if (!GetDirectManipulationHelper())
    return;

  ViewportStatusChanged(DIRECTMANIPULATION_RUNNING, DIRECTMANIPULATION_READY);
  ContentUpdated(1, 10, 0);

  std::vector<Event> events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kScrollBegin, events[0].gesture_);
  EXPECT_EQ(10, events[0].scroll_x_);
  EXPECT_EQ(0, events[0].scroll_y_);

  // For next update, should only apply the difference.
  ContentUpdated(1, 15, 0);

  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kScroll, events[0].gesture_);
  EXPECT_EQ(5, events[0].scroll_x_);
  EXPECT_EQ(0, events[0].scroll_y_);

  // Fling Begin.
  ViewportStatusChanged(DIRECTMANIPULATION_INERTIA, DIRECTMANIPULATION_RUNNING);

  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kFlingBegin, events[0].gesture_);

  ContentUpdated(1, 20, 0);

  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kFling, events[0].gesture_);
  EXPECT_EQ(5, events[0].scroll_x_);
  EXPECT_EQ(0, events[0].scroll_y_);

  ViewportStatusChanged(DIRECTMANIPULATION_READY, DIRECTMANIPULATION_INERTIA);
  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kFlingEnd, events[0].gesture_);
}

TEST_F(DirectManipulationUnitTest, ReceiveSimpleScaleTransform) {
  if (!GetDirectManipulationHelper())
    return;

  ViewportStatusChanged(DIRECTMANIPULATION_RUNNING, DIRECTMANIPULATION_READY);
  ContentUpdated(1.1f, 0, 0);
  std::vector<Event> events = GetEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(EventGesture::kScaleBegin, events[0].gesture_);
  EXPECT_EQ(EventGesture::kScale, events[1].gesture_);
  EXPECT_EQ(1.1f, events[1].scale_);

  // For next update, should only apply the difference.
  ContentUpdated(1.21f, 0, 0);
  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kScale, events[0].gesture_);
  EXPECT_EQ(1.1f, events[0].scale_);

  ViewportStatusChanged(DIRECTMANIPULATION_READY, DIRECTMANIPULATION_RUNNING);
  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kScaleEnd, events[0].gesture_);
}

TEST_F(DirectManipulationUnitTest, ReceiveScrollTransformLessThanOne) {
  if (!GetDirectManipulationHelper())
    return;

  // Scroll offset less than 1, should not apply.
  ViewportStatusChanged(DIRECTMANIPULATION_RUNNING, DIRECTMANIPULATION_READY);
  ContentUpdated(1, 0.1f, 0);
  std::vector<Event> events = GetEvents();
  EXPECT_EQ(0u, events.size());

  // Scroll offset less than 1, should not apply.
  ContentUpdated(1, 0.2f, 0);
  events = GetEvents();
  EXPECT_EQ(0u, events.size());

  // Scroll offset more than 1, should only apply integer part.
  ContentUpdated(1, 1.2f, 0);
  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kScrollBegin, events[0].gesture_);
  EXPECT_EQ(1, events[0].scroll_x_);
  EXPECT_EQ(0, events[0].scroll_y_);

  // Scroll offset difference less than 1, should not apply.
  ContentUpdated(1, 1.5f, 0);
  events = GetEvents();
  EXPECT_EQ(0u, events.size());

  // Scroll offset difference more than 1, should only apply integer part.
  ContentUpdated(1, 3.0f, 0);
  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kScroll, events[0].gesture_);
  EXPECT_EQ(2, events[0].scroll_x_);
  EXPECT_EQ(0, events[0].scroll_y_);
}

TEST_F(DirectManipulationUnitTest,
       ReceiveScaleTransformLessThanFloatPointError) {
  if (!GetDirectManipulationHelper())
    return;

  // Scale factor less than float point error, ignore.
  ViewportStatusChanged(DIRECTMANIPULATION_RUNNING, DIRECTMANIPULATION_READY);
  ContentUpdated(1.000001f, 0, 0);
  std::vector<Event> events = GetEvents();
  EXPECT_EQ(0u, events.size());

  // Scale factor more than float point error, apply.
  ContentUpdated(1.00001f, 0, 0);
  events = GetEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(EventGesture::kScaleBegin, events[0].gesture_);
  EXPECT_EQ(EventGesture::kScale, events[1].gesture_);
  EXPECT_EQ(1.00001f, events[1].scale_);

  // Scale factor difference less than float point error, ignore.
  ContentUpdated(1.000011f, 0, 0);
  events = GetEvents();
  EXPECT_EQ(0u, events.size());

  // Scale factor difference more than float point error, apply.
  ContentUpdated(1.000021f, 0, 0);
  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kScale, events[0].gesture_);
  EXPECT_EQ(1.000021f / 1.00001f, events[0].scale_);

  ViewportStatusChanged(DIRECTMANIPULATION_READY, DIRECTMANIPULATION_RUNNING);
  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kScaleEnd, events[0].gesture_);
}

TEST_F(DirectManipulationUnitTest, InSameSequenceReceiveBothScrollAndScale) {
  if (!GetDirectManipulationHelper())
    return;

  // Direct Manipulation maybe give incorrect predictions. In this case, we will
  // receive scroll first then scale after.

  // First event is a scroll event.
  ContentUpdated(1.0f, 5, 0);
  std::vector<Event> events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kScrollBegin, events[0].gesture_);

  // Second event comes with scale factor. Now the scroll offset only noise.
  ContentUpdated(1.00001f, 5, 0);
  events = GetEvents();
  EXPECT_EQ(3u, events.size());
  EXPECT_EQ(EventGesture::kScrollEnd, events[0].gesture_);
  EXPECT_EQ(EventGesture::kScaleBegin, events[1].gesture_);
  EXPECT_EQ(EventGesture::kScale, events[2].gesture_);
}

TEST_F(DirectManipulationUnitTest, InSameSequenceReceiveScaleAfterFling) {
  if (!GetDirectManipulationHelper())
    return;

  // Direct Manipulation maybe give pinch event after fling. In this case, we
  // should end the current sequence first.

  // First event is a scroll event.
  ContentUpdated(1.0f, 5, 0);
  std::vector<Event> events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kScrollBegin, events[0].gesture_);

  // Fling Begin.
  ViewportStatusChanged(DIRECTMANIPULATION_INERTIA, DIRECTMANIPULATION_RUNNING);
  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kFlingBegin, events[0].gesture_);

  ContentUpdated(1, 10, 0);
  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kFling, events[0].gesture_);

  // Event comes with scale factor. Now the scroll offset only noise.
  ViewportStatusChanged(DIRECTMANIPULATION_RUNNING, DIRECTMANIPULATION_INERTIA);
  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kFlingEnd, events[0].gesture_);

  ContentUpdated(1.00001f, 10, 0);
  events = GetEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(EventGesture::kScaleBegin, events[0].gesture_);
  EXPECT_EQ(EventGesture::kScale, events[1].gesture_);
}

TEST_F(DirectManipulationUnitTest, InSameSequenceReceiveScrollAfterFling) {
  if (!GetDirectManipulationHelper())
    return;

  // Direct Manipulation maybe give scroll event after fling. In this case, we
  // should end the current sequence first.

  // First event is a scroll event.
  ContentUpdated(1.0f, 5, 0);
  std::vector<Event> events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kScrollBegin, events[0].gesture_);

  // Fling Begin.
  ViewportStatusChanged(DIRECTMANIPULATION_INERTIA, DIRECTMANIPULATION_RUNNING);
  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kFlingBegin, events[0].gesture_);

  ContentUpdated(1, 10, 0);
  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kFling, events[0].gesture_);

  // Fling back to Scroll.
  ViewportStatusChanged(DIRECTMANIPULATION_RUNNING, DIRECTMANIPULATION_INERTIA);
  ContentUpdated(1, 15, 0);
  events = GetEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(EventGesture::kFlingEnd, events[0].gesture_);
  EXPECT_EQ(EventGesture::kScrollBegin, events[1].gesture_);
}

TEST_F(DirectManipulationUnitTest,
       ReceiveScaleAfterFlingWithoutViewportStatusChanged) {
  if (!GetDirectManipulationHelper())
    return;

  // We never see this when testing, but still what to test it.

  ContentUpdated(1.0f, 5, 0);
  std::vector<Event> events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kScrollBegin, events[0].gesture_);

  // Fling Begin.
  ViewportStatusChanged(DIRECTMANIPULATION_INERTIA, DIRECTMANIPULATION_RUNNING);
  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kFlingBegin, events[0].gesture_);

  ContentUpdated(1, 10, 0);
  events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kFling, events[0].gesture_);

  // Event comes with scale factor. But no ViewportStatusChanged.
  ContentUpdated(1.00001f, 10, 0);
  events = GetEvents();
  EXPECT_EQ(3u, events.size());
  EXPECT_EQ(EventGesture::kFlingEnd, events[0].gesture_);
  EXPECT_EQ(EventGesture::kScaleBegin, events[1].gesture_);
  EXPECT_EQ(EventGesture::kScale, events[2].gesture_);
}

TEST_F(DirectManipulationUnitTest,
       ZoomToRectShouldNotBeCalledInEmptyRunningReadySequence) {
  if (!GetDirectManipulationHelper())
    return;

  ContentUpdated(1.0f, 5, 0);

  // Receive first ready when gesture end.
  ViewportStatusChanged(DIRECTMANIPULATION_READY, DIRECTMANIPULATION_RUNNING);
  EXPECT_TRUE(WasZoomToRectCalled());

  // Receive second ready from ZoomToRect.
  ViewportStatusChanged(DIRECTMANIPULATION_READY, DIRECTMANIPULATION_RUNNING);
  EXPECT_FALSE(WasZoomToRectCalled());
}

TEST_F(DirectManipulationUnitTest, HiDPIScroll) {
  if (!GetDirectManipulationHelper())
    return;

  SetDeviceScaleFactor(10.0);
  ContentUpdated(1.0f, 50, 0);
  std::vector<Event> events = GetEvents();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(EventGesture::kScrollBegin, events[0].gesture_);
  EXPECT_EQ(5, events[0].scroll_x_);
}

}  //  namespace content
