// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_android.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "cc/layers/deadline_policy.h"
#include "cc/slim/layer.h"
#include "components/input/render_input_router.mojom.h"
#include "components/viz/common/features.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/mock_render_widget_host.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/android/test_view_android_delegate.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/events/android/motion_event_android_factory.h"
#include "ui/events/android/motion_event_android_java.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/motionevent_jni_headers/MotionEvent_jni.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::Return;

// Allows for RenderWidgetHostViewAndroidRotationTest to override the ScreenInfo
// so that different configurations can be tests. The default path fallbacks on
// an empty ScreenInfo in testing, assuming it has no effect.
class CustomScreenInfoRenderWidgetHostViewAndroid
    : public RenderWidgetHostViewAndroid {
 public:
  CustomScreenInfoRenderWidgetHostViewAndroid(
      RenderWidgetHostImpl* widget,
      gfx::NativeView parent_native_view,
      cc::slim::Layer* parent_layer);
  ~CustomScreenInfoRenderWidgetHostViewAndroid() override {}

  void SetScreenInfo(display::ScreenInfo screen_info);

  // RenderWidgetHostViewAndroid:
  display::ScreenInfos GetScreenInfos() const override;
  display::ScreenInfo GetScreenInfo() const override;

 private:
  CustomScreenInfoRenderWidgetHostViewAndroid(
      const CustomScreenInfoRenderWidgetHostViewAndroid&) = delete;
  CustomScreenInfoRenderWidgetHostViewAndroid& operator=(
      const CustomScreenInfoRenderWidgetHostViewAndroid&) = delete;

  display::ScreenInfo screen_info_;
};

CustomScreenInfoRenderWidgetHostViewAndroid::
    CustomScreenInfoRenderWidgetHostViewAndroid(
        RenderWidgetHostImpl* widget,
        gfx::NativeView parent_native_view,
        cc::slim::Layer* parent_layer)
    : RenderWidgetHostViewAndroid(widget, parent_native_view, parent_layer) {}

void CustomScreenInfoRenderWidgetHostViewAndroid::SetScreenInfo(
    display::ScreenInfo screen_info) {
  screen_info_ = screen_info;
}

display::ScreenInfos
CustomScreenInfoRenderWidgetHostViewAndroid::GetScreenInfos() const {
  return display::ScreenInfos(screen_info_);
}

display::ScreenInfo CustomScreenInfoRenderWidgetHostViewAndroid::GetScreenInfo()
    const {
  return screen_info_;
}

std::string PostTestCaseName(const ::testing::TestParamInfo<bool>& info) {
  return info.param ? "FullscreenKillswitch" : "Default";
}

}  // namespace

class MockInputTransferHandler : public InputTransferHandlerAndroid {
 public:
  bool OnTouchEvent(const ui::MotionEventAndroid& event,
                    bool is_ignoring_input_events = false) override {
    return OnTouchEventImpl(event, is_ignoring_input_events);
  }

  MOCK_METHOD(bool,
              OnTouchEventImpl,
              (const ui::MotionEventAndroid& event,
               bool is_ignoring_input_events));

  MOCK_METHOD(bool,
              IsTouchSequencePotentiallyActiveOnViz,
              (),
              (const, override));
};

class MockMojoRenderInputRouterDelegate
    : public input::mojom::RenderInputRouterDelegate {
 public:
  MockMojoRenderInputRouterDelegate() = default;
  ~MockMojoRenderInputRouterDelegate() override = default;

  mojo::PendingAssociatedRemote<input::mojom::RenderInputRouterDelegate>
  GetPendingRemote() {
    return receiver_.BindNewEndpointAndPassDedicatedRemote();
  }

  MOCK_METHOD1(StateOnTouchTransfer,
               void(input::mojom::TouchTransferStatePtr state));
  MOCK_METHOD2(NotifySiteIsMobileOptimized,
               void(bool is_mobile_optimized,
                    const viz::FrameSinkId& frame_sink_id));
  MOCK_METHOD2(ForceEnableZoomStateChanged,
               void(bool force_enable_zoom,
                    const viz::FrameSinkId& frame_sink_id));
  MOCK_METHOD1(StopFlingingOnViz, void(const viz::FrameSinkId& frame_sink_id));
  MOCK_METHOD1(RestartInputEventAckTimeoutIfNecessary,
               void(const viz::FrameSinkId& frame_sink_id));
  MOCK_METHOD2(NotifyVisibilityChanged,
               void(const viz::FrameSinkId& frame_sink_id, bool is_hidden));
  MOCK_METHOD1(ResetGestureDetection,
               void(const viz::FrameSinkId& frame_sink_id));

 private:
  mojo::AssociatedReceiver<input::mojom::RenderInputRouterDelegate> receiver_{
      this};
};

class RenderWidgetHostViewAndroidTest : public RenderViewHostImplTestHarness {
 public:
  RenderWidgetHostViewAndroidTest();

  RenderWidgetHostViewAndroidTest(const RenderWidgetHostViewAndroidTest&) =
      delete;
  RenderWidgetHostViewAndroidTest& operator=(
      const RenderWidgetHostViewAndroidTest&) = delete;

  ~RenderWidgetHostViewAndroidTest() override {}

  RenderWidgetHostViewAndroid* render_widget_host_view_android() {
    return render_widget_host_view_android_;
  }

  viz::LocalSurfaceId GetLocalSurfaceIdAndConfirmNewerThan(
      viz::LocalSurfaceId other);

  MockRenderWidgetHostDelegate* delegate() { return delegate_.get(); }

  // Directly map to `RenderWidgetHostViewAndroid` methods.
  bool SynchronizeVisualProperties(
      const cc::DeadlinePolicy& deadline_policy,
      const std::optional<viz::LocalSurfaceId>& child_local_surface_id);
  void WasEvicted();
  ui::ViewAndroid* GetNativeView();
  void OnRenderFrameMetadataChangedAfterActivation(
      cc::RenderFrameMetadata metadata,
      base::TimeTicks activation_time);

  ui::ViewAndroid* GetParentView();

  cc::slim::Layer* GetParentLayer();

 protected:
  virtual RenderWidgetHostViewAndroid* CreateRenderWidgetHostViewAndroid(
      RenderWidgetHostImpl* widget_host);

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<MockRenderProcessHost> process_;
  scoped_refptr<SiteInstanceGroup> site_instance_group_;
  std::unique_ptr<MockRenderWidgetHostDelegate> delegate_;

  // TestRenderViewHost
  scoped_refptr<RenderViewHostImpl> render_view_host_;

  // Owned by `render_view_host_`.
  raw_ptr<MockRenderWidgetHost> host_ = nullptr;
  raw_ptr<RenderWidgetHostViewAndroid> render_widget_host_view_android_ =
      nullptr;

  // Of the parent of this RWHVA.
  ui::ViewAndroid parent_view_{ui::ViewAndroid::LayoutType::kNormal};
  scoped_refptr<cc::slim::Layer> parent_layer_;
};

RenderWidgetHostViewAndroidTest::RenderWidgetHostViewAndroidTest() {
  parent_layer_ = cc::slim::Layer::Create();
  parent_view_.SetLayer(cc::slim::Layer::Create());
  parent_view_.GetLayer()->AddChild(parent_layer_);
}

viz::LocalSurfaceId
RenderWidgetHostViewAndroidTest::GetLocalSurfaceIdAndConfirmNewerThan(
    viz::LocalSurfaceId other) {
  auto local_surface_id =
      render_widget_host_view_android()->GetLocalSurfaceId();
  EXPECT_NE(other, local_surface_id);
  EXPECT_TRUE(local_surface_id.is_valid());
  EXPECT_TRUE(local_surface_id.IsNewerThan(other));
  return local_surface_id;
}

bool RenderWidgetHostViewAndroidTest::SynchronizeVisualProperties(
    const cc::DeadlinePolicy& deadline_policy,
    const std::optional<viz::LocalSurfaceId>& child_local_surface_id) {
  return render_widget_host_view_android_->SynchronizeVisualProperties(
      deadline_policy, child_local_surface_id);
}

void RenderWidgetHostViewAndroidTest::WasEvicted() {
  render_widget_host_view_android_->WasEvicted();
}

ui::ViewAndroid* RenderWidgetHostViewAndroidTest::GetNativeView() {
  return render_widget_host_view_android_->GetNativeView();
}

void RenderWidgetHostViewAndroidTest::
    OnRenderFrameMetadataChangedAfterActivation(
        cc::RenderFrameMetadata metadata,
        base::TimeTicks activation_time) {
  render_widget_host_view_android()
      ->host()
      ->render_frame_metadata_provider()
      ->OnRenderFrameMetadataChangedAfterActivation(metadata, activation_time);
}

ui::ViewAndroid* RenderWidgetHostViewAndroidTest::GetParentView() {
  return &parent_view_;
}

cc::slim::Layer* RenderWidgetHostViewAndroidTest::GetParentLayer() {
  return parent_layer_.get();
}

RenderWidgetHostViewAndroid*
RenderWidgetHostViewAndroidTest::CreateRenderWidgetHostViewAndroid(
    RenderWidgetHostImpl* widget_host) {
  return new RenderWidgetHostViewAndroid(widget_host, &parent_view_,
                                         parent_layer_.get());
}

void RenderWidgetHostViewAndroidTest::SetUp() {
  RenderViewHostImplTestHarness::SetUp();

  delegate_ = std::make_unique<MockRenderWidgetHostDelegate>();
  process_ = std::make_unique<MockRenderProcessHost>(browser_context());
  site_instance_group_ = base::WrapRefCounted(
      SiteInstanceGroup::CreateForTesting(browser_context(), process_.get()));
  // Initialized before ownership is given to `render_view_host_`.
  std::unique_ptr<MockRenderWidgetHost> mock_host =
      MockRenderWidgetHost::Create(
          &contents()->GetPrimaryFrameTree(), delegate_.get(),
          site_instance_group_->GetSafeRef(), process_->GetNextRoutingID());
  host_ = mock_host.get();
  render_view_host_ = new TestRenderViewHost(
      &contents()->GetPrimaryFrameTree(), site_instance_group_.get(),
      contents()->GetSiteInstance()->GetStoragePartitionConfig(),
      std::move(mock_host), contents(), process_->GetNextRoutingID(),
      process_->GetNextRoutingID(), nullptr,
      CreateRenderViewHostCase::kDefault);

  render_widget_host_view_android_ = CreateRenderWidgetHostViewAndroid(host_);
}

void RenderWidgetHostViewAndroidTest::TearDown() {
  render_widget_host_view_android_->Destroy();
  render_view_host_.reset();

  delegate_.reset();
  process_->Cleanup();
  site_instance_group_.reset();
  process_ = nullptr;

  RenderViewHostImplTestHarness::TearDown();
}

// Tests that when a child responds to a Surface Synchronization message, while
// we are evicted, that we do not attempt to embed an invalid
// viz::LocalSurfaceId. This test should not crash.
TEST_F(RenderWidgetHostViewAndroidTest, NoSurfaceSynchronizationWhileEvicted) {
  // Android default host and views initialize as visible.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  EXPECT_TRUE(rwhva->IsShowing());
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());

  // Evicting while hidden should invalidate the current viz::LocalSurfaceId.
  rwhva->Hide();
  EXPECT_FALSE(rwhva->IsShowing());
  WasEvicted();
  EXPECT_FALSE(rwhva->GetLocalSurfaceId().is_valid());

  // When a child acknowledges a Surface Synchronization message, and has no new
  // properties to change, it responds with the original viz::LocalSurfaceId.
  // If we are evicted, we should not attempt to embed our invalid id. Nor
  // should we continue the synchronization process. This should not cause a
  // crash in DelegatedFrameHostAndroid.
  EXPECT_FALSE(SynchronizeVisualProperties(
      cc::DeadlinePolicy::UseDefaultDeadline(), initial_local_surface_id));
}

// Tests insetting the Visual Viewport.
TEST_F(RenderWidgetHostViewAndroidTest, InsetVisualViewport) {
  ui::TestViewAndroidDelegate test_view_android_delegate;
  // Android default viewport should not have an inset bottom.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  EXPECT_EQ(0, rwhva->GetNativeView()->GetViewportInsetBottom());

  // Set up SurfaceId checking.
  const viz::LocalSurfaceId original_local_surface_id =
      rwhva->GetLocalSurfaceId();

  // Set up our test delegate connected to this ViewAndroid.
  test_view_android_delegate.SetupTestDelegate(rwhva->GetNativeView());
  EXPECT_EQ(0, rwhva->GetNativeView()->GetViewportInsetBottom());

  JNIEnv* env = base::android::AttachCurrentThread();

  // Now inset the bottom and make sure the surface changes, and the inset is
  // known to our ViewAndroid.
  test_view_android_delegate.InsetViewportBottom(100);
  EXPECT_EQ(100, rwhva->GetNativeView()->GetViewportInsetBottom());
  rwhva->OnViewportInsetBottomChanged(env);
  viz::LocalSurfaceId inset_surface = rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(inset_surface.IsNewerThan(original_local_surface_id));

  // Reset the bottom; should go back to the original inset and have a new
  // surface.
  test_view_android_delegate.InsetViewportBottom(0);
  rwhva->OnViewportInsetBottomChanged(env);
  EXPECT_EQ(0, rwhva->GetNativeView()->GetViewportInsetBottom());
  EXPECT_TRUE(rwhva->GetLocalSurfaceId().IsNewerThan(inset_surface));
}

TEST_F(RenderWidgetHostViewAndroidTest, HideWindowRemoveViewAddViewShowWindow) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window->get()->AddChild(GetParentView());
  EXPECT_TRUE(render_widget_host_view_android()->IsShowing());
  // The layer should be visible once attached to a window.
  EXPECT_FALSE(render_widget_host_view_android()
                   ->GetNativeView()
                   ->GetLayer()
                   ->hide_layer_and_subtree());

  // Hiding the window should and removing the view should hide the layer.
  window->get()->OnVisibilityChanged(nullptr, false);
  GetParentView()->RemoveFromParent();
  EXPECT_TRUE(render_widget_host_view_android()->IsShowing());
  EXPECT_TRUE(render_widget_host_view_android()
                  ->GetNativeView()
                  ->GetLayer()
                  ->hide_layer_and_subtree());

  // Adding the view back to a window and notifying the window is visible should
  // make the layer visible again.
  window->get()->AddChild(GetParentView());
  window->get()->OnVisibilityChanged(nullptr, true);
  EXPECT_TRUE(render_widget_host_view_android()->IsShowing());
  EXPECT_FALSE(render_widget_host_view_android()
                   ->GetNativeView()
                   ->GetLayer()
                   ->hide_layer_and_subtree());
}

TEST_F(RenderWidgetHostViewAndroidTest, DisplayFeature) {
  ui::TestViewAndroidDelegate test_view_android_delegate;
  // By default there is no display feature so verify we get back null.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  RenderWidgetHostViewBase* rwhv = rwhva;
  rwhva->GetNativeView()->SetLayoutForTesting(0, 0, 200, 400);
  test_view_android_delegate.SetupTestDelegate(rwhva->GetNativeView());
  EXPECT_EQ(std::nullopt, rwhv->GetDisplayFeature());

  // Set a vertical display feature, and verify this is reflected in the
  // computed display feature.
  rwhva->SetDisplayFeatureBoundsForTesting(gfx::Rect(95, 0, 10, 400));
  DisplayFeature expected_display_feature = {
      DisplayFeature::Orientation::kVertical,
      /* offset */ 95,
      /* mask_length */ 10};
  EXPECT_EQ(expected_display_feature, *rwhv->GetDisplayFeature());

  // Validate that a display feature in the middle of the view results in not
  // being exposed as a content::DisplayFeature (we currently only consider
  // display features that completely cover one of the view's dimensions).
  rwhva->GetNativeView()->SetLayoutForTesting(0, 0, 400, 200);
  rwhva->SetDisplayFeatureBoundsForTesting(gfx::Rect(200, 100, 100, 200));
  EXPECT_EQ(std::nullopt, rwhv->GetDisplayFeature());

  // Verify that horizontal display feature is correctly validated.
  rwhva->SetDisplayFeatureBoundsForTesting(gfx::Rect(0, 90, 400, 20));
  expected_display_feature = {DisplayFeature::Orientation::kHorizontal,
                              /* offset */ 90,
                              /* mask_length */ 20};
  EXPECT_EQ(expected_display_feature, *rwhv->GetDisplayFeature());

  rwhva->SetDisplayFeatureBoundsForTesting(gfx::Rect(0, 95, 600, 10));
  expected_display_feature = {DisplayFeature::Orientation::kHorizontal,
                              /* offset */ 95,
                              /* mask_length */ 10};
  EXPECT_EQ(expected_display_feature, *rwhv->GetDisplayFeature());

  rwhva->SetDisplayFeatureBoundsForTesting(gfx::Rect(195, 0, 10, 300));
  expected_display_feature = {DisplayFeature::Orientation::kVertical,
                              /* offset */ 195,
                              /* mask_length */ 10};
  EXPECT_EQ(expected_display_feature, *rwhv->GetDisplayFeature());
}

// Tests that if a Renderer submits content before a navigation is completed,
// that we generate a new Surface. So that the old content cannot be used as a
// fallback.
TEST_F(RenderWidgetHostViewAndroidTest, RenderFrameSubmittedBeforeNavigation) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();

  // Creating a visible RWHVA should have a valid surface.
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());
  EXPECT_TRUE(rwhva->IsShowing());

  {
    // Simulate that the Renderer submitted some content to the current Surface
    // before Navigation concludes.
    cc::RenderFrameMetadata metadata;
    metadata.local_surface_id = initial_local_surface_id;
    OnRenderFrameMetadataChangedAfterActivation(metadata,
                                                base::TimeTicks::Now());
  }

  // Pre-commit information of Navigation is not told to RWHVA, these are the
  // two entry points. We should have a new Surface afterwards.
  rwhva->OnOldViewDidNavigatePreCommit();
  rwhva->DidNavigate();
  GetLocalSurfaceIdAndConfirmNewerThan(initial_local_surface_id);
}

// Test that InputTransferHandler receives input before FilteredGestureProvider.
// This is to prevent crash related to transferred events which stayed in
// TouchDispositionGestureFilter's queue, which it received through
// FilteredGestureProvider.
TEST_F(RenderWidgetHostViewAndroidTest,
       EventsPassedToInputTransferHandlerBeforedGestureProvider) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();

  MockInputTransferHandler* handler = new MockInputTransferHandler();
  rwhva->SetInputTransferHandlerForTesting(handler);

  auto& gesture_provider = rwhva->GetGestureProvider();

  gfx::Point point(/*x=*/100, /*y=*/100);
  ui::MotionEventAndroid::Pointer p(0, point.x(), point.y(), 10, 0, 0, 0, 0, 0);
  JNIEnv* env = base::android::AttachCurrentThread();
  auto time_ns = (ui::EventTimeForNow() - base::TimeTicks()).InNanoseconds();
  auto action = ui::MotionEvent::Action::DOWN;

  base::android::ScopedJavaLocalRef<jobject> obj =
      JNI_MotionEvent::Java_MotionEvent_obtain(
          env, /*downTime=*/0, /*eventTime=*/0, /*action=*/0, /*x=*/0, /*y=*/0,
          /*metaState=*/0);
  auto touch_down = ui::MotionEventAndroidFactory::CreateFromJava(
      env, obj,
      /*pix_to_dip=*/1.f,
      /*ticks_x=*/0,
      /*ticks_y=*/0,
      /*tick_multiplier=*/0,
      /*oldest_event_time=*/base::TimeTicks::FromJavaNanoTime(time_ns),
      /*android_action=*/ui::MotionEventAndroid::GetAndroidAction(action),
      /*pointer_count=*/1,
      /*history_size=*/0,
      /*action_index=*/0,
      /*android_action_button=*/0,
      /*android_gesture_classification=*/0,
      /*android_button_state=*/0,
      /*raw_offset_x_pixels=*/0,
      /*raw_offset_y_pixels=*/0,
      /*for_touch_handle=*/false,
      /*pointer0=*/&p,
      /*pointer1=*/nullptr);

  EXPECT_CALL(*handler, OnTouchEventImpl(_, _)).WillOnce(Return(true));
  EXPECT_EQ(gesture_provider.GetCurrentDownEvent(), nullptr);
  rwhva->OnTouchEvent(*touch_down);
  EXPECT_EQ(gesture_provider.GetCurrentDownEvent(), nullptr);

  EXPECT_CALL(*handler, OnTouchEventImpl(_, _)).WillOnce(Return(false));
  rwhva->OnTouchEvent(*touch_down);
  EXPECT_NE(gesture_provider.GetCurrentDownEvent(), nullptr);
}

TEST_F(RenderWidgetHostViewAndroidTest, ResetGestureDetectionGeneratesCancel) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();

  gfx::Point point(/*x=*/100, /*y=*/100);
  ui::MotionEventAndroid::Pointer p(0, point.x(), point.y(), 10, 0, 0, 0, 0, 0);
  JNIEnv* env = base::android::AttachCurrentThread();
  auto time_ns = (ui::EventTimeForNow() - base::TimeTicks()).InNanoseconds();
  auto action = ui::MotionEvent::Action::DOWN;

  base::android::ScopedJavaLocalRef<jobject> obj =
      JNI_MotionEvent::Java_MotionEvent_obtain(
          env, /*downTime=*/0, /*eventTime=*/0, /*action=*/0, /*x=*/0, /*y=*/0,
          /*metaState=*/0);
  auto touch_down = ui::MotionEventAndroidFactory::CreateFromJava(
      env, obj,
      /*pix_to_dip=*/1.f,
      /*ticks_x=*/0,
      /*ticks_y=*/0,
      /*tick_multiplier=*/0,
      /*oldest_event_time=*/base::TimeTicks::FromJavaNanoTime(time_ns),
      /*android_action=*/ui::MotionEventAndroid::GetAndroidAction(action),
      /*pointer_count=*/1,
      /*history_size=*/0,
      /*action_index=*/0,
      /*android_action_button=*/0,
      /*android_gesture_classification=*/0,
      /*android_button_state=*/0,
      /*raw_offset_x_pixels=*/0,
      /*raw_offset_y_pixels=*/0,
      /*for_touch_handle=*/false,
      /*pointer0=*/&p,
      /*pointer1=*/nullptr);
  rwhva->OnTouchEvent(*touch_down);

  auto& gesture_provider = rwhva->GetGestureProvider();
  EXPECT_NE(gesture_provider.GetCurrentDownEvent(), nullptr);

  rwhva->ResetGestureDetection();

  // The current down should have been reset as a result of processing cancel
  // generated from `ResetGestureDetection` call.
  EXPECT_EQ(gesture_provider.GetCurrentDownEvent(), nullptr);

  MockRenderWidgetHost* mock_widget =
      static_cast<MockRenderWidgetHost*>(rwhva->host());
  std::optional<blink::WebTouchEvent> touch_event =
      mock_widget->mock_render_input_router()
          ->GetAndResetLastForwardedTouchEvent();
  CHECK(touch_event.has_value());
  CHECK_EQ(touch_event->GetType(), blink::WebInputEvent::Type::kTouchCancel);
}

TEST_F(RenderWidgetHostViewAndroidTest, ResetGestureDetectionOnViz) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();

  MockInputTransferHandler* handler = new MockInputTransferHandler();
  rwhva->SetInputTransferHandlerForTesting(handler);

  MockMojoRenderInputRouterDelegate rir_delegate;
  rwhva->host()
      ->mojo_rir_delegate()
      ->SetRenderInputRouterDelegateRemoteForTesting(
          rir_delegate.GetPendingRemote());

  EXPECT_CALL(*handler, IsTouchSequencePotentiallyActiveOnViz())
      .WillOnce(Return(true));
  EXPECT_CALL(rir_delegate, ResetGestureDetection).Times(1);

  rwhva->ResetGestureDetection();

  base::RunLoop().RunUntilIdle();
}

// Tests that when an input sequence is handled on browser with InputVizard,
// browser sends a StopFlingingOnViz mojo call to VizCompositorThread.
TEST_F(RenderWidgetHostViewAndroidTest, StopFlingingOnViz) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();

  MockInputTransferHandler* handler = new MockInputTransferHandler();
  rwhva->SetInputTransferHandlerForTesting(handler);

  MockMojoRenderInputRouterDelegate rir_delegate;
  rwhva->host()
      ->mojo_rir_delegate()
      ->SetRenderInputRouterDelegateRemoteForTesting(
          rir_delegate.GetPendingRemote());

  gfx::Point point(/*x=*/100, /*y=*/100);
  ui::MotionEventAndroid::Pointer p(0, point.x(), point.y(), 10, 0, 0, 0, 0, 0);
  JNIEnv* env = base::android::AttachCurrentThread();
  auto time_ns = (ui::EventTimeForNow() - base::TimeTicks()).InNanoseconds();
  auto action = ui::MotionEvent::Action::DOWN;

  base::android::ScopedJavaLocalRef<jobject> obj1 =
      JNI_MotionEvent::Java_MotionEvent_obtain(
          env, /*downTime=*/0, /*eventTime=*/0, /*action=*/0, /*x=*/0, /*y=*/0,
          /*metaState=*/0);
  auto touch_down1 = ui::MotionEventAndroidFactory::CreateFromJava(
      env, obj1,
      /*pix_to_dip=*/1.f,
      /*ticks_x=*/0,
      /*ticks_y=*/0,
      /*tick_multiplier=*/0,
      /*oldest_event_time=*/base::TimeTicks::FromJavaNanoTime(time_ns),
      /*android_action=*/ui::MotionEventAndroid::GetAndroidAction(action),
      /*pointer_count=*/1,
      /*history_size=*/0,
      /*action_index=*/0,
      /*android_action_button=*/0,
      /*android_gesture_classification=*/0,
      /*android_button_state=*/0,
      /*raw_offset_x_pixels=*/0,
      /*raw_offset_y_pixels=*/0,
      /*for_touch_handle=*/false,
      /*pointer0=*/&p,
      /*pointer1=*/nullptr);

  EXPECT_CALL(*handler, OnTouchEventImpl(_, _)).WillOnce(Return(true));
  rwhva->OnTouchEvent(*touch_down1);

  time_ns = (ui::EventTimeForNow() - base::TimeTicks()).InNanoseconds();

  base::android::ScopedJavaLocalRef<jobject> obj2 =
      JNI_MotionEvent::Java_MotionEvent_obtain(
          env, /*downTime=*/0, /*eventTime=*/0, /*action=*/0, /*x=*/0, /*y=*/0,
          /*metaState=*/0);
  auto touch_down2 = ui::MotionEventAndroidFactory::CreateFromJava(
      env, obj2,
      /*pix_to_dip=*/1.f,
      /*ticks_x=*/0,
      /*ticks_y=*/0,
      /*tick_multiplier=*/0,
      /*oldest_event_time=*/base::TimeTicks::FromJavaNanoTime(time_ns),
      /*android_action=*/ui::MotionEventAndroid::GetAndroidAction(action),
      /*pointer_count=*/1,
      /*history_size=*/0,
      /*action_index=*/0,
      /*android_action_button=*/0,
      /*android_gesture_classification=*/0,
      /*android_button_state=*/0,
      /*raw_offset_x_pixels=*/0,
      /*raw_offset_y_pixels=*/0,
      /*for_touch_handle=*/false,
      /*pointer0=*/&p,
      /*pointer1=*/nullptr);

  EXPECT_CALL(*handler, OnTouchEventImpl(_, _)).WillOnce(Return(false));
  rwhva->OnTouchEvent(*touch_down2);
  // Expect a call to StopFlingingOnViz mojo method if the input sequence hasn't
  // been transferred to VizCompositorThread for handling.
  EXPECT_CALL(rir_delegate, StopFlingingOnViz).Times(1);
  base::RunLoop().RunUntilIdle();
}

TEST_F(RenderWidgetHostViewAndroidTest, UpdateControls) {
  float dip_scale = 1.0f;
  float top_height = 90.f;
  float top_ratio = 1.f;
  float top_min_height = 0.f;
  float bottom_height = 50.f;
  float bottom_ratio = 1.f;
  float bottom_min_height = 0.f;

  // Get the test view instance from the fixture.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();

  // 1. First call should return true as controls are uninitialized.
  EXPECT_TRUE(rwhva->UpdateControls(dip_scale, top_height, top_ratio,
                                    top_min_height, bottom_height, bottom_ratio,
                                    bottom_min_height));

  EXPECT_FALSE(rwhva->UpdateControls(dip_scale, top_height, top_ratio,
                                     top_min_height, bottom_height,
                                     bottom_ratio, bottom_min_height));

  // 3. Change top_controls_height.
  float new_top_height = 100.f;
  EXPECT_TRUE(rwhva->UpdateControls(dip_scale, new_top_height, top_ratio,
                                    top_min_height, bottom_height, bottom_ratio,
                                    bottom_min_height));
  // Call again with same values, should return false.
  EXPECT_FALSE(rwhva->UpdateControls(dip_scale, new_top_height, top_ratio,
                                     top_min_height, bottom_height,
                                     bottom_ratio, bottom_min_height));

  // 4. Change top_controls_shown_ratio.
  float new_top_ratio = 0.5f;
  EXPECT_TRUE(rwhva->UpdateControls(dip_scale, new_top_height, new_top_ratio,
                                    top_min_height, bottom_height, bottom_ratio,
                                    bottom_min_height));
  EXPECT_FALSE(rwhva->UpdateControls(dip_scale, new_top_height, new_top_ratio,
                                     top_min_height, bottom_height,
                                     bottom_ratio, bottom_min_height));

  // 5. Change top_controls_min_height_offset.
  float new_top_min_height = 10.f;
  EXPECT_TRUE(rwhva->UpdateControls(dip_scale, new_top_height, new_top_ratio,
                                    new_top_min_height, bottom_height,
                                    bottom_ratio, bottom_min_height));
  // Call again with same values, should return false.
  EXPECT_FALSE(rwhva->UpdateControls(dip_scale, new_top_height, new_top_ratio,
                                     new_top_min_height, bottom_height,
                                     bottom_ratio, bottom_min_height));

  // 6. Change bottom_controls_shown_ratio.
  float new_bottom_ratio = 0.0f;
  EXPECT_TRUE(rwhva->UpdateControls(dip_scale, new_top_height, new_top_ratio,
                                    new_top_min_height, bottom_height,
                                    new_bottom_ratio, bottom_min_height));

  EXPECT_FALSE(rwhva->UpdateControls(dip_scale, new_top_height, new_top_ratio,
                                     new_top_min_height, bottom_height,
                                     new_bottom_ratio, bottom_min_height));

  // 7. Change bottom_controls_height while at 0% shown ratio.
  float new_bottom_height = 60.f;
  EXPECT_TRUE(rwhva->UpdateControls(dip_scale, new_top_height, new_top_ratio,
                                    new_top_min_height, new_bottom_height,
                                    new_bottom_ratio, bottom_min_height));
  EXPECT_FALSE(rwhva->UpdateControls(dip_scale, new_top_height, new_top_ratio,
                                     new_top_min_height, new_bottom_height,
                                     new_bottom_ratio, bottom_min_height));

  // 8. Change bottom_controls_min_height_offset.
  float new_bottom_min_height = 10.f;
  EXPECT_TRUE(rwhva->UpdateControls(dip_scale, new_top_height, new_top_ratio,
                                    new_top_min_height, new_bottom_height,
                                    new_bottom_ratio, new_bottom_min_height));
  EXPECT_FALSE(rwhva->UpdateControls(dip_scale, new_top_height, new_top_ratio,
                                     new_top_min_height, new_bottom_height,
                                     new_bottom_ratio, new_bottom_min_height));
}

// Test for scaling.
class RenderWidgetHostViewAndroidScalingTest
    : public RenderWidgetHostViewAndroidTest {
 public:
  RenderWidgetHostViewAndroidScalingTest() = default;
  ~RenderWidgetHostViewAndroidScalingTest() override = default;

  void SetScreenInfo(display::ScreenInfo screen_info) {
    static_cast<CustomScreenInfoRenderWidgetHostViewAndroid*>(
        render_widget_host_view_android())
        ->SetScreenInfo(screen_info);
  }

  void OnPhysicalBackingSizeChanged(const gfx::Size& size) {
    render_widget_host_view_android()
        ->GetNativeView()
        ->OnPhysicalBackingSizeChanged(size);
  }

  void OnVisibleViewportSizeChanged(int width, int height) {
    GetParentView()->OnSizeChanged(width, height);
  }

 protected:
  RenderWidgetHostViewAndroid* CreateRenderWidgetHostViewAndroid(
      RenderWidgetHostImpl* widget_host) override {
    return new CustomScreenInfoRenderWidgetHostViewAndroid(
        widget_host, GetParentView(), GetParentLayer());
  }
};

TEST_F(RenderWidgetHostViewAndroidScalingTest, UpdateOverrideScale) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  ui::ViewAndroid* view = rwhva->GetNativeView();

  const gfx::Size view_size_dip(100, 200);
  const gfx::Size view_size_px =
      ScaleToFlooredSize(view_size_dip, view->GetDipScale());
  OnVisibleViewportSizeChanged(view_size_dip.width(), view_size_dip.height());

  const gfx::Size backing_size_px(200, 400);
  OnPhysicalBackingSizeChanged(backing_size_px);
  EXPECT_EQ(backing_size_px, rwhva->GetCompositorViewportPixelSize());
  EXPECT_EQ(view_size_dip, rwhva->GetRequestedRendererSize());
  EXPECT_EQ(view_size_px, rwhva->GetRequestedRendererSizeDevicePx());
  EXPECT_EQ(view_size_dip, rwhva->GetVisibleViewportSize());
  EXPECT_EQ(view_size_px, rwhva->GetVisibleViewportSizeDevicePx());

  display::ScreenInfo screen_info;
  screen_info.device_scale_factor = 3.0f;
  SetScreenInfo(screen_info);
  EXPECT_EQ(3.0f, rwhva->GetDeviceScaleFactor());

  const gfx::Size scaled_view_size_px = ScaleToFlooredSize(
      view_size_dip, view->GetDipScale() * screen_info.device_scale_factor);
  const gfx::Size scaled_backing_size_px =
      ScaleToFlooredSize(backing_size_px, screen_info.device_scale_factor);
  EXPECT_EQ(scaled_backing_size_px, rwhva->GetCompositorViewportPixelSize());
  EXPECT_EQ(view_size_dip, rwhva->GetRequestedRendererSize());
  EXPECT_EQ(scaled_view_size_px, rwhva->GetRequestedRendererSizeDevicePx());
  EXPECT_EQ(view_size_dip, rwhva->GetVisibleViewportSize());
  EXPECT_EQ(scaled_view_size_px, rwhva->GetVisibleViewportSizeDevicePx());
}

// Tests rotation and fullscreen cases that are supported by visual properties
// analysis. Some of which fail with the fullscreen killswitch legacy path.
//
// Initializes to Portrait.
class RenderWidgetHostViewAndroidRotationTest
    : public RenderWidgetHostViewAndroidTest {
 public:
  RenderWidgetHostViewAndroidRotationTest() = default;
  ~RenderWidgetHostViewAndroidRotationTest() override = default;

  // If `rotation` is false this will be treated as an initialization. Same as
  // the first notifications after browser launch.
  void SetPortraitScreenInfo(bool rotation);
  void SetLandscapeScreenInfo(bool rotation);

  // From default portrait oriention to fullscreen with no rotation. Returns
  // resultant viz::LocalSurfaceId.
  viz::LocalSurfaceId PortraitToFullscreenPortrait();

  // From default portrait orientation to fullscreen with an orientation lock to
  // landscape applied. Triggering a rotation. Returns resultant
  // viz::LocalSurfaceId.
  viz::LocalSurfaceId PortraitToFullscreenLanscape();

  // Fires the rotation throttle timeout.
  void FireRotationTimeout();
  // Firet the fullscreen throttle timeout.
  void FireFullscreenTimeout();

  // RenderWidgetHostViewAndroid:
  void EnterFullscreenMode();
  void ExitFullscreenMode();
  void LockOrientation(device::mojom::ScreenOrientationLockType orientation);
  void UnlockOrientation();
  void TogglePictureInPicture(bool enabled);
  void OnDidUpdateVisualPropertiesComplete(
      const cc::RenderFrameMetadata& metadata);
  void SetScreenInfo(display::ScreenInfo screen_info);

  // ViewAndroid:
  void OnPhysicalBackingSizeChanged(const gfx::Size& size);
  void OnVisibleViewportSizeChanged(int width, int height);

  const gfx::Size fullscreen_landscape_physical_backing = gfx::Size(800, 600);
  const gfx::Size fullscreen_portrait_physical_backing = gfx::Size(600, 800);
  const gfx::Size landscape_physical_backing = gfx::Size(800, 590);
  const gfx::Size portrait_physical_backing = gfx::Size(600, 790);

 protected:
  RenderWidgetHostViewAndroid* CreateRenderWidgetHostViewAndroid(
      RenderWidgetHostImpl* widget_host) override;

  // testing::Test:
  void SetUp() override;
};

void RenderWidgetHostViewAndroidRotationTest::SetPortraitScreenInfo(
    bool rotation) {
  display::ScreenInfo screen_info;
  screen_info.display_id = display::kDefaultDisplayId;
  screen_info.orientation_type =
      display::mojom::ScreenOrientation::kPortraitPrimary;
  screen_info.orientation_angle = 0;
  screen_info.rect = gfx::Rect(0, 0, 300, 400);
  SetScreenInfo(screen_info);
  render_widget_host_view_android()->OnSynchronizedDisplayPropertiesChanged(
      rotation);
}

void RenderWidgetHostViewAndroidRotationTest::SetLandscapeScreenInfo(
    bool rotation) {
  display::ScreenInfo screen_info;
  screen_info.display_id = display::kDefaultDisplayId;
  screen_info.orientation_type =
      display::mojom::ScreenOrientation::kLandscapePrimary;
  screen_info.orientation_angle = 90;
  screen_info.rect = gfx::Rect(0, 0, 400, 300);

  SetScreenInfo(screen_info);
  render_widget_host_view_android()->OnSynchronizedDisplayPropertiesChanged(
      rotation);
}

viz::LocalSurfaceId
RenderWidgetHostViewAndroidRotationTest::PortraitToFullscreenPortrait() {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());

  // When we enter fullscreen mode we can't sync until we get a non-rotation
  // change to sizes.
  EnterFullscreenMode();
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());

  // The non-rotation change to the `visible_viewport_size` occurs when system
  // controls are hidden. We need to sync this to the Renderer. As we will not
  // know if a rotation is to follow.
  OnVisibleViewportSizeChanged(300, 400);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  OnPhysicalBackingSizeChanged(fullscreen_portrait_physical_backing);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  return GetLocalSurfaceIdAndConfirmNewerThan(initial_local_surface_id);
}

viz::LocalSurfaceId
RenderWidgetHostViewAndroidRotationTest::PortraitToFullscreenLanscape() {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());

  // When we enter fullscreen mode we can't sync until we get a non-rotation
  // change to sizes.
  EnterFullscreenMode();
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  LockOrientation(device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());

  // When we expect a rotation we will not advance the viz::LocalSurfaceId
  // ourselves until rotation has completed. However we will not block it's
  // advancement from other sources. As `visible_viewport_size` follows a
  // non-synchronized path and blocking other sync paths can lead to incorrect
  // surface size submissions.
  OnVisibleViewportSizeChanged(300, 400);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // We do throttle once rotation starts
  SetLandscapeScreenInfo(/*rotation=*/true);
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  OnVisibleViewportSizeChanged(400, 300);
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // We stop throttling once rotation ends
  OnPhysicalBackingSizeChanged(fullscreen_landscape_physical_backing);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  return GetLocalSurfaceIdAndConfirmNewerThan(initial_local_surface_id);
}

void RenderWidgetHostViewAndroidRotationTest::FireRotationTimeout() {
  render_widget_host_view_android()->rotation_timeout_.FireNow();
}

void RenderWidgetHostViewAndroidRotationTest::FireFullscreenTimeout() {
  render_widget_host_view_android()
      ->screen_state_change_handler_.throttle_timeout_.FireNow();
}

void RenderWidgetHostViewAndroidRotationTest::EnterFullscreenMode() {
  blink::mojom::FullscreenOptions options;
  render_widget_host_view_android()->EnterFullscreenMode(options);
  delegate()->set_is_fullscreen(true);
}

void RenderWidgetHostViewAndroidRotationTest::ExitFullscreenMode() {
  render_widget_host_view_android()->ExitFullscreenMode();
  delegate()->set_is_fullscreen(false);
}

void RenderWidgetHostViewAndroidRotationTest::LockOrientation(
    device::mojom::ScreenOrientationLockType orientation) {
  render_widget_host_view_android()->LockOrientation(orientation);
}

void RenderWidgetHostViewAndroidRotationTest::UnlockOrientation() {
  render_widget_host_view_android()->UnlockOrientation();
}

void RenderWidgetHostViewAndroidRotationTest::TogglePictureInPicture(
    bool enabled) {
  render_widget_host_view_android()->SetHasPersistentVideo(enabled);
}

void RenderWidgetHostViewAndroidRotationTest::
    OnDidUpdateVisualPropertiesComplete(
        const cc::RenderFrameMetadata& metadata) {
  render_widget_host_view_android()->OnDidUpdateVisualPropertiesComplete(
      metadata);
}

void RenderWidgetHostViewAndroidRotationTest::SetScreenInfo(
    display::ScreenInfo screen_info) {
  static_cast<CustomScreenInfoRenderWidgetHostViewAndroid*>(
      render_widget_host_view_android())
      ->SetScreenInfo(screen_info);
}

void RenderWidgetHostViewAndroidRotationTest::OnPhysicalBackingSizeChanged(
    const gfx::Size& size) {
  render_widget_host_view_android()->view_.OnPhysicalBackingSizeChanged(size);
}

void RenderWidgetHostViewAndroidRotationTest::OnVisibleViewportSizeChanged(
    int width,
    int height) {
  // Change the size via the parent native view. `RenderWidgetHostViewAndroid`
  // has `LayoutType` set to `MATCH_PARENT` so can't receive its own
  // `OnSizeChanged`.
  GetParentView()->OnSizeChanged(width, height);
}

RenderWidgetHostViewAndroid*
RenderWidgetHostViewAndroidRotationTest::CreateRenderWidgetHostViewAndroid(
    RenderWidgetHostImpl* widget_host) {
  return new CustomScreenInfoRenderWidgetHostViewAndroid(
      widget_host, GetParentView(), GetParentLayer());
}

void RenderWidgetHostViewAndroidRotationTest::SetUp() {
  RenderWidgetHostViewAndroidTest::SetUp();
  // Set initial state to Portrait
  SetPortraitScreenInfo(/*rotation=*/false);
  OnPhysicalBackingSizeChanged(portrait_physical_backing);
  OnVisibleViewportSizeChanged(300, 395);
}

// Tests transition from Portrait orientation to fullscreen, with no rotation.
// Along with exiting back to Portrait.
TEST_F(RenderWidgetHostViewAndroidRotationTest, PortraitOnlyFullscreen) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  const viz::LocalSurfaceId post_fullscreen_local_surface_id =
      PortraitToFullscreenPortrait();

  // When we exit fullscreen mode we don't throttle.
  ExitFullscreenMode();
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());

  // The non-rotation change to the `visible_viewport_size` occurs when system
  // controls are made visible again. We need to sync this to the Renderer. We
  // do not know if a rotation is to follow.
  OnVisibleViewportSizeChanged(300, 395);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  OnPhysicalBackingSizeChanged(portrait_physical_backing);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  GetLocalSurfaceIdAndConfirmNewerThan(post_fullscreen_local_surface_id);
}

// Tests the transition from Portrait to Fullscreen where a rotation lock to
// Landscape is applied. Followed by a return to the original configuration.
TEST_F(RenderWidgetHostViewAndroidRotationTest,
       PortraitToLandscapeRotationLockFullscreenRotation) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  const viz::LocalSurfaceId post_fullscreen_local_surface_id =
      PortraitToFullscreenLanscape();

  // When we exit fullscreen mode we can't be sure if we are going to rotate or
  // not. We don't throttle.
  ExitFullscreenMode();
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  UnlockOrientation();

  // When unthrottle upon the receipt of a new `visible_viewport_size`. Even if
  // it is a rotation there is no guarantee that we will receive paired physical
  // backing or screen info. When exiting from Landscape Fullscreen to
  // Landscape, this will be the last signal.
  OnVisibleViewportSizeChanged(300, 395);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());

  // When the other properties signal a rotation, throttle again.
  OnPhysicalBackingSizeChanged(portrait_physical_backing);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  SetPortraitScreenInfo(/*rotation=*/true);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  GetLocalSurfaceIdAndConfirmNewerThan(post_fullscreen_local_surface_id);
}

// Tests that if we receive an UnlockOrientation that we cancel any rotation
// throttle. As we may not receive a paired result, and may end up reversing the
// rotation.
TEST_F(RenderWidgetHostViewAndroidRotationTest,
       UnlockOrientationCancelsRotationThrottle) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());

  // When we enter fullscreen mode we can't sync until we get a non-rotation
  // change to sizes.
  EnterFullscreenMode();
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  LockOrientation(device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());

  // When we expect a rotation we will not advance the viz::LocalSurfaceId
  // ourselves until rotation has completed. However we will not block it's
  // advancement from other sources. As `visible_viewport_size` follows a
  // non-synchronized path and blocking other sync paths can lead to incorrect
  // surface size submissions.
  OnVisibleViewportSizeChanged(300, 400);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // We do throttle once rotation starts
  SetLandscapeScreenInfo(/*rotation=*/true);
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  OnVisibleViewportSizeChanged(400, 300);
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // We we unlock the rotation should be canceled, and we should sync again.
  UnlockOrientation();
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  GetLocalSurfaceIdAndConfirmNewerThan(initial_local_surface_id);
}

// Previously we had an ordering expectation that ScreenInfo would update for a
// rotation before Physical Backing. This is not a guarantee, and was locking us
// in rotation throttle. Ensure we handle this ordering.
TEST_F(RenderWidgetHostViewAndroidRotationTest,
       FullscreenRotationPhysicalBackingFirst) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());

  // When we enter fullscreen mode we can't sync until we get a non-rotation
  // change to sizes.
  EnterFullscreenMode();
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  LockOrientation(device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());

  // When we expect a rotation we will not advance the viz::LocalSurfaceId
  // ourselves until rotation has completed. However we will not block it's
  // advancement from other sources. As `visible_viewport_size` follows a
  // non-synchronized path and blocking other sync paths can lead to incorrect
  // surface size submissions.
  OnVisibleViewportSizeChanged(300, 400);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // We should throttle even when receiving a rotated physical backing before
  // any ScreenInfo.
  OnPhysicalBackingSizeChanged(fullscreen_landscape_physical_backing);
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  OnVisibleViewportSizeChanged(400, 300);
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // Receiving the rotated ScreenInfo should complete the rotation and end
  // throttling.
  SetLandscapeScreenInfo(/*rotation=*/true);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  GetLocalSurfaceIdAndConfirmNewerThan(initial_local_surface_id);
}

// Tests entering Picture-in-Picture from a fullscreen Portrait context, then
// returning back to the same Portrait state.
TEST_F(RenderWidgetHostViewAndroidRotationTest,
       PortraitPictureInPictureToPortrait) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  const viz::LocalSurfaceId post_fullscreen_local_surface_id =
      PortraitToFullscreenPortrait();

  // Entering Picture-in-Picture should not throttle.
  TogglePictureInPicture(/*enabled=*/true);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());

  // Each subsequent visual property change just synchronizes.
  OnVisibleViewportSizeChanged(150, 200);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_visual_viewport_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(post_fullscreen_local_surface_id);
  OnPhysicalBackingSizeChanged(gfx::Size(300, 400));
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_physical_backing_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(
          post_visual_viewport_local_surface_id);

  // Exiting Picture-in-Picutre should not throttle either.
  TogglePictureInPicture(/*enabled=*/false);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());

  // Each subsequent visual property change just synchronizes.
  OnVisibleViewportSizeChanged(300, 400);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_exit_visual_viewport_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(
          post_physical_backing_local_surface_id);
  OnPhysicalBackingSizeChanged(fullscreen_portrait_physical_backing);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  GetLocalSurfaceIdAndConfirmNewerThan(
      post_exit_visual_viewport_local_surface_id);
}

// Tests that Picture-in-Picture from a fullscreen Portrait context, then
// returning to a Landscape state does not throttle the rotation.
TEST_F(RenderWidgetHostViewAndroidRotationTest,
       PortraitPictureInPictureToLandscape) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  const viz::LocalSurfaceId post_fullscreen_local_surface_id =
      PortraitToFullscreenPortrait();

  // Entering Picture-in-Picture should not throttle.
  TogglePictureInPicture(/*enabled=*/true);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());

  // Each subsequent visual property change just synchronizes.
  OnVisibleViewportSizeChanged(150, 200);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_visual_viewport_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(post_fullscreen_local_surface_id);
  OnPhysicalBackingSizeChanged(gfx::Size(300, 400));
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_physical_backing_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(
          post_visual_viewport_local_surface_id);

  // We can be notified of new screen info before exiting. However being in
  // Picture-in-Picture should not throttle.
  SetLandscapeScreenInfo(/*rotation=*/true);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  // Exiting Picture-in-Picutre should not throttle either.
  TogglePictureInPicture(/*enabled=*/false);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());

  // Even a rotation while exiting should not throttle.
  OnVisibleViewportSizeChanged(400, 300);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_exit_visual_viewport_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(
          post_physical_backing_local_surface_id);
  OnPhysicalBackingSizeChanged(fullscreen_landscape_physical_backing);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_exit_physical_backing_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(
          post_exit_visual_viewport_local_surface_id);
  SetLandscapeScreenInfo(/*rotation=*/true);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  GetLocalSurfaceIdAndConfirmNewerThan(
      post_exit_physical_backing_local_surface_id);
}

// When transitioning from a Landscape Fullscreen to Picture-in-Picture we end
// up in a mixed layout state. With Landscape `visual_viewport_size` and
// physical backing, and a Portrait ScreenInfo. We should not start a rotation
// throttle during this.
TEST_F(RenderWidgetHostViewAndroidRotationTest,
       LandscapeFullscreenToMixedPictureInPicture) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  const viz::LocalSurfaceId post_fullscreen_local_surface_id =
      PortraitToFullscreenLanscape();

  // Entering Picture-in-Picture should not throttle.
  TogglePictureInPicture(/*enabled=*/true);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());

  // Each subsequent visual property change just synchronizes.
  OnVisibleViewportSizeChanged(200, 150);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_visual_viewport_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(post_fullscreen_local_surface_id);
  OnPhysicalBackingSizeChanged(gfx::Size(400, 300));
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_physical_backing_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(
          post_visual_viewport_local_surface_id);

  // Normally the rotation of ScreenInfo would trigger throttling. However in
  // Picture-in-Picture mode we can end up with mixed orientations. So we do not
  // throttle.
  SetPortraitScreenInfo(/*rotation=*/true);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  GetLocalSurfaceIdAndConfirmNewerThan(post_physical_backing_local_surface_id);
}

// Tests that when Picture-in-Picture mode is closed, that we do not throttle
// becoming hidden, or returning to visibility.
TEST_F(RenderWidgetHostViewAndroidRotationTest, PictureInPictureCloses) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  const viz::LocalSurfaceId post_fullscreen_local_surface_id =
      PortraitToFullscreenLanscape();

  // ScreenInfo to rotate to Portrait for Picture-in-Picture can arrive first.
  // This rotation is throttled and subsequent Picture-in-Picture mode toggles.
  SetPortraitScreenInfo(/*rotation=*/true);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  // Entering Picture-in-Picture should not throttle.
  TogglePictureInPicture(/*enabled=*/true);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());

  // Each subsequent visual property change just synchronizes.
  OnVisibleViewportSizeChanged(200, 150);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_visual_viewport_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(post_fullscreen_local_surface_id);
  OnPhysicalBackingSizeChanged(gfx::Size(400, 300));
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_physical_backing_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(
          post_visual_viewport_local_surface_id);

  // Closing Picture-in-Picture will first hide the view, and exit fullscreen.
  // Hiding should not lead to throttle
  rwhva->Hide();
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());

  // Exiting Fullscreen should not throttle.
  ExitFullscreenMode();
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());

  // Exiting Picture-in-Picutre should not throttle even if rotating back to
  // Portrait.
  TogglePictureInPicture(/*enabled=*/false);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  OnVisibleViewportSizeChanged(300, 400);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId hidden_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(
          post_physical_backing_local_surface_id);

  // No throttling when showing again, nor for completing the hidden rotation
  rwhva->Show();
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  OnPhysicalBackingSizeChanged(portrait_physical_backing);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  GetLocalSurfaceIdAndConfirmNewerThan(hidden_local_surface_id);
}

// Tests that when we are evicted while waiting for fullscreen transition, that
// we stop throttling, and can successfully re-embed later.
TEST_F(RenderWidgetHostViewAndroidRotationTest, FullscreenEviction) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  auto local_surface_id = rwhva->GetLocalSurfaceId();
  EnterFullscreenMode();
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());

  // When we are evicted while hidden, the viz::LocalSurfaceId should be
  // invalidated, and we should no longer throttle syncrhonizing.
  rwhva->Hide();
  rwhva->WasEvicted();
  auto evicted_local_surface_id = rwhva->GetLocalSurfaceId();
  EXPECT_FALSE(evicted_local_surface_id.is_valid());
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());

  // This shouldn't crash
  rwhva->ShowWithVisibility(blink::mojom::PageVisibilityState::kVisible);
  // We should also have a new viz::LocalSurfaceId to become embedded again.
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  GetLocalSurfaceIdAndConfirmNewerThan(local_surface_id);
}

// Tests that when Android fakes visibility to start a rotation, before hiding
// and completing the rotation later, that the rotation timeout and subsequent
// actual visibility change correctly updates the viz::LocalSurfaceId and stops
// all throttling. (https://crbug.com/1383446)
TEST_F(RenderWidgetHostViewAndroidRotationTest, FakeVisibilityScreenRotation) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  auto local_surface_id = rwhva->GetLocalSurfaceId();

  // Portrait orientation split screen. Same width, reduce height by half but
  // keep inset for system status bar. This should not throttle, but should
  // advance the viz::LocalSurfaceId
  OnVisibleViewportSizeChanged(300, 195);
  OnPhysicalBackingSizeChanged(gfx::Size(600, 390));
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  auto split_screen_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(local_surface_id);

  // Rotate device to landscape orientation
  SetLandscapeScreenInfo(/*rotation=*/true);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  OnVisibleViewportSizeChanged(200, 295);
  OnPhysicalBackingSizeChanged(gfx::Size(400, 590));
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  auto post_rotation_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(split_screen_local_surface_id);

  // Hiding should not change throttle
  rwhva->Hide();
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());

  // When turning off the screen some versions of Android lie. They set us
  // visible even with the screen off, and rotate the ScreenInfo, but nothing
  // else. Followed up by hiding us again.
  rwhva->Show();
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  SetPortraitScreenInfo(/*rotation=*/true);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  rwhva->Hide();

  // When off the timeout can fire. It should clear the throttling and advance
  // the viz::LocalSurfaceId
  FireRotationTimeout();
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  auto post_timeout_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(post_rotation_local_surface_id);

  // On some versions of Android the new post-rotation layout can be sent before
  // we become visible.
  OnVisibleViewportSizeChanged(300, 195);
  OnPhysicalBackingSizeChanged(gfx::Size(600, 390));
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  auto post_hidden_rotation_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(post_timeout_local_surface_id);

  // When becoming visible we should have the correct layout already and not
  // need to advance the viz::LocalSurfaceId. We should also not be throttling.
  rwhva->Show();
  auto post_show_local_surface_id = rwhva->GetLocalSurfaceId();
  EXPECT_EQ(post_show_local_surface_id, post_hidden_rotation_local_surface_id);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
}

// Tests that when toggling FullscreenMode, where no layout changes occur, that
// we unthrottle and advance the viz::LocalSurfaceId after each step.
TEST_F(RenderWidgetHostViewAndroidRotationTest, ToggleFullscreenWithoutResize) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  auto local_surface_id = rwhva->GetLocalSurfaceId();
  EnterFullscreenMode();
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());

  // When there has been no resize triggered the timeout can fire. It should
  // clear throttling and advance the viz::LocalSurfaceId;
  FireFullscreenTimeout();
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  auto post_timeout_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(local_surface_id);

  ExitFullscreenMode();
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  auto post_fullscreen_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(post_timeout_local_surface_id);

  // When we re-enter fullscreen we should throttle again.
  EnterFullscreenMode();
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());

  // The timeout should once again unthrottle and advance the
  // viz::LocalSurfaceId.
  FireFullscreenTimeout();
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  GetLocalSurfaceIdAndConfirmNewerThan(post_fullscreen_local_surface_id);
}

TEST_F(RenderWidgetHostViewAndroidRotationTest,
       FullscreenEvictionWithoutAnySizeChanged) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  // When we are evicted while hidden, the viz::LocalSurfaceId should be
  // invalidated, and we should no longer throttle synchronizing.
  rwhva->Hide();
  rwhva->WasEvicted();
  EXPECT_FALSE(rwhva->GetLocalSurfaceId().is_valid());
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());

  EnterFullscreenMode();
  // Entering fullscreen mode without `any_non_rotation_size_changed` blocks
  // synchronizing.
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());

  // Here we have web page in background and in fullscreen
  // with invalid surface and without ability to synchronizing.
  // This shouldn't crash. And should generate new surface to bring web in
  // visible state.
  rwhva->ShowWithVisibility(blink::mojom::PageVisibilityState::kVisible);
  EXPECT_TRUE(rwhva->GetLocalSurfaceId().is_valid());
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
}

// Tests rotation and fullscreen cases that are supported by both the visual
// properties analysis, and the fullscreen killswitch legacy path.
//
// Initializes to Portrait.
class RenderWidgetHostViewAndroidRotationKillswitchTest
    : public RenderWidgetHostViewAndroidRotationTest,
      public testing::WithParamInterface<bool> {
 public:
  RenderWidgetHostViewAndroidRotationKillswitchTest() = default;
  ~RenderWidgetHostViewAndroidRotationKillswitchTest() override = default;
};

// Tests that when a rotation occurs, that we only advance the
// viz::LocalSurfaceId once, and that no other visual changes occurring during
// this time can separately trigger SurfaceSync. (https://crbug.com/1203804)
TEST_F(RenderWidgetHostViewAndroidRotationKillswitchTest,
       RotationOnlyAdvancesSurfaceSyncOnce) {
  // Android default host and views initialize as visible.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  EXPECT_TRUE(rwhva->IsShowing());
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());

  // When rotation has started we should not be performing Surface Sync. The
  // viz::LocalSurfaceId should not have advanced.
  SetLandscapeScreenInfo(/*rotation=*/true);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // When rotation has completed we should begin Surface Sync again. There
  // should also be a new viz::LocalSurfaceId.
  OnPhysicalBackingSizeChanged(landscape_physical_backing);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  GetLocalSurfaceIdAndConfirmNewerThan(initial_local_surface_id);
}

// Tests that when a rotation occurs while the view is hidden, that we advance
// the viz::LocalSurfaceId upon becoming visible again. Then once rotation has
// completed we update again, and unblock all other visual changes.
// (https://crbug.com/1223299)
TEST_P(RenderWidgetHostViewAndroidRotationKillswitchTest,
       HiddenRotationDisplaysImmediately) {
  // Android default host and views initialize as visible.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  EXPECT_TRUE(rwhva->IsShowing());
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());

  rwhva->Hide();

  // When rotation has started we should not be performing Surface Sync. The
  // viz::LocalSurfaceId should not have advanced.
  SetLandscapeScreenInfo(/*rotation=*/true);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // When rotation occurs while hidden we will only have a partial state.
  // However we do not want to delay showing content until Android tells us of
  // the final state. So we advance the viz::LocalSurfaceId to have the newest
  // possible content ready.
  rwhva->Show();
  GetLocalSurfaceIdAndConfirmNewerThan(initial_local_surface_id);
  // We do not block synchronization, as there is no platform consistency in
  // resize messages when becoming visible.
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
}

// Test that when the view is hidden, and another enters Fullscreen, that we
// complete the advancement of the viz::LocalSurfaceId upon becoming visible
// again. Since the Fullscreen flow does not trigger
// OnPhysicalBackingSizeChanged, we will continue to block further Surface Sync
// until the post rotation frame has been generated. (https://crbug.com/1223299)
TEST_P(RenderWidgetHostViewAndroidRotationKillswitchTest,
       HiddenPartialRotationFromFullscreen) {
  // Android default host and views initialize as visible.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  EXPECT_TRUE(rwhva->IsShowing());
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());

  rwhva->Hide();

  // When another view enters Fullscreen, all hidden views are notified of the
  // start of rotation. We should not be performing Surface Sync. The
  // viz::LocalSurfaceId should not have advanced.
  SetLandscapeScreenInfo(/*rotation= */ true);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // When the other view exits Fullscreen, all hidden views are notified of a
  // rotation back to the original orientation. We should continue in the same
  // state.
  SetPortraitScreenInfo(/*rotation=*/true);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // When the cause of rotation is Fullscreen, we will NOT receive a call to
  // OnPhysicalBackingSizeChanged. Due to this we advance the
  // viz::LocalSurfaceId upon becoming visible, to send all visual updates to
  // the Renderer.
  rwhva->Show();
  GetLocalSurfaceIdAndConfirmNewerThan(initial_local_surface_id);
  // We do not block synchronization, as there is no platform consistency in
  // resize messages when becoming visible.
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
}

// Tests that when a rotation occurs, that we accept updated viz::LocalSurfaceId
// from the Renderer, and merge them with any of our own changes.
// (https://crbug.com/1223299)
TEST_P(RenderWidgetHostViewAndroidRotationKillswitchTest,
       ChildLocalSurfaceIdsAcceptedDuringRotation) {
  // Android default host and views initialize as visible.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  EXPECT_TRUE(rwhva->IsShowing());
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());

  // When rotation has started we should not be performing Surface Sync. The
  // viz::LocalSurfaceId should not have advanced.
  SetLandscapeScreenInfo(/*rotation=*/true);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // If the child Renderer advances the viz::LocalSurfaceId we should accept it
  // and merge it. So that we are up to date for when rotation completes.
  const viz::LocalSurfaceId child_advanced_local_surface_id(
      initial_local_surface_id.parent_sequence_number(),
      initial_local_surface_id.child_sequence_number() + 1,
      initial_local_surface_id.embed_token());
  cc::RenderFrameMetadata metadata;
  metadata.local_surface_id = child_advanced_local_surface_id;
  OnDidUpdateVisualPropertiesComplete(metadata);
  GetLocalSurfaceIdAndConfirmNewerThan(initial_local_surface_id);
  // Still wait for rotation to end before resuming Surface Sync from other
  // sources.
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
}

TEST_P(RenderWidgetHostViewAndroidRotationKillswitchTest, FullscreenRotation) {
  // Android default host and views initialize as visible.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  EXPECT_TRUE(rwhva->IsShowing());
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());

  // When entering fullscreen the rotation should behave as normal.
  EnterFullscreenMode();

  // When rotation has started we should not be performing Surface Sync. The
  // viz::LocalSurfaceId should not have advanced.
  SetLandscapeScreenInfo(/*rotation= */ true);
  // rwhva->OnSynchronizedDisplayPropertiesChanged(/*rotation= */ true);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // When rotation has completed we should begin Surface Sync again. There
  // should also be a new viz::LocalSurfaceId.
  OnPhysicalBackingSizeChanged(fullscreen_landscape_physical_backing);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_rotation_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(initial_local_surface_id);

  {
    cc::RenderFrameMetadata metadata;
    metadata.local_surface_id = post_rotation_local_surface_id;
    OnRenderFrameMetadataChangedAfterActivation(metadata,
                                                base::TimeTicks::Now());
  }

  // When exiting rotation the order of visual property updates can differ.
  // Though we need to always allow Surface Sync to continue, as WebView will
  // send two OnPhysicalBackingSizeChanged in a row to exit fullscreen. While
  // non-WebView can alternate some combination of 1-2
  // OnPhysicalBackingSizeChanged and OnSynchronizedDisplayPropertiesChanged.
  ExitFullscreenMode();

  OnPhysicalBackingSizeChanged(landscape_physical_backing);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_fullscreen_local_surface_id =
      GetLocalSurfaceIdAndConfirmNewerThan(post_rotation_local_surface_id);

  // When rotation begins again it should block Surface Sync again.
  SetPortraitScreenInfo(/*rotation=*/true);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(post_fullscreen_local_surface_id, rwhva->GetLocalSurfaceId());

  // When rotation has completed we should begin Surface Sync again.
  OnPhysicalBackingSizeChanged(portrait_physical_backing);
  GetLocalSurfaceIdAndConfirmNewerThan(post_fullscreen_local_surface_id);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());

  {
    cc::RenderFrameMetadata metadata;
    metadata.local_surface_id = post_fullscreen_local_surface_id;
    OnRenderFrameMetadataChangedAfterActivation(metadata,
                                                base::TimeTicks::Now());
  }
}

// Tests Rotation improvements that launched with
// features::kSurfaceSyncThrottling flag, and now only exist behind the
// kSurfaceSyncFullscreenKillswitch flag. When off they should directly compare
// visual properties to make throttling determination
INSTANTIATE_TEST_SUITE_P(,
                         RenderWidgetHostViewAndroidRotationKillswitchTest,
                         ::testing::Bool(),
                         &PostTestCaseName);

// Tests that when a device's initial orientation is Landscape, that we do not
// treat the initial UpdateScreenInfo as the start of a rotation.
// https://crbug.com/1263723
class RenderWidgetHostViewAndroidLandscapeStartupTest
    : public RenderWidgetHostViewAndroidRotationKillswitchTest {
 public:
  RenderWidgetHostViewAndroidLandscapeStartupTest() = default;
  ~RenderWidgetHostViewAndroidLandscapeStartupTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override;
};

void RenderWidgetHostViewAndroidLandscapeStartupTest::SetUp() {
  // The base rotation test sets up default of Landscape. Skip it.
  RenderWidgetHostViewAndroidTest::SetUp();
  SetLandscapeScreenInfo(/*rotation=*/false);
  OnPhysicalBackingSizeChanged(landscape_physical_backing);
}

// Tests that when a device's initial orientation is Landscape, that we do not
// treat the initial UpdateScreenInfo as the start of a rotation.
// https://crbug.com/1263723
TEST_P(RenderWidgetHostViewAndroidLandscapeStartupTest, LandscapeStartup) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  // This method is called when initializing the ScreenInfo, not just on
  // subsequent updates. Ensure that initializing to a Landscape orientation
  // does not trigger rotation.
  rwhva->UpdateScreenInfo();
  // We should not be blocking Surface Sync.
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
}

// Tests Rotation improvements that launched with
// features::kSurfaceSyncThrottling flag, and now only exist behind the
// kSurfaceSyncFullscreenKillswitch flag. When off they should directly compare
// visual properties to make throttling determination
INSTANTIATE_TEST_SUITE_P(,
                         RenderWidgetHostViewAndroidLandscapeStartupTest,
                         ::testing::Bool(),
                         &PostTestCaseName);

// Tests that when the ScreenInfo and PhysicalBacking are conflicting
// orientations, that entering Fullscreen and changing to a matching
// PhysicalBacking does not throttle. https://crbug.com/1418321
class RenderWidgetHostViewAndroidMixedOrientationStartupTest
    : public RenderWidgetHostViewAndroidRotationKillswitchTest {
 public:
  RenderWidgetHostViewAndroidMixedOrientationStartupTest() = default;
  ~RenderWidgetHostViewAndroidMixedOrientationStartupTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override;
};

void RenderWidgetHostViewAndroidMixedOrientationStartupTest::SetUp() {
  // The base rotation test sets up default of Portrait for both. Skip it.
  RenderWidgetHostViewAndroidTest::SetUp();
  SetPortraitScreenInfo(/*rotation=*/false);
  // Small Landscape view that will expand when switching to Fullscreen.
  OnPhysicalBackingSizeChanged(gfx::Size(400, 200));
}

// Tests that when we EnterFullscreenMode and the PhysicalBacking changes to
// match the Portrait ScreenInfo, that we do not block syncing.
TEST_F(RenderWidgetHostViewAndroidMixedOrientationStartupTest,
       EnterFullscreenMode) {
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();

  // When features::kSurfaceSyncFullscreenKillswitch is enabled, entering
  // fullscreen mode prevents syncing until we get a non-rotation change to
  // sizes.
  EnterFullscreenMode();
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());

  // This is a rotation compared to the initial physical backing, however by
  // matching the orientation of the ScreenInfo, we should sync and no longer
  // throttle.
  OnPhysicalBackingSizeChanged(fullscreen_portrait_physical_backing);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  GetLocalSurfaceIdAndConfirmNewerThan(initial_local_surface_id);
}

TEST_F(RenderWidgetHostViewAndroidTest, LockUnlockPointer) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(blink::features::kPointerLockOnAndroid);
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();

  // Create a window and attach, so that GetWindowAndroid() doesn't return
  // null.
  auto window = ui::WindowAndroid::CreateForTesting();
  window->get()->AddChild(GetParentView());

  ui::TestViewAndroidDelegate test_view_android_delegate;
  test_view_android_delegate.SetupTestDelegate(rwhva->GetNativeView());
  rwhva->Focus();

  EXPECT_FALSE(rwhva->IsPointerLocked());

  EXPECT_EQ(rwhva->LockPointer(false),
            blink::mojom::PointerLockResult::kSuccess);
  EXPECT_TRUE(rwhva->IsPointerLocked());

  EXPECT_EQ(rwhva->ChangePointerLock(false),
            blink::mojom::PointerLockResult::kSuccess);
  EXPECT_TRUE(rwhva->IsPointerLocked());

  rwhva->UnlockPointer();
  EXPECT_FALSE(rwhva->IsPointerLocked());
}

}  // namespace content
