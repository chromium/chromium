// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_webgl_layer.h"

#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/modules/xr/xr.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/modules/xr/xr_viewport.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/geometry/double_size.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"

namespace blink {

namespace {

const double kFramebufferMinScale = 0.2;

// Because including base::ClampToRange would be a dependency violation
double ClampToRange(const double value, const double min, const double max) {
  return std::min(std::max(value, min), max);
}

}  // namespace

XRWebGLLayer* XRWebGLLayer::Create(
    XRSession* session,
    const WebGLRenderingContextOrWebGL2RenderingContext& context,
    const XRWebGLLayerInit* initializer,
    ExceptionState& exception_state) {
  if (session->ended()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRWebGLLayer for an "
                                      "XRSession which has already ended.");
    return nullptr;
  }

  WebGLRenderingContextBase* webgl_context;
  if (context.IsWebGL2RenderingContext()) {
    webgl_context = context.GetAsWebGL2RenderingContext();
  } else {
    webgl_context = context.GetAsWebGLRenderingContext();
  }

  if (webgl_context->isContextLost()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRWebGLLayer with a "
                                      "lost WebGL context.");
    return nullptr;
  }

  if (session->immersive() && !webgl_context->IsXRCompatible()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "WebGL context must be marked as XR compatible in order to "
        "use with an immersive XRSession");
    return nullptr;
  }

  // TODO(crbug.com/941753): In the future this should be communicated by the
  // drawing buffer and indicate whether the depth buffers are being supplied to
  // the XR compositor.
  bool compositor_supports_depth_values = false;
  bool want_ignore_depth_values = initializer->ignoreDepthValues();

  if (want_ignore_depth_values) {
    UseCounter::Count(session->GetExecutionContext(),
                      WebFeature::kWebXrIgnoreDepthValues);
  }

  // The ignoreDepthValues attribute of XRWebGLLayer may only be set to false if
  // the compositor is actually making use of the depth values and the user did
  // not set ignoreDepthValues to true explicitly.
  bool ignore_depth_values =
      !compositor_supports_depth_values || want_ignore_depth_values;

  double framebuffer_scale = 1.0;

  // Inline sessions don't go through the XR compositor, so they don't need to
  // allocate a separate drawing buffer or expose a framebuffer.
  if (!session->immersive()) {
    return MakeGarbageCollected<XRWebGLLayer>(session, webgl_context, nullptr,
                                              nullptr, framebuffer_scale,
                                              ignore_depth_values);
  }

  bool want_antialiasing = initializer->antialias();
  bool want_depth_buffer = initializer->depth();
  bool want_stencil_buffer = initializer->stencil();
  bool want_alpha_channel = initializer->alpha();

  // Allocate a drawing buffer to back the framebuffer if needed.
  if (initializer->hasFramebufferScaleFactor()) {
    UseCounter::Count(session->GetExecutionContext(),
                      WebFeature::kWebXrFramebufferScale);

    // The max size will be either the native resolution or the default
    // if that happens to be larger than the native res. (That can happen on
    // desktop systems.)
    double max_scale = std::max(session->NativeFramebufferScale(), 1.0);

    // Clamp the developer-requested framebuffer size to ensure it's not too
    // small to see or unreasonably large.
    // TODO: Would be best to have the max value communicated from the service
    // rather than limited to the native res.
    framebuffer_scale = ClampToRange(initializer->framebufferScaleFactor(),
                                     kFramebufferMinScale, max_scale);
  }

  DoubleSize framebuffers_size = session->DefaultFramebufferSize();

  IntSize desired_size(framebuffers_size.Width() * framebuffer_scale,
                       framebuffers_size.Height() * framebuffer_scale);

  // Create an opaque WebGL Framebuffer
  WebGLFramebuffer* framebuffer = WebGLFramebuffer::CreateOpaque(webgl_context);

  scoped_refptr<XRWebGLDrawingBuffer> drawing_buffer =
      XRWebGLDrawingBuffer::Create(webgl_context->GetDrawingBuffer(),
                                   framebuffer->Object(), desired_size,
                                   want_alpha_channel, want_depth_buffer,
                                   want_stencil_buffer, want_antialiasing);

  if (!drawing_buffer) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Unable to create a framebuffer.");
    return nullptr;
  }

  return MakeGarbageCollected<XRWebGLLayer>(
      session, webgl_context, std::move(drawing_buffer), framebuffer,
      framebuffer_scale, ignore_depth_values);
}

XRWebGLLayer::XRWebGLLayer(XRSession* session,
                           WebGLRenderingContextBase* webgl_context,
                           scoped_refptr<XRWebGLDrawingBuffer> drawing_buffer,
                           WebGLFramebuffer* framebuffer,
                           double framebuffer_scale,
                           bool ignore_depth_values)
    : session_(session),
      webgl_context_(webgl_context),
      framebuffer_(framebuffer),
      framebuffer_scale_(framebuffer_scale),
      ignore_depth_values_(ignore_depth_values) {
  if (framebuffer) {
    // Must have a drawing buffer for immersive sessions.
    DCHECK(drawing_buffer);
    drawing_buffer_ = std::move(drawing_buffer);
  } else {
    // Only inline sessions are allowed to have a null drawing buffer.
    DCHECK(!session->immersive());
  }

  UpdateViewports();
}

XRWebGLLayer::~XRWebGLLayer() {
  if (drawing_buffer_) {
    drawing_buffer_->BeginDestruction();
  }
}

uint32_t XRWebGLLayer::framebufferWidth() const {
  if (drawing_buffer_) {
    return drawing_buffer_->size().Width();
  }
  return webgl_context_->drawingBufferWidth();
}

uint32_t XRWebGLLayer::framebufferHeight() const {
  if (drawing_buffer_) {
    return drawing_buffer_->size().Height();
  }
  return webgl_context_->drawingBufferHeight();
}

bool XRWebGLLayer::antialias() const {
  if (drawing_buffer_) {
    return drawing_buffer_->antialias();
  }
  return webgl_context_->GetDrawingBuffer()->Multisample();
}

XRViewport* XRWebGLLayer::getViewport(XRView* view) {
  if (!view || view->session() != session())
    return nullptr;

  return GetViewportForEye(view->EyeValue());
}

XRViewport* XRWebGLLayer::GetViewportForEye(XRView::XREye eye) {
  if (viewports_dirty_)
    UpdateViewports();

  if (eye == XRView::kEyeRight)
    return right_viewport_;

  // This code path also handles an eye of "none".
  return left_viewport_;
}

double XRWebGLLayer::getNativeFramebufferScaleFactor(XRSession* session) {
  return session->NativeFramebufferScale();
}

void XRWebGLLayer::UpdateViewports() {
  uint32_t framebuffer_width = framebufferWidth();
  uint32_t framebuffer_height = framebufferHeight();

  viewports_dirty_ = false;

  if (session()->immersive()) {
    if (session()->StereoscopicViews()) {
      left_viewport_ = MakeGarbageCollected<XRViewport>(
          0, 0, framebuffer_width * 0.5, framebuffer_height);
      right_viewport_ = MakeGarbageCollected<XRViewport>(
          framebuffer_width * 0.5, 0, framebuffer_width * 0.5,
          framebuffer_height);
    } else {
      // Phone immersive AR only uses one viewport, but the second viewport is
      // needed for the UpdateLayerBounds mojo call which currently expects
      // exactly two views. This should be revisited as part of a refactor to
      // handle a more general list of viewports, cf. https://crbug.com/928433.
      left_viewport_ = MakeGarbageCollected<XRViewport>(0, 0, framebuffer_width,
                                                        framebuffer_height);
      right_viewport_ = nullptr;
    }

    session()->xr()->frameProvider()->UpdateWebGLLayerViewports(this);
  } else {
    left_viewport_ = MakeGarbageCollected<XRViewport>(0, 0, framebuffer_width,
                                                      framebuffer_height);
  }
}

HTMLCanvasElement* XRWebGLLayer::output_canvas() const {
  if (!framebuffer_) {
    return webgl_context_->canvas();
  }
  return nullptr;
}

void XRWebGLLayer::OnFrameStart(
    const base::Optional<gpu::MailboxHolder>& buffer_mailbox_holder) {
  if (framebuffer_) {
    framebuffer_->MarkOpaqueBufferComplete(true);
    framebuffer_->SetContentsChanged(false);
    if (buffer_mailbox_holder) {
      drawing_buffer_->UseSharedBuffer(buffer_mailbox_holder.value());
      is_direct_draw_frame = true;
    } else {
      is_direct_draw_frame = false;
    }
  }
}

void XRWebGLLayer::OnFrameEnd() {
  if (framebuffer_) {
    framebuffer_->MarkOpaqueBufferComplete(false);
    if (is_direct_draw_frame) {
      drawing_buffer_->DoneWithSharedBuffer();
      is_direct_draw_frame = false;
    }

    // Submit the frame to the XR compositor.
    if (session()->immersive()) {
      // Always call submit, but notify if the contents were changed or not.
      session()->xr()->frameProvider()->SubmitWebGLLayer(
          this, framebuffer_->HaveContentsChanged());
    }
  }
}

void XRWebGLLayer::OnResize() {
  if (!session()->immersive() && drawing_buffer_) {
    // For non-immersive sessions a resize indicates we should adjust the
    // drawing buffer size to match the canvas.
    DoubleSize framebuffers_size = session()->DefaultFramebufferSize();

    IntSize desired_size(framebuffers_size.Width() * framebuffer_scale_,
                         framebuffers_size.Height() * framebuffer_scale_);
    drawing_buffer_->Resize(desired_size);
  }

  // With both immersive and non-immersive session the viewports should be
  // recomputed when the output canvas resizes.
  viewports_dirty_ = true;
}

scoped_refptr<StaticBitmapImage> XRWebGLLayer::TransferToStaticBitmapImage(
    std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback) {
  if (drawing_buffer_) {
    return drawing_buffer_->TransferToStaticBitmapImage(out_release_callback);
  }
  return nullptr;
}

void XRWebGLLayer::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(left_viewport_);
  visitor->Trace(right_viewport_);
  visitor->Trace(webgl_context_);
  visitor->Trace(framebuffer_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
