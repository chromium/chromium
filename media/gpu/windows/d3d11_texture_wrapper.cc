// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_texture_wrapper.h"

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_image_backing_d3d.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/win/mf_helpers.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/gl/gl_image.h"

namespace media {

namespace {

// Populates Viz |texture_formats| that map to the corresponding DXGI format and
// VideoPixelFormat. Returns true if they can be successfully mapped.
bool DXGIFormatToVizFormat(
    DXGI_FORMAT dxgi_format,
    VideoPixelFormat pixel_format,
    size_t textures_per_picture,
    std::array<viz::ResourceFormat, VideoFrame::kMaxPlanes>& texture_formats) {
  switch (dxgi_format) {
    case DXGI_FORMAT_NV12:
      DCHECK_EQ(textures_per_picture, 2u);
      texture_formats[0] = viz::RED_8;  // Y
      texture_formats[1] = viz::RG_88;  // UV
      return true;
    case DXGI_FORMAT_P010:
      // TODO(crbug.com/1011555): P010 formats are not fully supported.
      // Treat them to be the same as NV12 for the time being.
      DCHECK_EQ(textures_per_picture, 2u);
      texture_formats[0] = viz::RED_8;
      texture_formats[1] = viz::RG_88;
      return true;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
      DCHECK_EQ(textures_per_picture, 1u);
      if (pixel_format != PIXEL_FORMAT_ARGB) {
        return false;
      }
      texture_formats[0] = viz::BGRA_8888;
      return true;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      DCHECK_EQ(textures_per_picture, 1u);
      if (pixel_format != PIXEL_FORMAT_ARGB) {
        return false;
      }
      texture_formats[0] = viz::RGBA_F16;
      return true;
    default:  // Unsupported
      return false;
  }
}

}  // anonymous namespace

// Handy structure so that we can activate / bind one or two textures.
struct ScopedTextureEverything {
  ScopedTextureEverything(GLenum unit, GLuint service_id)
      : active_(unit), binder_(GL_TEXTURE_EXTERNAL_OES, service_id) {}
  ~ScopedTextureEverything() = default;

  // Order is important; we need |active_| to be constructed first
  // and destructed last.
  gl::ScopedActiveTexture active_;
  gl::ScopedTextureBinder binder_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTextureEverything);
};

// Another handy helper class to guarantee that ScopedTextureEverythings
// are deleted in reverse order.  This is required so that the scoped
// active texture unit doesn't change.  Surprisingly, none of the stl
// containers, or the chromium ones, seem to guarantee anything about
// the order of destruction.
struct OrderedDestructionList {
  OrderedDestructionList() = default;
  ~OrderedDestructionList() {
    // Erase last-to-first.
    while (!list_.empty())
      list_.pop_back();
  }

  template <typename... Args>
  void emplace_back(Args&&... args) {
    list_.emplace_back(std::forward<Args>(args)...);
  }

  std::list<ScopedTextureEverything> list_;
  DISALLOW_COPY_AND_ASSIGN(OrderedDestructionList);
};

Texture2DWrapper::Texture2DWrapper() = default;

Texture2DWrapper::~Texture2DWrapper() = default;

DefaultTexture2DWrapper::DefaultTexture2DWrapper(const gfx::Size& size,
                                                 DXGI_FORMAT dxgi_format,
                                                 VideoPixelFormat pixel_format)
    : size_(size), dxgi_format_(dxgi_format), pixel_format_(pixel_format) {}

DefaultTexture2DWrapper::~DefaultTexture2DWrapper() = default;

Status DefaultTexture2DWrapper::ProcessTexture(
    const gfx::ColorSpace& input_color_space,
    MailboxHolderArray* mailbox_dest,
    gfx::ColorSpace* output_color_space) {
  // If we've received an error, then return it to our caller.  This is probably
  // from some previous operation.
  // TODO(liberato): Return the error.
  if (received_error_)
    return Status(StatusCode::kProcessTextureFailed)
        .AddCause(std::move(*received_error_));

  // TODO(liberato): make sure that |mailbox_holders_| is zero-initialized in
  // case we don't use all the planes.
  for (size_t i = 0; i < VideoFrame::kMaxPlanes; i++)
    (*mailbox_dest)[i] = mailbox_holders_[i];

  // We're just binding, so the output and output color spaces are the same.
  *output_color_space = input_color_space;

  return OkStatus();
}

Status DefaultTexture2DWrapper::Init(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetCommandBufferHelperCB get_helper_cb,
    ComD3D11Texture2D texture,
    size_t array_slice) {
  gpu_resources_ = base::SequenceBound<GpuResources>(
      std::move(gpu_task_runner),
      BindToCurrentLoop(base::BindOnce(&DefaultTexture2DWrapper::OnError,
                                       weak_factory_.GetWeakPtr())));

  const size_t textures_per_picture = VideoFrame::NumPlanes(pixel_format_);

  std::array<viz::ResourceFormat, VideoFrame::kMaxPlanes> texture_formats;
  if (!DXGIFormatToVizFormat(dxgi_format_, pixel_format_, textures_per_picture,
                             texture_formats)) {
    return Status(StatusCode::kUnsupportedTextureFormatForBind);
  }

  // Generate mailboxes and holders.
  // TODO(liberato): Verify that this is really okay off the GPU main thread.
  // The current implementation is.
  std::vector<gpu::Mailbox> mailboxes;
  for (size_t texture_idx = 0; texture_idx < textures_per_picture;
       texture_idx++) {
    mailboxes.push_back(gpu::Mailbox::GenerateForSharedImage());
    mailbox_holders_[texture_idx] = gpu::MailboxHolder(
        mailboxes[texture_idx], gpu::SyncToken(), GL_TEXTURE_EXTERNAL_OES);
  }

  // Start construction of the GpuResources.
  // We send the texture itself, since we assume that we're using the angle
  // device for decoding.  Sharing seems not to work very well.  Otherwise, we
  // would create the texture with KEYED_MUTEX and NTHANDLE, then send along
  // a handle that we get from |texture| as an IDXGIResource1.
  gpu_resources_.Post(FROM_HERE, &GpuResources::Init, std::move(get_helper_cb),
                      std::move(mailboxes), GL_TEXTURE_EXTERNAL_OES, size_,
                      textures_per_picture, texture_formats, pixel_format_,
                      texture, array_slice);
  return OkStatus();
}

void DefaultTexture2DWrapper::OnError(Status status) {
  if (!received_error_)
    received_error_ = status;
}

void DefaultTexture2DWrapper::SetStreamHDRMetadata(
    const gl::HDRMetadata& stream_metadata) {}

void DefaultTexture2DWrapper::SetDisplayHDRMetadata(
    const DXGI_HDR_METADATA_HDR10& dxgi_display_metadata) {}

DefaultTexture2DWrapper::GpuResources::GpuResources(OnErrorCB on_error_cb)
    : on_error_cb_(std::move(on_error_cb)) {}

DefaultTexture2DWrapper::GpuResources::~GpuResources() {
  if (helper_ && helper_->MakeContextCurrent()) {
    for (uint32_t service_id : service_ids_)
      helper_->DestroyTexture(service_id);
  }
}

void DefaultTexture2DWrapper::GpuResources::Init(
    GetCommandBufferHelperCB get_helper_cb,
    const std::vector<gpu::Mailbox> mailboxes,
    GLenum target,
    gfx::Size size,
    size_t textures_per_picture,
    std::array<viz::ResourceFormat, VideoFrame::kMaxPlanes> texture_formats,
    VideoPixelFormat pixel_format,
    ComD3D11Texture2D texture,
    size_t array_slice) {
  helper_ = get_helper_cb.Run();

  if (!helper_ || !helper_->MakeContextCurrent()) {
    NotifyError(StatusCode::kMakeContextCurrentFailed);
    return;
  }

  // Create the stream for zero-copy use by gl.
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  const EGLint stream_attributes[] = {
      // clang-format off
      EGL_CONSUMER_LATENCY_USEC_KHR,         0,
      EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR, 0,
      EGL_NONE,
      // clang-format on
  };
  EGLStreamKHR stream = eglCreateStreamKHR(egl_display, stream_attributes);
  if (!stream) {
    NotifyError(StatusCode::kCreateEglStreamFailed);
    return;
  }

  // |stream| will be destroyed when the GLImage is.
  // TODO(liberato): for tests, it will be destroyed pretty much at the end of
  // this function unless |helper_| retains it.  Also, this won't work if we
  // have a FakeCommandBufferHelper since the service IDs aren't meaningful.
  gl_image_ = base::MakeRefCounted<gl::GLImageDXGI>(size, stream);

  // Create the textures and attach them to the mailboxes.
  // TODO(liberato): Should we use GL_FLOAT for an fp16 texture?  It doesn't
  // really seem to matter so far as I can tell.
  for (size_t texture_idx = 0; texture_idx < textures_per_picture;
       texture_idx++) {
    const viz::ResourceFormat format = texture_formats[texture_idx];
    const GLenum internal_format = viz::GLInternalFormat(format);
    const GLenum data_type = viz::GLDataType(format);
    const GLenum data_format = viz::GLDataFormat(format);

    // Adjust the size by the subsampling factor.
    const size_t width =
        VideoFrame::Columns(texture_idx, pixel_format, size.width());
    const size_t height =
        VideoFrame::Rows(texture_idx, pixel_format, size.height());
    const gfx::Size plane_size(width, height);

    // TODO(crbug.com/1011555): CreateTexture allocates a GL texture, figure out
    // if this can be removed.
    const uint32_t service_id =
        helper_->CreateTexture(target, internal_format, plane_size.width(),
                               plane_size.height(), data_format, data_type);

    const auto& mailbox = mailboxes[texture_idx];

    // Shared image does not need to store the colorspace since it is already
    // stored on the VideoFrame which is provided upon presenting the overlay.
    // To prevent the developer from mistakenly using it, provide the invalid
    // value from default-construction.
    const gfx::ColorSpace kInvalidColorSpace;

    // Usage flags to allow the display compositor to draw from it, video to
    // decode, and allow webgl/canvas access.
    const uint32_t shared_image_usage =
        gpu::SHARED_IMAGE_USAGE_VIDEO_DECODE | gpu::SHARED_IMAGE_USAGE_GLES2 |
        gpu::SHARED_IMAGE_USAGE_RASTER | gpu::SHARED_IMAGE_USAGE_DISPLAY |
        gpu::SHARED_IMAGE_USAGE_SCANOUT;

    // Create a shared image
    // TODO(crbug.com/1011555): Need key shared mutex if shared image is ever
    // used by another device.
    scoped_refptr<gpu::gles2::TexturePassthrough> gl_texture =
        gpu::gles2::TexturePassthrough::CheckedCast(
            helper_->GetTexture(service_id));

    auto shared_image = std::make_unique<gpu::SharedImageBackingD3D>(
        mailbox, format, plane_size, kInvalidColorSpace,
        kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, shared_image_usage,
        /*swap_chain=*/nullptr, std::move(gl_texture), gl_image_,
        /*buffer_index=*/0, texture, base::win::ScopedHandle(),
        /*dxgi_key_mutex=*/nullptr);

    // Caller is assumed to provide cleared d3d textures.
    shared_image->SetCleared();

    // Shared images will be destroyed when this wrapper goes away.
    // Only GpuResource can be used to safely destroy the shared images on the
    // gpu main thread.
    shared_images_.push_back(helper_->Register(std::move(shared_image)));

    service_ids_.push_back(service_id);
  }

  // Bind all the textures so that the stream can find them.
  OrderedDestructionList texture_everythings;
  for (size_t i = 0; i < textures_per_picture; i++)
    texture_everythings.emplace_back(GL_TEXTURE0 + i, service_ids_[i]);

  std::vector<EGLAttrib> consumer_attributes;
  if (textures_per_picture == 2) {
    // Assume NV12.
    consumer_attributes = {
        // clang-format off
        EGL_COLOR_BUFFER_TYPE,               EGL_YUV_BUFFER_EXT,
        EGL_YUV_NUMBER_OF_PLANES_EXT,        2,
        EGL_YUV_PLANE0_TEXTURE_UNIT_NV,      0,
        EGL_YUV_PLANE1_TEXTURE_UNIT_NV,      1,
        EGL_NONE,
        // clang-format on
    };
  } else {
    // Assume some rgb format.
    consumer_attributes = {
        // clang-format off
        EGL_COLOR_BUFFER_TYPE,               EGL_RGB_BUFFER,
        EGL_NONE,
        // clang-format on
    };
  }
  EGLBoolean result = eglStreamConsumerGLTextureExternalAttribsNV(
      egl_display, stream, consumer_attributes.data());
  if (!result) {
    NotifyError(StatusCode::kCreateEglStreamConsumerFailed);
    return;
  }

  EGLAttrib producer_attributes[] = {
      EGL_NONE,
  };

  result = eglCreateStreamProducerD3DTextureANGLE(egl_display, stream,
                                                  producer_attributes);
  if (!result) {
    NotifyError(StatusCode::kCreateEglStreamProducerFailed);
    return;
  }

  // Note that this is valid as long as |gl_image_| is valid; it is
  // what deletes the stream.
  stream_ = stream;

  // Bind the image to each texture.
  for (size_t texture_idx = 0; texture_idx < service_ids_.size();
       texture_idx++) {
    helper_->BindImage(service_ids_[texture_idx], gl_image_.get(),
                       false /* client_managed */);
  }

  // Specify the texture so ProcessTexture knows how to process it using a GL
  // image.
  gl_image_->SetTexture(texture, array_slice);

  PushNewTexture();
}

void DefaultTexture2DWrapper::GpuResources::PushNewTexture() {
  // If init didn't complete, then signal (another) error that will probably be
  // ignored in favor of whatever we signalled earlier.
  if (!gl_image_ || !stream_) {
    NotifyError(StatusCode::kDecoderInitializeNeverCompleted);
    return;
  }

  if (!helper_ || !helper_->MakeContextCurrent()) {
    NotifyError(StatusCode::kMakeContextCurrentFailed);
    return;
  }

  // Notify angle that it has a new texture.
  EGLAttrib frame_attributes[] = {
      EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE,
      gl_image_->level(),
      EGL_NONE,
  };

  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  if (!eglStreamPostD3DTextureANGLE(
          egl_display, stream_, static_cast<void*>(gl_image_->texture().Get()),
          frame_attributes)) {
    NotifyError(StatusCode::kPostTextureFailed);
    return;
  }

  if (!eglStreamConsumerAcquireKHR(egl_display, stream_)) {
    NotifyError(StatusCode::kPostAcquireStreamFailed);
    return;
  }
}

void DefaultTexture2DWrapper::GpuResources::NotifyError(Status status) {
  if (on_error_cb_)
    std::move(on_error_cb_).Run(std::move(status));
  // else this isn't the first error, so skip it.
}

}  // namespace media
