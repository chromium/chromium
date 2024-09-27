// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_webgl_layer.h"

#include <algorithm>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/modules/xr/xr_viewport.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

namespace {

const double kFramebufferMinScale = 0.2;
const uint32_t kCleanFrameWarningLimit = 5;

const char kCleanFrameWarning[] =
    "Note: The XRSession has completed multiple animation frames without "
    "drawing anything to the baseLayer's framebuffer, resulting in no visible "
    "output.";

}  // namespace

XRWebGLLayer* XRWebGLLayer::Create(XRSession* session,
                                   const V8XRWebGLRenderingContext* context,
                                   const XRWebGLLayerInit* initializer,
                                   ExceptionState& exception_state) {
  if (session->ended()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRWebGLLayer for an "
                                      "XRSession which has already ended.");
    return nullptr;
  }

  WebGLRenderingContextBase* webgl_context =
      webglRenderingContextBaseFromUnion(context);

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

  if (session->GraphicsApi() != XRGraphicsBinding::Api::kWebGL) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRWebGLLayer with a "
                                      "WebGPU-based XRSession.");
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

  const bool want_antialiasing =
      initializer->antialias() && session->CanEnableAntiAliasing();
  const bool want_depth_buffer = initializer->depth();
  const bool want_stencil_buffer = initializer->stencil();
  const bool want_alpha_channel = initializer->alpha();

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
    // TODO(bajones): Would be best to have the max value communicated from the
    // service rather than limited to the native res.
    framebuffer_scale = std::clamp(initializer->framebufferScaleFactor(),
                                   kFramebufferMinScale, max_scale);
  }

  gfx::SizeF framebuffers_size = session->RecommendedFramebufferSize();

  gfx::Size desired_size =
      gfx::ToFlooredSize(gfx::ScaleSize(framebuffers_size, framebuffer_scale));

  // Create an opaque WebGL Framebuffer
  WebGLFramebuffer* framebuffer = WebGLFramebuffer::CreateOpaque(
      webgl_context, want_depth_buffer, want_stencil_buffer);

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
    : XRLayer(session),
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
    return drawing_buffer_->size().width();
  }
  return webgl_context_->drawingBufferWidth();
}

uint32_t XRWebGLLayer::framebufferHeight() const {
  if (drawing_buffer_) {
    return drawing_buffer_->size().height();
  }
  return webgl_context_->drawingBufferHeight();
}

bool XRWebGLLayer::antialias() const {
  if (drawing_buffer_) {
    return drawing_buffer_->antialias();
  }
  if (!webgl_context_->isContextLost()) {
    return webgl_context_->GetDrawingBuffer()->Multisample();
  }
  return false;
}

XRViewport* XRWebGLLayer::getViewport(XRView* view) {
  if (!view || view->session() != session())
    return nullptr;

  // Dynamic viewport scaling, see steps 6 and 7 in
  // https://immersive-web.github.io/webxr/#dom-xrwebgllayer-getviewport
  XRViewData* view_data = view->ViewData();
  if (view_data->ViewportModifiable() &&
      view_data->CurrentViewportScale() !=
          view_data->RequestedViewportScale()) {
    DVLOG(2) << __func__
             << ": apply ViewportScale=" << view_data->RequestedViewportScale();
    view_data->SetCurrentViewportScale(view_data->RequestedViewportScale());
    viewports_dirty_ = true;
  }
  TRACE_COUNTER1("xr", "XR viewport scale (%)",
                 view_data->CurrentViewportScale() * 100);
  view_data->SetViewportModifiable(false);

  if (viewports_dirty_) {
    UpdateViewports();
  }

  // framebuffer_scale_ is the scale requested by the web developer when this
  // layer was created. The session's recommended framebuffer scale is the scale
  // requested by the XR runtime. Both scales must be applied to the viewport.
  return view->Viewport(framebuffer_scale_ *
                        session()->RecommendedFramebufferScale());
}

XRViewport* XRWebGLLayer::GetViewportForEye(device::mojom::blink::XREye eye) {
  if (viewports_dirty_)
    UpdateViewports();

  if (eye == device::mojom::blink::XREye::kRight)
    return right_viewport_.Get();

  // This code path also handles an eye of "none".
  return left_viewport_.Get();
}

double XRWebGLLayer::getNativeFramebufferScaleFactor(XRSession* session) {
  return session->NativeFramebufferScale();
}

void XRWebGLLayer::UpdateViewports() {
  uint32_t framebuffer_width = framebufferWidth();
  uint32_t framebuffer_height = framebufferHeight();
  if (framebuffer_width == 0U || framebuffer_height == 0U) {
    LOG_IF(ERROR, !webgl_context_->isContextLost())
        << __func__ << " Received width=" << framebuffer_width
        << " height=" << framebuffer_height << " without having lost context";
    return;
  }

  viewports_dirty_ = false;

  // When calculating the scaled viewport size, round down to integer value, but
  // ensure that the value is nonzero and doesn't overflow. See
  // https://immersive-web.github.io/webxr/#xrview-obtain-a-scaled-viewport
  auto rounded = [](double v) {
    return std::max(1, base::saturated_cast<int>(v));
  };

  if (session()->immersive()) {
    // Calculate new sizes with optional viewport scale applied. This assumes
    // that XRSession::views() returns views in matching order.
    if (session()->StereoscopicViews()) {
      // TODO(1275873): This technically works fine because the entire bounds is
      // still sent to the XR process, but if there are more than two views,
      // the terms "left" and "right" are not accurate. The entire bounds of
      // all viewports should be sent instead.
      double left_scale = session()->views()[0]->CurrentViewportScale();
      left_viewport_ = MakeGarbageCollected<XRViewport>(
          0, 0, rounded(framebuffer_width * 0.5 * left_scale),
          rounded(framebuffer_height * left_scale));
      double right_scale = session()->views()[1]->CurrentViewportScale();
      right_viewport_ = MakeGarbageCollected<XRViewport>(
          framebuffer_width * 0.5, 0,
          rounded(framebuffer_width * 0.5 * right_scale),
          rounded(framebuffer_height * right_scale));
    } else {
      // Phone immersive AR only uses one viewport, but the second viewport is
      // needed for the UpdateLayerBounds mojo call which currently expects
      // exactly two views. This should be revisited as part of a refactor to
      // handle a more general list of viewports, cf. https://crbug.com/928433.
      double mono_scale = session()->views()[0]->CurrentViewportScale();
      left_viewport_ = MakeGarbageCollected<XRViewport>(
          0, 0, rounded(framebuffer_width * mono_scale),
          rounded(framebuffer_height * mono_scale));
      right_viewport_ = nullptr;
    }

    session()->xr()->frameProvider()->UpdateWebGLLayerViewports(this);
  } else {
    // Currently, only immersive sessions implement dynamic viewport scaling.
    // Ignore the setting for non-immersive sessions, effectively treating
    // the minimum viewport scale as 1.0 which disables the feature.
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

WebGLTexture* XRWebGLLayer::GetCameraTexture() {
  DVLOG(1) << __func__;

  // We already have a WebGL texture for the camera image - return it:
  if (camera_image_texture_) {
    return camera_image_texture_.Get();
  }

  // We don't have a WebGL texture, and we cannot create it - return null:
  if (!camera_image_texture_id_) {
    return nullptr;
  }

  // We don't have a WebGL texture, but we can create it, so create, store and
  // return it:
  camera_image_texture_ = MakeGarbageCollected<WebGLUnownedTexture>(
      webgl_context_, camera_image_texture_id_, GL_TEXTURE_2D);

  return camera_image_texture_.Get();
}

void XRWebGLLayer::OnFrameStart(
    const std::optional<gpu::MailboxHolder>& buffer_mailbox_holder,
    const std::optional<gpu::MailboxHolder>& camera_image_mailbox_holder) {
  if (framebuffer_) {
    framebuffer_->MarkOpaqueBufferComplete(true);
    framebuffer_->SetContentsChanged(false);
    if (buffer_mailbox_holder) {
      drawing_buffer_->UseSharedBuffer(buffer_mailbox_holder.value());
      DVLOG(3) << __func__ << ": buffer_mailbox_holder->mailbox="
               << buffer_mailbox_holder->mailbox.ToDebugString();
      is_direct_draw_frame = true;
    } else {
      is_direct_draw_frame = false;
    }

    if (camera_image_mailbox_holder) {
      DVLOG(3) << __func__ << ":camera_image_mailbox_holder->mailbox="
               << camera_image_mailbox_holder->mailbox.ToDebugString();
      camera_image_mailbox_holder_ = camera_image_mailbox_holder;
      camera_image_texture_id_ =
          GetBufferTextureId(camera_image_mailbox_holder_);
      DVLOG(3) << __func__
               << ": camera_image_texture_id_=" << camera_image_texture_id_;
      BindCameraBufferTexture(camera_image_mailbox_holder_);
    }
  }
}

uint32_t XRWebGLLayer::GetBufferTextureId(
    const std::optional<gpu::MailboxHolder>& buffer_mailbox_holder) {
  gpu::gles2::GLES2Interface* gl = drawing_buffer_->ContextGL();
  gl->WaitSyncTokenCHROMIUM(buffer_mailbox_holder->sync_token.GetConstData());
  DVLOG(3) << __func__ << ": buffer_mailbox_holder->sync_token="
           << buffer_mailbox_holder->sync_token.ToDebugString();
  GLuint texture_id = gl->CreateAndTexStorage2DSharedImageCHROMIUM(
      buffer_mailbox_holder->mailbox.name);
  DVLOG(3) << __func__ << ": texture_id=" << texture_id;
  return texture_id;
}

void XRWebGLLayer::BindCameraBufferTexture(
    const std::optional<gpu::MailboxHolder>& buffer_mailbox_holder) {
  gpu::gles2::GLES2Interface* gl = drawing_buffer_->ContextGL();

  if (buffer_mailbox_holder) {
    uint32_t texture_target = buffer_mailbox_holder->texture_target;
    gl->BindTexture(texture_target, camera_image_texture_id_);
    gl->BeginSharedImageAccessDirectCHROMIUM(
        camera_image_texture_id_, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
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
      bool framebuffer_dirty = framebuffer_->HaveContentsChanged();

      // Not drawing to the framebuffer during a session's rAF callback is
      // usually a sign that something is wrong, such as the app drawing to the
      // wrong render target. Show a warning in the console if we see that
      // happen too many times.
      if (!framebuffer_dirty) {
        // If the session doesn't have a pose then the framebuffer being clean
        // may be expected, so we won't count those frames.
        bool frame_had_pose = !!session()->GetMojoFrom(
            device::mojom::blink::XRReferenceSpaceType::kViewer);
        if (frame_had_pose) {
          clean_frame_count++;
          if (clean_frame_count == kCleanFrameWarningLimit) {
            session()->xr()->GetExecutionContext()->AddConsoleMessage(
                MakeGarbageCollected<ConsoleMessage>(
                    mojom::blink::ConsoleMessageSource::kRendering,
                    mojom::blink::ConsoleMessageLevel::kWarning,
                    kCleanFrameWarning));
          }
        }
      }

      // Need to stop accessing the camera image texture before calling
      // `SubmitWebGLLayer` so that we stop using it before the sync token
      // that `SubmitWebGLLayer` will generate.
      if (camera_image_texture_id_) {
        // We shouldn't ever have a camera texture if the holder wasn't present:
        DCHECK(camera_image_mailbox_holder_);

        DVLOG(3) << __func__
                 << ": deleting camera image texture, camera_image_texture_id_="
                 << camera_image_texture_id_;
        gpu::gles2::GLES2Interface* gl = drawing_buffer_->ContextGL();

        gl->EndSharedImageAccessDirectCHROMIUM(camera_image_texture_id_);
        gl->DeleteTextures(1, &camera_image_texture_id_);

        // Notify our WebGLUnownedTexture (created from
        // camera_image_texture_id_) that we have deleted it. Also, release the
        // reference since we no longer need it (note that it could still be
        // kept alive by the JS application, but should be a defunct object).
        if (camera_image_texture_) {
          camera_image_texture_->OnGLDeleteTextures();
          camera_image_texture_ = nullptr;
        }

        camera_image_texture_id_ = 0;
        camera_image_mailbox_holder_ = std::nullopt;
      }

      // Always call submit, but notify if the contents were changed or not.
      session()->xr()->frameProvider()->SubmitWebGLLayer(this,
                                                         framebuffer_dirty);
    }
  }
}

void XRWebGLLayer::OnResize() {
  if (drawing_buffer_) {
    gfx::SizeF framebuffers_size = session()->RecommendedFramebufferSize();

    gfx::Size desired_size = gfx::ToFlooredSize(
        gfx::ScaleSize(framebuffers_size, framebuffer_scale_));
    drawing_buffer_->Resize(desired_size);
  }

  // With both immersive and non-immersive session the viewports should be
  // recomputed when the output canvas resizes.
  viewports_dirty_ = true;
}

scoped_refptr<StaticBitmapImage> XRWebGLLayer::TransferToStaticBitmapImage() {
  if (drawing_buffer_) {
    return drawing_buffer_->TransferToStaticBitmapImage();
  }
  return nullptr;
}

void XRWebGLLayer::Trace(Visitor* visitor) const {
  visitor->Trace(left_viewport_);
  visitor->Trace(right_viewport_);
  visitor->Trace(webgl_context_);
  visitor->Trace(framebuffer_);
  visitor->Trace(camera_image_texture_);
  XRLayer::Trace(visitor);
}

}  // namespace blink
