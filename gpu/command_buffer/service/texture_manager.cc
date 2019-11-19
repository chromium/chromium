// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <set>
#include <tuple>
#include <utility>

#include "base/bits.h"
#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gl_stream_texture_image.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_state_restorer.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/progress_reporter.h"
#include "ui/gl/trace_util.h"

using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryDumpLevelOfDetail;

namespace gpu {
namespace gles2 {

namespace {

// This should contain everything to uniquely identify a Texture.
const char TextureTag[] = "|Texture|";
struct TextureSignature {
  GLenum target_;
  GLint level_;
  GLenum min_filter_;
  GLenum mag_filter_;
  GLenum wrap_r_;
  GLenum wrap_s_;
  GLenum wrap_t_;
  GLenum usage_;
  GLenum internal_format_;
  GLenum compare_func_;
  GLenum compare_mode_;
  GLsizei width_;
  GLsizei height_;
  GLsizei depth_;
  GLfloat max_lod_;
  GLfloat min_lod_;
  GLint base_level_;
  GLint border_;
  GLint max_level_;
  GLenum format_;
  GLenum type_;
  bool has_image_;
  bool can_render_;
  bool can_render_to_;
  bool npot_;
  bool emulating_rgb_;

  // Since we will be hashing this signature structure, the padding must be
  // zero initialized. Although the C++11 specifications specify that this is
  // true, we will use a constructor with a memset to further enforce it instead
  // of relying on compilers adhering to this deep dark corner specification.
  TextureSignature(GLenum target,
                   GLint level,
                   const SamplerState& sampler_state,
                   GLenum usage,
                   GLenum internal_format,
                   GLsizei width,
                   GLsizei height,
                   GLsizei depth,
                   GLint base_level,
                   GLint border,
                   GLint max_level,
                   GLenum format,
                   GLenum type,
                   bool has_image,
                   bool can_render,
                   bool can_render_to,
                   bool npot,
                   bool emulating_rgb) {
    memset(this, 0, sizeof(TextureSignature));
    target_ = target;
    level_ = level;
    min_filter_ = sampler_state.min_filter;
    mag_filter_ = sampler_state.mag_filter;
    wrap_r_ = sampler_state.wrap_r;
    wrap_s_ = sampler_state.wrap_s;
    wrap_t_ = sampler_state.wrap_t;
    usage_ = usage;
    internal_format_ = internal_format;
    compare_func_ = sampler_state.compare_func;
    compare_mode_ = sampler_state.compare_mode;
    width_ = width;
    height_ = height;
    depth_ = depth;
    max_lod_ = sampler_state.max_lod;
    min_lod_ = sampler_state.min_lod;
    base_level_ = base_level;
    border_ = border;
    max_level_ = max_level;
    format_ = format;
    type_ = type;
    has_image_ = has_image;
    can_render_ = can_render;
    can_render_to_ = can_render_to;
    npot_ = npot;
    emulating_rgb_ = emulating_rgb;
  }
};

class FormatTypeValidator {
 public:
  FormatTypeValidator() {
    static const FormatType kSupportedFormatTypes[] = {
        // ES2.
        {GL_RGB, GL_RGB, GL_UNSIGNED_BYTE},
        {GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
        {GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE},
        {GL_RGBA, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},
        {GL_RGBA, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1},
        {GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE},
        {GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE},
        {GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE},

        // ES3.
        {GL_R8, GL_RED, GL_UNSIGNED_BYTE},
        {GL_R8_SNORM, GL_RED, GL_BYTE},
        {GL_R16F, GL_RED, GL_HALF_FLOAT},
        {GL_R16F, GL_RED, GL_FLOAT},
        {GL_R32F, GL_RED, GL_FLOAT},
        {GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE},
        {GL_R8I, GL_RED_INTEGER, GL_BYTE},
        {GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT},
        {GL_R16I, GL_RED_INTEGER, GL_SHORT},
        {GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT},
        {GL_R32I, GL_RED_INTEGER, GL_INT},
        {GL_RG8, GL_RG, GL_UNSIGNED_BYTE},
        {GL_RG8_SNORM, GL_RG, GL_BYTE},
        {GL_RG16F, GL_RG, GL_HALF_FLOAT},
        {GL_RG16F, GL_RG, GL_FLOAT},
        {GL_RG32F, GL_RG, GL_FLOAT},
        {GL_RG8UI, GL_RG_INTEGER, GL_UNSIGNED_BYTE},
        {GL_RG8I, GL_RG_INTEGER, GL_BYTE},
        {GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT},
        {GL_RG16I, GL_RG_INTEGER, GL_SHORT},
        {GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT},
        {GL_RG32I, GL_RG_INTEGER, GL_INT},
        {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE},
        {GL_SRGB8, GL_RGB, GL_UNSIGNED_BYTE},
        {GL_RGB565, GL_RGB, GL_UNSIGNED_BYTE},
        {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
        {GL_RGB8_SNORM, GL_RGB, GL_BYTE},
        {GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV},
        {GL_R11F_G11F_B10F, GL_RGB, GL_HALF_FLOAT},
        {GL_R11F_G11F_B10F, GL_RGB, GL_FLOAT},
        {GL_RGB9_E5, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV},
        {GL_RGB9_E5, GL_RGB, GL_HALF_FLOAT},
        {GL_RGB9_E5, GL_RGB, GL_FLOAT},
        {GL_RGB16F, GL_RGB, GL_HALF_FLOAT},
        {GL_RGB16F, GL_RGB, GL_FLOAT},
        {GL_RGB32F, GL_RGB, GL_FLOAT},
        {GL_RGB8UI, GL_RGB_INTEGER, GL_UNSIGNED_BYTE},
        {GL_RGB8I, GL_RGB_INTEGER, GL_BYTE},
        {GL_RGB16UI, GL_RGB_INTEGER, GL_UNSIGNED_SHORT},
        {GL_RGB16I, GL_RGB_INTEGER, GL_SHORT},
        {GL_RGB32UI, GL_RGB_INTEGER, GL_UNSIGNED_INT},
        {GL_RGB32I, GL_RGB_INTEGER, GL_INT},
        {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE},
        {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE},
        {GL_RGBA8_SNORM, GL_RGBA, GL_BYTE},
        {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_BYTE},
        {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1},
        {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV},
        {GL_RGBA4, GL_RGBA, GL_UNSIGNED_BYTE},
        {GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},
        {GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV},
        {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT},
        {GL_RGBA16F, GL_RGBA, GL_FLOAT},
        {GL_RGBA32F, GL_RGBA, GL_FLOAT},
        {GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE},
        {GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE},
        {GL_RGB10_A2UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV},
        {GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT},
        {GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT},
        {GL_RGBA32I, GL_RGBA_INTEGER, GL_INT},
        {GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT},
        {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT},
        {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT},
        {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT},
        {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT},
        {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8},
        {GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL,
         GL_FLOAT_32_UNSIGNED_INT_24_8_REV},

        // Exposed by GL_APPLE_texture_format_BGRA8888 for TexStorage*
        // TODO(kainino): this actually exposes it for (Copy)TexImage* as well,
        // which is incorrect. crbug.com/663086
        {GL_BGRA8_EXT, GL_BGRA_EXT, GL_UNSIGNED_BYTE},

        // Exposed by GL_APPLE_texture_format_BGRA8888 and
        // GL_EXT_texture_format_BGRA8888
        {GL_BGRA_EXT, GL_BGRA_EXT, GL_UNSIGNED_BYTE},

        // Exposed by GL_EXT_texture_norm16
        {GL_R16_EXT, GL_RED, GL_UNSIGNED_SHORT},
    };

    static const FormatType kSupportedFormatTypesES2Only[] = {
        // Exposed by GL_OES_texture_float and GL_OES_texture_half_float
        {GL_RGB, GL_RGB, GL_FLOAT},
        {GL_RGBA, GL_RGBA, GL_FLOAT},
        {GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_FLOAT},
        {GL_LUMINANCE, GL_LUMINANCE, GL_FLOAT},
        {GL_ALPHA, GL_ALPHA, GL_FLOAT},
        {GL_RGB, GL_RGB, GL_HALF_FLOAT_OES},
        {GL_RGBA, GL_RGBA, GL_HALF_FLOAT_OES},
        {GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT_OES},
        {GL_LUMINANCE, GL_LUMINANCE, GL_HALF_FLOAT_OES},
        {GL_ALPHA, GL_ALPHA, GL_HALF_FLOAT_OES},

        // Exposed by GL_ANGLE_depth_texture
        {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT},
        {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT},
        {GL_DEPTH_STENCIL, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8},

        // Exposed by GL_EXT_sRGB
        {GL_SRGB, GL_SRGB, GL_UNSIGNED_BYTE},
        {GL_SRGB_ALPHA, GL_SRGB_ALPHA, GL_UNSIGNED_BYTE},

        // Exposed by GL_EXT_texture_rg
        {GL_RED, GL_RED, GL_UNSIGNED_BYTE},
        {GL_RG, GL_RG, GL_UNSIGNED_BYTE},
        {GL_RED, GL_RED, GL_FLOAT},
        {GL_RG, GL_RG, GL_FLOAT},
        {GL_RED, GL_RED, GL_HALF_FLOAT_OES},
        {GL_RG, GL_RG, GL_HALF_FLOAT_OES},

        // Exposed by GL_EXT_texture_norm16
        {GL_RED, GL_RED, GL_UNSIGNED_SHORT},
    };

    for (size_t ii = 0; ii < base::size(kSupportedFormatTypes); ++ii) {
      supported_combinations_.insert(kSupportedFormatTypes[ii]);
    }

    for (size_t ii = 0; ii < base::size(kSupportedFormatTypesES2Only); ++ii) {
      supported_combinations_es2_only_.insert(kSupportedFormatTypesES2Only[ii]);
    }
  }

  // This may be accessed from multiple threads.
  bool IsValid(ContextType context_type, GLenum internal_format, GLenum format,
               GLenum type) const {
    FormatType query = { internal_format, format, type };
    if (supported_combinations_.find(query) != supported_combinations_.end()) {
      return true;
    }
    if (context_type == CONTEXT_TYPE_OPENGLES2 ||
        context_type == CONTEXT_TYPE_WEBGL1) {
      if (supported_combinations_es2_only_.find(query) !=
          supported_combinations_es2_only_.end()) {
        return true;
      }
    }
    return false;
  }

 private:
  // FormatType is a tuple of <internal_format, format, type>
  typedef std::tuple<GLenum, GLenum, GLenum> FormatType;
  struct FormatTypeCompare {
    bool operator() (const FormatType& lhs, const FormatType& rhs) const {
      return (std::get<0>(lhs) < std::get<0>(rhs) ||
              ((std::get<0>(lhs) == std::get<0>(rhs)) &&
               (std::get<1>(lhs) < std::get<1>(rhs))) ||
              ((std::get<0>(lhs) == std::get<0>(rhs)) &&
               (std::get<1>(lhs) == std::get<1>(rhs)) &&
               (std::get<2>(lhs) < std::get<2>(rhs))));
    }
  };

  // This class needs to be thread safe, so once supported_combinations_
  // are initialized in the constructor, it should never be modified later.
  std::set<FormatType, FormatTypeCompare> supported_combinations_;
  std::set<FormatType, FormatTypeCompare> supported_combinations_es2_only_;
};

static const Texture::CompatibilitySwizzle kSwizzledFormats[] = {
    {GL_ALPHA, GL_RED, GL_ZERO, GL_ZERO, GL_ZERO, GL_RED},
    {GL_LUMINANCE, GL_RED, GL_RED, GL_RED, GL_RED, GL_ONE},
    {GL_LUMINANCE_ALPHA, GL_RG, GL_RED, GL_RED, GL_RED, GL_GREEN},
};

const Texture::CompatibilitySwizzle* GetCompatibilitySwizzleInternal(
    GLenum format) {
  size_t count = base::size(kSwizzledFormats);
  for (size_t i = 0; i < count; ++i) {
    if (kSwizzledFormats[i].format == format)
      return &kSwizzledFormats[i];
  }
  return nullptr;
}

GLenum GetSwizzleForChannel(GLenum channel,
                            const Texture::CompatibilitySwizzle* swizzle) {
  if (!swizzle)
    return channel;

  switch (channel) {
    case GL_ZERO:
      return GL_ZERO;
    case GL_ONE:
      return GL_ONE;
    case GL_RED:
      return swizzle->red;
    case GL_GREEN:
      return swizzle->green;
    case GL_BLUE:
      return swizzle->blue;
    case GL_ALPHA:
      return swizzle->alpha;
    default:
      NOTREACHED();
      return GL_NONE;
  }
}

bool SizedFormatAvailable(const FeatureInfo* feature_info,
                          bool immutable,
                          GLenum internal_format) {
  if (immutable)
    return true;

  if (feature_info->feature_flags().ext_texture_norm16 &&
      internal_format == GL_R16_EXT) {
    return true;
  }

  if ((feature_info->feature_flags().chromium_image_ycbcr_420v &&
       internal_format == GL_RGB_YCBCR_420V_CHROMIUM) ||
      (feature_info->feature_flags().chromium_image_ycbcr_422 &&
       internal_format == GL_RGB_YCBCR_422_CHROMIUM)) {
    return true;
  }

  if (internal_format == GL_RGB10_A2_EXT &&
      (feature_info->feature_flags().chromium_image_xr30 ||
       feature_info->feature_flags().chromium_image_xb30)) {
    return true;
  }

  // TODO(dshwang): check if it's possible to remove
  // CHROMIUM_color_buffer_float_rgb. crbug.com/329605
  if (feature_info->feature_flags().chromium_color_buffer_float_rgb &&
      internal_format == GL_RGB32F) {
    return true;
  }
  if (feature_info->feature_flags().chromium_color_buffer_float_rgba &&
      internal_format == GL_RGBA32F) {
    return true;
  }
  // RGBA16F textures created as WebGL 2 backbuffers (in GLES3 contexts) may be
  // shared with compositor GLES2 contexts for compositing.
  // https://crbug.com/777750
  if (feature_info->feature_flags().enable_texture_half_float_linear &&
      internal_format == GL_RGBA16F) {
    return true;
  }
  return feature_info->IsWebGL2OrES3Context();
}

base::LazyInstance<const FormatTypeValidator>::Leaky g_format_type_validator =
    LAZY_INSTANCE_INITIALIZER;

class ScopedResetPixelUnpackBuffer{
 public:
  explicit ScopedResetPixelUnpackBuffer(ContextState* state)
      : buffer_(nullptr) {
    buffer_ = state->bound_pixel_unpack_buffer.get();
    if (buffer_) {
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
  }

  ~ScopedResetPixelUnpackBuffer() {
    if (buffer_) {
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer_->service_id());
    }
  }

 private:
    Buffer* buffer_;
};

class ScopedMemTrackerChange {
 public:
  explicit ScopedMemTrackerChange(Texture* texture)
      : texture_(texture),
        previous_tracker_(texture->GetMemTracker()),
        previous_size_(texture->estimated_size()) {}
  ~ScopedMemTrackerChange() {
    MemoryTypeTracker* new_tracker = texture_->GetMemTracker();
    uint32_t new_size = texture_->estimated_size();
    if ((new_tracker == previous_tracker_) && (new_size == previous_size_))
      return;
    if (previous_tracker_)
      previous_tracker_->TrackMemFree(previous_size_);
    if (new_tracker)
      new_tracker->TrackMemAlloc(new_size);
  }

 private:
  Texture* texture_;
  MemoryTypeTracker* previous_tracker_;
  uint32_t previous_size_;
};

}  // namespace anonymous

DecoderTextureState::DecoderTextureState(
    const GpuDriverBugWorkarounds& workarounds)
    : tex_image_failed(false),
      force_cube_map_positive_x_allocation(
          workarounds.force_cube_map_positive_x_allocation),
      force_cube_complete(workarounds.force_cube_complete),
      force_int_or_srgb_cube_texture_complete(
          workarounds.force_int_or_srgb_cube_texture_complete),
      unpack_alignment_workaround_with_unpack_buffer(
          workarounds.unpack_alignment_workaround_with_unpack_buffer),
      unpack_overlapping_rows_separately_unpack_buffer(
          workarounds.unpack_overlapping_rows_separately_unpack_buffer),
      unpack_image_height_workaround_with_unpack_buffer(
          workarounds.unpack_image_height_workaround_with_unpack_buffer) {}

TextureManager::DestructionObserver::DestructionObserver() = default;

TextureManager::DestructionObserver::~DestructionObserver() = default;

TextureManager::~TextureManager() {
  for (unsigned int i = 0; i < destruction_observers_.size(); i++)
    destruction_observers_[i]->OnTextureManagerDestroying(this);

  DCHECK(textures_.empty());

  // If this triggers, that means something is keeping a reference to
  // a Texture belonging to this.
  CHECK_EQ(texture_count_, 0u);

  DCHECK_EQ(0, num_unsafe_textures_);
  DCHECK_EQ(0, num_uncleared_mips_);
  DCHECK_EQ(0, num_images_);

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void TextureManager::MarkContextLost() {
  have_context_ = false;
}

void TextureManager::Destroy() {
  // Retreive any outstanding unlocked textures from the discardable manager so
  // we can clean them up here.
  discardable_manager_->OnTextureManagerDestruction(this);

  while (!textures_.empty()) {
    textures_.erase(textures_.begin());
    if (progress_reporter_)
      progress_reporter_->ReportProgress();
  }
  for (int ii = 0; ii < kNumDefaultTextures; ++ii) {
    default_textures_[ii] = nullptr;
    if (progress_reporter_)
      progress_reporter_->ReportProgress();
  }

  if (have_context_) {
    glDeleteTextures(base::size(black_texture_ids_), black_texture_ids_);
  }

  DCHECK_EQ(0u, memory_type_tracker_->GetMemRepresented());
}

TexturePassthrough::LevelInfo::LevelInfo() = default;

TexturePassthrough::LevelInfo::LevelInfo(const LevelInfo& rhs) = default;

TexturePassthrough::LevelInfo::~LevelInfo() = default;

TexturePassthrough::TexturePassthrough(GLuint service_id, GLenum target)
    : TextureBase(service_id),
      owned_service_id_(service_id),
      have_context_(true),
      level_images_(target == GL_TEXTURE_CUBE_MAP ? 6 : 1) {
  TextureBase::SetTarget(target);
}

TexturePassthrough::TexturePassthrough(GLuint service_id,
                                       GLenum target,
                                       GLenum internal_format,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth,
                                       GLint border,
                                       GLenum format,
                                       GLenum type)
    : TexturePassthrough(service_id, target) {
  DCHECK(target != GL_TEXTURE_CUBE_MAP);
  LevelInfo* level_info = GetLevelInfo(target, 0);
  level_info->internal_format = internal_format;
  level_info->width = width;
  level_info->height = height;
  level_info->depth = depth;
  level_info->border = border;
  level_info->format = format;
  level_info->type = type;
}

TexturePassthrough::~TexturePassthrough() {
  DeleteFromMailboxManager();
  if (have_context_) {
    glDeleteTextures(1, &owned_service_id_);
  }
}

TextureBase::Type TexturePassthrough::GetType() const {
  return TextureBase::Type::kPassthrough;
}

// static
TexturePassthrough* TexturePassthrough::CheckedCast(TextureBase* texture) {
  if (!texture)
    return nullptr;
  if (texture->GetType() == TextureBase::Type::kPassthrough)
    return static_cast<TexturePassthrough*>(texture);
  DLOG(ERROR) << "Bad typecast";
  return nullptr;
}

void TexturePassthrough::MarkContextLost() {
  have_context_ = false;
}

void TexturePassthrough::SetLevelImage(GLenum target,
                                       GLint level,
                                       gl::GLImage* image) {
  SetLevelImageInternal(target, level, image, nullptr, owned_service_id_);
}

gl::GLImage* TexturePassthrough::GetLevelImage(GLenum target,
                                               GLint level) const {
  size_t face_idx = 0;
  if (!LevelInfoExists(target, level, &face_idx)) {
    return nullptr;
  }

  return level_images_[face_idx][level].image.get();
}

void TexturePassthrough::SetStreamLevelImage(
    GLenum target,
    GLint level,
    GLStreamTextureImage* stream_texture_image,
    GLuint service_id) {
  SetLevelImageInternal(target, level, stream_texture_image,
                        stream_texture_image, service_id);
}

GLStreamTextureImage* TexturePassthrough::GetStreamLevelImage(
    GLenum target,
    GLint level) const {
  size_t face_idx = 0;
  if (!LevelInfoExists(target, level, &face_idx)) {
    return nullptr;
  }

  return level_images_[face_idx][level].stream_texture_image.get();
}

void TexturePassthrough::SetEstimatedSize(size_t size) {
  estimated_size_ = size;
}

bool TexturePassthrough::LevelInfoExists(GLenum target,
                                         GLint level,
                                         size_t* out_face_idx) const {
  DCHECK(out_face_idx);

  if (GLES2Util::GLFaceTargetToTextureTarget(target) != target_) {
    return false;
  }

  size_t face_idx = GLES2Util::GLTargetToFaceIndex(target);
  DCHECK(face_idx < level_images_.size());
  DCHECK(level >= 0);

  if (static_cast<GLint>(level_images_[face_idx].size()) <= level) {
    return false;
  }

  *out_face_idx = face_idx;
  return true;
}

void TexturePassthrough::SetLevelImageInternal(
    GLenum target,
    GLint level,
    gl::GLImage* image,
    GLStreamTextureImage* stream_texture_image,
    GLuint service_id) {
  LevelInfo* level_info = GetLevelInfo(target, level);
  level_info->image = image;
  level_info->stream_texture_image = stream_texture_image;

  if (service_id != 0 && service_id != service_id_) {
    service_id_ = service_id;
  }

  if (stream_texture_image &&
      gl::g_current_gl_driver->ext.b_GL_ANGLE_texture_external_update) {
    // Notify the texture that its size has changed
    GLint prev_texture = 0;
    glGetIntegerv(GetTextureBindingQuery(target_), &prev_texture);
    glBindTexture(target_, service_id_);

    glTexImage2DExternalANGLE(target_, level, level_info->internal_format,
                              level_info->width, level_info->height,
                              level_info->border, level_info->format,
                              level_info->type);

    glBindTexture(target_, prev_texture);
  }
}

TexturePassthrough::LevelInfo* TexturePassthrough::GetLevelInfo(GLenum target,
                                                                GLint level) {
  size_t face_idx = GLES2Util::GLTargetToFaceIndex(target);
  DCHECK(face_idx < level_images_.size());
  DCHECK(level >= 0);

  // Don't allocate space for the images until needed
  if (static_cast<GLint>(level_images_[face_idx].size()) <= level) {
    level_images_[face_idx].resize(level + 1);
  }

  return &level_images_[face_idx][level];
}

Texture::Texture(GLuint service_id)
    : TextureBase(service_id),
      owned_service_id_(service_id) {}

Texture::~Texture() {
  DeleteFromMailboxManager();
}

void Texture::AddTextureRef(TextureRef* ref) {
  DCHECK(refs_.find(ref) == refs_.end());
  refs_.insert(ref);
  ScopedMemTrackerChange change(this);
  if (!memory_tracking_ref_)
    memory_tracking_ref_ = ref;
}

void Texture::RemoveTextureRef(TextureRef* ref, bool have_context) {
  {
    ScopedMemTrackerChange change(this);
    if (memory_tracking_ref_ == ref)
      memory_tracking_ref_ = nullptr;
    size_t result = refs_.erase(ref);
    DCHECK_EQ(result, 1u);
    if (!memory_tracking_ref_ && !refs_.empty())
      memory_tracking_ref_ = *refs_.begin();
  }
  MaybeDeleteThis(have_context);
}

void Texture::SetLightweightRef() {
  ScopedMemTrackerChange change(this);
  has_lightweight_ref_ = true;
}

void Texture::RemoveLightweightRef(bool have_context) {
  DCHECK(has_lightweight_ref_);
  {
    ScopedMemTrackerChange change(this);
    has_lightweight_ref_ = false;
  }
  MaybeDeleteThis(have_context);
}

void Texture::MaybeDeleteThis(bool have_context) {
  if (!refs_.empty() || has_lightweight_ref_)
    return;
  if (have_context)
    glDeleteTextures(1, &owned_service_id_);
  delete this;
}

TextureBase::Type Texture::GetType() const {
  return TextureBase::Type::kValidated;
}

// static
Texture* Texture::CheckedCast(TextureBase* texture) {
  if (!texture)
    return nullptr;
  if (texture->GetType() == TextureBase::Type::kValidated)
    return static_cast<Texture*>(texture);
  DLOG(ERROR) << "Bad typecast";
  return nullptr;
}

MemoryTypeTracker* Texture::GetMemTracker() {
  if (has_lightweight_ref_) {
    // Memory tracking is handled externally in the SharedImage system.
    return nullptr;
  } else if (memory_tracking_ref_) {
    return memory_tracking_ref_->manager()->GetMemTracker();
  } else {
    return nullptr;
  }
}

Texture::LevelInfo::LevelInfo()
    : target(0),
      level(-1),
      internal_format(0),
      width(0),
      height(0),
      depth(0),
      border(0),
      format(0),
      type(0),
      image_state(UNBOUND),
      estimated_size(0),
      internal_workaround(false) {}

Texture::LevelInfo::LevelInfo(const LevelInfo& rhs)
    : cleared_rect(rhs.cleared_rect),
      target(rhs.target),
      level(rhs.level),
      internal_format(rhs.internal_format),
      width(rhs.width),
      height(rhs.height),
      depth(rhs.depth),
      border(rhs.border),
      format(rhs.format),
      type(rhs.type),
      image(rhs.image),
      image_state(rhs.image_state),
      estimated_size(rhs.estimated_size),
      internal_workaround(rhs.internal_workaround) {}

Texture::LevelInfo::~LevelInfo() = default;

Texture::FaceInfo::FaceInfo()
    : num_mip_levels(0) {
}

Texture::FaceInfo::FaceInfo(const FaceInfo& other) = default;

Texture::FaceInfo::~FaceInfo() = default;

Texture::CanRenderCondition Texture::GetCanRenderCondition() const {
  if (target_ == 0)
    return CAN_RENDER_ALWAYS;

  if (face_infos_.empty() ||
      static_cast<size_t>(base_level_) >= face_infos_[0].level_infos.size()) {
    return CAN_RENDER_NEVER;
  }
  const Texture::LevelInfo& first_face =
      face_infos_[0].level_infos[base_level_];
  if (first_face.width == 0 || first_face.height == 0 ||
      first_face.depth == 0) {
    return CAN_RENDER_NEVER;
  }

  if (target_ == GL_TEXTURE_CUBE_MAP && !cube_complete())
    return CAN_RENDER_NEVER;

  // Texture may be renderable, but it depends on the sampler it's used with,
  // the context that's using it, and the extensions available.
  return CAN_RENDER_NEEDS_VALIDATION;
}

bool Texture::CanRender(const FeatureInfo* feature_info) const {
  return CanRenderWithSampler(feature_info, sampler_state());
}

bool Texture::CanRenderWithSampler(const FeatureInfo* feature_info,
                                   const SamplerState& sampler_state) const {
  switch (can_render_condition_) {
    case CAN_RENDER_ALWAYS:
      return true;
    case CAN_RENDER_NEVER:
      return false;
    case CAN_RENDER_NEEDS_VALIDATION:
      break;
  }

  bool needs_mips = sampler_state.min_filter != GL_NEAREST &&
                    sampler_state.min_filter != GL_LINEAR;
  if (target_ == GL_TEXTURE_EXTERNAL_OES) {
    if (needs_mips) {
      return false;
    }
    if (sampler_state.wrap_s != GL_CLAMP_TO_EDGE ||
        sampler_state.wrap_t != GL_CLAMP_TO_EDGE) {
      return false;
    }
    return true;
  }

  if (needs_mips && !texture_complete()) {
    return false;
  }
  if ((sampler_state.min_filter != GL_NEAREST &&
       sampler_state.min_filter != GL_NEAREST_MIPMAP_NEAREST) ||
      sampler_state.mag_filter != GL_NEAREST) {
    DCHECK(!face_infos_.empty());
    DCHECK_LT(static_cast<size_t>(base_level_),
              face_infos_[0].level_infos.size());
    const Texture::LevelInfo& first_level =
        face_infos_[0].level_infos[base_level_];
    if ((GLES2Util::GetChannelsForFormat(first_level.internal_format) &
         (GLES2Util::kDepth | GLES2Util::kStencil)) != 0) {
      if (sampler_state.compare_mode == GL_NONE) {
        // In ES2 with OES_depth_texture, such limitation isn't specified.
        if (feature_info->IsWebGL2OrES3Context()) {
          return false;
        }
      }
    } else if (feature_info->validators()->compressed_texture_format.IsValid(
        first_level.internal_format)) {
      // TODO(zmo): The assumption that compressed textures are all filterable
      // may not be true in the future.
    } else {
      if (!Texture::TextureFilterable(feature_info, first_level.internal_format,
                                      first_level.type, immutable_)) {
        return false;
      }
    }
  }

  if (!feature_info->IsWebGL2OrES3Context()) {
    bool is_npot_compatible = !needs_mips &&
        sampler_state.wrap_s == GL_CLAMP_TO_EDGE &&
        sampler_state.wrap_t == GL_CLAMP_TO_EDGE;

    if (!is_npot_compatible) {
      if (target_ == GL_TEXTURE_RECTANGLE_ARB)
        return false;
      else if (npot())
        return feature_info->feature_flags().npot_ok;
    }
  }

  return true;
}

void Texture::AddToSignature(
    const FeatureInfo* feature_info,
    GLenum target,
    GLint level,
    std::string* signature) const {
  DCHECK(feature_info);
  DCHECK(signature);
  DCHECK_GE(level, 0);
  size_t face_index = GLES2Util::GLTargetToFaceIndex(target);
  DCHECK_LT(face_index, face_infos_.size());
  DCHECK_LT(static_cast<size_t>(level),
            face_infos_[face_index].level_infos.size());

  const Texture::LevelInfo& info =
      face_infos_[face_index].level_infos[level];

  TextureSignature signature_data(
      target, level, sampler_state_, usage_, info.internal_format, info.width,
      info.height, info.depth, base_level_, info.border, max_level_,
      info.format, info.type, info.image.get() != nullptr,
      CanRender(feature_info), CanRenderTo(feature_info, level), npot_,
      emulating_rgb_);

  signature->append(TextureTag, sizeof(TextureTag));
  signature->append(reinterpret_cast<const char*>(&signature_data),
                    sizeof(signature_data));
}

void Texture::MarkMipmapsGenerated() {
  for (size_t ii = 0; ii < face_infos_.size(); ++ii) {
    const Texture::FaceInfo& face_info = face_infos_[ii];
    const Texture::LevelInfo& level0_info = face_info.level_infos[base_level_];
    GLsizei width = level0_info.width;
    GLsizei height = level0_info.height;
    GLsizei depth = level0_info.depth;
    GLenum target = target_ == GL_TEXTURE_CUBE_MAP ?
        GLES2Util::IndexToGLFaceTarget(ii) : target_;

    const GLsizei num_mips = face_info.num_mip_levels;
    for (GLsizei level = base_level_ + 1;
         level < base_level_ + num_mips; ++level) {
      width = std::max(1, width >> 1);
      height = std::max(1, height >> 1);
      depth = target == GL_TEXTURE_2D_ARRAY ? depth : std::max(1, depth >> 1);
      SetLevelInfo(target, level, level0_info.internal_format,
                   width, height, depth, level0_info.border, level0_info.format,
                   level0_info.type, gfx::Rect(width, height));
    }
  }
}

void Texture::SetTarget(GLenum target, GLint max_levels) {
  TextureBase::SetTarget(target);
  size_t num_faces = (target == GL_TEXTURE_CUBE_MAP) ? 6 : 1;
  face_infos_.resize(num_faces);
  for (size_t ii = 0; ii < num_faces; ++ii) {
    face_infos_[ii].level_infos.resize(max_levels);
  }

  if (target == GL_TEXTURE_EXTERNAL_OES || target == GL_TEXTURE_RECTANGLE_ARB) {
    sampler_state_.min_filter = GL_LINEAR;
    sampler_state_.wrap_s = sampler_state_.wrap_t = GL_CLAMP_TO_EDGE;
  }

  if (target == GL_TEXTURE_EXTERNAL_OES) {
    immutable_ = true;
  }
  Update();
  UpdateCanRenderCondition();
}

bool Texture::CanGenerateMipmaps(const FeatureInfo* feature_info) const {
  if ((npot() && !feature_info->feature_flags().npot_ok) ||
      face_infos_.empty() ||
      target_ == GL_TEXTURE_EXTERNAL_OES ||
      target_ == GL_TEXTURE_RECTANGLE_ARB) {
    return false;
  }

  if (static_cast<size_t>(base_level_) >= face_infos_[0].level_infos.size()) {
    return false;
  }

  // Can't generate mips for depth or stencil textures.
  const Texture::LevelInfo& base = face_infos_[0].level_infos[base_level_];
  uint32_t channels = GLES2Util::GetChannelsForFormat(base.format);
  if (channels & (GLES2Util::kDepth | GLES2Util::kStencil)) {
    return false;
  }

  // WebGL forbids generating mipmaps on zero-size textures.
  // See https://crbug.com/898351
  if (feature_info->IsWebGLContext() && (base.width == 0 || base.height == 0)) {
    return false;
  }

  // According to the OpenGL extension spec EXT_sRGB.txt, EXT_SRGB is based on
  // ES 2.0 and generateMipmap is not allowed if texture format is SRGB_EXT or
  // SRGB_ALPHA_EXT.
  if (feature_info->IsWebGL1OrES2Context() &&
      (base.format == GL_SRGB_EXT || base.format == GL_SRGB_ALPHA_EXT)) {
    return false;
  }

  if (!feature_info->validators()->texture_unsized_internal_format.IsValid(
      base.internal_format)) {
    if (!Texture::ColorRenderable(feature_info, base.internal_format,
                                  immutable_) ||
        !Texture::TextureFilterable(feature_info, base.internal_format,
                                    base.type,
                                    immutable_)) {
      return false;
    }
  }

  for (size_t ii = 0; ii < face_infos_.size(); ++ii) {
    const LevelInfo& info = face_infos_[ii].level_infos[base_level_];
    if ((info.target == 0) ||
        feature_info->validators()->compressed_texture_format.IsValid(
            info.internal_format) ||
        info.image.get()) {
      return false;
    }
  }
  if (face_infos_.size() == 6 && !cube_complete()) {
    return false;
  }
  return true;
}

bool Texture::TextureIsNPOT(GLsizei width,
                            GLsizei height,
                            GLsizei depth) {
  return (GLES2Util::IsNPOT(width) ||
          GLES2Util::IsNPOT(height) ||
          GLES2Util::IsNPOT(depth));
}

bool Texture::TextureFaceComplete(const Texture::LevelInfo& first_face,
                                  size_t face_index,
                                  GLenum target,
                                  GLenum internal_format,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth,
                                  GLenum format,
                                  GLenum type) {
  bool complete = (target != 0 && depth == 1);
  if (face_index != 0) {
    complete &= (width == first_face.width &&
                 height == first_face.height &&
                 internal_format == first_face.internal_format &&
                 format == first_face.format &&
                 type == first_face.type);
  }
  return complete;
}

bool Texture::TextureMipComplete(const Texture::LevelInfo& base_level_face,
                                 GLenum target,
                                 GLint level_diff,
                                 GLenum internal_format,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLenum format,
                                 GLenum type) {
  bool complete = (target != 0);
  if (level_diff > 0) {
    const GLsizei mip_width = std::max(1, base_level_face.width >> level_diff);
    const GLsizei mip_height =
        std::max(1, base_level_face.height >> level_diff);
    const GLsizei mip_depth = target == GL_TEXTURE_2D_ARRAY ?
        base_level_face.depth :
        std::max(1, base_level_face.depth >> level_diff);

    complete &= (width == mip_width &&
                 height == mip_height &&
                 depth == mip_depth &&
                 internal_format == base_level_face.internal_format &&
                 format == base_level_face.format &&
                 type == base_level_face.type);
  }
  return complete;
}

// static
bool Texture::ColorRenderable(const FeatureInfo* feature_info,
                              GLenum internal_format,
                              bool immutable) {
  if (feature_info->validators()->texture_unsized_internal_format.IsValid(
      internal_format)) {
    return internal_format != GL_ALPHA && internal_format != GL_LUMINANCE &&
           internal_format != GL_LUMINANCE_ALPHA &&
           internal_format != GL_SRGB_EXT;
  }

  return SizedFormatAvailable(feature_info, immutable, internal_format) &&
         feature_info->validators()
             ->texture_sized_color_renderable_internal_format.IsValid(
                 internal_format);
}

// static
bool Texture::TextureFilterable(const FeatureInfo* feature_info,
                                GLenum internal_format,
                                GLenum type,
                                bool immutable) {
  if (feature_info->validators()->texture_unsized_internal_format.IsValid(
      internal_format)) {
    switch (type) {
      case GL_FLOAT:
        return feature_info->feature_flags().enable_texture_float_linear;
      case GL_HALF_FLOAT_OES:
        return feature_info->feature_flags().enable_texture_half_float_linear;
      default:
        // GL_HALF_FLOAT is ES3 only and should only be used with sized formats.
        return true;
    }
  }
  return SizedFormatAvailable(feature_info, immutable, internal_format) &&
         feature_info->validators()
             ->texture_sized_texture_filterable_internal_format.IsValid(
                 internal_format);
}

void Texture::SetLevelClearedRect(GLenum target,
                                  GLint level,
                                  const gfx::Rect& cleared_rect) {
  DCHECK_GE(level, 0);
  size_t face_index = GLES2Util::GLTargetToFaceIndex(target);
  DCHECK_LT(face_index, face_infos_.size());
  DCHECK_LT(static_cast<size_t>(level),
            face_infos_[face_index].level_infos.size());
  Texture::LevelInfo& info =
      face_infos_[face_index].level_infos[level];
  UpdateMipCleared(&info, info.width, info.height, cleared_rect);
  UpdateCleared();
}

void Texture::SetLevelCleared(GLenum target, GLint level, bool cleared) {
  DCHECK_GE(level, 0);
  size_t face_index = GLES2Util::GLTargetToFaceIndex(target);
  DCHECK_LT(face_index, face_infos_.size());
  DCHECK_LT(static_cast<size_t>(level),
            face_infos_[face_index].level_infos.size());
  Texture::LevelInfo& info = face_infos_[face_index].level_infos[level];
  UpdateMipCleared(&info, info.width, info.height,
                   cleared ? gfx::Rect(info.width, info.height) : gfx::Rect());
  UpdateCleared();
}

void Texture::UpdateCleared() {
  if (face_infos_.empty()) {
    return;
  }

  const bool cleared = (num_uncleared_mips_ == 0);

  // If texture is uncleared and is attached to a framebuffer,
  // that framebuffer must be marked possibly incomplete.
  if (!cleared && IsAttachedToFramebuffer()) {
    IncAllFramebufferStateChangeCount();
  }

  UpdateSafeToRenderFrom(cleared);
}

void Texture::UpdateSafeToRenderFrom(bool cleared) {
  if (cleared_ == cleared)
    return;
  cleared_ = cleared;
  int delta = cleared ? -1 : +1;
  for (RefSet::iterator it = refs_.begin(); it != refs_.end(); ++it)
    (*it)->manager()->UpdateSafeToRenderFrom(delta);
}

void Texture::UpdateMipCleared(LevelInfo* info,
                               GLsizei width,
                               GLsizei height,
                               const gfx::Rect& cleared_rect) {
  bool was_cleared = info->cleared_rect == gfx::Rect(info->width, info->height);
  info->width = width;
  info->height = height;
  info->cleared_rect = cleared_rect;
  bool cleared = info->cleared_rect == gfx::Rect(info->width, info->height);
  if (cleared == was_cleared)
    return;
  int delta = cleared ? -1 : +1;
  num_uncleared_mips_ += delta;
  for (RefSet::iterator it = refs_.begin(); it != refs_.end(); ++it)
    (*it)->manager()->UpdateUnclearedMips(delta);
}

void Texture::UpdateCanRenderCondition() {
  can_render_condition_ = GetCanRenderCondition();
}

void Texture::UpdateHasImages() {
  if (face_infos_.empty())
    return;

  bool has_images = false;
  for (size_t ii = 0; ii < face_infos_.size(); ++ii) {
    for (size_t jj = 0; jj < face_infos_[ii].level_infos.size(); ++jj) {
      const Texture::LevelInfo& info = face_infos_[ii].level_infos[jj];
      if (info.image.get() != nullptr) {
        has_images = true;
        break;
      }
    }
  }

  if (has_images_ == has_images)
    return;
  has_images_ = has_images;
  int delta = has_images ? +1 : -1;
  for (RefSet::iterator it = refs_.begin(); it != refs_.end(); ++it)
    (*it)->manager()->UpdateNumImages(delta);
}

void Texture::UpdateEmulatingRGB() {
  for (const FaceInfo& face_info : face_infos_) {
    for (const LevelInfo& level_info : face_info.level_infos) {
      if (level_info.image && level_info.image->EmulatingRGB()) {
        emulating_rgb_ = true;
        return;
      }
    }
  }
  emulating_rgb_ = false;
}


void Texture::IncAllFramebufferStateChangeCount() {
  for (RefSet::iterator it = refs_.begin(); it != refs_.end(); ++it)
    (*it)->manager()->IncFramebufferStateChangeCount();
}

void Texture::UpdateBaseLevel(GLint base_level,
                              const FeatureInfo* feature_info) {
  if (unclamped_base_level_ == base_level)
    return;
  unclamped_base_level_ = base_level;

  UpdateNumMipLevels();
  ApplyFormatWorkarounds(feature_info);
}

void Texture::UpdateMaxLevel(GLint max_level) {
  if (unclamped_max_level_ == max_level)
    return;
  unclamped_max_level_ = max_level;

  UpdateNumMipLevels();
}

void Texture::UpdateFaceNumMipLevels(size_t face_index,
                                     GLint width,
                                     GLint height,
                                     GLint depth) {
  DCHECK_LT(face_index, face_infos_.size());
  DCHECK_LE(0, base_level_);
  Texture::FaceInfo& face_info = face_infos_[face_index];
  if (static_cast<size_t>(base_level_) >= face_info.level_infos.size()) {
    face_info.num_mip_levels = 0;
  } else {
    DCHECK_LE(1u, face_info.level_infos.size());
    GLint safe_max_level = std::min(
        max_level_, static_cast<GLint>(face_info.level_infos.size() - 1));
    GLint max_num_mip_levels = std::max(0, safe_max_level - base_level_ + 1);
    face_info.num_mip_levels = std::min(
        max_num_mip_levels,
        TextureManager::ComputeMipMapCount(target_, width, height, depth));
  }
}

void Texture::UpdateFaceNumMipLevels(size_t face_index) {
  DCHECK_LT(face_index, face_infos_.size());
  DCHECK_LE(0, base_level_);
  Texture::FaceInfo& face_info = face_infos_[face_index];
  GLint width = 0, height = 0, depth = 0;
  if (static_cast<size_t>(base_level_) < face_info.level_infos.size()) {
    const Texture::LevelInfo& info = face_info.level_infos[base_level_];
    width = info.width;
    height = info.height;
    depth = info.depth;
  }
  UpdateFaceNumMipLevels(face_index, width, height, depth);
}

void Texture::UpdateNumMipLevels() {
  if (face_infos_.empty())
    return;

  if (immutable_) {
    GLint levels = GetImmutableLevels();
    DCHECK_LE(1, levels);
    DCHECK_LE(0, unclamped_base_level_);
    DCHECK_LE(0, unclamped_max_level_);
    base_level_ = std::min(unclamped_base_level_, levels - 1);
    max_level_ = std::max(base_level_, unclamped_max_level_);
    max_level_ = std::min(max_level_, levels - 1);
  } else {
    base_level_ = unclamped_base_level_;
    max_level_ = unclamped_max_level_;
  }
  for (size_t ii = 0; ii < face_infos_.size(); ++ii)
    UpdateFaceNumMipLevels(ii);

  // mipmap-completeness needs to be re-evaluated.
  completeness_dirty_ = true;
  Update();
  UpdateCanRenderCondition();
}

void Texture::ApplyClampedBaseLevelAndMaxLevelToDriver() {
  if (base_level_ != unclamped_base_level_) {
    glTexParameteri(target_, GL_TEXTURE_BASE_LEVEL, base_level_);
  }
  if (max_level_ != unclamped_max_level_) {
    glTexParameteri(target_, GL_TEXTURE_MAX_LEVEL, max_level_);
  }
}

void Texture::SetLevelInfo(GLenum target,
                           GLint level,
                           GLenum internal_format,
                           GLsizei width,
                           GLsizei height,
                           GLsizei depth,
                           GLint border,
                           GLenum format,
                           GLenum type,
                           const gfx::Rect& cleared_rect) {
  DCHECK_GE(level, 0);
  size_t face_index = GLES2Util::GLTargetToFaceIndex(target);
  DCHECK_LT(face_index, face_infos_.size());
  DCHECK_LT(static_cast<size_t>(level),
            face_infos_[face_index].level_infos.size());
  DCHECK_GE(width, 0);
  DCHECK_GE(height, 0);
  DCHECK_GE(depth, 0);
  Texture::LevelInfo& info =
      face_infos_[face_index].level_infos[level];

  // Update counters only if any attributes have changed. Counters are
  // comparisons between the old and new values so it must be done before any
  // assignment has been done to the LevelInfo.
  if (info.target != target || info.internal_format != internal_format ||
      info.width != width || info.height != height || info.depth != depth ||
      info.format != format || info.type != type || info.internal_workaround) {
    if (level == base_level_) {
      UpdateFaceNumMipLevels(face_index, width, height, depth);

      // Update NPOT face count for the first level.
      bool prev_npot = TextureIsNPOT(info.width, info.height, info.depth);
      bool now_npot = TextureIsNPOT(width, height, depth);
      if (prev_npot != now_npot)
        num_npot_faces_ += now_npot ? 1 : -1;
    }

    // Signify that at least one of the mips has changed.
    completeness_dirty_ = true;
  }

  info.target = target;
  info.level = level;
  info.internal_format = internal_format;
  info.depth = depth;
  info.border = border;
  info.format = format;
  info.type = type;
  info.image.reset();
  info.stream_texture_image.reset();
  info.image_state = UNBOUND;
  info.internal_workaround = false;

  UpdateMipCleared(&info, width, height, cleared_rect);

  {
    ScopedMemTrackerChange change(this);
    estimated_size_ -= info.estimated_size;

    if (format != GL_NONE) {
      // Uncompressed image
      GLES2Util::ComputeImageDataSizes(width, height, depth, format, type, 4,
                                       &info.estimated_size, nullptr, nullptr);
    } else if (internal_format != GL_NONE) {
      // Compressed image
      GLsizei compressed_size = 0;
      GetCompressedTexSizeInBytes(nullptr, width, height, depth,
                                  internal_format, &compressed_size, nullptr);
      info.estimated_size = compressed_size;
    } else {
      // No image
      info.estimated_size = 0;
    }

    estimated_size_ += info.estimated_size;
  }

  max_level_set_ = std::max(max_level_set_, level);
  Update();
  UpdateCleared();
  UpdateCanRenderCondition();
  UpdateHasImages();
  if (IsAttachedToFramebuffer()) {
    // TODO(gman): If textures tracked which framebuffers they were attached to
    // we could just mark those framebuffers as not complete.
    IncAllFramebufferStateChangeCount();
  }
}

void Texture::SetStreamTextureServiceId(GLuint service_id) {
  GLuint new_service_id = service_id ? service_id : owned_service_id_;

  // Take no action if this isn't an OES_EXTERNAL texture.
  if (target_ && target_ != GL_TEXTURE_EXTERNAL_OES)
    return;

  if (service_id_ != new_service_id) {
    service_id_ = new_service_id;
    IncrementManagerServiceIdGeneration();
    if (gl::GLContext* context = gl::GLContext::GetCurrent()) {
      // It would be preferable to pass in the decoder, and ask it to do this
      // instead.  However, there are several cases, such as TextureDefinition,
      // that show up without a clear context owner.  So, instead, we use the
      // current state's state restorer.
      if (gl::GLStateRestorer* restorer = context->GetGLStateRestorer())
        restorer->RestoreAllExternalTextureBindingsIfNeeded();
    }
  }
}

void Texture::MarkLevelAsInternalWorkaround(GLenum target, GLint level) {
  DCHECK_GE(level, 0);
  size_t face_index = GLES2Util::GLTargetToFaceIndex(target);
  DCHECK_LT(face_index, face_infos_.size());
  DCHECK_LT(static_cast<size_t>(level),
            face_infos_[face_index].level_infos.size());
  Texture::LevelInfo& info =
      face_infos_[face_index].level_infos[level];
  info.internal_workaround = true;
  completeness_dirty_ = true;
  Update();
  UpdateCanRenderCondition();
}

bool Texture::ValidForTexture(
    GLint target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLsizei width,
    GLsizei height,
    GLsizei depth) const {
  size_t face_index = GLES2Util::GLTargetToFaceIndex(target);
  if (level >= 0 && face_index < face_infos_.size() &&
      static_cast<size_t>(level) < face_infos_[face_index].level_infos.size()) {
    const LevelInfo& info = face_infos_[face_index].level_infos[level];
    int32_t max_x;
    int32_t max_y;
    int32_t max_z;
    return xoffset >= 0 && yoffset >= 0 && zoffset >= 0 && width >= 0 &&
           height >= 0 && depth >= 0 &&
           base::CheckAdd(xoffset, width).AssignIfValid(&max_x) &&
           base::CheckAdd(yoffset, height).AssignIfValid(&max_y) &&
           base::CheckAdd(zoffset, depth).AssignIfValid(&max_z) &&
           max_x <= info.width && max_y <= info.height && max_z <= info.depth;
  }
  return false;
}

bool Texture::GetLevelSize(
    GLint target, GLint level,
    GLsizei* width, GLsizei* height, GLsizei* depth) const {
  DCHECK(width);
  DCHECK(height);
  size_t face_index = GLES2Util::GLTargetToFaceIndex(target);
  if (level >= 0 && face_index < face_infos_.size() &&
      static_cast<size_t>(level) < face_infos_[face_index].level_infos.size()) {
    const LevelInfo& info = face_infos_[face_index].level_infos[level];
    if (info.target != 0) {
      *width = info.width;
      *height = info.height;
      if (depth)
        *depth = info.depth;
      return true;
    }
  }
  return false;
}

bool Texture::GetLevelType(
    GLint target, GLint level, GLenum* type, GLenum* internal_format) const {
  DCHECK(type);
  DCHECK(internal_format);
  size_t face_index = GLES2Util::GLTargetToFaceIndex(target);
  if (level >= 0 && face_index < face_infos_.size() &&
      static_cast<size_t>(level) < face_infos_[face_index].level_infos.size()) {
    const LevelInfo& info = face_infos_[face_index].level_infos[level];
    if (info.target != 0) {
      *type = info.type;
      *internal_format = info.internal_format;
      return true;
    }
  }
  return false;
}

GLenum Texture::SetParameteri(
    const FeatureInfo* feature_info, GLenum pname, GLint param) {
  DCHECK(feature_info);

  if (target_ == GL_TEXTURE_EXTERNAL_OES ||
      target_ == GL_TEXTURE_RECTANGLE_ARB) {
    if (pname == GL_TEXTURE_MIN_FILTER &&
        (param != GL_NEAREST && param != GL_LINEAR))
      return GL_INVALID_ENUM;
    if ((pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T) &&
        param != GL_CLAMP_TO_EDGE)
      return GL_INVALID_ENUM;
  }

  switch (pname) {
    case GL_TEXTURE_MIN_LOD:
    case GL_TEXTURE_MAX_LOD:
    case GL_TEXTURE_MAX_ANISOTROPY_EXT: {
      GLfloat fparam = static_cast<GLfloat>(param);
      return SetParameterf(feature_info, pname, fparam);
      }
    case GL_TEXTURE_MIN_FILTER:
      if (!feature_info->validators()->texture_min_filter_mode.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      sampler_state_.min_filter = param;
      break;
    case GL_TEXTURE_MAG_FILTER:
      if (!feature_info->validators()->texture_mag_filter_mode.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      sampler_state_.mag_filter = param;
      break;
    case GL_TEXTURE_WRAP_R:
      if (!feature_info->validators()->texture_wrap_mode.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      sampler_state_.wrap_r = param;
      break;
    case GL_TEXTURE_WRAP_S:
      if (!feature_info->validators()->texture_wrap_mode.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      sampler_state_.wrap_s = param;
      break;
    case GL_TEXTURE_WRAP_T:
      if (!feature_info->validators()->texture_wrap_mode.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      sampler_state_.wrap_t = param;
      break;
    case GL_TEXTURE_COMPARE_FUNC:
      if (!feature_info->validators()->texture_compare_func.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      sampler_state_.compare_func = param;
      break;
    case GL_TEXTURE_COMPARE_MODE:
      if (!feature_info->validators()->texture_compare_mode.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      sampler_state_.compare_mode = param;
      break;
    case GL_TEXTURE_BASE_LEVEL:
      if (param < 0) {
        return GL_INVALID_VALUE;
      }
      UpdateBaseLevel(param, feature_info);
      break;
    case GL_TEXTURE_MAX_LEVEL:
      if (param < 0) {
        return GL_INVALID_VALUE;
      }
      UpdateMaxLevel(param);
      break;
    case GL_TEXTURE_USAGE_ANGLE:
      if (!feature_info->validators()->texture_usage.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      usage_ = param;
      break;
    case GL_TEXTURE_SWIZZLE_R:
      if (!feature_info->validators()->texture_swizzle.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      swizzle_r_ = param;
      break;
    case GL_TEXTURE_SWIZZLE_G:
      if (!feature_info->validators()->texture_swizzle.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      swizzle_g_ = param;
      break;
    case GL_TEXTURE_SWIZZLE_B:
      if (!feature_info->validators()->texture_swizzle.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      swizzle_b_ = param;
      break;
    case GL_TEXTURE_SWIZZLE_A:
      if (!feature_info->validators()->texture_swizzle.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      swizzle_a_ = param;
      break;
    case GL_TEXTURE_SRGB_DECODE_EXT:
      if (!feature_info->validators()->texture_srgb_decode_ext.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      break;
    case GL_TEXTURE_IMMUTABLE_FORMAT:
    case GL_TEXTURE_IMMUTABLE_LEVELS:
    case GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES:
      return GL_INVALID_ENUM;
    default:
      NOTREACHED();
      return GL_INVALID_ENUM;
  }
  Update();
  UpdateCleared();
  UpdateCanRenderCondition();
  return GL_NO_ERROR;
}

GLenum Texture::SetParameterf(
    const FeatureInfo* feature_info, GLenum pname, GLfloat param) {
  // Only handle float parameters here. Handle everything else, including error
  // cases, in SetParameteri.
  switch (pname) {
    case GL_TEXTURE_MIN_LOD:
      sampler_state_.min_lod = param;
      break;
    case GL_TEXTURE_MAX_LOD:
      sampler_state_.max_lod = param;
      break;
    case GL_TEXTURE_MAX_ANISOTROPY_EXT:
      if (param < 1.f) {
        return GL_INVALID_VALUE;
      }
      break;
    default: {
      GLint iparam = static_cast<GLint>(std::round(param));
      return SetParameteri(feature_info, pname, iparam);
    }
  }
  return GL_NO_ERROR;
}

void Texture::Update() {
  // Update npot status.
  // Assume GL_TEXTURE_EXTERNAL_OES textures are npot, all others
  npot_ = (target_ == GL_TEXTURE_EXTERNAL_OES) || (num_npot_faces_ > 0);

  if (!completeness_dirty_)
    return;

  if (face_infos_.empty() ||
      static_cast<size_t>(base_level_) >= face_infos_[0].level_infos.size()) {
    texture_complete_ = false;
    cube_complete_ = false;
    return;
  }

  // Update texture_complete and cube_complete status.
  const Texture::FaceInfo& first_face = face_infos_[0];
  const Texture::LevelInfo& first_level = first_face.level_infos[base_level_];
  const GLsizei levels_needed = first_face.num_mip_levels;

  texture_complete_ =
      max_level_set_ >= (levels_needed - 1) && max_level_set_ >= 0;
  cube_complete_ = (face_infos_.size() == 6) &&
                   (first_level.width == first_level.height) &&
                   (first_level.width > 0);

  if (first_level.width == 0 || first_level.height == 0) {
    texture_complete_ = false;
  }

  bool texture_level0_complete = true;
  if (cube_complete_) {
    for (size_t ii = 0; ii < face_infos_.size(); ++ii) {
      const Texture::LevelInfo& face_base_level =
          face_infos_[ii].level_infos[base_level_];
      if (face_base_level.internal_workaround ||
          !TextureFaceComplete(first_level,
                               ii,
                               face_base_level.target,
                               face_base_level.internal_format,
                               face_base_level.width,
                               face_base_level.height,
                               face_base_level.depth,
                               face_base_level.format,
                               face_base_level.type)) {
        texture_level0_complete = false;
        break;
      }
    }
  }
  cube_complete_ &= texture_level0_complete;

  bool texture_mips_complete = true;
  if (texture_complete_) {
    for (size_t ii = 0; ii < face_infos_.size() && texture_mips_complete;
         ++ii) {
      const Texture::FaceInfo& face_info = face_infos_[ii];
      const Texture::LevelInfo& base_level_info =
          face_info.level_infos[base_level_];
      for (GLsizei jj = 1; jj < levels_needed; ++jj) {
        const Texture::LevelInfo& level_info =
            face_infos_[ii].level_infos[base_level_ + jj];
        if (!TextureMipComplete(base_level_info,
                                level_info.target,
                                jj,  // level - base_level_
                                level_info.internal_format,
                                level_info.width,
                                level_info.height,
                                level_info.depth,
                                level_info.format,
                                level_info.type)) {
          texture_mips_complete = false;
          break;
        }
      }
    }
  }
  texture_complete_ &= texture_mips_complete;
  completeness_dirty_ = false;
}

bool Texture::ClearRenderableLevels(DecoderContext* decoder) {
  DCHECK(decoder);
  if (cleared_) {
    return true;
  }

  for (size_t ii = 0; ii < face_infos_.size(); ++ii) {
    const Texture::FaceInfo& face_info = face_infos_[ii];
    for (GLint jj = base_level_;
         jj < base_level_ + face_info.num_mip_levels; ++jj) {
      const Texture::LevelInfo& info = face_info.level_infos[jj];
      if (info.target != 0) {
        if (!ClearLevel(decoder, info.target, jj)) {
          return false;
        }
      }
    }
  }
  UpdateSafeToRenderFrom(true);
  return true;
}

void Texture::SetImmutable(bool immutable, bool immutable_storage) {
  DCHECK(!immutable_storage || immutable);

  if (immutable_ == immutable && immutable_storage_ == immutable_storage)
    return;

  immutable_ = immutable;
  immutable_storage_ = immutable_storage;

  UpdateNumMipLevels();
}

GLint Texture::GetImmutableLevels() const {
  if (!immutable_)
    return 0;
  GLint levels = 0;
  DCHECK(face_infos_.size() > 0);
  for (size_t ii = 0; ii < face_infos_[0].level_infos.size(); ++ii) {
    const Texture::LevelInfo& info = face_infos_[0].level_infos[ii];
    if (info.target != 0)
      levels++;
  }
  return levels;
}

gfx::Rect Texture::GetLevelClearedRect(GLenum target, GLint level) const {
  size_t face_index = GLES2Util::GLTargetToFaceIndex(target);
  if (face_index >= face_infos_.size() ||
      level >= static_cast<GLint>(face_infos_[face_index].level_infos.size())) {
    return gfx::Rect();
  }

  const Texture::LevelInfo& info = face_infos_[face_index].level_infos[level];

  return info.cleared_rect;
}

bool Texture::IsLevelCleared(GLenum target, GLint level) const {
  size_t face_index = GLES2Util::GLTargetToFaceIndex(target);
  if (face_index >= face_infos_.size() ||
      level < 0 ||
      level >= static_cast<GLint>(face_infos_[face_index].level_infos.size())) {
    return true;
  }
  const Texture::LevelInfo& info = face_infos_[face_index].level_infos[level];
  return info.cleared_rect == gfx::Rect(info.width, info.height);
}

bool Texture::IsLevelPartiallyCleared(GLenum target, GLint level) const {
  size_t face_index = GLES2Util::GLTargetToFaceIndex(target);
  if (face_index >= face_infos_.size() ||
      level < 0 ||
      level >= static_cast<GLint>(face_infos_[face_index].level_infos.size())) {
    return false;
  }
  const Texture::LevelInfo& info = face_infos_[face_index].level_infos[level];
  return (info.cleared_rect != gfx::Rect(info.width, info.height) &&
          info.cleared_rect != gfx::Rect());
}

void Texture::InitTextureMaxAnisotropyIfNeeded(GLenum target) {
  if (texture_max_anisotropy_initialized_)
    return;
  texture_max_anisotropy_initialized_ = true;
  GLfloat params[] = { 1.0f };
  glTexParameterfv(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, params);
}

bool Texture::ClearLevel(DecoderContext* decoder, GLenum target, GLint level) {
  DCHECK(decoder);
  size_t face_index = GLES2Util::GLTargetToFaceIndex(target);
  if (face_index >= face_infos_.size() || level < 0 ||
      level >= static_cast<GLint>(face_infos_[face_index].level_infos.size())) {
    return true;
  }

  Texture::LevelInfo& info = face_infos_[face_index].level_infos[level];

  DCHECK_EQ(target, info.target);

  if (info.target == 0 ||
      info.cleared_rect == gfx::Rect(info.width, info.height) ||
      info.width == 0 || info.height == 0 || info.depth == 0) {
    return true;
  }

  if (info.target == GL_TEXTURE_3D || info.target == GL_TEXTURE_2D_ARRAY) {
    // For 3D textures, we always clear the entire texture.
    DCHECK(info.cleared_rect == gfx::Rect());
    bool cleared = decoder->ClearLevel3D(
        this, info.target, info.level,
        TextureManager::AdjustTexFormat(decoder->GetFeatureInfo(), info.format),
        info.type, info.width, info.height, info.depth);
    if (!cleared)
      return false;
  } else {
    if (decoder->IsCompressedTextureFormat(info.internal_format)) {
      // An uncleared level of a compressed texture can only occur when
      // allocating the texture with TexStorage2D. In this case the level
      // is cleared just before a call to CompressedTexSubImage2D, to avoid
      // having to clear a sub-rectangle of a compressed texture, which
      // would be problematic.
      DCHECK(IsImmutable());
      DCHECK(info.cleared_rect == gfx::Rect());
      bool cleared = decoder->ClearCompressedTextureLevel(
          this, info.target, info.level, info.internal_format,
          info.width, info.height);
      if (!cleared)
        return false;
    } else {
      // Clear all remaining sub regions.
      const int x[] = {
        0, info.cleared_rect.x(), info.cleared_rect.right(), info.width};
      const int y[] = {
        0, info.cleared_rect.y(), info.cleared_rect.bottom(), info.height};

      for (size_t j = 0; j < 3; ++j) {
        for (size_t i = 0; i < 3; ++i) {
          // Center of nine patch is already cleared.
          if (j == 1 && i == 1)
            continue;

          gfx::Rect rect(x[i], y[j], x[i + 1] - x[i], y[j + 1] - y[j]);
          if (rect.IsEmpty())
            continue;

          // NOTE: It seems kind of gross to call back into the decoder for this
          // but only the decoder knows all the state (like unpack_alignment_)
          // that's needed to be able to call GL correctly.
          bool cleared = decoder->ClearLevel(
              this, info.target, info.level,
              TextureManager::AdjustTexFormat(decoder->GetFeatureInfo(),
                                              info.format),
              info.type, rect.x(), rect.y(), rect.width(), rect.height());
          if (!cleared)
            return false;
        }
      }
    }
  }

  UpdateMipCleared(&info, info.width, info.height,
                   gfx::Rect(info.width, info.height));
  return true;
}

void Texture::SetLevelImageInternal(GLenum target,
                                    GLint level,
                                    gl::GLImage* image,
                                    GLStreamTextureImage* stream_texture_image,
                                    ImageState state) {
  DCHECK(!stream_texture_image || stream_texture_image == image);
  DCHECK_GE(level, 0);
  size_t face_index = GLES2Util::GLTargetToFaceIndex(target);
  DCHECK_LT(face_index, face_infos_.size());
  DCHECK_LT(static_cast<size_t>(level),
            face_infos_[face_index].level_infos.size());
  Texture::LevelInfo& info = face_infos_[face_index].level_infos[level];
  DCHECK_EQ(info.target, target);
  DCHECK_EQ(info.level, level);
  info.image = image;
  info.stream_texture_image = stream_texture_image;
  info.image_state = state;

  UpdateCanRenderCondition();
  UpdateHasImages();
  UpdateEmulatingRGB();
}

void Texture::SetLevelImage(GLenum target,
                            GLint level,
                            gl::GLImage* image,
                            ImageState state) {
  SetStreamTextureServiceId(0);
  SetLevelImageInternal(target, level, image, nullptr, state);
}

void Texture::SetLevelStreamTextureImage(GLenum target,
                                         GLint level,
                                         GLStreamTextureImage* image,
                                         ImageState state,
                                         GLuint service_id) {
  SetStreamTextureServiceId(service_id);
  SetLevelImageInternal(target, level, image, image, state);
}

void Texture::SetLevelImageState(GLenum target, GLint level, ImageState state) {
  DCHECK_GE(level, 0);
  size_t face_index = GLES2Util::GLTargetToFaceIndex(target);
  DCHECK_LT(face_index, face_infos_.size());
  DCHECK_LT(static_cast<size_t>(level),
            face_infos_[face_index].level_infos.size());
  Texture::LevelInfo& info = face_infos_[face_index].level_infos[level];
  DCHECK_EQ(info.target, target);
  DCHECK_EQ(info.level, level);
  // Workaround for StreamTexture which must be re-copied on each access.
  // TODO(ericrk): Remove this once SharedImage transition is complete.
  if (info.image && !info.image->HasMutableState())
    return;
  info.image_state = state;
}

const Texture::LevelInfo* Texture::GetLevelInfo(GLint target,
                                                GLint level) const {
  if (target != GL_TEXTURE_2D && target != GL_TEXTURE_EXTERNAL_OES &&
      target != GL_TEXTURE_RECTANGLE_ARB) {
    return nullptr;
  }

  size_t face_index = GLES2Util::GLTargetToFaceIndex(target);
  if (level >= 0 && face_index < face_infos_.size() &&
      static_cast<size_t>(level) < face_infos_[face_index].level_infos.size()) {
    const LevelInfo& info = face_infos_[face_index].level_infos[level];
    if (info.target != 0)
      return &info;
  }
  return nullptr;
}

gl::GLImage* Texture::GetLevelImage(GLint target,
                                    GLint level,
                                    ImageState* state) const {
  const LevelInfo* info = GetLevelInfo(target, level);
  if (!info)
    return nullptr;

  if (state)
    *state = info->image_state;
  return info->image.get();
}

gl::GLImage* Texture::GetLevelImage(GLint target, GLint level) const {
  return GetLevelImage(target, level, nullptr);
}

GLStreamTextureImage* Texture::GetLevelStreamTextureImage(GLint target,
                                                          GLint level) const {
  const LevelInfo* info = GetLevelInfo(target, level);
  if (!info)
    return nullptr;

  return info->stream_texture_image.get();
}

void Texture::DumpLevelMemory(base::trace_event::ProcessMemoryDump* pmd,
                              uint64_t client_tracing_id,
                              const std::string& dump_name) const {
  for (uint32_t face_index = 0; face_index < face_infos_.size(); ++face_index) {
    const auto& level_infos = face_infos_[face_index].level_infos;
    for (uint32_t level_index = 0; level_index < level_infos.size();
         ++level_index) {
      // Skip levels with no size. Textures will have empty levels for all
      // potential mip levels which are not in use.
      if (!level_infos[level_index].estimated_size)
        continue;

      std::string level_dump_name = base::StringPrintf(
          "%s/face_%d/level_%d", dump_name.c_str(), face_index, level_index);

      // If a level has a GLImage, ask the GLImage to dump itself.
      // If a level does not have a GLImage bound to it, then dump the
      // texture allocation also as the storage is not provided by the
      // GLImage in that case.
      if (level_infos[level_index].image) {
        level_infos[level_index].image->OnMemoryDump(pmd, client_tracing_id,
                                                     level_dump_name);
      } else {
        MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(level_dump_name);
        dump->AddScalar(
            MemoryAllocatorDump::kNameSize, MemoryAllocatorDump::kUnitsBytes,
            static_cast<uint64_t>(level_infos[level_index].estimated_size));
      }
    }
  }
}

bool Texture::CanRenderTo(const FeatureInfo* feature_info, GLint level) const {
  if (target_ == GL_TEXTURE_EXTERNAL_OES || target_ == 0)
    return false;
  DCHECK_LT(0u, face_infos_.size());
  // In GLES2, cube completeness is not required for framebuffer completeness.
  // However, it is required if command buffer is implemented on top of
  // recent OpenGL core versions or OpenGL ES 3.0+. Therefore, for consistency,
  // it is better to deviate from ES2 spec and require cube completeness all
  // the time.
  if (face_infos_.size() == 6 && !cube_complete())
    return false;
  DCHECK(level >= 0 &&
         level < static_cast<GLint>(face_infos_[0].level_infos.size()));
  if (level > base_level_ && !texture_complete()) {
    return false;
  }
  GLenum internal_format = face_infos_[0].level_infos[level].internal_format;
  bool color_renderable = ColorRenderable(feature_info, internal_format,
                                          immutable_);
  bool depth_renderable = feature_info->validators()->
      texture_depth_renderable_internal_format.IsValid(internal_format);
  bool stencil_renderable = feature_info->validators()->
      texture_stencil_renderable_internal_format.IsValid(internal_format);
  return (color_renderable || depth_renderable || stencil_renderable);
}

GLenum Texture::GetCompatibilitySwizzleForChannel(GLenum channel) {
  return GetSwizzleForChannel(channel, compatibility_swizzle_);
}

void Texture::SetCompatibilitySwizzle(const CompatibilitySwizzle* swizzle) {
  if (compatibility_swizzle_ == swizzle)
    return;

  compatibility_swizzle_ = swizzle;
  glTexParameteri(target_, GL_TEXTURE_SWIZZLE_R,
                  GetSwizzleForChannel(swizzle_r_, swizzle));
  glTexParameteri(target_, GL_TEXTURE_SWIZZLE_G,
                  GetSwizzleForChannel(swizzle_g_, swizzle));
  glTexParameteri(target_, GL_TEXTURE_SWIZZLE_B,
                  GetSwizzleForChannel(swizzle_b_, swizzle));
  glTexParameteri(target_, GL_TEXTURE_SWIZZLE_A,
                  GetSwizzleForChannel(swizzle_a_, swizzle));
}

void Texture::ApplyFormatWorkarounds(const FeatureInfo* feature_info) {
  if (feature_info->gl_version_info().NeedsLuminanceAlphaEmulation()) {
    if (static_cast<size_t>(base_level_) >= face_infos_[0].level_infos.size())
      return;
    const Texture::LevelInfo& info = face_infos_[0].level_infos[base_level_];
    SetCompatibilitySwizzle(GetCompatibilitySwizzleInternal(info.format));
  }
}

bool Texture::EmulatingRGB() {
  return emulating_rgb_;
}

TextureRef::TextureRef(TextureManager* manager,
                       GLuint client_id,
                       Texture* texture)
    : manager_(manager),
      texture_(texture),
      client_id_(client_id),
      num_observers_(0),
      force_context_lost_(false) {
  DCHECK(manager_);
  DCHECK(texture_);
  texture_->AddTextureRef(this);
  manager_->StartTracking(this);
}

scoped_refptr<TextureRef> TextureRef::Create(TextureManager* manager,
                                             GLuint client_id,
                                             GLuint service_id) {
  return new TextureRef(manager, client_id, new Texture(service_id));
}

TextureRef::~TextureRef() {
  manager_->StopTracking(this);
  bool have_context = force_context_lost_ ? false : manager_->have_context_;
  texture_->RemoveTextureRef(this, have_context);
  manager_ = nullptr;
  if (!have_context && shared_image_)
    shared_image_->OnContextLost();
}

bool TextureRef::BeginAccessSharedImage(GLenum mode) {
  shared_image_scoped_access_.emplace(shared_image_.get(), mode);
  if (!shared_image_scoped_access_->success()) {
    shared_image_scoped_access_.reset();
    return false;
  }
  return true;
}

void TextureRef::EndAccessSharedImage() {
  shared_image_scoped_access_.reset();
}

void TextureRef::ForceContextLost() {
  force_context_lost_ = true;
}

void TextureRef::SetSharedImageRepresentation(
    std::unique_ptr<SharedImageRepresentationGLTexture> shared_image) {
  shared_image_ = std::move(shared_image);
}

TextureManager::TextureManager(MemoryTracker* memory_tracker,
                               FeatureInfo* feature_info,
                               GLint max_texture_size,
                               GLint max_cube_map_texture_size,
                               GLint max_rectangle_texture_size,
                               GLint max_3d_texture_size,
                               GLint max_array_texture_layers,
                               bool use_default_textures,
                               gl::ProgressReporter* progress_reporter,
                               ServiceDiscardableManager* discardable_manager)
    : memory_type_tracker_(new MemoryTypeTracker(memory_tracker)),
      memory_tracker_(memory_tracker),
      feature_info_(feature_info),
      max_texture_size_(max_texture_size),
      max_cube_map_texture_size_(max_cube_map_texture_size),
      max_rectangle_texture_size_(max_rectangle_texture_size),
      max_3d_texture_size_(max_3d_texture_size),
      max_array_texture_layers_(max_array_texture_layers),
      max_levels_(ComputeMipMapCount(GL_TEXTURE_2D,
                                     max_texture_size,
                                     max_texture_size,
                                     0)),
      max_cube_map_levels_(ComputeMipMapCount(GL_TEXTURE_CUBE_MAP,
                                              max_cube_map_texture_size,
                                              max_cube_map_texture_size,
                                              0)),
      max_3d_levels_(ComputeMipMapCount(GL_TEXTURE_3D,
                                        max_3d_texture_size,
                                        max_3d_texture_size,
                                        max_3d_texture_size)),
      use_default_textures_(use_default_textures),
      num_unsafe_textures_(0),
      num_uncleared_mips_(0),
      num_images_(0),
      texture_count_(0),
      have_context_(true),
      current_service_id_generation_(0),
      progress_reporter_(progress_reporter),
      discardable_manager_(discardable_manager) {
  for (int ii = 0; ii < kNumDefaultTextures; ++ii) {
    black_texture_ids_[ii] = 0;
  }
}

void TextureManager::AddFramebufferManager(
    FramebufferManager* framebuffer_manager) {
  framebuffer_managers_.push_back(framebuffer_manager);
}

void TextureManager::RemoveFramebufferManager(
    FramebufferManager* framebuffer_manager) {
  for (unsigned int i = 0; i < framebuffer_managers_.size(); ++i) {
    if (framebuffer_managers_[i] == framebuffer_manager) {
      std::swap(framebuffer_managers_[i], framebuffer_managers_.back());
      framebuffer_managers_.pop_back();
      return;
    }
  }
  NOTREACHED();
}

void TextureManager::Initialize() {
  // Reset PIXEL_UNPACK_BUFFER to avoid unrelated GL error on some GL drivers.
  if (feature_info_->gl_version_info().is_es3_capable) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  }

  // TODO(gman): The default textures have to be real textures, not the 0
  // texture because we simulate non shared resources on top of shared
  // resources and all contexts that share resource share the same default
  // texture.
  default_textures_[kTexture2D] = CreateDefaultAndBlackTextures(
      GL_TEXTURE_2D, &black_texture_ids_[kTexture2D]);
  default_textures_[kCubeMap] = CreateDefaultAndBlackTextures(
      GL_TEXTURE_CUBE_MAP, &black_texture_ids_[kCubeMap]);

  if (feature_info_->IsWebGL2OrES3Context()) {
    DCHECK(feature_info_->IsES3Capable());
    default_textures_[kTexture3D] = CreateDefaultAndBlackTextures(
        GL_TEXTURE_3D, &black_texture_ids_[kTexture3D]);
    default_textures_[kTexture2DArray] = CreateDefaultAndBlackTextures(
        GL_TEXTURE_2D_ARRAY, &black_texture_ids_[kTexture2DArray]);
  }

  if (feature_info_->feature_flags().oes_egl_image_external ||
      feature_info_->feature_flags().nv_egl_stream_consumer_external) {
    default_textures_[kExternalOES] = CreateDefaultAndBlackTextures(
        GL_TEXTURE_EXTERNAL_OES, &black_texture_ids_[kExternalOES]);
  }

  if (feature_info_->feature_flags().arb_texture_rectangle) {
    default_textures_[kRectangleARB] = CreateDefaultAndBlackTextures(
        GL_TEXTURE_RECTANGLE_ARB, &black_texture_ids_[kRectangleARB]);
  }

  // When created from InProcessCommandBuffer, we won't have a |memory_tracker_|
  // so don't register a dump provider.
  if (memory_tracker_) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "gpu::TextureManager", base::ThreadTaskRunnerHandle::Get());
  }
}

scoped_refptr<TextureRef>
    TextureManager::CreateDefaultAndBlackTextures(
        GLenum target,
        GLuint* black_texture) {
  static uint8_t black[] = {0, 0, 0, 255};

  // Sampling a texture not associated with any EGLImage sibling will return
  // black values according to the spec.
  bool needs_initialization = (target != GL_TEXTURE_EXTERNAL_OES);
  bool needs_faces = (target == GL_TEXTURE_CUBE_MAP);
  bool is_3d_or_2d_array_target = (target == GL_TEXTURE_3D ||
      target == GL_TEXTURE_2D_ARRAY);

  // Make default textures and texture for replacing non-renderable textures.
  GLuint ids[2];
  const int num_ids = use_default_textures_ ? 2 : 1;
  glGenTextures(num_ids, ids);
  for (int ii = 0; ii < num_ids; ++ii) {
    glBindTexture(target, ids[ii]);
    if (needs_initialization) {
      if (needs_faces) {
        for (int jj = 0; jj < GLES2Util::kNumFaces; ++jj) {
          glTexImage2D(GLES2Util::IndexToGLFaceTarget(jj), 0, GL_RGBA, 1, 1, 0,
                       GL_RGBA, GL_UNSIGNED_BYTE, black);
        }
      } else {
        if (is_3d_or_2d_array_target) {
          glTexImage3D(target, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA,
                       GL_UNSIGNED_BYTE, black);
        } else {
          glTexImage2D(target, 0, GL_RGBA, 1, 1, 0, GL_RGBA,
                       GL_UNSIGNED_BYTE, black);
        }
      }
    }
  }
  glBindTexture(target, 0);

  scoped_refptr<TextureRef> default_texture;
  if (use_default_textures_) {
    default_texture = TextureRef::Create(this, 0, ids[1]);
    SetTarget(default_texture.get(), target);
    if (needs_faces) {
      for (int ii = 0; ii < GLES2Util::kNumFaces; ++ii) {
        SetLevelInfo(default_texture.get(), GLES2Util::IndexToGLFaceTarget(ii),
                     0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     gfx::Rect(1, 1));
      }
    } else {
      SetLevelInfo(default_texture.get(), target, 0, GL_RGBA, 1, 1, 1, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(1, 1));
    }
  }

  *black_texture = ids[0];
  return default_texture;
}

bool TextureManager::ValidForTarget(
    GLenum target, GLint level, GLsizei width, GLsizei height, GLsizei depth) {
  if (level < 0 || level >= MaxLevelsForTarget(target))
    return false;
  GLsizei max_size = MaxSizeForTarget(target) >> level;
  GLsizei max_depth =
      (target == GL_TEXTURE_2D_ARRAY ? max_array_texture_layers() : max_size);
  return width >= 0 &&
         height >= 0 &&
         depth >= 0 &&
         width <= max_size &&
         height <= max_size &&
         depth <= max_depth &&
         (level == 0 || feature_info_->feature_flags().npot_ok ||
          (!GLES2Util::IsNPOT(width) &&
           !GLES2Util::IsNPOT(height) &&
           !GLES2Util::IsNPOT(depth))) &&
         (target != GL_TEXTURE_CUBE_MAP || (width == height && depth == 1)) &&
         (target != GL_TEXTURE_2D || (depth == 1));
}

void TextureManager::SetTarget(TextureRef* ref, GLenum target) {
  DCHECK(ref);
  ref->texture()->SetTarget(target, MaxLevelsForTarget(target));
}

void TextureManager::SetLevelClearedRect(TextureRef* ref,
                                         GLenum target,
                                         GLint level,
                                         const gfx::Rect& cleared_rect) {
  DCHECK(ref);
  ref->texture()->SetLevelClearedRect(target, level, cleared_rect);
}

void TextureManager::SetLevelCleared(TextureRef* ref,
                                     GLenum target,
                                     GLint level,
                                     bool cleared) {
  DCHECK(ref);
  ref->texture()->SetLevelCleared(target, level, cleared);
}

bool TextureManager::ClearRenderableLevels(DecoderContext* decoder,
                                           TextureRef* ref) {
  DCHECK(ref);
  return ref->texture()->ClearRenderableLevels(decoder);
}

// static
bool TextureManager::ClearTextureLevel(DecoderContext* decoder,
                                       TextureRef* ref,
                                       GLenum target,
                                       GLint level) {
  DCHECK(ref);
  Texture* texture = ref->texture();
  return ClearTextureLevel(decoder, texture, target, level);
}

// static
bool TextureManager::ClearTextureLevel(DecoderContext* decoder,
                                       Texture* texture,
                                       GLenum target,
                                       GLint level) {
  if (texture->num_uncleared_mips() == 0) {
    return true;
  }
  bool result = texture->ClearLevel(decoder, target, level);
  texture->UpdateCleared();
  return result;
}

void TextureManager::SetLevelInfo(TextureRef* ref,
                                  GLenum target,
                                  GLint level,
                                  GLenum internal_format,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth,
                                  GLint border,
                                  GLenum format,
                                  GLenum type,
                                  const gfx::Rect& cleared_rect) {
  DCHECK(gfx::Rect(width, height).Contains(cleared_rect));
  DCHECK(ref);
  Texture* texture = ref->texture();
  texture->SetLevelInfo(target, level, internal_format, width, height, depth,
                        border, format, type, cleared_rect);
  discardable_manager_->OnTextureSizeChanged(ref->client_id(), this,
                                             texture->estimated_size());
}

TextureRef* TextureManager::Consume(
    GLuint client_id,
    Texture* texture) {
  DCHECK(client_id);
  scoped_refptr<TextureRef> ref(new TextureRef(this, client_id, texture));
  bool result = textures_.insert(std::make_pair(client_id, ref)).second;
  DCHECK(result);
  return ref.get();
}

TextureRef* TextureManager::ConsumeSharedImage(
    GLuint client_id,
    std::unique_ptr<SharedImageRepresentationGLTexture> shared_image) {
  DCHECK(client_id);
  Texture* texture = shared_image->GetTexture();
  TextureRef* ref = Consume(client_id, texture);
  if (ref)
    ref->SetSharedImageRepresentation(std::move(shared_image));
  return ref;
}

void TextureManager::SetParameteri(
    const char* function_name, ErrorState* error_state,
    TextureRef* ref, GLenum pname, GLint param) {
  DCHECK(error_state);
  DCHECK(ref);
  Texture* texture = ref->texture();
  GLenum result = texture->SetParameteri(feature_info_.get(), pname, param);
  if (result != GL_NO_ERROR) {
    if (result == GL_INVALID_ENUM) {
      ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(
          error_state, function_name, param, "param");
    } else {
      ERRORSTATE_SET_GL_ERROR_INVALID_PARAMI(
          error_state, result, function_name, pname, param);
    }
  } else {
    switch (pname) {
      case GL_TEXTURE_SWIZZLE_R:
      case GL_TEXTURE_SWIZZLE_G:
      case GL_TEXTURE_SWIZZLE_B:
      case GL_TEXTURE_SWIZZLE_A:
        glTexParameteri(texture->target(), pname,
                        texture->GetCompatibilitySwizzleForChannel(param));
        break;
      case GL_TEXTURE_BASE_LEVEL:
        // base level might have been clamped.
        glTexParameteri(texture->target(), pname, texture->base_level());
        break;
      case GL_TEXTURE_MAX_LEVEL:
        // max level might have been clamped.
        glTexParameteri(texture->target(), pname, texture->max_level());
        break;
      default:
        glTexParameteri(texture->target(), pname, param);
        break;
    }
  }
}

void TextureManager::SetParameterf(
    const char* function_name, ErrorState* error_state,
    TextureRef* ref, GLenum pname, GLfloat param) {
  DCHECK(error_state);
  DCHECK(ref);
  Texture* texture = ref->texture();
  GLenum result = texture->SetParameterf(feature_info_.get(), pname, param);
  if (result != GL_NO_ERROR) {
    if (result == GL_INVALID_ENUM) {
      ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(
          error_state, function_name, pname, "pname");
    } else {
      ERRORSTATE_SET_GL_ERROR_INVALID_PARAMF(
          error_state, result, function_name, pname, param);
    }
  } else {
    switch (pname) {
      case GL_TEXTURE_BASE_LEVEL:
        // base level might have been clamped.
        glTexParameterf(texture->target(), pname,
                        static_cast<GLfloat>(texture->base_level()));
        break;
      case GL_TEXTURE_MAX_LEVEL:
        // max level might have been clamped.
        glTexParameterf(texture->target(), pname,
                        static_cast<GLfloat>(texture->max_level()));
        break;
      default:
        glTexParameterf(texture->target(), pname, param);
        break;
    }
  }
}

void TextureManager::MarkMipmapsGenerated(TextureRef* ref) {
  DCHECK(ref);
  ref->texture()->MarkMipmapsGenerated();
}

TextureRef* TextureManager::CreateTexture(
    GLuint client_id, GLuint service_id) {
  DCHECK_NE(0u, service_id);
  scoped_refptr<TextureRef> ref(TextureRef::Create(
      this, client_id, service_id));
  std::pair<TextureMap::iterator, bool> result =
      textures_.insert(std::make_pair(client_id, ref));
  DCHECK(result.second);
  return ref.get();
}

TextureRef* TextureManager::GetTexture(
    GLuint client_id) const {
  TextureMap::const_iterator it = textures_.find(client_id);
  return it != textures_.end() ? it->second.get() : nullptr;
}

scoped_refptr<TextureRef> TextureManager::TakeTexture(GLuint client_id) {
  auto it = textures_.find(client_id);
  if (it == textures_.end())
    return nullptr;

  scoped_refptr<TextureRef> ref = it->second;
  textures_.erase(it);
  return ref;
}

void TextureManager::ReturnTexture(scoped_refptr<TextureRef> texture_ref) {
  GLuint client_id = texture_ref->client_id();
  // If we've generated a replacement texture due to "bind generates resource",
  // behavior, just delete the resource being returned.
  TextureMap::iterator it = textures_.find(client_id);
  if (it != textures_.end()) {
    // Reset the client id so it doesn't interfere with the generated resource.
    texture_ref->reset_client_id();
    return;
  }

  textures_.emplace(client_id, std::move(texture_ref));
}

void TextureManager::RemoveTexture(GLuint client_id) {
  TextureMap::iterator it = textures_.find(client_id);
  if (it != textures_.end()) {
    discardable_manager_->OnTextureDeleted(client_id, this);
    it->second->reset_client_id();
    textures_.erase(it);
  }
}

void TextureManager::StartTracking(TextureRef* ref) {
  Texture* texture = ref->texture();
  ++texture_count_;
  num_uncleared_mips_ += texture->num_uncleared_mips();
  if (!texture->SafeToRenderFrom())
    ++num_unsafe_textures_;
  if (texture->HasImages())
    ++num_images_;
}

void TextureManager::StopTracking(TextureRef* ref) {
  if (ref->num_observers()) {
    for (unsigned int i = 0; i < destruction_observers_.size(); i++) {
      destruction_observers_[i]->OnTextureRefDestroying(ref);
    }
    DCHECK_EQ(ref->num_observers(), 0);
  }

  Texture* texture = ref->texture();

  --texture_count_;
  if (texture->HasImages()) {
    DCHECK_NE(0, num_images_);
    --num_images_;
  }
  if (!texture->SafeToRenderFrom()) {
    DCHECK_NE(0, num_unsafe_textures_);
    --num_unsafe_textures_;
  }
  num_uncleared_mips_ -= texture->num_uncleared_mips();
  DCHECK_GE(num_uncleared_mips_, 0);

  if (ref->client_id())
    discardable_manager_->OnTextureDeleted(ref->client_id(), this);
}

MemoryTypeTracker* TextureManager::GetMemTracker() {
  return memory_type_tracker_.get();
}

Texture* TextureManager::GetTextureForServiceId(GLuint service_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (TextureMap::const_iterator it = textures_.begin();
       it != textures_.end(); ++it) {
    Texture* texture = it->second->texture();
    if (texture->service_id() == service_id)
      return texture;
  }
  return nullptr;
}

GLsizei TextureManager::ComputeMipMapCount(GLenum target,
                                           GLsizei width,
                                           GLsizei height,
                                           GLsizei depth) {
  switch (target) {
    case GL_TEXTURE_EXTERNAL_OES:
    case GL_TEXTURE_RECTANGLE_ARB:
      return 1;
    case GL_TEXTURE_3D:
      return 1 + base::bits::Log2Floor(std::max({width, height, depth}));
    default:
      return 1 + base::bits::Log2Floor(std::max(width, height));
  }
}

void TextureManager::SetLevelImage(TextureRef* ref,
                                   GLenum target,
                                   GLint level,
                                   gl::GLImage* image,
                                   Texture::ImageState state) {
  DCHECK(ref);
  ref->texture()->SetLevelImage(target, level, image, state);
}

void TextureManager::SetLevelStreamTextureImage(TextureRef* ref,
                                                GLenum target,
                                                GLint level,
                                                GLStreamTextureImage* image,
                                                Texture::ImageState state,
                                                GLuint service_id) {
  DCHECK(ref);
  ref->texture()->SetLevelStreamTextureImage(target, level, image, state,
                                             service_id);
}

void TextureManager::SetLevelImageState(TextureRef* ref,
                                        GLenum target,
                                        GLint level,
                                        Texture::ImageState state) {
  DCHECK(ref);
  ref->texture()->SetLevelImageState(target, level, state);
}

size_t TextureManager::GetSignatureSize() const {
  return sizeof(TextureTag) + sizeof(TextureSignature);
}

void TextureManager::AddToSignature(
    TextureRef* ref,
    GLenum target,
    GLint level,
    std::string* signature) const {
  ref->texture()->AddToSignature(feature_info_.get(), target, level, signature);
}

void TextureManager::UpdateSafeToRenderFrom(int delta) {
  num_unsafe_textures_ += delta;
  DCHECK_GE(num_unsafe_textures_, 0);
}

void TextureManager::UpdateUnclearedMips(int delta) {
  num_uncleared_mips_ += delta;
  DCHECK_GE(num_uncleared_mips_, 0);
}

void TextureManager::UpdateNumImages(int delta) {
  num_images_ += delta;
  DCHECK_GE(num_images_, 0);
}

void TextureManager::IncFramebufferStateChangeCount() {
  for (unsigned int i = 0; i < framebuffer_managers_.size(); ++i) {
    framebuffer_managers_[i]->IncFramebufferStateChangeCount();
  }
}

bool TextureManager::ValidateTextureParameters(
    ErrorState* error_state, const char* function_name, bool tex_image_call,
    GLenum format, GLenum type, GLint internal_format, GLint level) {
  const Validators* validators = feature_info_->validators();
  if (!validators->texture_format.IsValid(format)) {
    ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(
        error_state, function_name, format, "format");
    return false;
  }
  if (!validators->pixel_type.IsValid(type)) {
    ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(
        error_state, function_name, type, "type");
    return false;
  }
  // For TexSubImage calls, internal_format isn't part of the parameters.
  // So the validation is not necessary for TexSubImage.
  if (tex_image_call &&
      !validators->texture_internal_format.IsValid(internal_format)) {
    std::string msg = std::string("invalid internal_format ") +
        GLES2Util::GetStringEnum(internal_format);
    ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_VALUE, function_name,
                             msg.c_str());
    return false;
  }
  if (!g_format_type_validator.Get().IsValid(feature_info_->context_type(),
                                             internal_format, format, type)) {
    std::string msg = std::string(
        "invalid internalformat/format/type combination ") +
        GLES2Util::GetStringEnum(internal_format) + std::string("/") +
        GLES2Util::GetStringEnum(format) + std::string("/") +
        GLES2Util::GetStringEnum(type);
    ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION, function_name,
                            msg.c_str());
    return false;
  }
  if (!feature_info_->IsWebGL2OrES3Context()) {
    uint32_t channels = GLES2Util::GetChannelsForFormat(format);
    if ((channels & (GLES2Util::kDepth | GLES2Util::kStencil)) != 0 && level) {
      ERRORSTATE_SET_GL_ERROR(
          error_state, GL_INVALID_OPERATION, function_name,
          (std::string("invalid format ") + GLES2Util::GetStringEnum(format) +
           " for level != 0").c_str());
      return false;
    }
  }
  return true;
}

// Gets the texture id for a given target.
TextureRef* TextureManager::GetTextureInfoForTarget(
    ContextState* state, GLenum target) {
  TextureUnit& unit = state->texture_units[state->active_texture_unit];
  TextureRef* texture = nullptr;
  switch (target) {
    case GL_TEXTURE_2D:
      texture = unit.bound_texture_2d.get();
      break;
    case GL_TEXTURE_CUBE_MAP:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      texture = unit.bound_texture_cube_map.get();
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      texture = unit.bound_texture_external_oes.get();
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      texture = unit.bound_texture_rectangle_arb.get();
      break;
    case GL_TEXTURE_3D:
      texture = unit.bound_texture_3d.get();
      break;
    case GL_TEXTURE_2D_ARRAY:
      texture = unit.bound_texture_2d_array.get();
      break;
    default:
      NOTREACHED();
      return nullptr;
  }
  return texture;
}

TextureRef* TextureManager::GetTextureInfoForTargetUnlessDefault(
    ContextState* state, GLenum target) {
  TextureRef* texture = GetTextureInfoForTarget(state, target);
  if (!texture)
    return nullptr;
  if (texture == GetDefaultTextureInfo(target))
    return nullptr;
  return texture;
}

bool TextureManager::ValidateTexImage(ContextState* state,
                                      ErrorState* error_state,
                                      const char* function_name,
                                      const DoTexImageArguments& args,
                                      TextureRef** texture_ref) {
  const Validators* validators = feature_info_->validators();
  if (((args.command_type == DoTexImageArguments::CommandType::kTexImage2D) &&
       !validators->texture_target.IsValid(args.target)) ||
      ((args.command_type == DoTexImageArguments::CommandType::kTexImage3D) &&
       !validators->texture_3_d_target.IsValid(args.target))) {
    ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(
        error_state, function_name, args.target, "target");
    return false;
  }
  // TODO(ccameron): Add a separate texture from |texture_target| for
  // [Compressed]Tex[Sub]Image2D and related functions.
  // http://crbug.com/536854
  if (args.target == GL_TEXTURE_RECTANGLE_ARB) {
    ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(
        error_state, function_name, args.target, "target");
    return false;
  }
  if (feature_info_->IsWebGL1OrES2Context()) {
    switch (args.format) {
      case GL_DEPTH_COMPONENT:
      case GL_DEPTH_STENCIL:
        if (args.target != GL_TEXTURE_2D) {
          ERRORSTATE_SET_GL_ERROR(
              error_state, GL_INVALID_OPERATION, function_name,
              "invalid target for depth/stencil textures");
          return false;
        }
        break;
      default:
        break;
    }
  }
  if (!ValidateTextureParameters(
      error_state, function_name, true, args.format, args.type,
      args.internal_format, args.level)) {
    return false;
  }
  if (!ValidForTarget(args.target, args.level,
                      args.width, args.height, args.depth) ||
      args.border != 0) {
    ERRORSTATE_SET_GL_ERROR(
        error_state, GL_INVALID_VALUE, function_name,
        "dimensions out of range");
    return false;
  }
  if ((GLES2Util::GetChannelsForFormat(args.format) &
       (GLES2Util::kDepth | GLES2Util::kStencil)) != 0 && args.pixels
      && !feature_info_->IsWebGL2OrES3Context()) {
    ERRORSTATE_SET_GL_ERROR(
        error_state, GL_INVALID_OPERATION,
        function_name, "can not supply data for depth or stencil textures");
    return false;
  }

  TextureRef* local_texture_ref = GetTextureInfoForTarget(state, args.target);
  if (!local_texture_ref) {
    ERRORSTATE_SET_GL_ERROR(
        error_state, GL_INVALID_OPERATION, function_name,
        "unknown texture for target");
    return false;
  }
  if (local_texture_ref->texture()->IsImmutable()) {
    ERRORSTATE_SET_GL_ERROR(
        error_state, GL_INVALID_OPERATION, function_name,
        "texture is immutable");
    return false;
  }

  Buffer* buffer = state->bound_pixel_unpack_buffer.get();
  if (buffer) {
    if (buffer->GetMappedRange()) {
      ERRORSTATE_SET_GL_ERROR(
          error_state, GL_INVALID_OPERATION, function_name,
          "pixel unpack buffer should not be mapped to client memory");
      return false;
    }
    if (buffer->IsBoundForTransformFeedbackAndOther()) {
      ERRORSTATE_SET_GL_ERROR(
          error_state, GL_INVALID_OPERATION, function_name,
          "pixel unpack buffer is simultaneously bound for transform feedback");
      return error::kNoError;
    }
    base::CheckedNumeric<uint32_t> size = args.pixels_size;
    GLuint offset = ToGLuint(args.pixels);
    size += offset;
    if (!size.IsValid()) {
      ERRORSTATE_SET_GL_ERROR(
          error_state, GL_INVALID_VALUE, function_name,
          "size + offset overflow");
      return false;
    }
    uint32_t buffer_size = static_cast<uint32_t>(buffer->size());
    if (buffer_size < size.ValueOrDefault(0)) {
      ERRORSTATE_SET_GL_ERROR(
          error_state, GL_INVALID_OPERATION, function_name,
          "pixel unpack buffer is not large enough");
      return false;
    }
    uint32_t type_size = GLES2Util::GetGLTypeSizeForTextures(args.type);
    DCHECK_LT(0u, type_size);
    if (offset % type_size != 0) {
      ERRORSTATE_SET_GL_ERROR(
          error_state, GL_INVALID_OPERATION, function_name,
          "offset is not evenly divisible by elements");
      return false;
    }
  }

  // Write the TextureReference since this is valid.
  *texture_ref = local_texture_ref;
  return true;
}

void TextureManager::DoCubeMapWorkaround(
    DecoderTextureState* texture_state,
    ContextState* state,
    ErrorState* error_state,
    DecoderFramebufferState* framebuffer_state,
    TextureRef* texture_ref,
    const char* function_name,
    const DoTexImageArguments& args) {
  std::vector<GLenum> undefined_faces;
  Texture* texture = texture_ref->texture();
  if (texture_state->force_cube_complete ||
      texture_state->force_int_or_srgb_cube_texture_complete) {
    int width = 0;
    int height = 0;
    for (unsigned i = 0; i < 6; i++) {
      GLenum target = static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i);
      bool defined = texture->GetLevelSize(
          target, args.level, &width, &height, nullptr);
      if (!defined && target != args.target)
        undefined_faces.push_back(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i);
    }
  } else {
    DCHECK(args.target != GL_TEXTURE_CUBE_MAP_POSITIVE_X);
    int width = 0;
    int height = 0;
    if (!texture->GetLevelSize(GL_TEXTURE_CUBE_MAP_POSITIVE_X, args.level,
                               &width, &height, nullptr)) {
      undefined_faces.push_back(GL_TEXTURE_CUBE_MAP_POSITIVE_X);
    }
  }
  DoTexImageArguments new_args = args;
  std::unique_ptr<char[]> zero(new char[args.pixels_size]);
  memset(zero.get(), 0, args.pixels_size);
  // Need to clear PIXEL_UNPACK_BUFFER and UNPACK params for data uploading.
  state->PushTextureUnpackState();
  for (GLenum face : undefined_faces) {
    new_args.target = face;
    new_args.pixels = zero.get();
    DoTexImage(texture_state, state, error_state, framebuffer_state,
               function_name, texture_ref, new_args);
    texture->MarkLevelAsInternalWorkaround(face, args.level);
  }
  state->RestoreUnpackState();
}

void TextureManager::ValidateAndDoTexImage(
    DecoderTextureState* texture_state,
    ContextState* state,
    ErrorState* error_state,
    DecoderFramebufferState* framebuffer_state,
    const char* function_name,
    const DoTexImageArguments& args) {
  TextureRef* texture_ref;
  if (!ValidateTexImage(state, error_state, function_name, args,
                        &texture_ref)) {
    return;
  }

  Buffer* buffer = state->bound_pixel_unpack_buffer.get();

  // ValidateTexImage is passed already.
  Texture* texture = texture_ref->texture();
  bool need_cube_map_workaround =
      !feature_info_->IsWebGL2OrES3Context() &&
      texture->target() == GL_TEXTURE_CUBE_MAP &&
      (texture_state->force_cube_complete ||
       (texture_state->force_cube_map_positive_x_allocation &&
        args.target != GL_TEXTURE_CUBE_MAP_POSITIVE_X));
  // Force integer or srgb cube map texture complete, see crbug.com/712117.
  need_cube_map_workaround =
      need_cube_map_workaround ||
      (texture->target() == GL_TEXTURE_CUBE_MAP &&
       texture_state->force_int_or_srgb_cube_texture_complete &&
       (GLES2Util::IsIntegerFormat(args.internal_format) ||
        GLES2Util::GetColorEncodingFromInternalFormat(args.internal_format) ==
            GL_SRGB));

  if (need_cube_map_workaround && !buffer) {
    DoCubeMapWorkaround(texture_state, state, error_state, framebuffer_state,
                        texture_ref, function_name, args);
  }

  if (texture_state->unpack_overlapping_rows_separately_unpack_buffer &&
      buffer) {
    ContextState::Dimension dimension =
        (args.command_type == DoTexImageArguments::CommandType::kTexImage3D)
            ? ContextState::k3D
            : ContextState::k2D;
    const PixelStoreParams unpack_params(state->GetUnpackParams(dimension));
    if (unpack_params.row_length != 0 &&
        unpack_params.row_length < args.width) {
      // The rows overlap in unpack memory. Upload the texture row by row to
      // work around driver bug.

      ReserveTexImageToBeFilled(texture_state, state, error_state,
                                framebuffer_state, function_name, texture_ref,
                                args);

      DoTexSubImageArguments sub_args = {
          args.target,
          args.level,
          0,
          0,
          0,
          args.width,
          args.height,
          args.depth,
          args.format,
          args.type,
          args.pixels,
          args.pixels_size,
          args.padding,
          args.command_type == DoTexImageArguments::CommandType::kTexImage3D
              ? DoTexSubImageArguments::CommandType::kTexSubImage3D
              : DoTexSubImageArguments::CommandType::kTexSubImage2D};
      DoTexSubImageRowByRowWorkaround(texture_state, state, sub_args,
                                      unpack_params);

      SetLevelCleared(texture_ref, args.target, args.level, true);
      return;
    }
  }

  if (args.command_type == DoTexImageArguments::CommandType::kTexImage3D &&
      texture_state->unpack_image_height_workaround_with_unpack_buffer &&
      buffer) {
    ContextState::Dimension dimension = ContextState::k3D;
    const PixelStoreParams unpack_params(state->GetUnpackParams(dimension));
    if (unpack_params.image_height != 0 &&
        unpack_params.image_height != args.height) {
      ReserveTexImageToBeFilled(texture_state, state, error_state,
                                framebuffer_state, function_name, texture_ref,
                                args);

      DoTexSubImageArguments sub_args = {
          args.target,
          args.level,
          0,
          0,
          0,
          args.width,
          args.height,
          args.depth,
          args.format,
          args.type,
          args.pixels,
          args.pixels_size,
          args.padding,
          DoTexSubImageArguments::CommandType::kTexSubImage3D};
      DoTexSubImageLayerByLayerWorkaround(texture_state, state, sub_args,
                                          unpack_params);

      SetLevelCleared(texture_ref, args.target, args.level, true);
      return;
    }
  }

  if (texture_state->unpack_alignment_workaround_with_unpack_buffer && buffer &&
      args.width && args.height && args.depth) {
    uint32_t buffer_size = static_cast<uint32_t>(buffer->size());
    if (buffer_size - args.pixels_size - ToGLuint(args.pixels) < args.padding) {
      // In ValidateTexImage(), we already made sure buffer size is no less
      // than offset + pixels_size.
      ReserveTexImageToBeFilled(texture_state, state, error_state,
                                framebuffer_state, function_name, texture_ref,
                                args);

      DoTexSubImageArguments sub_args = {
          args.target,
          args.level,
          0,
          0,
          0,
          args.width,
          args.height,
          args.depth,
          args.format,
          args.type,
          args.pixels,
          args.pixels_size,
          args.padding,
          args.command_type == DoTexImageArguments::CommandType::kTexImage3D
              ? DoTexSubImageArguments::CommandType::kTexSubImage3D
              : DoTexSubImageArguments::CommandType::kTexSubImage2D};
      DoTexSubImageWithAlignmentWorkaround(texture_state, state, sub_args);

      SetLevelCleared(texture_ref, args.target, args.level, true);
      return;
    }
  }
  DoTexImage(texture_state, state, error_state, framebuffer_state,
             function_name, texture_ref, args);
}

void TextureManager::ReserveTexImageToBeFilled(
    DecoderTextureState* texture_state,
    ContextState* state,
    ErrorState* error_state,
    DecoderFramebufferState* framebuffer_state,
    const char* function_name,
    TextureRef* texture_ref,
    const DoTexImageArguments& args) {
  Buffer* buffer = state->bound_pixel_unpack_buffer.get();
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  state->SetBoundBuffer(GL_PIXEL_UNPACK_BUFFER, nullptr);
  DoTexImageArguments new_args = args;
  new_args.pixels = nullptr;
  // pixels_size might be incorrect, but it's not used in this case.
  DoTexImage(texture_state, state, error_state, framebuffer_state,
             function_name, texture_ref, new_args);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer->service_id());
  state->SetBoundBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
}

bool TextureManager::ValidateTexSubImage(ContextState* state,
                                         ErrorState* error_state,
                                         const char* function_name,
                                         const DoTexSubImageArguments& args,
                                         TextureRef** texture_ref) {
  const Validators* validators = feature_info_->validators();

  if ((args.command_type ==
           DoTexSubImageArguments::CommandType::kTexSubImage2D &&
       !validators->texture_target.IsValid(args.target)) ||
      (args.command_type ==
           DoTexSubImageArguments::CommandType::kTexSubImage3D &&
       !validators->texture_3_d_target.IsValid(args.target))) {
    ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(error_state, function_name,
                                         args.target, "target");
    return false;
  }
  DCHECK(args.width >= 0 && args.height >= 0 && args.depth >= 0);
  TextureRef* local_texture_ref = GetTextureInfoForTarget(state, args.target);
  if (!local_texture_ref) {
    ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION, function_name,
                            "unknown texture for target");
    return false;
  }
  Texture* texture = local_texture_ref->texture();
  GLenum current_type = 0;
  GLenum internal_format = 0;
  if (!texture->GetLevelType(args.target, args.level, &current_type,
                             &internal_format)) {
    std::string msg = base::StringPrintf(
        "level %d does not exist", args.level);
    ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION, function_name,
                            msg.c_str());
    return false;
  }
  if (!ValidateTextureParameters(error_state, function_name, false, args.format,
                                 args.type, internal_format, args.level)) {
    return false;
  }
  if (args.type != current_type && !feature_info_->IsWebGL2OrES3Context()) {
    // It isn't explicitly required in the ES2 spec, but some drivers generate
    // an error. It is better to be consistent across drivers.
    ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION, function_name,
                            "type does not match type of texture.");
    return false;
  }
  if (!texture->ValidForTexture(args.target, args.level,
                                args.xoffset, args.yoffset, args.zoffset,
                                args.width, args.height, args.depth)) {
    ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_VALUE, function_name,
                            "bad dimensions.");
    return false;
  }
  if ((GLES2Util::GetChannelsForFormat(args.format) &
       (GLES2Util::kDepth | GLES2Util::kStencil)) != 0 &&
      !feature_info_->IsWebGL2OrES3Context()) {
    ERRORSTATE_SET_GL_ERROR(
        error_state, GL_INVALID_OPERATION, function_name,
        "can not supply data for depth or stencil textures");
    return false;
  }

  Buffer* buffer = state->bound_pixel_unpack_buffer.get();
  if (buffer) {
    if (buffer->GetMappedRange()) {
      ERRORSTATE_SET_GL_ERROR(
          error_state, GL_INVALID_OPERATION, function_name,
          "pixel unpack buffer should not be mapped to client memory");
      return false;
    }
    if (buffer->IsBoundForTransformFeedbackAndOther()) {
      ERRORSTATE_SET_GL_ERROR(
          error_state, GL_INVALID_OPERATION, function_name,
          "pixel unpack buffer is simultaneously bound for transform feedback");
      return error::kNoError;
    }
    base::CheckedNumeric<uint32_t> size = args.pixels_size;
    GLuint offset = ToGLuint(args.pixels);
    size += offset;
    if (!size.IsValid()) {
      ERRORSTATE_SET_GL_ERROR(
          error_state, GL_INVALID_VALUE, function_name,
          "size + offset overflow");
      return false;
    }
    uint32_t buffer_size = static_cast<uint32_t>(buffer->size());
    if (buffer_size < size.ValueOrDefault(0)) {
      ERRORSTATE_SET_GL_ERROR(
          error_state, GL_INVALID_OPERATION, function_name,
          "pixel unpack buffer is not large enough");
      return false;
    }
    uint32_t type_size = GLES2Util::GetGLTypeSizeForTextures(args.type);
    DCHECK_LT(0u, type_size);
    if (offset % type_size != 0) {
      ERRORSTATE_SET_GL_ERROR(
          error_state, GL_INVALID_OPERATION, function_name,
          "offset is not evenly divisible by elements");
      return false;
    }
  } else {
    if (!args.pixels && args.pixels_size) {
      // This isn't in the spec, but the spec would define dereferencing NULL
      // here. Fail instead.
      ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION, function_name,
                              "non-empty rect without valid data");
      return false;
    }
  }
  *texture_ref = local_texture_ref;
  return true;
}

void TextureManager::ValidateAndDoTexSubImage(
    DecoderContext* decoder,
    DecoderTextureState* texture_state,
    ContextState* state,
    ErrorState* error_state,
    DecoderFramebufferState* framebuffer_state,
    const char* function_name,
    const DoTexSubImageArguments& args) {
  TRACE_EVENT0("gpu", "TextureManager::ValidateAndDoTexSubImage");
  TextureRef* texture_ref;
  if (!ValidateTexSubImage(state, error_state, function_name, args,
                           &texture_ref)) {
    return;
  }

  Texture* texture = texture_ref->texture();
  GLsizei tex_width = 0;
  GLsizei tex_height = 0;
  GLsizei tex_depth = 0;
  bool ok = texture->GetLevelSize(args.target, args.level, &tex_width,
                                  &tex_height, &tex_depth);
  DCHECK(ok);
  bool full_image;
  if (args.xoffset != 0 || args.yoffset != 0 || args.zoffset != 0 ||
      args.width != tex_width || args.height != tex_height ||
      args.depth != tex_depth) {
    gfx::Rect cleared_rect;
    if (args.command_type ==
            DoTexSubImageArguments::CommandType::kTexSubImage2D &&
        CombineAdjacentRects(
            texture->GetLevelClearedRect(args.target, args.level),
            gfx::Rect(args.xoffset, args.yoffset, args.width, args.height),
            &cleared_rect)) {
      DCHECK_GE(cleared_rect.size().GetArea(),
                texture->GetLevelClearedRect(args.target, args.level)
                    .size()
                    .GetArea());
      SetLevelClearedRect(texture_ref, args.target, args.level, cleared_rect);
    } else {
      // Otherwise clear part of texture level that is not already cleared.
      if (!ClearTextureLevel(decoder, texture_ref, args.target, args.level)) {
        ERRORSTATE_SET_GL_ERROR(error_state, GL_OUT_OF_MEMORY,
                                function_name, "dimensions too big");
        return;
      }
    }
    full_image = false;
  } else {
    SetLevelCleared(texture_ref, args.target, args.level, true);
    full_image = true;
  }

  Buffer* buffer = state->bound_pixel_unpack_buffer.get();

  if (texture_state->unpack_overlapping_rows_separately_unpack_buffer &&
      buffer) {
    ContextState::Dimension dimension =
        (args.command_type ==
         DoTexSubImageArguments::CommandType::kTexSubImage3D)
            ? ContextState::k3D
            : ContextState::k2D;
    const PixelStoreParams unpack_params(state->GetUnpackParams(dimension));
    if (unpack_params.row_length != 0 &&
        unpack_params.row_length < args.width) {
      TRACE_EVENT0("gpu", "RowByRowWorkaround");
      // The rows overlap in unpack memory. Upload the texture row by row to
      // work around driver bug.
      DoTexSubImageRowByRowWorkaround(texture_state, state, args,
                                      unpack_params);
      return;
    }
  }

  if (args.command_type ==
          DoTexSubImageArguments::CommandType::kTexSubImage3D &&
      texture_state->unpack_image_height_workaround_with_unpack_buffer &&
      buffer) {
    ContextState::Dimension dimension = ContextState::k3D;
    const PixelStoreParams unpack_params(state->GetUnpackParams(dimension));
    if (unpack_params.image_height != 0 &&
        unpack_params.image_height != args.height) {
      TRACE_EVENT0("gpu", "LayerByLayerWorkaround");
      DoTexSubImageLayerByLayerWorkaround(texture_state, state, args,
                                          unpack_params);
      return;
    }
  }

  if (texture_state->unpack_alignment_workaround_with_unpack_buffer && buffer &&
      args.width && args.height && args.depth) {
    uint32_t buffer_size = static_cast<uint32_t>(buffer->size());
    if (buffer_size - args.pixels_size - ToGLuint(args.pixels) < args.padding) {
      TRACE_EVENT0("gpu", "WithAlignmentWorkaround");
      DoTexSubImageWithAlignmentWorkaround(texture_state, state, args);
      return;
    }
  }

  if (full_image && !texture->IsImmutable() && !texture->HasImages()) {
    TRACE_EVENT0("gpu", "FullImage");
    GLenum internal_format;
    GLenum tex_type;
    texture->GetLevelType(args.target, args.level, &tex_type, &internal_format);
    // NOTE: In OpenGL ES 2/3 border is always zero. If that changes we'll need
    // to look it up.
    if (args.command_type ==
        DoTexSubImageArguments::CommandType::kTexSubImage3D) {
      glTexImage3D(args.target, args.level,
                   AdjustTexInternalFormat(feature_info_.get(), internal_format,
                                           args.type),
                   args.width, args.height, args.depth, 0,
                   AdjustTexFormat(feature_info_.get(), args.format), args.type,
                   args.pixels);
    } else {
      glTexImage2D(args.target, args.level,
                   AdjustTexInternalFormat(feature_info_.get(), internal_format,
                                           args.type),
                   args.width, args.height, 0,
                   AdjustTexFormat(feature_info_.get(), args.format), args.type,
                   args.pixels);
    }
  } else {
    TRACE_EVENT0("gpu", "SubImage");
    if (args.command_type ==
        DoTexSubImageArguments::CommandType::kTexSubImage3D) {
      glTexSubImage3D(args.target, args.level, args.xoffset, args.yoffset,
                      args.zoffset, args.width, args.height, args.depth,
                      AdjustTexFormat(feature_info_.get(), args.format),
                      args.type, args.pixels);
    } else {
      glTexSubImage2D(args.target, args.level, args.xoffset, args.yoffset,
                      args.width, args.height,
                      AdjustTexFormat(feature_info_.get(), args.format),
                      args.type, args.pixels);
    }
  }
}

void TextureManager::DoTexSubImageWithAlignmentWorkaround(
    DecoderTextureState* texture_state,
    ContextState* state,
    const DoTexSubImageArguments& args) {
  DCHECK(state->bound_pixel_unpack_buffer.get());
  DCHECK(args.width > 0 && args.height > 0 && args.depth > 0);

  uint32_t offset = ToGLuint(args.pixels);
  if (args.command_type ==
      DoTexSubImageArguments::CommandType::kTexSubImage2D) {
    PixelStoreParams params = state->GetUnpackParams(ContextState::k2D);
    if (args.height > 1) {
      glTexSubImage2D(args.target, args.level, args.xoffset, args.yoffset,
                      args.width, args.height - 1,
                      AdjustTexFormat(feature_info_.get(), args.format),
                      args.type, args.pixels);
      GLint actual_width = state->unpack_row_length > 0 ?
          state->unpack_row_length : args.width;
      uint32_t size;
      uint32_t padding;
      // No need to worry about integer overflow here.
      GLES2Util::ComputeImageDataSizesES3(actual_width, args.height - 1, 1,
                                          args.format, args.type,
                                          params,
                                          &size,
                                          nullptr, nullptr, nullptr,
                                          &padding);
      DCHECK_EQ(args.padding, padding);
      // Last row should be padded, not unpadded.
      offset += size + padding;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(args.target, args.level, args.xoffset,
                    args.yoffset + args.height - 1, args.width, 1,
                    AdjustTexFormat(feature_info_.get(), args.format),
                    args.type, reinterpret_cast<const void*>(offset));
    glPixelStorei(GL_UNPACK_ALIGNMENT, state->unpack_alignment);
    {
      uint32_t size;
      GLES2Util::ComputeImageDataSizesES3(args.width, 1, 1,
                                          args.format, args.type,
                                          params,
                                          &size,
                                          nullptr, nullptr, nullptr, nullptr);
      offset += size;
    }
  } else {  // kTexSubImage3D
    PixelStoreParams params = state->GetUnpackParams(ContextState::k3D);
    GLint actual_width = state->unpack_row_length > 0 ?
        state->unpack_row_length : args.width;
    if (args.depth > 1) {
      glTexSubImage3D(args.target, args.level, args.xoffset, args.yoffset,
                      args.zoffset, args.width, args.height, args.depth - 1,
                      AdjustTexFormat(feature_info_.get(), args.format),
                      args.type, args.pixels);
      GLint actual_height = state->unpack_image_height > 0 ?
          state->unpack_image_height : args.height;
      uint32_t size;
      uint32_t padding;
      // No need to worry about integer overflow here.
      GLES2Util::ComputeImageDataSizesES3(actual_width, actual_height,
                                          args.depth - 1,
                                          args.format, args.type,
                                          params,
                                          &size,
                                          nullptr, nullptr, nullptr,
                                          &padding);
      DCHECK_EQ(args.padding, padding);
      // Last row should be padded, not unpadded.
      offset += size + padding;
    }
    if (args.height > 1) {
      glTexSubImage3D(args.target, args.level, args.xoffset, args.yoffset,
                      args.zoffset + args.depth - 1, args.width,
                      args.height - 1, 1,
                      AdjustTexFormat(feature_info_.get(), args.format),
                      args.type, reinterpret_cast<const void*>(offset));
      uint32_t size;
      uint32_t padding;
      // No need to worry about integer overflow here.
      GLES2Util::ComputeImageDataSizesES3(actual_width, args.height - 1, 1,
                                          args.format, args.type,
                                          params,
                                          &size,
                                          nullptr, nullptr, nullptr,
                                          &padding);
      DCHECK_EQ(args.padding, padding);
      // Last row should be padded, not unpadded.
      offset += size + padding;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage3D(args.target, args.level, args.xoffset,
                    args.yoffset + args.height - 1,
                    args.zoffset + args.depth - 1, args.width, 1, 1,
                    AdjustTexFormat(feature_info_.get(), args.format),
                    args.type, reinterpret_cast<const void*>(offset));
    glPixelStorei(GL_UNPACK_ALIGNMENT, state->unpack_alignment);
    {
      uint32_t size;
      GLES2Util::ComputeImageDataSizesES3(args.width, 1, 1,
                                          args.format, args.type,
                                          params,
                                          &size,
                                          nullptr, nullptr, nullptr, nullptr);
      offset += size;
    }
  }
  DCHECK_EQ(ToGLuint(args.pixels) + args.pixels_size, offset);
}

void TextureManager::DoTexSubImageRowByRowWorkaround(
    DecoderTextureState* texture_state,
    ContextState* state,
    const DoTexSubImageArguments& args,
    const PixelStoreParams& unpack_params) {
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  DCHECK_EQ(0, state->unpack_skip_pixels);
  DCHECK_EQ(0, state->unpack_skip_rows);
  DCHECK_EQ(0, state->unpack_skip_images);

  GLenum format = AdjustTexFormat(feature_info_.get(), args.format);

  GLsizei row_bytes = unpack_params.row_length *
                      GLES2Util::ComputeImageGroupSize(format, args.type);
  GLsizei alignment_diff = row_bytes % unpack_params.alignment;
  if (alignment_diff != 0) {
    row_bytes += unpack_params.alignment - alignment_diff;
  }
  DCHECK_EQ(0, row_bytes % unpack_params.alignment);
  if (args.command_type ==
      DoTexSubImageArguments::CommandType::kTexSubImage3D) {
    GLsizei image_height = args.height;
    if (unpack_params.image_height != 0) {
      image_height = unpack_params.image_height;
    }
    GLsizei image_bytes = row_bytes * image_height;
    for (GLsizei image = 0; image < args.depth; ++image) {
      GLsizei image_byte_offset = image * image_bytes;
      for (GLsizei row = 0; row < args.height; ++row) {
        GLsizei byte_offset = image_byte_offset + row * row_bytes;
        const GLubyte* row_pixels =
            reinterpret_cast<const GLubyte*>(args.pixels) + byte_offset;
        glTexSubImage3D(args.target, args.level, args.xoffset,
                        row + args.yoffset, image + args.zoffset, args.width, 1,
                        1, format, args.type, row_pixels);
      }
    }
  } else {
    for (GLsizei row = 0; row < args.height; ++row) {
      GLsizei byte_offset = row * row_bytes;
      const GLubyte* row_pixels =
          reinterpret_cast<const GLubyte*>(args.pixels) + byte_offset;
      glTexSubImage2D(args.target, args.level, args.xoffset, row + args.yoffset,
                      args.width, 1, format, args.type, row_pixels);
    }
  }

  // Restore unpack state
  glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_params.alignment);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, unpack_params.row_length);
}

void TextureManager::DoTexSubImageLayerByLayerWorkaround(
    DecoderTextureState* texture_state,
    ContextState* state,
    const DoTexSubImageArguments& args,
    const PixelStoreParams& unpack_params) {
  glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);

  GLenum format = AdjustTexFormat(feature_info_.get(), args.format);

  GLsizei row_length =
      unpack_params.row_length ? unpack_params.row_length : args.width;
  GLsizei row_bytes =
      row_length * GLES2Util::ComputeImageGroupSize(format, args.type);
  GLsizei alignment_diff = row_bytes % unpack_params.alignment;
  if (alignment_diff != 0) {
    row_bytes += unpack_params.alignment - alignment_diff;
  }
  DCHECK_EQ(0, row_bytes % unpack_params.alignment);

  // process the texture layer by layer
  GLsizei image_height = unpack_params.image_height;
  GLsizei image_bytes = row_bytes * image_height;
  const GLubyte* image_pixels = reinterpret_cast<const GLubyte*>(args.pixels);
  for (GLsizei image = 0; image < args.depth - 1; ++image) {
    glTexSubImage3D(args.target, args.level, args.xoffset, args.yoffset,
                    image + args.zoffset, args.width, args.height, 1, format,
                    args.type, image_pixels);

    image_pixels += image_bytes;
  }

  // Process the last image row by row
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  const GLubyte* row_pixels = image_pixels;
  for (GLsizei row = 0; row < args.height; ++row) {
    glTexSubImage3D(args.target, args.level, args.xoffset, row + args.yoffset,
                    args.depth - 1 + args.zoffset, args.width, 1, 1, format,
                    args.type, row_pixels);
    row_pixels += row_bytes;
  }
  // Restore unpack state
  glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_params.alignment);
  glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, unpack_params.image_height);
}

// static
const Texture::CompatibilitySwizzle* TextureManager::GetCompatibilitySwizzle(
    const gles2::FeatureInfo* feature_info,
    GLenum format) {
  if (feature_info->gl_version_info().NeedsLuminanceAlphaEmulation()) {
    return GetCompatibilitySwizzleInternal(format);
  } else {
    return nullptr;
  }
}

// static
GLenum TextureManager::AdjustTexInternalFormat(
    const gles2::FeatureInfo* feature_info,
    GLenum format,
    GLenum type) {
  if (feature_info->gl_version_info().NeedsLuminanceAlphaEmulation()) {
    const Texture::CompatibilitySwizzle* swizzle =
        GetCompatibilitySwizzleInternal(format);
    if (swizzle) {
      if (swizzle->dest_format == GL_RED) {
        switch (type) {
          case GL_FLOAT:
            return GL_R32F;
          case GL_HALF_FLOAT:
          case GL_HALF_FLOAT_OES:
            return GL_R16F;
          default:
            return GL_R8;
        }
      } else if (swizzle->dest_format == GL_RG) {
        switch (type) {
          case GL_FLOAT:
            return GL_RG32F;
          case GL_HALF_FLOAT:
          case GL_HALF_FLOAT_OES:
            return GL_RG16F;
          default:
            return GL_RG8;
        }
      } else {
        NOTREACHED();
      }
    }
  }
  return format;
}

// static
GLenum TextureManager::AdjustTexFormat(const gles2::FeatureInfo* feature_info,
                                       GLenum format) {
  // TODO(bajones): GLES 3 allows for internal format and format to differ.
  // This logic may need to change as a result.
  if (!feature_info->gl_version_info().is_es) {
    if (format == GL_SRGB_EXT)
      return GL_RGB;
    if (format == GL_SRGB_ALPHA_EXT)
      return GL_RGBA;
  }
  if (feature_info->gl_version_info().NeedsLuminanceAlphaEmulation()) {
    const Texture::CompatibilitySwizzle* swizzle =
        GetCompatibilitySwizzleInternal(format);
    if (swizzle)
      return swizzle->dest_format;
  }
  return format;
}

// static
GLenum TextureManager::AdjustTexStorageFormat(
    const gles2::FeatureInfo* feature_info,
    GLenum format) {
  if (feature_info->gl_version_info().NeedsLuminanceAlphaEmulation()) {
    switch (format) {
      case GL_ALPHA8_EXT:
        return GL_R8_EXT;
      case GL_LUMINANCE8_EXT:
        return GL_R8_EXT;
      case GL_LUMINANCE8_ALPHA8_EXT:
        return GL_RG8_EXT;
      case GL_ALPHA16F_EXT:
        return GL_R16F_EXT;
      case GL_LUMINANCE16F_EXT:
        return GL_R16F_EXT;
      case GL_LUMINANCE_ALPHA16F_EXT:
        return GL_RG16F_EXT;
      case GL_ALPHA32F_EXT:
        return GL_R32F_EXT;
      case GL_LUMINANCE32F_EXT:
        return GL_R32F_EXT;
      case GL_LUMINANCE_ALPHA32F_EXT:
        return GL_RG32F_EXT;
    }
  }
  return format;
}

void TextureManager::DoTexImage(DecoderTextureState* texture_state,
                                ContextState* state,
                                ErrorState* error_state,
                                DecoderFramebufferState* framebuffer_state,
                                const char* function_name,
                                TextureRef* texture_ref,
                                const DoTexImageArguments& args) {
  Texture* texture = texture_ref->texture();
  GLsizei tex_width = 0;
  GLsizei tex_height = 0;
  GLsizei tex_depth = 0;
  GLenum tex_type = 0;
  GLenum tex_internal_format = 0;
  bool level_is_same =
      texture->GetLevelSize(
          args.target, args.level, &tex_width, &tex_height, &tex_depth) &&
      args.width == tex_width && args.height == tex_height &&
      args.depth == tex_depth &&
      texture->GetLevelType(
          args.target, args.level, &tex_type, &tex_internal_format) &&
      args.type == tex_type && args.internal_format == tex_internal_format;

  bool unpack_buffer_bound =
      (state->bound_pixel_unpack_buffer.get() != nullptr);

  if (level_is_same && !args.pixels && !unpack_buffer_bound) {
    // Just set the level texture but mark the texture as uncleared.
    SetLevelInfo(
        texture_ref, args.target, args.level, args.internal_format, args.width,
        args.height, args.depth, args.border, args.format, args.type,
        gfx::Rect());
    texture_state->tex_image_failed = false;
    return;
  }

  if (texture->IsAttachedToFramebuffer()) {
    framebuffer_state->clear_state_dirty = true;
  }

  ERRORSTATE_COPY_REAL_GL_ERRORS_TO_WRAPPER(error_state, function_name);
  {
    if (args.command_type == DoTexImageArguments::CommandType::kTexImage3D) {
      glTexImage3D(args.target, args.level,
                   AdjustTexInternalFormat(feature_info_.get(),
                                           args.internal_format, args.type),
                   args.width, args.height, args.depth, args.border,
                   AdjustTexFormat(feature_info_.get(), args.format), args.type,
                   args.pixels);
    } else {
      glTexImage2D(args.target, args.level,
                   AdjustTexInternalFormat(feature_info_.get(),
                                           args.internal_format, args.type),
                   args.width, args.height, args.border,
                   AdjustTexFormat(feature_info_.get(), args.format), args.type,
                   args.pixels);
    }
  }
  GLenum error = ERRORSTATE_PEEK_GL_ERROR(error_state, function_name);
  if (args.command_type == DoTexImageArguments::CommandType::kTexImage3D) {
    UMA_HISTOGRAM_CUSTOM_ENUMERATION("GPU.Error_TexImage3D", error,
        GetAllGLErrors());
  } else {
    UMA_HISTOGRAM_CUSTOM_ENUMERATION("GPU.Error_TexImage2D", error,
        GetAllGLErrors());
  }
  if (error == GL_NO_ERROR) {
    bool set_as_cleared = (args.pixels != nullptr || unpack_buffer_bound);
    SetLevelInfo(
        texture_ref, args.target, args.level, args.internal_format, args.width,
        args.height, args.depth, args.border, args.format, args.type,
        set_as_cleared ? gfx::Rect(args.width, args.height) : gfx::Rect());
    texture->ApplyFormatWorkarounds(feature_info_.get());
    texture_state->tex_image_failed = false;
  }
}

bool TextureManager::CombineAdjacentRects(const gfx::Rect& rect1,
                                          const gfx::Rect& rect2,
                                          gfx::Rect* result) {
  // Return |rect2| if |rect1| is empty or |rect2| contains |rect1|.
  if (rect1.IsEmpty() || rect2.Contains(rect1)) {
    *result = rect2;
    return true;
  }

  // Return |rect1| if |rect2| is empty or |rect1| contains |rect2|.
  if (rect2.IsEmpty() || rect1.Contains(rect2)) {
    *result = rect1;
    return true;
  }

  // Return the union of |rect1| and |rect2| if they share an edge.
  if (rect1.SharesEdgeWith(rect2)) {
    *result = gfx::UnionRects(rect1, rect2);
    return true;
  }

  // Return false if it's not possible to combine |rect1| and |rect2|.
  return false;
}

bool TextureManager::OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                                  base::trace_event::ProcessMemoryDump* pmd) {
  if (args.level_of_detail == MemoryDumpLevelOfDetail::BACKGROUND) {
    std::string dump_name =
        base::StringPrintf("gpu/gl/textures/context_group_0x%" PRIX64,
                           memory_tracker_->ContextGroupTracingId());
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, mem_represented());

    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  for (const auto& resource : textures_) {
    // Only dump memory info for textures actually owned by this
    // TextureManager.
    DumpTextureRef(pmd, resource.second.get());
  }

  // Also dump TextureManager internal textures, if allocated.
  for (int i = 0; i < kNumDefaultTextures; i++) {
    if (default_textures_[i]) {
      DumpTextureRef(pmd, default_textures_[i].get());
    }
  }

  return true;
}

void TextureManager::DumpTextureRef(base::trace_event::ProcessMemoryDump* pmd,
                                    TextureRef* ref) {
  uint32_t size = ref->texture()->estimated_size();

  // Ignore unallocated texture IDs.
  if (size == 0)
    return;

  std::string dump_name = base::StringPrintf(
      "gpu/gl/textures/context_group_0x%" PRIX64 "/texture_0x%" PRIX32,
      memory_tracker_->ContextGroupTracingId(), ref->client_id());

  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes,
                  static_cast<uint64_t>(size));

  // Add the |client_guid| which expresses shared ownership with the client
  // process.
  auto client_guid = gl::GetGLTextureClientGUIDForTracing(
      memory_tracker_->ContextGroupTracingId(), ref->client_id());
  pmd->CreateSharedGlobalAllocatorDump(client_guid);
  pmd->AddOwnershipEdge(dump->guid(), client_guid);

  // Add a |service_guid| which expresses shared ownership between the various
  // |client_guid|s.
  auto service_guid =
      gl::GetGLTextureServiceGUIDForTracing(ref->texture()->service_id());
  pmd->CreateSharedGlobalAllocatorDump(service_guid);

  int importance = 0;  // Default importance.
  // The link to the memory tracking |client_id| is given a higher importance
  // than other refs.
  if (!ref->texture()->has_lightweight_ref_ &&
      (ref == ref->texture()->memory_tracking_ref_))
    importance = 2;

  pmd->AddOwnershipEdge(client_guid, service_guid, importance);

  // Dump all sub-levels held by the texture. They will appear below the main
  // gl/textures/client_X/texture_Y dump.
  ref->texture()->DumpLevelMemory(pmd, memory_tracker_->ClientTracingId(),
                                  dump_name);
}

GLenum TextureManager::ExtractFormatFromStorageFormat(GLenum internalformat) {
  switch (internalformat) {
    case GL_COMPRESSED_R11_EAC:
    case GL_COMPRESSED_SIGNED_R11_EAC:
    case GL_RED:
    case GL_R8:
    case GL_R8_SNORM:
    case GL_R16F:
    case GL_R32F:
    case GL_R16_EXT:
      return GL_RED;
    case GL_R8UI:
    case GL_R8I:
    case GL_R16UI:
    case GL_R16I:
    case GL_R32UI:
    case GL_R32I:
      return GL_RED_INTEGER;
    case GL_COMPRESSED_RG11_EAC:
    case GL_COMPRESSED_SIGNED_RG11_EAC:
    case GL_RG:
    case GL_RG8:
    case GL_RG8_SNORM:
    case GL_RG16F:
    case GL_RG32F:
      return GL_RG;
    case GL_RG8UI:
    case GL_RG8I:
    case GL_RG16UI:
    case GL_RG16I:
    case GL_RG32UI:
    case GL_RG32I:
      return GL_RG_INTEGER;
    case GL_ATC_RGB_AMD:
    case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
    case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGB8_ETC2:
    case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB8_ETC2:
    case GL_ETC1_RGB8_OES:
    case GL_RGB:
    case GL_RGB8:
    case GL_SRGB8:
    case GL_R11F_G11F_B10F:
    case GL_RGB565:
    case GL_RGB8_SNORM:
    case GL_RGB9_E5:
    case GL_RGB16F:
    case GL_RGB32F:
    case GL_RGB_YCBCR_420V_CHROMIUM:
    case GL_RGB_YCBCR_422_CHROMIUM:
    case GL_RGB_YCRCB_420_CHROMIUM:
      return GL_RGB;
    case GL_RGB8UI:
    case GL_RGB8I:
    case GL_RGB16UI:
    case GL_RGB16I:
    case GL_RGB32UI:
    case GL_RGB32I:
      return GL_RGB_INTEGER;
    case GL_SRGB:
      return GL_SRGB;
    case GL_ATC_RGBA_EXPLICIT_ALPHA_AMD:
    case GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD:
    case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GL_COMPRESSED_RGBA8_ETC2_EAC:
    case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
    case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
    case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GL_RGBA:
    case GL_RGBA8:
    case GL_SRGB8_ALPHA8:
    case GL_RGBA8_SNORM:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGB10_A2:
    case GL_RGBA16F:
    case GL_RGBA32F:
      return GL_RGBA;
    case GL_SRGB_ALPHA:
      return GL_SRGB_ALPHA;
    case GL_RGBA8UI:
    case GL_RGBA8I:
    case GL_RGB10_A2UI:
    case GL_RGBA16UI:
    case GL_RGBA16I:
    case GL_RGBA32UI:
    case GL_RGBA32I:
      return GL_RGBA_INTEGER;
    case GL_BGRA_EXT:
    case GL_BGRA8_EXT:
      return GL_BGRA_EXT;
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32F:
      return GL_DEPTH_COMPONENT;
    case GL_DEPTH24_STENCIL8:
    case GL_DEPTH32F_STENCIL8:
      return GL_DEPTH_STENCIL;
    case GL_LUMINANCE_ALPHA:
    case GL_LUMINANCE8_ALPHA8_EXT:
      return GL_LUMINANCE_ALPHA;
    case GL_LUMINANCE:
    case GL_LUMINANCE8_EXT:
      return GL_LUMINANCE;
    case GL_ALPHA:
    case GL_ALPHA8_EXT:
      return GL_ALPHA;
    case GL_ALPHA32F_EXT:
      return GL_ALPHA;
    case GL_LUMINANCE32F_EXT:
      return GL_LUMINANCE;
    case GL_LUMINANCE_ALPHA32F_EXT:
      return GL_LUMINANCE_ALPHA;
    case GL_ALPHA16F_EXT:
      return GL_ALPHA;
    case GL_LUMINANCE16F_EXT:
      return GL_LUMINANCE;
    case GL_LUMINANCE_ALPHA16F_EXT:
      return GL_LUMINANCE_ALPHA;
    default:
      return GL_NONE;
  }
}

GLenum TextureManager::ExtractTypeFromStorageFormat(GLenum internalformat) {
  switch (internalformat) {
    case GL_RED:
    case GL_RG:
    case GL_RGB:
    case GL_SRGB:
    case GL_RGBA:
    case GL_BGRA_EXT:
    case GL_SRGB_ALPHA:
    case GL_LUMINANCE_ALPHA:
    case GL_LUMINANCE:
    case GL_ALPHA:
    case GL_R8:
      return GL_UNSIGNED_BYTE;
    case GL_R8_SNORM:
      return GL_BYTE;
    case GL_R16F:
      return GL_HALF_FLOAT;
    case GL_R32F:
      return GL_FLOAT;
    case GL_R8UI:
      return GL_UNSIGNED_BYTE;
    case GL_R8I:
      return GL_BYTE;
    case GL_R16UI:
      return GL_UNSIGNED_SHORT;
    case GL_R16I:
      return GL_SHORT;
    case GL_R16_EXT:
      return GL_UNSIGNED_SHORT;
    case GL_R32UI:
      return GL_UNSIGNED_INT;
    case GL_R32I:
      return GL_INT;
    case GL_RG8:
      return GL_UNSIGNED_BYTE;
    case GL_RG8_SNORM:
      return GL_BYTE;
    case GL_RG16F:
      return GL_HALF_FLOAT;
    case GL_RG32F:
      return GL_FLOAT;
    case GL_RG8UI:
      return GL_UNSIGNED_BYTE;
    case GL_RG8I:
      return GL_BYTE;
    case GL_RG16UI:
      return GL_UNSIGNED_SHORT;
    case GL_RG16I:
      return GL_SHORT;
    case GL_RG32UI:
      return GL_UNSIGNED_INT;
    case GL_RG32I:
      return GL_INT;
    case GL_RGB8:
    case GL_SRGB8:
      return GL_UNSIGNED_BYTE;
    case GL_R11F_G11F_B10F:
      return GL_UNSIGNED_INT_10F_11F_11F_REV;
    case GL_RGB565:
      return GL_UNSIGNED_SHORT_5_6_5;
    case GL_RGB8_SNORM:
      return GL_BYTE;
    case GL_RGB9_E5:
      return GL_UNSIGNED_INT_5_9_9_9_REV;
    case GL_RGB16F:
      return GL_HALF_FLOAT;
    case GL_RGB32F:
      return GL_FLOAT;
    case GL_RGB8UI:
      return GL_UNSIGNED_BYTE;
    case GL_RGB8I:
      return GL_BYTE;
    case GL_RGB16UI:
      return GL_UNSIGNED_SHORT;
    case GL_RGB16I:
      return GL_SHORT;
    case GL_RGB32UI:
      return GL_UNSIGNED_INT;
    case GL_RGB32I:
      return GL_INT;
    case GL_RGBA8:
      return GL_UNSIGNED_BYTE;
    case GL_SRGB8_ALPHA8:
      return GL_UNSIGNED_BYTE;
    case GL_RGBA8_SNORM:
      return GL_BYTE;
    case GL_RGBA4:
      return GL_UNSIGNED_SHORT_4_4_4_4;
    case GL_RGB10_A2:
      return GL_UNSIGNED_INT_2_10_10_10_REV;
    case GL_RGB5_A1:
      return GL_UNSIGNED_SHORT_5_5_5_1;
    case GL_RGBA16F:
      return GL_HALF_FLOAT;
    case GL_RGBA32F:
      return GL_FLOAT;
    case GL_RGBA8UI:
      return GL_UNSIGNED_BYTE;
    case GL_RGBA8I:
      return GL_BYTE;
    case GL_RGB10_A2UI:
      return GL_UNSIGNED_INT_2_10_10_10_REV;
    case GL_RGBA16UI:
      return GL_UNSIGNED_SHORT;
    case GL_RGBA16I:
      return GL_SHORT;
    case GL_RGBA32I:
      return GL_INT;
    case GL_RGBA32UI:
      return GL_UNSIGNED_INT;
    case GL_DEPTH_COMPONENT16:
      return GL_UNSIGNED_SHORT;
    case GL_DEPTH_COMPONENT24:
      return GL_UNSIGNED_INT;
    case GL_DEPTH_COMPONENT32F:
      return GL_FLOAT;
    case GL_DEPTH24_STENCIL8:
      return GL_UNSIGNED_INT_24_8;
    case GL_DEPTH32F_STENCIL8:
      return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
    case GL_LUMINANCE8_ALPHA8_EXT:
      return GL_UNSIGNED_BYTE;
    case GL_LUMINANCE8_EXT:
      return GL_UNSIGNED_BYTE;
    case GL_ALPHA8_EXT:
      return GL_UNSIGNED_BYTE;
    case GL_ALPHA32F_EXT:
      return GL_FLOAT;
    case GL_LUMINANCE32F_EXT:
      return GL_FLOAT;
    case GL_LUMINANCE_ALPHA32F_EXT:
      return GL_FLOAT;
    case GL_ALPHA16F_EXT:
      return GL_HALF_FLOAT_OES;
    case GL_LUMINANCE16F_EXT:
      return GL_HALF_FLOAT_OES;
    case GL_LUMINANCE_ALPHA16F_EXT:
      return GL_HALF_FLOAT_OES;
    case GL_BGRA8_EXT:
      return GL_UNSIGNED_BYTE;
    default:
      return GL_NONE;
  }
}

void Texture::IncrementManagerServiceIdGeneration() {
  for (auto* ref : refs_) {
    TextureManager* manager = ref->manager();
    manager->IncrementServiceIdGeneration();
  }
}

uint32_t TextureManager::GetServiceIdGeneration() const {
  return current_service_id_generation_;
}

void TextureManager::IncrementServiceIdGeneration() {
  current_service_id_generation_++;
}

const Texture::LevelInfo* Texture::GetBaseLevelInfo() const {
  if (face_infos_.empty() ||
      static_cast<size_t>(base_level_) >= face_infos_[0].level_infos.size()) {
    return nullptr;
  }
  return &face_infos_[0].level_infos[base_level_];
}

GLenum Texture::GetInternalFormatOfBaseLevel() const {
  const LevelInfo* level_info = GetBaseLevelInfo();
  return level_info ? level_info->internal_format : GL_NONE;
}

bool Texture::CompatibleWithSamplerUniformType(
    GLenum type,
    const SamplerState& sampler_state) const {
  enum {
    SAMPLER_INVALID,
    SAMPLER_FLOAT,
    SAMPLER_UNSIGNED,
    SAMPLER_SIGNED,
    SAMPLER_SHADOW,
  } category = SAMPLER_INVALID;

  switch (type) {
    case GL_SAMPLER_2D:
    case GL_SAMPLER_2D_RECT_ARB:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_EXTERNAL_OES:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_2D_ARRAY:
      category = SAMPLER_FLOAT;
      break;
    case GL_INT_SAMPLER_2D:
    case GL_INT_SAMPLER_3D:
    case GL_INT_SAMPLER_CUBE:
    case GL_INT_SAMPLER_2D_ARRAY:
      category = SAMPLER_SIGNED;
      break;
    case GL_UNSIGNED_INT_SAMPLER_2D:
    case GL_UNSIGNED_INT_SAMPLER_3D:
    case GL_UNSIGNED_INT_SAMPLER_CUBE:
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
      category = SAMPLER_UNSIGNED;
      break;
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_2D_ARRAY_SHADOW:
    case GL_SAMPLER_CUBE_SHADOW:
    case GL_SAMPLER_2D_RECT_SHADOW_ARB:
      category = SAMPLER_SHADOW;
      break;
    default:
      NOTREACHED();
  }

  const LevelInfo* level_info = GetBaseLevelInfo();
  if (!level_info) {
    return false;
  }
  if ((level_info->format == GL_DEPTH_COMPONENT ||
       level_info->format == GL_DEPTH_STENCIL) &&
      sampler_state.compare_mode != GL_NONE) {
    // If TEXTURE_COMPARE_MODE is set, then depth textures can only be sampled
    // by shadow samplers.
    return category == SAMPLER_SHADOW;
  }

  if (level_info->type == GL_NONE && level_info->format == GL_NONE &&
      level_info->internal_format != GL_NONE) {
    // This is probably a compressed texture format. All compressed formats are
    // sampled as float.
    return category == SAMPLER_FLOAT;
  }

  bool normalized =
      level_info->format == GL_RED || level_info->format == GL_RG ||
      level_info->format == GL_RGB || level_info->format == GL_RGBA ||
      level_info->format == GL_DEPTH_COMPONENT ||
      level_info->format == GL_DEPTH_STENCIL ||
      level_info->format == GL_LUMINANCE_ALPHA ||
      level_info->format == GL_LUMINANCE || level_info->format == GL_ALPHA ||
      level_info->format == GL_BGRA_EXT || level_info->format == GL_SRGB_EXT ||
      level_info->format == GL_SRGB_ALPHA_EXT;
  if (normalized) {
    // All normalized texture formats are sampled as float.
    return category == SAMPLER_FLOAT;
  }

  switch (level_info->type) {
    case GL_HALF_FLOAT:
    case GL_FLOAT:
    case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
      // Float formats.
      return category == SAMPLER_FLOAT;
    case GL_BYTE:
    case GL_SHORT:
    case GL_INT:
      // Signed integer formats.
      return category == SAMPLER_SIGNED;
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_SHORT:
    case GL_UNSIGNED_INT:
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
    case GL_UNSIGNED_INT_5_9_9_9_REV:
    case GL_UNSIGNED_INT_24_8:
      // Unsigned integer formats.
      return category == SAMPLER_UNSIGNED;
    default:
      NOTREACHED() << "Type: " << GLES2Util::GetStringEnum(level_info->type)
                   << " Format: "
                   << GLES2Util::GetStringEnum(level_info->format)
                   << "  Internal format: "
                   << GLES2Util::GetStringEnum(level_info->internal_format);
  }
  return false;
}

}  // namespace gles2
}  // namespace gpu
