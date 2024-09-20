// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <memory>

#import "remoting/ios/display/gl_display_handler.h"

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "remoting/client/display/sys_opengl.h"
#import "remoting/ios/display/eagl_view.h"
#import "remoting/ios/display/gl_demo_screen.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/chromoting_client_runtime.h"
#include "remoting/client/cursor_shape_stub_proxy.h"
#include "remoting/client/display/gl_canvas.h"
#include "remoting/client/display/gl_renderer.h"
#include "remoting/client/display/gl_renderer_delegate.h"
#include "remoting/client/display/renderer_proxy.h"
#include "remoting/client/dual_buffer_frame_consumer.h"
#include "remoting/client/software_video_renderer.h"

namespace remoting {

class ViewMatrix;

namespace GlDisplayHandler {

// The core that lives on the display thread.
class Core : public protocol::CursorShapeStub, public GlRendererDelegate {
 public:
  Core();

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core() override;

  void Initialize();

  void SetHandlerDelegate(id<GlDisplayHandlerDelegate> delegate);

  // CursorShapeStub interface.
  void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape) override;

  // GlRendererDelegate interface.
  bool CanRenderFrame() override;
  void OnFrameRendered() override;
  void OnSizeChanged(int width, int height) override;

  void OnFrameReceived(std::unique_ptr<webrtc::DesktopFrame> frame,
                       base::OnceClosure done);
  void CreateRendererContext(EAGLView* view);
  void DestroyRendererContext();
  void SetSurfaceSize(int width, int height);

  std::unique_ptr<protocol::FrameConsumer> GrabFrameConsumer();

  // The renderer proxy should be used on the UI thread.
  RendererProxy* renderer_proxy() { return renderer_proxy_.get(); }

  // Returns a weak pointer to be used on the display thread.
  base::WeakPtr<Core> GetWeakPtr();

 private:
  raw_ptr<remoting::ChromotingClientRuntime> runtime_;

  // Will be std::move'd when GrabFrameConsumer() is called.
  std::unique_ptr<DualBufferFrameConsumer> owned_frame_consumer_;
  base::WeakPtr<DualBufferFrameConsumer> frame_consumer_;

  EAGLContext* eagl_context_;
  std::unique_ptr<GlRenderer> renderer_;
  __weak id<GlDisplayHandlerDelegate> handler_delegate_;

  // This should be used and deleted on the UI thread.
  std::unique_ptr<RendererProxy> renderer_proxy_;

  // Valid only when the surface is created.
  __weak EAGLView* view_;

  // Used on display thread.
  base::WeakPtr<Core> weak_ptr_;
  base::WeakPtrFactory<Core> weak_factory_;
};

Core::Core() : weak_factory_(this) {
  runtime_ = ChromotingClientRuntime::GetInstance();
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());

  weak_ptr_ = weak_factory_.GetWeakPtr();

  // Do not bind GlRenderer::OnFrameReceived. |renderer_| is not ready yet.
  owned_frame_consumer_ = std::make_unique<remoting::DualBufferFrameConsumer>(
      base::BindRepeating(&Core::OnFrameReceived, weak_ptr_),
      runtime_->display_task_runner(),
      protocol::FrameConsumer::PixelFormat::FORMAT_RGBA);
  frame_consumer_ = owned_frame_consumer_->GetWeakPtr();

  renderer_proxy_ =
      std::make_unique<RendererProxy>(runtime_->display_task_runner());

  runtime_->display_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Core::Initialize, GetWeakPtr()));
}

Core::~Core() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  runtime_->ui_task_runner()->DeleteSoon(FROM_HERE, renderer_proxy_.release());
}

void Core::Initialize() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());

  eagl_context_ = [EAGLContext currentContext];
  if (!eagl_context_) {
    eagl_context_ =
        [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    if (!eagl_context_) {
      LOG(WARNING) << "Failed to create GLES3 context. Attempting to create "
                   << "GLES2 context.";
      eagl_context_ =
          [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    }
    DCHECK(eagl_context_);
    [EAGLContext setCurrentContext:eagl_context_];
  }

  renderer_ = remoting::GlRenderer::CreateGlRendererWithDesktop();

  renderer_->SetDelegate(weak_ptr_);

  // Safe to use base::Unretained because |renderer_proxy_| is destroyed on UI
  // after Core is destroyed.
  runtime_->ui_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RendererProxy::Initialize,
                                base::Unretained(renderer_proxy_.get()),
                                renderer_->GetWeakPtr()));
}

void Core::SetHandlerDelegate(id<GlDisplayHandlerDelegate> delegate) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  handler_delegate_ = delegate;
}

void Core::SetCursorShape(const protocol::CursorShapeInfo& cursor_shape) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  renderer_->OnCursorShapeChanged(cursor_shape);
}

bool Core::CanRenderFrame() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());

  return eagl_context_ != nil;
}

std::unique_ptr<protocol::FrameConsumer> Core::GrabFrameConsumer() {
  DCHECK(owned_frame_consumer_) << "The frame consumer is already grabbed.";
  return std::move(owned_frame_consumer_);
}

void Core::OnFrameReceived(std::unique_ptr<webrtc::DesktopFrame> frame,
                           base::OnceClosure done) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  renderer_->OnFrameReceived(std::move(frame), std::move(done));
}

void Core::OnFrameRendered() {
  [eagl_context_ presentRenderbuffer:GL_RENDERBUFFER];
  // Do not directly use |handler_delegate_| in the block. That will force the
  // block to dereference |this|, which is thread unsafe because it doesn't
  // support ARC.
  __weak id<GlDisplayHandlerDelegate> handler_delegate = handler_delegate_;
  runtime_->ui_task_runner()->PostTask(FROM_HERE, base::BindOnce(^{
                                         [handler_delegate rendererTicked];
                                       }));
}

void Core::OnSizeChanged(int width, int height) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  __weak id<GlDisplayHandlerDelegate> handler_delegate = handler_delegate_;
  runtime_->ui_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(^{
        [handler_delegate canvasSizeChanged:CGSizeMake(width, height)];
      }));
}

void Core::CreateRendererContext(EAGLView* view) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  DCHECK(eagl_context_);

  if (view_) {
    return;
  }
  view_ = view;

  runtime_->ui_task_runner()->PostTask(FROM_HERE, base::BindOnce(^{
                                         [view startWithContext:eagl_context_];
                                       }));

  // TODO(yuweih): Rename methods in GlRenderer.
  renderer_->OnSurfaceCreated(
      std::make_unique<GlCanvas>(static_cast<int>([eagl_context_ API])));

  renderer_->RequestCanvasSize();

  runtime_->network_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DualBufferFrameConsumer::RequestFullDesktopFrame,
                     frame_consumer_));
}

void Core::DestroyRendererContext() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());

  if (!view_) {
    return;
  }

  renderer_->OnSurfaceDestroyed();
  __weak EAGLView* view = view_;
  runtime_->ui_task_runner()->PostTask(FROM_HERE, base::BindOnce(^{
                                         [view stop];
                                       }));
  view_ = nil;
}

void Core::SetSurfaceSize(int width, int height) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  renderer_->OnSurfaceChanged(width, height);
}

base::WeakPtr<remoting::GlDisplayHandler::Core> Core::GetWeakPtr() {
  return weak_ptr_;
}

}  // namespace GlDisplayHandler
}  // namespace remoting

@interface GlDisplayHandler () {
  std::unique_ptr<remoting::GlDisplayHandler::Core> _core;
  raw_ptr<remoting::ChromotingClientRuntime> _runtime;
}
@end

@implementation GlDisplayHandler

- (id)init {
  self = [super init];
  if (self) {
    _runtime = remoting::ChromotingClientRuntime::GetInstance();
    _core.reset(new remoting::GlDisplayHandler::Core());
  }
  return self;
}

- (void)dealloc {
  _runtime->display_task_runner()->DeleteSoon(FROM_HERE, _core.release());
}

#pragma mark - Public

- (std::unique_ptr<remoting::protocol::VideoRenderer>)createVideoRenderer {
  return std::make_unique<remoting::SoftwareVideoRenderer>(
      _core->GrabFrameConsumer());
}

- (std::unique_ptr<remoting::protocol::CursorShapeStub>)createCursorShapeStub {
  return std::make_unique<remoting::CursorShapeStubProxy>(
      _core->GetWeakPtr(), _runtime->display_task_runner());
}

- (void)createRendererContext:(EAGLView*)view {
  _runtime->display_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&remoting::GlDisplayHandler::Core::CreateRendererContext,
                     _core->GetWeakPtr(), view));
}

- (void)destroyRendererContext {
  _runtime->display_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&remoting::GlDisplayHandler::Core::DestroyRendererContext,
                     _core->GetWeakPtr()));
}

- (void)setSurfaceSize:(const CGRect&)frame {
  _runtime->display_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&remoting::GlDisplayHandler::Core::SetSurfaceSize,
                     _core->GetWeakPtr(), frame.size.width, frame.size.height));
}

#pragma mark - Properties

- (remoting::RendererProxy*)rendererProxy {
  return _core->renderer_proxy();
}

- (void)setDelegate:(id<GlDisplayHandlerDelegate>)delegate {
  _runtime->display_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&remoting::GlDisplayHandler::Core::SetHandlerDelegate,
                     _core->GetWeakPtr(), delegate));
}

- (id<GlDisplayHandlerDelegate>)delegate {
  // Implementation is still required for UNAVAILABLE_ATTRIBUTE.
  NOTREACHED();
}

@end
