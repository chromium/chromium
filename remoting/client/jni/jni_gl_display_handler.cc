// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_gl_display_handler.h"

#include <android/native_window_jni.h>
#include <array>
#include <memory>

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/check.h"
#include "remoting/android/jni_headers/GlDisplay_jni.h"
#include "remoting/client/chromoting_client_runtime.h"
#include "remoting/client/cursor_shape_stub_proxy.h"
#include "remoting/client/display/gl_canvas.h"
#include "remoting/client/dual_buffer_frame_consumer.h"
#include "remoting/client/jni/egl_thread_context.h"
#include "remoting/client/software_video_renderer.h"
#include "remoting/protocol/frame_consumer.h"

namespace remoting {

// The core that lives on the display thread. Must not be created on the display
// thread.
class JniGlDisplayHandler::Core : public protocol::CursorShapeStub,
                                  public GlRendererDelegate {
 public:
  Core(base::WeakPtr<JniGlDisplayHandler> shell);
  ~Core() override;

  // GlRendererDelegate interface.
  bool CanRenderFrame() override;
  void OnFrameRendered() override;
  void OnSizeChanged(int width, int height) override;

  // CursorShapeStub interface.
  void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape) override;

  // Returns the frame consumer for updating desktop frame. Can be called on any
  // thread but no more than once.
  std::unique_ptr<protocol::FrameConsumer> GrabFrameConsumer();

  void OnFrameReceived(std::unique_ptr<webrtc::DesktopFrame> frame,
                       base::OnceClosure done);

  void SurfaceCreated(base::android::ScopedJavaGlobalRef<jobject> surface);
  void SurfaceChanged(int width, int height);
  void SurfaceDestroyed();

  void SetTransformation(const std::array<float, 9>& matrix);
  void MoveCursor(float x, float y);
  void SetCursorVisibility(bool visible);
  void StartInputFeedback(float x, float y, float diameter);

  base::WeakPtr<Core> GetWeakPtr();

 private:
  // Initializes the core on the display thread.
  void Initialize();

  ChromotingClientRuntime* runtime_;
  base::WeakPtr<JniGlDisplayHandler> shell_;

  // Will be std::move'd when GrabFrameConsumer() is called.
  std::unique_ptr<DualBufferFrameConsumer> owned_frame_consumer_;

  base::WeakPtr<DualBufferFrameConsumer> frame_consumer_;

  ANativeWindow* window_ = nullptr;
  std::unique_ptr<EglThreadContext> egl_context_;
  std::unique_ptr<GlRenderer> renderer_;

  // Used on display thread.
  base::WeakPtr<Core> weak_ptr_;
  base::WeakPtrFactory<Core> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Core);
};

JniGlDisplayHandler::Core::Core(base::WeakPtr<JniGlDisplayHandler> shell)
    : shell_(shell) {
  runtime_ = ChromotingClientRuntime::GetInstance();
  DCHECK(!runtime_->display_task_runner()->BelongsToCurrentThread());

  weak_ptr_ = weak_factory_.GetWeakPtr();

  runtime_->display_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&JniGlDisplayHandler::Core::Initialize,
                                base::Unretained(this)));

  // Do not bind GlRenderer::OnFrameReceived. |renderer_| is not ready yet.
  owned_frame_consumer_ = std::make_unique<DualBufferFrameConsumer>(
      base::BindRepeating(&JniGlDisplayHandler::Core::OnFrameReceived,
                          weak_ptr_),
      runtime_->display_task_runner(),
      protocol::FrameConsumer::PixelFormat::FORMAT_RGBA);
  frame_consumer_ = owned_frame_consumer_->GetWeakPtr();
}

JniGlDisplayHandler::Core::~Core() {}

bool JniGlDisplayHandler::Core::CanRenderFrame() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  return egl_context_ && egl_context_->IsWindowBound();
}

void JniGlDisplayHandler::Core::OnFrameRendered() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  egl_context_->SwapBuffers();
  runtime_->ui_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&JniGlDisplayHandler::OnRenderDone, shell_));
}

void JniGlDisplayHandler::Core::OnSizeChanged(int width, int height) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  runtime_->ui_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&JniGlDisplayHandler::OnCanvasSizeChanged,
                                shell_, width, height));
}

void JniGlDisplayHandler::Core::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  renderer_->OnCursorShapeChanged(cursor_shape);
}

std::unique_ptr<protocol::FrameConsumer>
JniGlDisplayHandler::Core::GrabFrameConsumer() {
  DCHECK(owned_frame_consumer_) << "The frame consumer is already grabbed.";
  return std::move(owned_frame_consumer_);
}

void JniGlDisplayHandler::Core::OnFrameReceived(
    std::unique_ptr<webrtc::DesktopFrame> frame,
    base::OnceClosure done) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  renderer_->OnFrameReceived(std::move(frame), std::move(done));
}

void JniGlDisplayHandler::Core::SurfaceCreated(
    base::android::ScopedJavaGlobalRef<jobject> surface) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  DCHECK(!egl_context_);
  DCHECK(!window_);
  renderer_->RequestCanvasSize();
  window_ = ANativeWindow_fromSurface(base::android::AttachCurrentThread(),
                                      surface.obj());
  egl_context_.reset(new EglThreadContext());
  egl_context_->BindToWindow(window_);

  renderer_->OnSurfaceCreated(std::make_unique<GlCanvas>(
      static_cast<int>(egl_context_->client_version())));

  runtime_->network_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DualBufferFrameConsumer::RequestFullDesktopFrame,
                     frame_consumer_));
}

void JniGlDisplayHandler::Core::SurfaceChanged(int width, int height) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  // Note that this doesn't resize the OpenGL viewport. The OpenGL viewport is
  // initialized once it is first bound to the surface. We don't need to call
  // glViewport() since the activity/surface is recreated and hence the viewport
  // is re-initialized every time the surface size is changed.
  renderer_->OnSurfaceChanged(width, height);
}

void JniGlDisplayHandler::Core::SurfaceDestroyed() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  DCHECK(egl_context_);
  DCHECK(window_);
  renderer_->OnSurfaceDestroyed();
  egl_context_.reset();
  ANativeWindow_release(window_);
  window_ = nullptr;
}

void JniGlDisplayHandler::Core::SetTransformation(
    const std::array<float, 9>& matrix) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  renderer_->OnPixelTransformationChanged(matrix);
}

void JniGlDisplayHandler::Core::MoveCursor(float x, float y) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  renderer_->OnCursorMoved(x, y);
}

void JniGlDisplayHandler::Core::SetCursorVisibility(bool visible) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  renderer_->OnCursorVisibilityChanged(visible);
}

void JniGlDisplayHandler::Core::StartInputFeedback(float x,
                                                   float y,
                                                   float diameter) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  renderer_->OnCursorInputFeedback(x, y, diameter);
}

base::WeakPtr<JniGlDisplayHandler::Core>
JniGlDisplayHandler::Core::GetWeakPtr() {
  return weak_ptr_;
}

void JniGlDisplayHandler::Core::Initialize() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());

  renderer_ = GlRenderer::CreateGlRendererWithDesktop();
  renderer_->SetDelegate(weak_ptr_);
}

// Shell implementations.

JniGlDisplayHandler::JniGlDisplayHandler(
    const base::android::JavaRef<jobject>& java_client)
    : runtime_(ChromotingClientRuntime::GetInstance()),
      ui_task_poster_(runtime_->display_task_runner()) {
  core_.reset(new Core(weak_factory_.GetWeakPtr()));
  JNIEnv* env = base::android::AttachCurrentThread();
  java_display_.Reset(Java_GlDisplay_createJavaDisplayObject(
      env, reinterpret_cast<intptr_t>(this)));
  Java_GlDisplay_initializeClient(env, java_display_, java_client);
}

JniGlDisplayHandler::~JniGlDisplayHandler() {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  Java_GlDisplay_invalidate(base::android::AttachCurrentThread(),
                            java_display_);
  runtime_->display_task_runner()->DeleteSoon(FROM_HERE, core_.release());
}

std::unique_ptr<protocol::CursorShapeStub>
JniGlDisplayHandler::CreateCursorShapeStub() {
  return std::make_unique<CursorShapeStubProxy>(
      core_->GetWeakPtr(), runtime_->display_task_runner());
}

std::unique_ptr<protocol::VideoRenderer>
JniGlDisplayHandler::CreateVideoRenderer() {
  return std::make_unique<SoftwareVideoRenderer>(core_->GrabFrameConsumer());
}

void JniGlDisplayHandler::OnSurfaceCreated(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const base::android::JavaParamRef<jobject>& surface) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  runtime_->display_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Core::SurfaceCreated, core_->GetWeakPtr(),
                                base::android::ScopedJavaGlobalRef<jobject>(
                                    env, surface)));
}

void JniGlDisplayHandler::OnSurfaceChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    int width,
    int height) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  runtime_->display_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Core::SurfaceChanged, core_->GetWeakPtr(),
                                width, height));
}

void JniGlDisplayHandler::OnSurfaceDestroyed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  runtime_->display_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Core::SurfaceDestroyed, core_->GetWeakPtr()));
}

void JniGlDisplayHandler::OnPixelTransformationChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const base::android::JavaParamRef<jfloatArray>& jmatrix) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  DCHECK(env->GetArrayLength(jmatrix.obj()) == 9);
  std::array<float, 9> matrix;
  env->GetFloatArrayRegion(jmatrix.obj(), 0, 9, matrix.data());
  ui_task_poster_.AddTask(
      base::BindOnce(&Core::SetTransformation, core_->GetWeakPtr(), matrix));
}

void JniGlDisplayHandler::OnCursorPixelPositionChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    float x,
    float y) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  ui_task_poster_.AddTask(
      base::BindOnce(&Core::MoveCursor, core_->GetWeakPtr(), x, y));
}

void JniGlDisplayHandler::OnCursorVisibilityChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    bool visible) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  ui_task_poster_.AddTask(
      base::BindOnce(&Core::SetCursorVisibility, core_->GetWeakPtr(), visible));
}

void JniGlDisplayHandler::OnCursorInputFeedback(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    float x,
    float y,
    float diameter) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  ui_task_poster_.AddTask(base::BindOnce(&Core::StartInputFeedback,
                                         core_->GetWeakPtr(), x, y, diameter));
}

void JniGlDisplayHandler::OnRenderDone() {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  Java_GlDisplay_canvasRendered(base::android::AttachCurrentThread(),
                                java_display_);
}

void JniGlDisplayHandler::OnCanvasSizeChanged(int width, int height) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  Java_GlDisplay_changeCanvasSize(base::android::AttachCurrentThread(),
                                  java_display_, width, height);
}

}  // namespace remoting
