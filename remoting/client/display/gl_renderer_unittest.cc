// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/display/gl_renderer.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/client/display/fake_canvas.h"
#include "remoting/client/display/gl_renderer_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

class FakeGlRendererDelegate : public GlRendererDelegate {
 public:
  FakeGlRendererDelegate() {}

  bool CanRenderFrame() override {
    can_render_frame_call_count_++;
    return can_render_frame_;
  }

  void OnFrameRendered() override {
    on_frame_rendered_call_count_++;
    if (on_frame_rendered_callback_) {
      on_frame_rendered_callback_.Run();
    }
  }

  void OnSizeChanged(int width, int height) override {
    canvas_width_ = width;
    canvas_height_ = height;
    on_size_changed_call_count_++;
  }

  void SetOnFrameRenderedCallback(const base::Closure& callback) {
    on_frame_rendered_callback_ = callback;
  }

  int canvas_width() { return canvas_width_; }

  int canvas_height() { return canvas_height_; }

  base::WeakPtr<FakeGlRendererDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  int can_render_frame_call_count() { return can_render_frame_call_count_; }

  int on_frame_rendered_call_count() { return on_frame_rendered_call_count_; }

  int on_size_changed_call_count() { return on_size_changed_call_count_; }

  bool can_render_frame_ = false;

 private:
  int can_render_frame_call_count_ = 0;
  int on_frame_rendered_call_count_ = 0;
  int on_size_changed_call_count_ = 0;

  int canvas_width_ = 0;
  int canvas_height_ = 0;

  base::Closure on_frame_rendered_callback_;
  base::WeakPtrFactory<FakeGlRendererDelegate> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeGlRendererDelegate);
};

class FakeDrawable : public Drawable {
 public:
  FakeDrawable() {}

  void SetId(int id) { id_ = id; }
  int GetId() { return id_; }

  base::WeakPtr<Drawable> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  void SetCanvas(base::WeakPtr<Canvas> canvas) override {}

  bool Draw() override {
    drawn_++;
    return false;
  }

  void SetZIndex(int z_index) { z_index_ = z_index; }

  int GetZIndex() override { return z_index_; }

  int DrawnCount() { return drawn_; }

 private:
  int drawn_ = 0;
  int id_ = -1;
  int z_index_ = -1;

  base::WeakPtrFactory<FakeDrawable> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeDrawable);
};

class GlRendererTest : public testing::Test {
 public:
  void SetUp() override;
  void SetDesktopFrameWithSize(const webrtc::DesktopSize& size);
  void PostSetDesktopFrameTasks(const webrtc::DesktopSize& size, int count);
  int GetDrawablesCount();
  std::vector<base::WeakPtr<Drawable>> GetDrawables();

 protected:
  void RequestRender();
  void OnDesktopFrameProcessed();
  void RunTasksInCurrentQueue();
  void RunUntilRendered();
  int on_desktop_frame_processed_call_count() {
    return on_desktop_frame_processed_call_count_;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<GlRenderer> renderer_;
  FakeGlRendererDelegate delegate_;

 private:
  int on_desktop_frame_processed_call_count_ = 0;
};

void GlRendererTest::SetUp() {
  renderer_.reset(new GlRenderer());
  renderer_->SetDelegate(delegate_.GetWeakPtr());
}

void GlRendererTest::RequestRender() {
  renderer_->RequestRender();
}

int GlRendererTest::GetDrawablesCount() {
  return renderer_->drawables_.size();
}

std::vector<base::WeakPtr<Drawable>> GlRendererTest::GetDrawables() {
  return renderer_->drawables_;
}

void GlRendererTest::SetDesktopFrameWithSize(const webrtc::DesktopSize& size) {
  renderer_->OnFrameReceived(
      std::make_unique<webrtc::BasicDesktopFrame>(size),
      base::Bind(&GlRendererTest::OnDesktopFrameProcessed,
                 base::Unretained(this)));
}

void GlRendererTest::PostSetDesktopFrameTasks(const webrtc::DesktopSize& size,
                                              int count) {
  for (int i = 0; i < count; i++) {
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&GlRendererTest::SetDesktopFrameWithSize,
                                  base::Unretained(this), size));
  }
}

void GlRendererTest::OnDesktopFrameProcessed() {
  on_desktop_frame_processed_call_count_++;
}

void GlRendererTest::RunTasksInCurrentQueue() {
  base::RunLoop run_loop;
  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  run_loop.Run();
}

void GlRendererTest::RunUntilRendered() {
  base::RunLoop run_loop;
  delegate_.SetOnFrameRenderedCallback(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(GlRendererTest, TestDelegateCanRenderFrame) {
  delegate_.can_render_frame_ = true;
  RequestRender();
  RunTasksInCurrentQueue();
  EXPECT_EQ(1, delegate_.can_render_frame_call_count());
  EXPECT_EQ(1, delegate_.on_frame_rendered_call_count());

  delegate_.can_render_frame_ = false;
  RequestRender();
  RunTasksInCurrentQueue();
  EXPECT_EQ(2, delegate_.can_render_frame_call_count());
  EXPECT_EQ(1, delegate_.on_frame_rendered_call_count());
}

TEST_F(GlRendererTest, TestRequestRenderOnlyScheduleOnce) {
  delegate_.can_render_frame_ = true;

  RequestRender();
  RequestRender();
  RequestRender();
  RunTasksInCurrentQueue();
  EXPECT_EQ(1, delegate_.can_render_frame_call_count());
  EXPECT_EQ(1, delegate_.on_frame_rendered_call_count());

  RequestRender();
  RunTasksInCurrentQueue();
  EXPECT_EQ(2, delegate_.can_render_frame_call_count());
  EXPECT_EQ(2, delegate_.on_frame_rendered_call_count());
}

TEST_F(GlRendererTest, TestDelegateOnSizeChanged) {
  SetDesktopFrameWithSize(webrtc::DesktopSize(16, 16));
  ASSERT_EQ(1, delegate_.on_size_changed_call_count());
  ASSERT_EQ(16, delegate_.canvas_width());
  ASSERT_EQ(16, delegate_.canvas_height());

  SetDesktopFrameWithSize(webrtc::DesktopSize(16, 16));
  ASSERT_EQ(1, delegate_.on_size_changed_call_count());
  ASSERT_EQ(16, delegate_.canvas_width());
  ASSERT_EQ(16, delegate_.canvas_height());

  SetDesktopFrameWithSize(webrtc::DesktopSize(32, 32));
  ASSERT_EQ(2, delegate_.on_size_changed_call_count());
  ASSERT_EQ(32, delegate_.canvas_width());
  ASSERT_EQ(32, delegate_.canvas_height());

  renderer_->RequestCanvasSize();
  ASSERT_EQ(3, delegate_.on_size_changed_call_count());
  ASSERT_EQ(32, delegate_.canvas_width());
  ASSERT_EQ(32, delegate_.canvas_height());
}

TEST_F(GlRendererTest, TestOnFrameReceivedDoneCallbacks) {
  delegate_.can_render_frame_ = true;

  // Implicitly calls RequestRender().

  PostSetDesktopFrameTasks(webrtc::DesktopSize(16, 16), 1);
  RunUntilRendered();
  EXPECT_EQ(1, delegate_.on_frame_rendered_call_count());
  EXPECT_EQ(1, on_desktop_frame_processed_call_count());

  PostSetDesktopFrameTasks(webrtc::DesktopSize(16, 16), 20);
  RunUntilRendered();
  ASSERT_EQ(2, delegate_.on_frame_rendered_call_count());
  ASSERT_EQ(21, on_desktop_frame_processed_call_count());
}

// TODO(yuweih): Add tests to validate the rendered output.

TEST_F(GlRendererTest, TestAddDrawable) {
  std::unique_ptr<FakeDrawable> drawable0 = std::make_unique<FakeDrawable>();
  drawable0->SetId(0);
  renderer_->AddDrawable(drawable0->GetWeakPtr());
  ASSERT_EQ(1, GetDrawablesCount());
}

TEST_F(GlRendererTest, TestAddDrawableDefaultOrder) {
  std::unique_ptr<FakeDrawable> drawable0 = std::make_unique<FakeDrawable>();
  drawable0->SetId(0);
  renderer_->AddDrawable(drawable0->GetWeakPtr());
  ASSERT_EQ(1, GetDrawablesCount());

  std::unique_ptr<FakeDrawable> drawable1 = std::make_unique<FakeDrawable>();
  drawable1->SetId(1);
  renderer_->AddDrawable(drawable1->GetWeakPtr());
  ASSERT_EQ(2, GetDrawablesCount());

  std::unique_ptr<FakeDrawable> drawable2 = std::make_unique<FakeDrawable>();
  drawable2->SetId(2);
  renderer_->AddDrawable(drawable2->GetWeakPtr());
  ASSERT_EQ(3, GetDrawablesCount());

  int i = 0;
  for (auto& drawable : GetDrawables()) {
    FakeDrawable* fg = static_cast<FakeDrawable*>(drawable.get());
    ASSERT_EQ(i, fg->GetId());
    i++;
  }
  ASSERT_EQ(3, i);
}

TEST_F(GlRendererTest, TestAddDrawableOrder) {
  std::unique_ptr<FakeDrawable> drawable2 = std::make_unique<FakeDrawable>();
  drawable2->SetId(2);
  drawable2->SetZIndex(2);
  renderer_->AddDrawable(drawable2->GetWeakPtr());
  ASSERT_EQ(1, GetDrawablesCount());

  std::unique_ptr<FakeDrawable> drawable0 = std::make_unique<FakeDrawable>();
  drawable0->SetId(0);
  renderer_->AddDrawable(drawable0->GetWeakPtr());
  ASSERT_EQ(2, GetDrawablesCount());

  std::unique_ptr<FakeDrawable> drawable1 = std::make_unique<FakeDrawable>();
  drawable1->SetId(1);
  drawable1->SetZIndex(1);
  renderer_->AddDrawable(drawable1->GetWeakPtr());
  ASSERT_EQ(3, GetDrawablesCount());

  int i = 0;
  for (auto& drawable : GetDrawables()) {
    FakeDrawable* fg = static_cast<FakeDrawable*>(drawable.get());
    ASSERT_EQ(i, fg->GetId());
    i++;
  }
  ASSERT_EQ(3, i);
}

TEST_F(GlRendererTest, TestAddDrawableDrawn) {
  std::unique_ptr<Canvas> fakeCanvas = std::make_unique<FakeCanvas>();
  renderer_->OnSurfaceCreated(std::move(fakeCanvas));
  delegate_.can_render_frame_ = true;
  PostSetDesktopFrameTasks(webrtc::DesktopSize(16, 16), 1);
  std::unique_ptr<FakeDrawable> drawable0 = std::make_unique<FakeDrawable>();
  drawable0->SetId(3);
  renderer_->AddDrawable(drawable0->GetWeakPtr());
  RequestRender();
  RunTasksInCurrentQueue();
  std::unique_ptr<FakeDrawable> drawable1 = std::make_unique<FakeDrawable>();
  drawable1->SetId(2);
  drawable1->SetZIndex(1);
  renderer_->AddDrawable(drawable1->GetWeakPtr());

  RequestRender();
  RunTasksInCurrentQueue();

  std::unique_ptr<FakeDrawable> drawable2 = std::make_unique<FakeDrawable>();
  drawable2->SetId(1);
  drawable2->SetZIndex(2);
  renderer_->AddDrawable(drawable2->GetWeakPtr());
  ASSERT_EQ(3, GetDrawablesCount());

  RequestRender();
  RunTasksInCurrentQueue();
  for (auto& drawable : GetDrawables()) {
    FakeDrawable* fg = static_cast<FakeDrawable*>(drawable.get());
    EXPECT_EQ(fg->GetId(), fg->DrawnCount());
  }
}

TEST_F(GlRendererTest, TestCreateGlRendererWithDesktop) {
  renderer_ = GlRenderer::CreateGlRendererWithDesktop();
  renderer_->SetDelegate(delegate_.GetWeakPtr());
  ASSERT_EQ(3, GetDrawablesCount());
}

// TODO(nicholss): Add a test where the drawable is destructed and the renderer
// gets a dead weakptr.

}  // namespace remoting
