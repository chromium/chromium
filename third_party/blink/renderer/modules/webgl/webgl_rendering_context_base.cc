/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/numerics/checked_math.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "device/vr/buildflags/buildflags.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "media/base/video_frame.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/gpu/gpu.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_htmlcanvaselement_offscreencanvas.h"
#include "third_party/blink/renderer/bindings/modules/v8/webgl_any.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/dactyloscoper.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webgl/angle_instanced_arrays.h"
#include "third_party/blink/renderer/modules/webgl/ext_blend_min_max.h"
#include "third_party/blink/renderer/modules/webgl/ext_frag_depth.h"
#include "third_party/blink/renderer/modules/webgl/ext_shader_texture_lod.h"
#include "third_party/blink/renderer/modules/webgl/ext_texture_filter_anisotropic.h"
#include "third_party/blink/renderer/modules/webgl/gl_string_query.h"
#include "third_party/blink/renderer/modules/webgl/oes_element_index_uint.h"
#include "third_party/blink/renderer/modules/webgl/oes_standard_derivatives.h"
#include "third_party/blink/renderer/modules/webgl/oes_texture_float.h"
#include "third_party/blink/renderer/modules/webgl/oes_texture_float_linear.h"
#include "third_party/blink/renderer/modules/webgl/oes_texture_half_float.h"
#include "third_party/blink/renderer/modules/webgl/oes_texture_half_float_linear.h"
#include "third_party/blink/renderer/modules/webgl/oes_vertex_array_object.h"
#include "third_party/blink/renderer/modules/webgl/webgl_active_info.h"
#include "third_party/blink/renderer/modules/webgl/webgl_buffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_astc.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_etc.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_etc1.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_pvrtc.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_s3tc.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_s3tc_srgb.h"
#include "third_party/blink/renderer/modules/webgl/webgl_context_attribute_helpers.h"
#include "third_party/blink/renderer/modules/webgl/webgl_context_event.h"
#include "third_party/blink/renderer/modules/webgl/webgl_context_group.h"
#include "third_party/blink/renderer/modules/webgl/webgl_debug_renderer_info.h"
#include "third_party/blink/renderer/modules/webgl/webgl_debug_shaders.h"
#include "third_party/blink/renderer/modules/webgl/webgl_depth_texture.h"
#include "third_party/blink/renderer/modules/webgl/webgl_draw_buffers.h"
#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_lose_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_program.h"
#include "third_party/blink/renderer/modules/webgl/webgl_renderbuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_shader.h"
#include "third_party/blink/renderer/modules/webgl/webgl_shader_precision_format.h"
#include "third_party/blink/renderer/modules/webgl/webgl_uniform_location.h"
#include "third_party/blink/renderer/modules/webgl/webgl_vertex_array_object.h"
#include "third_party/blink/renderer/modules/webgl/webgl_vertex_array_object_oes.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/image_extractor.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/sk_image_info_hash.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/size.h"

// Populates parameters from texImage2D except for border, width, height, and
// depth (which are not present for all texImage2D functions).
#define POPULATE_TEX_IMAGE_2D_PARAMS(params, src_type) \
  params = {                                           \
      .source_type = src_type,                         \
      .function_id = kTexImage2D,                      \
      .target = target,                                \
      .level = level,                                  \
      .internalformat = internalformat,                \
      .format = format,                                \
      .type = type,                                    \
  };                                                   \
  GetCurrentUnpackState(params)

#define POPULATE_TEX_SUB_IMAGE_2D_PARAMS(params, src_type) \
  params = {                                               \
      .source_type = src_type,                             \
      .function_id = kTexSubImage2D,                       \
      .target = target,                                    \
      .level = level,                                      \
      .xoffset = xoffset,                                  \
      .yoffset = yoffset,                                  \
      .format = format,                                    \
      .type = type,                                        \
  };                                                       \
  GetCurrentUnpackState(params)

namespace blink {

bool WebGLRenderingContextBase::webgl_context_limits_initialized_ = false;
unsigned WebGLRenderingContextBase::max_active_webgl_contexts_ = 0;
unsigned WebGLRenderingContextBase::max_active_webgl_contexts_on_worker_ = 0;

namespace {

enum class WebGLANGLEImplementation {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // vWebGL = 0 (for WebGL1) or 2 (for WebGL2).
  // vWebGLANGLEImplementation = vWebGL * 10 + vANGLEImplementation
  // where vANGLEImplementation is aligned with ANGLEImplementation enum
  // values defined in ui/gl/gl_implementation.h.

  kWebGL1_None = 0,
  kWebGL1_D3D9 = 1,
  kWebGL1_D3D11 = 2,
  kWebGL1_OpenGL = 3,
  kWebGL1_OpenGLES = 4,
  kWebGL1_Null = 5,
  kWebGL1_Vulkan = 6,
  kWebGL1_SwiftShader = 7,
  kWebGL1_Metal = 8,
  kWebGL1_Default = 9,

  // Leave some space between WebGL1 and WebGL2 enums in case ANGLE has
  // new implementations, say ANGLE/Dawn.

  kWebGL2_None = 20,
  kWebGL2_D3D9 = 21,  // Should never happen
  kWebGL2_D3D11 = 22,
  kWebGL2_OpenGL = 23,
  kWebGL2_OpenGLES = 24,
  kWebGL2_Null = 25,
  kWebGL2_Vulkan = 26,
  kWebGL2_SwiftShader = 27,
  kWebGL2_Metal = 28,
  kWebGL2_Default = 29,

  kMaxValue = kWebGL2_Default,
};

constexpr base::TimeDelta kDurationBetweenRestoreAttempts = base::Seconds(1);
const int kMaxGLErrorsAllowedToConsole = 256;

base::Lock& WebGLContextLimitLock() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(base::Lock, lock, ());
  return lock;
}

using WebGLRenderingContextBaseSet =
    HeapHashSet<WeakMember<WebGLRenderingContextBase>>;
WebGLRenderingContextBaseSet& ActiveContexts() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<Persistent<WebGLRenderingContextBaseSet>>, active_contexts,
      ());
  Persistent<WebGLRenderingContextBaseSet>& active_contexts_persistent =
      *active_contexts;
  if (!active_contexts_persistent) {
    active_contexts_persistent =
        MakeGarbageCollected<WebGLRenderingContextBaseSet>();
    LEAK_SANITIZER_IGNORE_OBJECT(&active_contexts_persistent);
  }
  return *active_contexts_persistent;
}

using WebGLRenderingContextBaseMap =
    HeapHashMap<WeakMember<WebGLRenderingContextBase>, int>;
WebGLRenderingContextBaseMap& ForciblyEvictedContexts() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<Persistent<WebGLRenderingContextBaseMap>>,
      forcibly_evicted_contexts, ());
  Persistent<WebGLRenderingContextBaseMap>&
      forcibly_evicted_contexts_persistent = *forcibly_evicted_contexts;
  if (!forcibly_evicted_contexts_persistent) {
    forcibly_evicted_contexts_persistent =
        MakeGarbageCollected<WebGLRenderingContextBaseMap>();
    LEAK_SANITIZER_IGNORE_OBJECT(&forcibly_evicted_contexts_persistent);
  }
  return *forcibly_evicted_contexts_persistent;
}

}  // namespace

ScopedRGBEmulationColorMask::ScopedRGBEmulationColorMask(
    WebGLRenderingContextBase* context,
    GLboolean* color_mask,
    DrawingBuffer* drawing_buffer)
    : context_(context),
      requires_emulation_(drawing_buffer->RequiresAlphaChannelToBePreserved()) {
  if (requires_emulation_) {
    context_->active_scoped_rgb_emulation_color_masks_++;
    memcpy(color_mask_.data(), color_mask, 4 * sizeof(GLboolean));
    context_->ContextGL()->ColorMask(color_mask_[0], color_mask_[1],
                                     color_mask_[2], false);
  }
}

ScopedRGBEmulationColorMask::~ScopedRGBEmulationColorMask() {
  if (requires_emulation_) {
    DCHECK(context_->active_scoped_rgb_emulation_color_masks_);
    context_->active_scoped_rgb_emulation_color_masks_--;
    context_->ContextGL()->ColorMask(color_mask_[0], color_mask_[1],
                                     color_mask_[2], color_mask_[3]);
  }
}

void WebGLRenderingContextBase::InitializeWebGLContextLimits(
    WebGraphicsContext3DProvider* context_provider) {
  base::AutoLock locker(WebGLContextLimitLock());
  if (!webgl_context_limits_initialized_) {
    // These do not change over the lifetime of the browser.
    auto webgl_preferences = context_provider->GetWebglPreferences();
    max_active_webgl_contexts_ = webgl_preferences.max_active_webgl_contexts;
    max_active_webgl_contexts_on_worker_ =
        webgl_preferences.max_active_webgl_contexts_on_worker;
    webgl_context_limits_initialized_ = true;
  }
}

unsigned WebGLRenderingContextBase::CurrentMaxGLContexts() {
  base::AutoLock locker(WebGLContextLimitLock());
  DCHECK(webgl_context_limits_initialized_);
  return IsMainThread() ? max_active_webgl_contexts_
                        : max_active_webgl_contexts_on_worker_;
}

void WebGLRenderingContextBase::ForciblyLoseOldestContext(
    const String& reason) {
  WebGLRenderingContextBase* candidate = OldestContext();
  if (!candidate)
    return;

  candidate->PrintWarningToConsole(reason);
  probe::DidFireWebGLWarning(candidate->canvas());

  // This will call deactivateContext once the context has actually been lost.
  candidate->ForceLostContext(WebGLRenderingContextBase::kSyntheticLostContext,
                              WebGLRenderingContextBase::kWhenAvailable);
}

WebGLRenderingContextBase* WebGLRenderingContextBase::OldestContext() {
  if (ActiveContexts().empty())
    return nullptr;

  WebGLRenderingContextBase* candidate = *(ActiveContexts().begin());
  DCHECK(!candidate->isContextLost());
  for (WebGLRenderingContextBase* context : ActiveContexts()) {
    DCHECK(!context->isContextLost());
    if (context->ContextGL()->GetLastFlushIdCHROMIUM() <
        candidate->ContextGL()->GetLastFlushIdCHROMIUM()) {
      candidate = context;
    }
  }

  return candidate;
}

WebGLRenderingContextBase* WebGLRenderingContextBase::OldestEvictedContext() {
  if (ForciblyEvictedContexts().empty())
    return nullptr;

  WebGLRenderingContextBase* candidate = nullptr;
  int generation = -1;
  for (WebGLRenderingContextBase* context : ForciblyEvictedContexts().Keys()) {
    if (!candidate || ForciblyEvictedContexts().at(context) < generation) {
      candidate = context;
      generation = ForciblyEvictedContexts().at(context);
    }
  }

  return candidate;
}

void WebGLRenderingContextBase::ActivateContext(
    WebGLRenderingContextBase* context) {
  unsigned max_gl_contexts = CurrentMaxGLContexts();
  unsigned removed_contexts = 0;
  while (ActiveContexts().size() >= max_gl_contexts &&
         removed_contexts < max_gl_contexts) {
    ForciblyLoseOldestContext(
        "WARNING: Too many active WebGL contexts. Oldest context will be "
        "lost.");
    removed_contexts++;
  }

  DCHECK(!context->isContextLost());
  ActiveContexts().insert(context);
}

void WebGLRenderingContextBase::DeactivateContext(
    WebGLRenderingContextBase* context) {
  ActiveContexts().erase(context);
}

void WebGLRenderingContextBase::AddToEvictedList(
    WebGLRenderingContextBase* context) {
  static int generation = 0;
  ForciblyEvictedContexts().Set(context, generation++);
}

void WebGLRenderingContextBase::RemoveFromEvictedList(
    WebGLRenderingContextBase* context) {
  ForciblyEvictedContexts().erase(context);
}

void WebGLRenderingContextBase::RestoreEvictedContext(
    WebGLRenderingContextBase* context) {
  // These two sets keep weak references to their contexts;
  // verify that the GC already removed the |context| entries.
  DCHECK(!ForciblyEvictedContexts().Contains(context));
  DCHECK(!ActiveContexts().Contains(context));

  unsigned max_gl_contexts = CurrentMaxGLContexts();
  // Try to re-enable the oldest inactive contexts.
  while (ActiveContexts().size() < max_gl_contexts &&
         ForciblyEvictedContexts().size()) {
    WebGLRenderingContextBase* evicted_context = OldestEvictedContext();
    if (!evicted_context->restore_allowed_) {
      ForciblyEvictedContexts().erase(evicted_context);
      continue;
    }

    gfx::Size desired_size = DrawingBuffer::AdjustSize(
        evicted_context->ClampedCanvasSize(), gfx::Size(),
        evicted_context->max_texture_size_);

    // If there's room in the pixel budget for this context, restore it.
    if (!desired_size.IsEmpty()) {
      ForciblyEvictedContexts().erase(evicted_context);
      evicted_context->ForceRestoreContext();
    }
    break;
  }
}

namespace {

GLint Clamp(GLint value, GLint min, GLint max) {
  if (value < min)
    value = min;
  if (value > max)
    value = max;
  return value;
}

// Replaces non-ASCII characters with a placeholder. Given
// shaderSource's new rules as of
// https://github.com/KhronosGroup/WebGL/pull/3206 , the browser must
// not generate INVALID_VALUE for these out-of-range characters.
// Shader compilation must fail for invalid constructs farther in the
// pipeline.
class ReplaceNonASCII {
 public:
  ReplaceNonASCII(const String& str) { Parse(str); }

  String Result() { return builder_.ToString(); }

 private:
  void Parse(const String& source_string) {
    unsigned len = source_string.length();
    for (unsigned i = 0; i < len; ++i) {
      UChar current = source_string[i];
      if (WTF::IsASCII(current))
        builder_.Append(current);
      else
        builder_.Append('?');
    }
  }

  StringBuilder builder_;
};

static bool g_should_fail_context_creation_for_testing = false;
}  // namespace

// This class interrupts any active pixel local storage rendering pass, if the
// extension has been used by the context.
class ScopedPixelLocalStorageInterrupt {
  STACK_ALLOCATED();

 public:
  explicit ScopedPixelLocalStorageInterrupt(WebGLRenderingContextBase* context)
      : context_(context),
        needs_interrupt_(context_->has_activated_pixel_local_storage_) {
    if (needs_interrupt_) {
      context_->ContextGL()->FramebufferPixelLocalStorageInterruptANGLE();
    }
  }

  ~ScopedPixelLocalStorageInterrupt() {
    // The context should never activate PLS during an interrupt.
    DCHECK_EQ(context_->has_activated_pixel_local_storage_, needs_interrupt_);
    if (needs_interrupt_) {
      context_->ContextGL()->FramebufferPixelLocalStorageRestoreANGLE();
    }
  }

 private:
  WebGLRenderingContextBase* context_;
  bool needs_interrupt_;
};

class ScopedTexture2DRestorer {
  STACK_ALLOCATED();

 public:
  explicit ScopedTexture2DRestorer(WebGLRenderingContextBase* context)
      : context_(context) {}

  ~ScopedTexture2DRestorer() { context_->RestoreCurrentTexture2D(); }

 private:
  WebGLRenderingContextBase* context_;
};

class ScopedFramebufferRestorer {
  STACK_ALLOCATED();

 public:
  explicit ScopedFramebufferRestorer(WebGLRenderingContextBase* context)
      : context_(context) {}

  ~ScopedFramebufferRestorer() { context_->RestoreCurrentFramebuffer(); }

 private:
  WebGLRenderingContextBase* context_;
};

class ScopedUnpackParametersResetRestore {
  STACK_ALLOCATED();

 public:
  explicit ScopedUnpackParametersResetRestore(
      WebGLRenderingContextBase* context,
      bool enabled = true)
      : context_(context), enabled_(enabled) {
    if (enabled)
      context_->ResetUnpackParameters();
  }

  ~ScopedUnpackParametersResetRestore() {
    if (enabled_)
      context_->RestoreUnpackParameters();
  }

 private:
  WebGLRenderingContextBase* context_;
  bool enabled_;
};

class ScopedDisableRasterizerDiscard {
  STACK_ALLOCATED();

 public:
  explicit ScopedDisableRasterizerDiscard(WebGLRenderingContextBase* context,
                                          bool was_enabled)
      : context_(context), was_enabled_(was_enabled) {
    if (was_enabled_) {
      context_->disable(GL_RASTERIZER_DISCARD);
    }
  }

  ~ScopedDisableRasterizerDiscard() {
    if (was_enabled_) {
      context_->enable(GL_RASTERIZER_DISCARD);
    }
  }

 private:
  WebGLRenderingContextBase* context_;
  bool was_enabled_;
};

static void FormatWebGLStatusString(const StringView& gl_info,
                                    const StringView& info_string,
                                    StringBuilder& builder) {
  if (info_string.empty())
    return;
  builder.Append(", ");
  builder.Append(gl_info);
  builder.Append(" = ");
  builder.Append(info_string);
}

static String ExtractWebGLContextCreationError(
    const Platform::GraphicsInfo& info) {
  StringBuilder builder;
  builder.Append("Could not create a WebGL context");
  FormatWebGLStatusString(
      "VENDOR",
      info.vendor_id ? String::Format("0x%04x", info.vendor_id) : "0xffff",
      builder);
  FormatWebGLStatusString(
      "DEVICE",
      info.device_id ? String::Format("0x%04x", info.device_id) : "0xffff",
      builder);
  FormatWebGLStatusString("GL_VENDOR", info.vendor_info, builder);
  FormatWebGLStatusString("GL_RENDERER", info.renderer_info, builder);
  FormatWebGLStatusString("GL_VERSION", info.driver_version, builder);
  FormatWebGLStatusString("Sandboxed", info.sandboxed ? "yes" : "no", builder);
  FormatWebGLStatusString("Optimus", info.optimus ? "yes" : "no", builder);
  FormatWebGLStatusString("AMD switchable", info.amd_switchable ? "yes" : "no",
                          builder);
  FormatWebGLStatusString(
      "Reset notification strategy",
      String::Format("0x%04x", info.reset_notification_strategy).Utf8().c_str(),
      builder);
  FormatWebGLStatusString("ErrorMessage", info.error_message.Utf8().c_str(),
                          builder);
  builder.Append('.');
  return builder.ToString();
}

std::unique_ptr<WebGraphicsContext3DProvider>
WebGLRenderingContextBase::CreateContextProviderInternal(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attributes,
    Platform::ContextType context_type,
    Platform::GraphicsInfo* graphics_info) {
  DCHECK(host);
  ExecutionContext* execution_context = host->GetTopExecutionContext();
  DCHECK(execution_context);

  Platform::ContextAttributes context_attributes =
      ToPlatformContextAttributes(attributes, context_type);

  // To run our tests with Chrome rendering on the low power GPU and WebGL on
  // the high performance GPU, we need to force the power preference attribute.
  if (base::FeatureList::IsEnabled(
          blink::features::kForceHighPerformanceGPUForWebGL)) {
    context_attributes.prefer_low_power_gpu = false;
  }

  const auto& url = execution_context->Url();
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider =
      CreateOffscreenGraphicsContext3DProvider(context_attributes,
                                               graphics_info, url);
  if (context_provider && !context_provider->BindToCurrentSequence()) {
    context_provider = nullptr;
    graphics_info->error_message = String("BindToCurrentSequence failed: " +
                                          String(graphics_info->error_message));
  }
  if (!context_provider || g_should_fail_context_creation_for_testing) {
    g_should_fail_context_creation_for_testing = false;
    host->HostDispatchEvent(WebGLContextEvent::Create(
        event_type_names::kWebglcontextcreationerror,
        ExtractWebGLContextCreationError(*graphics_info)));
    return nullptr;
  }
  gpu::gles2::GLES2Interface* gl = context_provider->ContextGL();
  if (!String(gl->GetString(GL_EXTENSIONS))
           .Contains("GL_OES_packed_depth_stencil")) {
    host->HostDispatchEvent(WebGLContextEvent::Create(
        event_type_names::kWebglcontextcreationerror,
        "OES_packed_depth_stencil support is required."));
    return nullptr;
  }
  return context_provider;
}

std::unique_ptr<WebGraphicsContext3DProvider>
WebGLRenderingContextBase::CreateWebGraphicsContext3DProvider(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attributes,
    Platform::ContextType context_type,
    Platform::GraphicsInfo* graphics_info) {
  if ((context_type == Platform::kWebGL1ContextType &&
       !host->IsWebGL1Enabled()) ||
      (context_type == Platform::kWebGL2ContextType &&
       !host->IsWebGL2Enabled())) {
    host->HostDispatchEvent(WebGLContextEvent::Create(
        event_type_names::kWebglcontextcreationerror,
        "disabled by enterprise policy or commandline switch"));
    return nullptr;
  }

  // We create a context *before* checking whether WebGL is blocked. This is
  // because new context creation is effectively synchronized against the
  // browser having a working GPU process connection, and that is in turn
  // synchronized against any updates to the browser's set of blocked domains.
  // See https://crbug.com/1215907#c10 for more details.
  auto provider = CreateContextProviderInternal(host, attributes, context_type,
                                                graphics_info);

  // The host might block creation of a new WebGL context despite the
  // page settings; in particular, if WebGL contexts were lost one or
  // more times via the GL_ARB_robustness extension.
  if (!host->IsWebGLBlocked())
    return provider;

  host->SetContextCreationWasBlocked();
  host->HostDispatchEvent(WebGLContextEvent::Create(
      event_type_names::kWebglcontextcreationerror,
      "Web page caused context loss and was blocked"));
  return nullptr;
}

void WebGLRenderingContextBase::ForceNextWebGLContextCreationToFail() {
  g_should_fail_context_creation_for_testing = true;
}

ImageBitmap* WebGLRenderingContextBase::TransferToImageBitmapBase(
    ScriptState* script_state) {
  WebFeature feature = WebFeature::kOffscreenCanvasTransferToImageBitmapWebGL;
  UseCounter::Count(ExecutionContext::From(script_state), feature);
  if (!GetDrawingBuffer()) {
    // Context is lost.
    return nullptr;
  }

  return MakeGarbageCollected<ImageBitmap>(
      GetDrawingBuffer()->TransferToStaticBitmapImage());
}

void WebGLRenderingContextBase::drawingBufferStorage(GLenum sizedformat,
                                                     GLsizei width,
                                                     GLsizei height) {
  if (!GetDrawingBuffer())
    return;

  const char* function_name = "drawingBufferStorage";
  const CanvasContextCreationAttributesCore& attrs = CreationAttributes();

  // Ensure that the width and height are valid.
  if (width <= 0) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "width < 0");
    return;
  }
  if (height <= 0) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "height < 0");
    return;
  }
  if (width > max_renderbuffer_size_) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      "width > MAX_RENDERBUFFER_SIZE");
    return;
  }
  if (height > max_renderbuffer_size_) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      "height > MAX_RENDERBUFFER_SIZE");
    return;
  }
  if (!attrs.alpha) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "alpha is required for drawingBufferStorage");
    return;
  }

  // Ensure that the format is supported, and set the corresponding alpha
  // type.
  SkAlphaType alpha_type =
      attrs.premultiplied_alpha ? kPremul_SkAlphaType : kUnpremul_SkAlphaType;
  switch (sizedformat) {
    case GL_RGBA8:
      break;
    case GL_SRGB8_ALPHA8:
      if (!IsWebGL2() && !ExtensionEnabled(kEXTsRGBName)) {
        SynthesizeGLError(GL_INVALID_ENUM, function_name,
                          "EXT_sRGB not enabled");
        return;
      }
      break;
    case GL_RGBA16F:
      if (base::FeatureList::IsEnabled(
              blink::features::kCorrectFloatExtensionTestForWebGL)) {
        // Correct float extension testing for WebGL1/2.
        // See: https://github.com/KhronosGroup/WebGL/pull/3222
        if (IsWebGL2()) {
          if (!ExtensionEnabled(kEXTColorBufferFloatName) &&
              !ExtensionEnabled(kEXTColorBufferHalfFloatName)) {
            SynthesizeGLError(GL_INVALID_ENUM, function_name,
                              "EXT_color_buffer_float/"
                              "EXT_color_buffer_half_float not enabled");
            return;
          }
        } else {
          if (!ExtensionEnabled(kEXTColorBufferHalfFloatName)) {
            SynthesizeGLError(GL_INVALID_ENUM, function_name,
                              "EXT_color_buffer_half_float not enabled");
            return;
          }
        }
      } else {
        // This is the original incorrect extension testing. Remove this code
        // once this correction safely launches.
        if (IsWebGL2()) {
          if (!ExtensionEnabled(kEXTColorBufferFloatName) &&
              !ExtensionEnabled(kEXTColorBufferHalfFloatName)) {
            SynthesizeGLError(GL_INVALID_ENUM, function_name,
                              "EXT_color_buffer_float/"
                              "EXT_color_buffer_half_float not enabled");
            return;
          } else {
            if (!ExtensionEnabled(kEXTColorBufferHalfFloatName)) {
              SynthesizeGLError(GL_INVALID_ENUM, function_name,
                                "EXT_color_buffer_half_float not enabled");
              return;
            }
          }
        }
      }
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid sizedformat");
      return;
  }

  GetDrawingBuffer()->ResizeWithFormat(sizedformat, alpha_type,
                                       gfx::Size(width, height));
}

void WebGLRenderingContextBase::commit() {
  if (!GetDrawingBuffer() || (Host() && Host()->IsOffscreenCanvas()))
    return;

  int width = GetDrawingBuffer()->Size().width();
  int height = GetDrawingBuffer()->Size().height();

  if (PaintRenderingResultsToCanvas(kBackBuffer)) {
    if (Host()->GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)) {
      Host()->Commit(
          Host()->ResourceProvider()->ProduceCanvasResource(FlushReason::kNone),
          SkIRect::MakeWH(width, height));
    }
  }
  MarkLayerComposited();
}

scoped_refptr<StaticBitmapImage> WebGLRenderingContextBase::GetImage(
    FlushReason reason) {
  if (!GetDrawingBuffer())
    return nullptr;

  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(this);
  ScopedFramebufferRestorer fbo_restorer(this);
  // In rare situations on macOS the drawing buffer can be destroyed
  // during the resolve process, specifically during automatic
  // graphics switching. Guard against this.
  if (!GetDrawingBuffer()->ResolveAndBindForReadAndDraw())
    return nullptr;

  // Use the drawing buffer size here instead of the canvas size to ensure that
  // sizing is consistent. The forced downsizing logic in Reshape() can lead to
  // the drawing buffer being smaller than the canvas size.
  // See https://crbug.com/845742.
  gfx::Size size = GetDrawingBuffer()->Size();
  // We are grabbing a snapshot that is generally not for compositing, so use a
  // custom resource provider. This avoids consuming compositing-specific
  // resources (e.g. GpuMemoryBuffer). We tag the SharedImage with display usage
  // since there are uncommon paths which may use this snapshot for compositing.
  const auto image_info =
      SkImageInfo::Make(SkISize::Make(size.width(), size.height()),
                        CanvasRenderingContextSkColorInfo());
  constexpr auto kShouldInitialize =
      CanvasResourceProvider::ShouldInitialize::kNo;
  std::unique_ptr<CanvasResourceProvider> resource_provider =
      CanvasResourceProvider::CreateSharedImageProvider(
          image_info, GetDrawingBuffer()->FilterQuality(), kShouldInitialize,
          SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
          gpu::SHARED_IMAGE_USAGE_DISPLAY_READ);
  if (!resource_provider || !resource_provider->IsValid()) {
    resource_provider = CanvasResourceProvider::CreateBitmapProvider(
        image_info, GetDrawingBuffer()->FilterQuality(),
        CanvasResourceProvider::ShouldInitialize::kNo);
  }

  if (!resource_provider || !resource_provider->IsValid())
    return nullptr;

  if (!CopyRenderingResultsFromDrawingBuffer(resource_provider.get(),
                                             kBackBuffer)) {
    // CopyRenderingResultsFromDrawingBuffer handles both the
    // hardware-accelerated and software cases, so there is no
    // possible additional fallback for failures seen at this point.
    return nullptr;
  }
  return resource_provider->Snapshot(reason);
}

ScriptPromise<IDLUndefined> WebGLRenderingContextBase::makeXRCompatible(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (isContextLost()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Context lost.");
    return EmptyPromise();
  }

  // Return a resolved promise if we're already xr compatible. Once we're
  // compatible, we should always be compatible unless a context lost occurs.
  // DispatchContextLostEvent() resets this flag to false.
  if (xr_compatible_)
    return ToResolvedUndefinedPromise(script_state);

  // If there's a request currently in progress, return the same promise.
  if (make_xr_compatible_resolver_)
    return make_xr_compatible_resolver_->Promise();

  make_xr_compatible_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
          script_state, exception_state.GetContext());
  auto promise = make_xr_compatible_resolver_->Promise();

  MakeXrCompatibleAsync();

  return promise;
}

bool WebGLRenderingContextBase::IsXRCompatible() const {
  return xr_compatible_;
}

bool WebGLRenderingContextBase::IsXrCompatibleFromResult(
    device::mojom::blink::XrCompatibleResult result) {
  return result ==
             device::mojom::blink::XrCompatibleResult::kAlreadyCompatible ||
         result ==
             device::mojom::blink::XrCompatibleResult::kCompatibleAfterRestart;
}

bool WebGLRenderingContextBase::DidGpuRestart(
    device::mojom::blink::XrCompatibleResult result) {
  return result == device::mojom::blink::XrCompatibleResult::
                       kCompatibleAfterRestart ||
         result == device::mojom::blink::XrCompatibleResult::
                       kNotCompatibleAfterRestart;
}

XRSystem* WebGLRenderingContextBase::GetXrSystemFromHost(
    CanvasRenderingContextHost* host) {
  XRSystem* xr = nullptr;

  if (host->IsOffscreenCanvas()) {
    OffscreenCanvas* offscreen_canvas = static_cast<OffscreenCanvas*>(host);
    if (auto* window = DynamicTo<LocalDOMWindow>(
            offscreen_canvas->GetExecutionContext())) {
      if (Document* document = window->document()) {
        xr = XRSystem::From(*document);
      }
    }
  } else {
    HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(host);
    xr = XRSystem::From(canvas->GetDocument());
  }

  return xr;
}

bool WebGLRenderingContextBase::MakeXrCompatibleSync(
    CanvasRenderingContextHost* host) {
  device::mojom::blink::XrCompatibleResult xr_compatible_result =
      device::mojom::blink::XrCompatibleResult::kNoDeviceAvailable;

  if constexpr (BUILDFLAG(ENABLE_VR)) {
    if (XRSystem* xr = GetXrSystemFromHost(host)) {
      xr->MakeXrCompatibleSync(&xr_compatible_result);
    }
  }

  return IsXrCompatibleFromResult(xr_compatible_result);
}

void WebGLRenderingContextBase::MakeXrCompatibleAsync() {
  if (XRSystem* xr = GetXrSystemFromHost(Host())) {
    // The promise will be completed on the callback.
    xr->MakeXrCompatibleAsync(
        WTF::BindOnce(&WebGLRenderingContextBase::OnMakeXrCompatibleFinished,
                      WrapWeakPersistent(this)));
  } else {
    xr_compatible_ = false;
    CompleteXrCompatiblePromiseIfPending(DOMExceptionCode::kAbortError);
  }
}

void WebGLRenderingContextBase::OnMakeXrCompatibleFinished(
    device::mojom::blink::XrCompatibleResult xr_compatible_result) {
  xr_compatible_ = IsXrCompatibleFromResult(xr_compatible_result);

  // If the gpu process is restarted, MaybeRestoreContext will resolve the
  // promise on the subsequent restore.
  if (!DidGpuRestart(xr_compatible_result)) {
    DOMExceptionCode exception_code = DOMExceptionCode::kUnknownError;
    switch (xr_compatible_result) {
      case device::mojom::blink::XrCompatibleResult::kAlreadyCompatible:
        exception_code = DOMExceptionCode::kNoError;
        break;
      case device::mojom::blink::XrCompatibleResult::kNoDeviceAvailable:
        // Per WebXR spec, reject with an InvalidStateError if device is null.
        exception_code = DOMExceptionCode::kInvalidStateError;
        break;
      case device::mojom::blink::XrCompatibleResult::kWebXrFeaturePolicyBlocked:
        exception_code = DOMExceptionCode::kSecurityError;
        break;
      case device::mojom::blink::XrCompatibleResult::kCompatibleAfterRestart:
      case device::mojom::blink::XrCompatibleResult::kNotCompatibleAfterRestart:
        NOTREACHED_IN_MIGRATION();
    }
    CompleteXrCompatiblePromiseIfPending(exception_code);
  }
}

void WebGLRenderingContextBase::CompleteXrCompatiblePromiseIfPending(
    DOMExceptionCode exception_code) {
  if (make_xr_compatible_resolver_) {
    if (xr_compatible_) {
      DCHECK(exception_code == DOMExceptionCode::kNoError);
      make_xr_compatible_resolver_->Resolve();
    } else {
      DCHECK(exception_code != DOMExceptionCode::kNoError);
      make_xr_compatible_resolver_->Reject(
          MakeGarbageCollected<DOMException>(exception_code));
    }

    make_xr_compatible_resolver_ = nullptr;

    if (IdentifiabilityStudySettings::Get()->ShouldSampleSurface(
            IdentifiableSurface::FromTypeAndToken(
                IdentifiableSurface::Type::kWebFeature,
                WebFeature::kWebGLRenderingContextMakeXRCompatible))) {
      const auto& ukm_params = GetUkmParameters();
      IdentifiabilityMetricBuilder(ukm_params.source_id)
          .AddWebFeature(WebFeature::kWebGLRenderingContextMakeXRCompatible,
                         exception_code == DOMExceptionCode::kNoError)
          .Record(ukm_params.ukm_recorder);
    }
  }
}

void WebGLRenderingContextBase::
    UpdateNumberOfUserAllocatedMultisampledRenderbuffers(int delta) {
  DCHECK(delta >= -1 && delta <= 1);
  number_of_user_allocated_multisampled_renderbuffers_ += delta;
  DCHECK_GE(number_of_user_allocated_multisampled_renderbuffers_, 0);
}

namespace {

// Exposed by GL_ANGLE_depth_texture
static constexpr std::array<GLenum, 2> kSupportedInternalFormatsOESDepthTex = {
    GL_DEPTH_COMPONENT,
    GL_DEPTH_STENCIL,
};

// Exposed by GL_EXT_sRGB
static constexpr std::array<GLenum, 2> kSupportedInternalFormatsEXTsRGB = {
    GL_SRGB,
    GL_SRGB_ALPHA_EXT,
};

// ES3 enums supported by both CopyTexImage and TexImage.
static constexpr auto kSupportedInternalFormatsES3 = std::to_array<GLenum>({
    GL_R8,           GL_RG8,      GL_RGB565,   GL_RGB8,       GL_RGBA4,
    GL_RGB5_A1,      GL_RGBA8,    GL_RGB10_A2, GL_RGB10_A2UI, GL_SRGB8,
    GL_SRGB8_ALPHA8, GL_R8I,      GL_R8UI,     GL_R16I,       GL_R16UI,
    GL_R32I,         GL_R32UI,    GL_RG8I,     GL_RG8UI,      GL_RG16I,
    GL_RG16UI,       GL_RG32I,    GL_RG32UI,   GL_RGBA8I,     GL_RGBA8UI,
    GL_RGBA16I,      GL_RGBA16UI, GL_RGBA32I,  GL_RGBA32UI,   GL_RGB32I,
    GL_RGB32UI,      GL_RGB8I,    GL_RGB8UI,   GL_RGB16I,     GL_RGB16UI,
});

// ES3 enums only supported by TexImage
static constexpr auto kSupportedInternalFormatsTexImageES3 =
    std::to_array<GLenum>({
        GL_R8_SNORM,
        GL_R16F,
        GL_R32F,
        GL_RG8_SNORM,
        GL_RG16F,
        GL_RG32F,
        GL_RGB8_SNORM,
        GL_R11F_G11F_B10F,
        GL_RGB9_E5,
        GL_RGB16F,
        GL_RGB32F,
        GL_RGBA8_SNORM,
        GL_RGBA16F,
        GL_RGBA32F,
        GL_DEPTH_COMPONENT16,
        GL_DEPTH_COMPONENT24,
        GL_DEPTH_COMPONENT32F,
        GL_DEPTH24_STENCIL8,
        GL_DEPTH32F_STENCIL8,
    });

// Exposed by EXT_texture_norm16
static constexpr auto kSupportedInternalFormatsEXTTextureNorm16ES3 =
    std::to_array<GLenum>({GL_R16_EXT, GL_RG16_EXT, GL_RGB16_EXT, GL_RGBA16_EXT,
                           GL_R16_SNORM_EXT, GL_RG16_SNORM_EXT,
                           GL_RGB16_SNORM_EXT, GL_RGBA16_SNORM_EXT});

static constexpr std::array<GLenum, 2> kSupportedFormatsEXTTextureNorm16ES3 = {
    GL_RED, GL_RG};

static constexpr std::array<GLenum, 2> kSupportedTypesEXTTextureNorm16ES3 = {
    GL_SHORT, GL_UNSIGNED_SHORT};

// Exposed by EXT_color_buffer_float
static constexpr auto kSupportedInternalFormatsCopyTexImageFloatES3 =
    std::to_array<GLenum>({GL_R16F, GL_R32F, GL_RG16F, GL_RG32F, GL_RGB16F,
                           GL_RGB32F, GL_RGBA16F, GL_RGBA32F,
                           GL_R11F_G11F_B10F});

// Exposed by EXT_color_buffer_half_float
static constexpr std::array<GLenum, 4>
    kSupportedInternalFormatsCopyTexImageHalfFloatES3 = {
        GL_R16F,
        GL_RG16F,
        GL_RGB16F,
        GL_RGBA16F,
};

// ES3 enums supported by TexImageSource
static constexpr auto kSupportedInternalFormatsTexImageSourceES3 =
    std::to_array<GLenum>({
        GL_R8,      GL_R16F,           GL_R32F,         GL_R8UI,     GL_RG8,
        GL_RG16F,   GL_RG32F,          GL_RG8UI,        GL_RGB8,     GL_SRGB8,
        GL_RGB565,  GL_R11F_G11F_B10F, GL_RGB9_E5,      GL_RGB16F,   GL_RGB32F,
        GL_RGB8UI,  GL_RGBA8,          GL_SRGB8_ALPHA8, GL_RGB5_A1,  GL_RGBA4,
        GL_RGBA16F, GL_RGBA32F,        GL_RGBA8UI,      GL_RGB10_A2,
    });

// ES2 enums
// Internalformat must equal format in ES2.
static constexpr auto kSupportedFormatsES2 = std::to_array<GLenum>({
    GL_RGB,
    GL_RGBA,
    GL_LUMINANCE_ALPHA,
    GL_LUMINANCE,
    GL_ALPHA,
});

// Exposed by GL_ANGLE_depth_texture
static constexpr std::array<GLenum, 2> kSupportedFormatsOESDepthTex = {
    GL_DEPTH_COMPONENT,
    GL_DEPTH_STENCIL,
};

// Exposed by GL_EXT_sRGB
static constexpr std::array<GLenum, 2> kSupportedFormatsEXTsRGB = {
    GL_SRGB,
    GL_SRGB_ALPHA_EXT,
};

// ES3 enums
static constexpr auto kSupportedFormatsES3 = std::to_array<GLenum>({
    GL_RED,
    GL_RED_INTEGER,
    GL_RG,
    GL_RG_INTEGER,
    GL_RGB,
    GL_RGB_INTEGER,
    GL_RGBA,
    GL_RGBA_INTEGER,
    GL_DEPTH_COMPONENT,
    GL_DEPTH_STENCIL,
});

// ES3 enums supported by TexImageSource
static constexpr auto kSupportedFormatsTexImageSourceES3 =
    std::to_array<GLenum>({
        GL_RED,
        GL_RED_INTEGER,
        GL_RG,
        GL_RG_INTEGER,
        GL_RGB,
        GL_RGB_INTEGER,
        GL_RGBA,
        GL_RGBA_INTEGER,
    });

// ES2 enums
static constexpr std::array<GLenum, 4> kSupportedTypesES2 = {
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_SHORT_5_6_5,
    GL_UNSIGNED_SHORT_4_4_4_4,
    GL_UNSIGNED_SHORT_5_5_5_1,
};

// Exposed by GL_OES_texture_float
static constexpr std::array<GLenum, 1> kSupportedTypesOESTexFloat = {
    GL_FLOAT,
};

// Exposed by GL_OES_texture_half_float
static constexpr std::array<GLenum, 1> kSupportedTypesOESTexHalfFloat = {
    GL_HALF_FLOAT_OES,
};

// Exposed by GL_ANGLE_depth_texture
static constexpr std::array<GLenum, 3> kSupportedTypesOESDepthTex = {
    GL_UNSIGNED_SHORT,
    GL_UNSIGNED_INT,
    GL_UNSIGNED_INT_24_8,
};

// ES3 enums
static constexpr auto kSupportedTypesES3 = std::to_array<GLenum>({
    GL_BYTE,
    GL_UNSIGNED_SHORT,
    GL_SHORT,
    GL_UNSIGNED_INT,
    GL_INT,
    GL_HALF_FLOAT,
    GL_FLOAT,
    GL_UNSIGNED_INT_2_10_10_10_REV,
    GL_UNSIGNED_INT_10F_11F_11F_REV,
    GL_UNSIGNED_INT_5_9_9_9_REV,
    GL_UNSIGNED_INT_24_8,
    GL_FLOAT_32_UNSIGNED_INT_24_8_REV,
});

// ES3 enums supported by TexImageSource
static constexpr std::array<GLenum, 4> kSupportedTypesTexImageSourceES3 = {
    GL_HALF_FLOAT,
    GL_FLOAT,
    GL_UNSIGNED_INT_10F_11F_11F_REV,
    GL_UNSIGNED_INT_2_10_10_10_REV,
};

}  // namespace

WebGLRenderingContextBase::WebGLRenderingContextBase(
    CanvasRenderingContextHost* host,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    const Platform::GraphicsInfo& graphics_info,
    const CanvasContextCreationAttributesCore& requested_attributes,
    Platform::ContextType version)
    : WebGLRenderingContextBase(
          host,
          host->GetTopExecutionContext()->GetTaskRunner(TaskType::kWebGL),
          std::move(context_provider),
          graphics_info,
          requested_attributes,
          version) {}

WebGLRenderingContextBase::WebGLRenderingContextBase(
    CanvasRenderingContextHost* host,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    const Platform::GraphicsInfo& graphics_info,
    const CanvasContextCreationAttributesCore& requested_attributes,
    Platform::ContextType context_type)
    : CanvasRenderingContext(host,
                             requested_attributes,
                             context_type == Platform::kWebGL2ContextType
                                 ? CanvasRenderingAPI::kWebgl2
                                 : CanvasRenderingAPI::kWebgl),
      context_group_(MakeGarbageCollected<WebGLContextGroup>()),
      dispatch_context_lost_event_timer_(
          task_runner,
          this,
          &WebGLRenderingContextBase::DispatchContextLostEvent),
      restore_timer_(task_runner,
                     this,
                     &WebGLRenderingContextBase::MaybeRestoreContext),
      task_runner_(task_runner),
      num_gl_errors_to_console_allowed_(kMaxGLErrorsAllowedToConsole),
      context_type_(context_type),
      number_of_user_allocated_multisampled_renderbuffers_(0) {
  DCHECK(context_provider);

  xr_compatible_ = requested_attributes.xr_compatible;

  context_group_->AddContext(this);

  max_viewport_dims_ = {};
  context_provider->ContextGL()->GetIntegerv(GL_MAX_VIEWPORT_DIMS,
                                             max_viewport_dims_.data());
  InitializeWebGLContextLimits(context_provider.get());

  scoped_refptr<DrawingBuffer> buffer =
      CreateDrawingBuffer(std::move(context_provider), graphics_info);
  if (!buffer) {
    context_lost_mode_ = kSyntheticLostContext;
    return;
  }

  drawing_buffer_ = std::move(buffer);
  GetDrawingBuffer()->Bind(GL_FRAMEBUFFER);
  SetupFlags();

  String disabled_webgl_extensions(GetDrawingBuffer()
                                       ->ContextProvider()
                                       ->GetGpuFeatureInfo()
                                       .disabled_webgl_extensions.c_str());
  Vector<String> disabled_extension_list;
  disabled_webgl_extensions.Split(' ', disabled_extension_list);
  for (const auto& entry : disabled_extension_list) {
    disabled_extensions_.insert(entry);
  }

#define ADD_VALUES_TO_SET(set, values)             \
  for (size_t i = 0; i < std::size(values); ++i) { \
    set.insert(values[i]);                         \
  }

  ADD_VALUES_TO_SET(supported_internal_formats_, kSupportedFormatsES2);
  ADD_VALUES_TO_SET(supported_tex_image_source_internal_formats_,
                    kSupportedFormatsES2);
  ADD_VALUES_TO_SET(supported_internal_formats_copy_tex_image_,
                    kSupportedFormatsES2);
  ADD_VALUES_TO_SET(supported_formats_, kSupportedFormatsES2);
  ADD_VALUES_TO_SET(supported_tex_image_source_formats_, kSupportedFormatsES2);
  ADD_VALUES_TO_SET(supported_types_, kSupportedTypesES2);
  ADD_VALUES_TO_SET(supported_tex_image_source_types_, kSupportedTypesES2);
}

scoped_refptr<DrawingBuffer> WebGLRenderingContextBase::CreateDrawingBuffer(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    const Platform::GraphicsInfo& graphics_info) {
  const CanvasContextCreationAttributesCore& attrs = CreationAttributes();
  bool premultiplied_alpha = attrs.premultiplied_alpha;
  bool want_alpha_channel = attrs.alpha;
  bool want_depth_buffer = attrs.depth;
  bool want_stencil_buffer = attrs.stencil;
  bool want_antialiasing = attrs.antialias;
  bool desynchronized = attrs.desynchronized;
  DrawingBuffer::PreserveDrawingBuffer preserve = attrs.preserve_drawing_buffer
                                                      ? DrawingBuffer::kPreserve
                                                      : DrawingBuffer::kDiscard;
  DrawingBuffer::WebGLVersion web_gl_version = DrawingBuffer::kWebGL1;
  if (context_type_ == Platform::kWebGL1ContextType) {
    web_gl_version = DrawingBuffer::kWebGL1;
  } else if (context_type_ == Platform::kWebGL2ContextType) {
    web_gl_version = DrawingBuffer::kWebGL2;
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  // On Mac OS, DrawingBuffer is using an IOSurface as its backing storage, this
  // allows WebGL-rendered canvases to be composited by the OS rather than
  // Chrome.
  // IOSurfaces are only compatible with the GL_TEXTURE_RECTANGLE_ARB binding
  // target. So to avoid the knowledge of GL_TEXTURE_RECTANGLE_ARB type textures
  // being introduced into more areas of the code, we use the code path of
  // non-WebGLImageChromium for OffscreenCanvas.
  // See detailed discussion in crbug.com/649668.
  DrawingBuffer::ChromiumImageUsage chromium_image_usage =
      Host()->IsOffscreenCanvas() ? DrawingBuffer::kDisallowChromiumImage
                                  : DrawingBuffer::kAllowChromiumImage;

  bool using_swap_chain = context_provider->SharedImageInterface()
                              ->GetCapabilities()
                              .shared_image_swap_chain &&
                          desynchronized;

  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(this);
  return DrawingBuffer::Create(
      std::move(context_provider), graphics_info, using_swap_chain, this,
      ClampedCanvasSize(), premultiplied_alpha, want_alpha_channel,
      want_depth_buffer, want_stencil_buffer, want_antialiasing, desynchronized,
      preserve, web_gl_version, chromium_image_usage, Host()->FilterQuality(),
      drawing_buffer_color_space_,
      PowerPreferenceToGpuPreference(attrs.power_preference));
}

void WebGLRenderingContextBase::InitializeNewContext() {
  DCHECK(!isContextLost());
  DCHECK(GetDrawingBuffer());

  marked_canvas_dirty_ = false;
  must_paint_to_canvas_ = false;
  active_texture_unit_ = 0;
  pack_alignment_ = 4;
  unpack_alignment_ = 4;
  unpack_flip_y_ = false;
  unpack_premultiply_alpha_ = false;
  unpack_colorspace_conversion_ = GC3D_BROWSER_DEFAULT_WEBGL;
  bound_array_buffer_ = nullptr;
  current_program_ = nullptr;
  framebuffer_binding_ = nullptr;
  renderbuffer_binding_ = nullptr;
  depth_mask_ = true;
  depth_enabled_ = false;
  stencil_enabled_ = false;
  stencil_mask_ = 0xFFFFFFFF;
  stencil_mask_back_ = 0xFFFFFFFF;
  stencil_func_ref_ = 0;
  stencil_func_ref_back_ = 0;
  stencil_func_mask_ = 0xFFFFFFFF;
  stencil_func_mask_back_ = 0xFFFFFFFF;
  num_gl_errors_to_console_allowed_ = kMaxGLErrorsAllowedToConsole;

  rasterizer_discard_enabled_ = false;

  clear_color_[0] = clear_color_[1] = clear_color_[2] = clear_color_[3] = 0;
  scissor_enabled_ = false;
  clear_depth_ = 1;
  clear_stencil_ = 0;
  color_mask_[0] = color_mask_[1] = color_mask_[2] = color_mask_[3] = true;

  GLint num_combined_texture_image_units = 0;
  ContextGL()->GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                           &num_combined_texture_image_units);
  texture_units_.clear();
  texture_units_.resize(num_combined_texture_image_units);

  GLint num_vertex_attribs = 0;
  ContextGL()->GetIntegerv(GL_MAX_VERTEX_ATTRIBS, &num_vertex_attribs);
  max_vertex_attribs_ = num_vertex_attribs;

  max_texture_size_ = 0;
  ContextGL()->GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size_);
  max_texture_level_ =
      WebGLTexture::ComputeLevelCount(max_texture_size_, max_texture_size_, 1);
  max_cube_map_texture_size_ = 0;
  ContextGL()->GetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE,
                           &max_cube_map_texture_size_);
  max3d_texture_size_ = 0;
  max3d_texture_level_ = 0;
  max_array_texture_layers_ = 0;
  if (IsWebGL2()) {
    ContextGL()->GetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &max3d_texture_size_);
    max3d_texture_level_ = WebGLTexture::ComputeLevelCount(
        max3d_texture_size_, max3d_texture_size_, max3d_texture_size_);
    ContextGL()->GetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS,
                             &max_array_texture_layers_);
  }
  max_cube_map_texture_level_ = WebGLTexture::ComputeLevelCount(
      max_cube_map_texture_size_, max_cube_map_texture_size_, 1);
  max_renderbuffer_size_ = 0;
  ContextGL()->GetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer_size_);

  // These two values from EXT_draw_buffers are lazily queried.
  max_draw_buffers_ = 0;
  max_color_attachments_ = 0;

  back_draw_buffer_ = GL_BACK;

  read_buffer_of_default_framebuffer_ = GL_BACK;

  default_vertex_array_object_ = MakeGarbageCollected<WebGLVertexArrayObject>(
      this, WebGLVertexArrayObjectBase::kVaoTypeDefault);

  bound_vertex_array_object_ = default_vertex_array_object_;

  vertex_attrib_type_.resize(max_vertex_attribs_);

  ContextGL()->Viewport(0, 0, drawingBufferWidth(), drawingBufferHeight());
  scissor_box_[0] = scissor_box_[1] = 0;
  scissor_box_[2] = drawingBufferWidth();
  scissor_box_[3] = drawingBufferHeight();
  ContextGL()->Scissor(scissor_box_[0], scissor_box_[1], scissor_box_[2],
                       scissor_box_[3]);

  GetDrawingBuffer()->ContextProvider()->SetLostContextCallback(
      WTF::BindRepeating(&WebGLRenderingContextBase::ForceLostContext,
                         WrapWeakPersistent(this),
                         WebGLRenderingContextBase::kRealLostContext,
                         WebGLRenderingContextBase::kAuto));
  GetDrawingBuffer()->ContextProvider()->SetErrorMessageCallback(
      WTF::BindRepeating(&WebGLRenderingContextBase::OnErrorMessage,
                         WrapWeakPersistent(this)));

  // If the context has the flip_y extension, it will behave as having the
  // origin of coordinates on the top left.
  is_origin_top_left_ = GetDrawingBuffer()
                            ->ContextProvider()
                            ->GetCapabilities()
                            .mesa_framebuffer_flip_y;

  // If WebGL 2, the PRIMITIVE_RESTART_FIXED_INDEX should be always enabled.
  // See the section <Primitive Restart is Always Enabled> in WebGL 2 spec:
  // https://www.khronos.org/registry/webgl/specs/latest/2.0/#4.1.4
  if (IsWebGL2())
    ContextGL()->Enable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

  // This ensures that the context has a valid "lastFlushID" and won't be
  // mistakenly identified as the "least recently used" context.
  ContextGL()->Flush();

  for (int i = 0; i < kWebGLExtensionNameCount; ++i)
    extension_enabled_[i] = false;

  // This limits the count of threads if the extension is yet to be requested.
  if (String(ContextGL()->GetString(GL_EXTENSIONS))
          .Contains("GL_KHR_parallel_shader_compile")) {
    ContextGL()->MaxShaderCompilerThreadsKHR(2);
  }
  is_web_gl2_formats_types_added_ = false;
  is_web_gl2_tex_image_source_formats_types_added_ = false;
  is_web_gl2_internal_formats_copy_tex_image_added_ = false;
  is_oes_texture_float_formats_types_added_ = false;
  is_oes_texture_half_float_formats_types_added_ = false;
  is_web_gl_depth_texture_formats_types_added_ = false;
  is_ext_srgb_formats_types_added_ = false;
  is_ext_color_buffer_float_formats_added_ = false;
  is_ext_color_buffer_half_float_formats_added_ = false;
  is_ext_texture_norm16_added_ = false;

  supported_internal_formats_.clear();
  ADD_VALUES_TO_SET(supported_internal_formats_, kSupportedFormatsES2);
  supported_tex_image_source_internal_formats_.clear();
  ADD_VALUES_TO_SET(supported_tex_image_source_internal_formats_,
                    kSupportedFormatsES2);
  supported_internal_formats_copy_tex_image_.clear();
  ADD_VALUES_TO_SET(supported_internal_formats_copy_tex_image_,
                    kSupportedFormatsES2);
  supported_formats_.clear();
  ADD_VALUES_TO_SET(supported_formats_, kSupportedFormatsES2);
  supported_tex_image_source_formats_.clear();
  ADD_VALUES_TO_SET(supported_tex_image_source_formats_, kSupportedFormatsES2);
  supported_types_.clear();
  ADD_VALUES_TO_SET(supported_types_, kSupportedTypesES2);
  supported_tex_image_source_types_.clear();
  ADD_VALUES_TO_SET(supported_tex_image_source_types_, kSupportedTypesES2);

  number_of_user_allocated_multisampled_renderbuffers_ = 0;

  // The DrawingBuffer was unable to store the state that dirtied when it was
  // initialized. Restore it now.
  GetDrawingBuffer()->RestoreAllState();
  ActivateContext(this);
}

void WebGLRenderingContextBase::SetupFlags() {
  DCHECK(GetDrawingBuffer());
  if (canvas()) {
    synthesized_errors_to_console_ =
        canvas()->GetSettings()->GetWebGLErrorsToConsoleEnabled();
  }

  is_depth_stencil_supported_ =
      ExtensionsUtil()->IsExtensionEnabled("GL_OES_packed_depth_stencil");
}

void WebGLRenderingContextBase::AddCompressedTextureFormat(GLenum format) {
  if (!compressed_texture_formats_.Contains(format))
    compressed_texture_formats_.push_back(format);
}

void WebGLRenderingContextBase::RemoveAllCompressedTextureFormats() {
  compressed_texture_formats_.clear();
}

// Helper function for V8 bindings to identify what version of WebGL a
// CanvasRenderingContext supports.
unsigned WebGLRenderingContextBase::GetWebGLVersion(
    const CanvasRenderingContext* context) {
  if (!context->IsWebGL())
    return 0;
  return static_cast<const WebGLRenderingContextBase*>(context)->ContextType();
}

WebGLRenderingContextBase::~WebGLRenderingContextBase() {
  // It's forbidden to refer to other GC'd objects in a GC'd object's
  // destructor. It's useful for DrawingBuffer to guarantee that it
  // calls its DrawingBufferClient during its own destruction, but if
  // the WebGL context is also being destroyed, then it's essential
  // that the DrawingBufferClient methods not try to touch other
  // objects like WebGLTextures that were previously hooked into the
  // context state.
  destruction_in_progress_ = true;

  // Now that the context and context group no longer hold on to the
  // objects they create, and now that the objects are eagerly finalized
  // rather than the context, there is very little useful work that this
  // destructor can do, since it's not allowed to touch other on-heap
  // objects. All it can do is destroy its underlying context, which, if
  // there are no other contexts in the same share group, will cause all of
  // the underlying graphics resources to be deleted. (Currently, it's
  // always the case that there are no other contexts in the same share
  // group -- resource sharing between WebGL contexts is not yet
  // implemented, and due to its complex semantics, it's doubtful that it
  // ever will be.)
  DestroyContext();

  // Now that this context is destroyed, see if there's a
  // previously-evicted one that should be restored.
  RestoreEvictedContext(this);
}

void WebGLRenderingContextBase::DestroyContext() {
  if (!GetDrawingBuffer())
    return;

  // Ensure pixel local storage isn't active and blocking calls during our
  // destruction process.
  if (has_activated_pixel_local_storage_) {
    ContextGL()->FramebufferPixelLocalStorageInterruptANGLE();
  }

  clearProgramCompletionQueries();

  extensions_util_.reset();

  base::RepeatingClosure null_closure;
  base::RepeatingCallback<void(const char*, int32_t)> null_function;
  GetDrawingBuffer()->ContextProvider()->SetLostContextCallback(
      std::move(null_closure));
  GetDrawingBuffer()->ContextProvider()->SetErrorMessageCallback(
      std::move(null_function));

  DCHECK(GetDrawingBuffer());
  drawing_buffer_->BeginDestruction();
  drawing_buffer_ = nullptr;
}

void WebGLRenderingContextBase::MarkContextChanged(
    ContentChangeType change_type,
    CanvasPerformanceMonitor::DrawType draw_type) {
  if (isContextLost())
    return;

  if (framebuffer_binding_) {
    framebuffer_binding_->SetContentsChanged(true);
    return;
  }

  // Regardless of whether dirty propagations are optimized away, the back
  // buffer is now out of sync with respect to the canvas's internal backing
  // store -- which is only used for certain purposes, like printing.
  must_paint_to_canvas_ = true;

  if (!GetDrawingBuffer()->MarkContentsChanged() && marked_canvas_dirty_) {
    return;
  }

  if (Host()->IsOffscreenCanvas()) {
    marked_canvas_dirty_ = true;
    DidDraw(draw_type);
    return;
  }

  if (!canvas())
    return;

  if (!marked_canvas_dirty_) {
    marked_canvas_dirty_ = true;
    if (auto* cc_layer = CcLayer())
      cc_layer->SetNeedsDisplay();
    DidDraw(draw_type);
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
WebGLRenderingContextBase::GetContextTaskRunner() {
  return task_runner_;
}

bool WebGLRenderingContextBase::PushFrame() {
  TRACE_EVENT0("blink", "WebGLRenderingContextBase::PushFrame");
  DCHECK(Host());
  DCHECK(Host()->IsOffscreenCanvas());
  if (isContextLost() || !GetDrawingBuffer())
    return false;

  bool must_clear_now = ClearIfComposited(kClearCallerOther) != kSkipped;
  if (!must_paint_to_canvas_ && !must_clear_now)
    return false;

  if (!Host()->LowLatencyEnabled() &&
      GetDrawingBuffer()->IsUsingGpuCompositing()) {
    // If LowLatency is not enabled, and it's using Gpu Compositing, it will try
    // to export the mailbox, synctoken and callback mechanism for the
    // compositor to present the frame in the offscrencanvas.
    if (PushFrameNoCopy())
      return true;
  }

  return PushFrameWithCopy();
}

bool WebGLRenderingContextBase::PushFrameNoCopy() {
  auto canvas_resource = GetDrawingBuffer()->ExportCanvasResource();
  if (!canvas_resource)
    return false;
  const int width = GetDrawingBuffer()->Size().width();
  const int height = GetDrawingBuffer()->Size().height();
  const bool submitted_frame = Host()->PushFrame(
      std::move(canvas_resource), SkIRect::MakeWH(width, height));

  MarkLayerComposited();
  return submitted_frame;
}

bool WebGLRenderingContextBase::PushFrameWithCopy() {
  bool submitted_frame = false;
  if (PaintRenderingResultsToCanvas(kBackBuffer)) {
    if (Host()->GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)) {
      const int width = GetDrawingBuffer()->Size().width();
      const int height = GetDrawingBuffer()->Size().height();
      submitted_frame =
          Host()->PushFrame(Host()->ResourceProvider()->ProduceCanvasResource(
                                FlushReason::kNon2DCanvas),
                            SkIRect::MakeWH(width, height));
    }
  }
  MarkLayerComposited();
  return submitted_frame;
}

void WebGLRenderingContextBase::FinalizeFrame(FlushReason) {
  if (Host()->LowLatencyEnabled()) {
    // PaintRenderingResultsToCanvas will export drawing buffer if the resource
    // provider is single buffered.  Otherwise it will copy the drawing buffer.
    PaintRenderingResultsToCanvas(kBackBuffer);
  }
  marked_canvas_dirty_ = false;
}

void WebGLRenderingContextBase::OnErrorMessage(const char* message,
                                               int32_t id) {
  if (synthesized_errors_to_console_)
    PrintGLErrorToConsole(message);
  NotifyWebGLErrorOrWarning(message);
}

WebGLRenderingContextBase::HowToClear
WebGLRenderingContextBase::ClearIfComposited(
    WebGLRenderingContextBase::ClearCaller caller,
    GLbitfield mask) {
  if (isContextLost())
    return kSkipped;

  if (!GetDrawingBuffer()->BufferClearNeeded() ||
      (mask && framebuffer_binding_) ||
      (rasterizer_discard_enabled_ && caller == kClearCallerDrawOrClear))
    return kSkipped;

  if (isContextLost()) {
    // Unlikely, but context was lost.
    return kSkipped;
  }

  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(this);

  // Determine if it's possible to combine the clear the user asked for and this
  // clear.
  bool combined_clear =
      mask && !scissor_enabled_ && back_draw_buffer_ == GL_BACK;

  ContextGL()->Disable(GL_SCISSOR_TEST);
  if (combined_clear && (mask & GL_COLOR_BUFFER_BIT)) {
    ContextGL()->ClearColor(color_mask_[0] ? clear_color_[0] : 0,
                            color_mask_[1] ? clear_color_[1] : 0,
                            color_mask_[2] ? clear_color_[2] : 0,
                            color_mask_[3] ? clear_color_[3] : 0);
  } else {
    ContextGL()->ClearColor(0, 0, 0, 0);
  }

  GLbitfield clear_mask = GL_COLOR_BUFFER_BIT;

  const bool has_depth =
      CreationAttributes().depth && GetDrawingBuffer()->HasDepthBuffer();
  const bool has_stencil =
      CreationAttributes().stencil && GetDrawingBuffer()->HasStencilBuffer();

  if (has_depth) {
    if (!combined_clear || !depth_mask_ || !(mask & GL_DEPTH_BUFFER_BIT))
      ContextGL()->ClearDepthf(1.0f);
    clear_mask |= GL_DEPTH_BUFFER_BIT;
    ContextGL()->DepthMask(true);
  }
  if (has_stencil || GetDrawingBuffer()->HasImplicitStencilBuffer()) {
    if (combined_clear && (mask & GL_STENCIL_BUFFER_BIT))
      ContextGL()->ClearStencil(clear_stencil_ & stencil_mask_);
    else
      ContextGL()->ClearStencil(0);
    clear_mask |= GL_STENCIL_BUFFER_BIT;
    ContextGL()->StencilMaskSeparate(GL_FRONT, 0xFFFFFFFF);
  }

  if (ExtensionEnabled(kOESDrawBuffersIndexedName)) {
    ContextGL()->ColorMaskiOES(
        0, true, true, true,
        !GetDrawingBuffer()->DefaultBufferRequiresAlphaChannelToBePreserved());
  } else {
    ContextGL()->ColorMask(
        true, true, true,
        !GetDrawingBuffer()->DefaultBufferRequiresAlphaChannelToBePreserved());
  }
  {
    ScopedDisableRasterizerDiscard scoped_disable(this,
                                                  rasterizer_discard_enabled_);
    GetDrawingBuffer()->ClearFramebuffers(clear_mask);
  }

  // Call the DrawingBufferClient method to restore scissor test, mask, and
  // clear values, because we dirtied them above.
  DrawingBufferClientRestoreScissorTest();
  DrawingBufferClientRestoreMaskAndClearValues();

  GetDrawingBuffer()->SetBufferClearNeeded(false);

  return combined_clear ? kCombinedClear : kJustClear;
}

void WebGLRenderingContextBase::RestoreScissorEnabled() {
  if (isContextLost())
    return;

  if (scissor_enabled_) {
    ContextGL()->Enable(GL_SCISSOR_TEST);
  } else {
    ContextGL()->Disable(GL_SCISSOR_TEST);
  }
}

void WebGLRenderingContextBase::RestoreScissorBox() {
  if (isContextLost())
    return;

  ContextGL()->Scissor(scissor_box_[0], scissor_box_[1], scissor_box_[2],
                       scissor_box_[3]);
}

void WebGLRenderingContextBase::RestoreClearColor() {
  if (isContextLost())
    return;

  ContextGL()->ClearColor(clear_color_[0], clear_color_[1], clear_color_[2],
                          clear_color_[3]);
}

void WebGLRenderingContextBase::RestoreColorMask() {
  if (isContextLost())
    return;

  ContextGL()->ColorMask(color_mask_[0], color_mask_[1], color_mask_[2],
                         color_mask_[3]);
}

void WebGLRenderingContextBase::MarkLayerComposited() {
  if (!isContextLost())
    GetDrawingBuffer()->SetBufferClearNeeded(true);
}

bool WebGLRenderingContextBase::UsingSwapChain() const {
  return GetDrawingBuffer() && GetDrawingBuffer()->UsingSwapChain();
}

bool WebGLRenderingContextBase::IsOriginTopLeft() const {
  if (isContextLost())
    return false;
  return is_origin_top_left_;
}

void WebGLRenderingContextBase::PageVisibilityChanged() {
  if (GetDrawingBuffer())
    GetDrawingBuffer()->SetIsInHiddenPage(!Host()->IsPageVisible());
}

bool WebGLRenderingContextBase::PaintRenderingResultsToCanvas(
    SourceDrawingBuffer source_buffer) {
  TRACE_EVENT0("blink",
               "WebGLRenderingContextBase::PaintRenderingResultsToCanvas");
  if (isContextLost() || !GetDrawingBuffer())
    return false;

  bool must_clear_now = ClearIfComposited(kClearCallerOther) != kSkipped;

  if (Host()->ResourceProvider() &&
      Host()->ResourceProvider()->Size() != GetDrawingBuffer()->Size()) {
    Host()->DiscardResourceProvider();
  }

  // The host's ResourceProvider is purged to save memory when the tab
  // is backgrounded.

  if (!must_paint_to_canvas_ && !must_clear_now && Host()->ResourceProvider())
    return false;

  must_paint_to_canvas_ = false;

  CanvasResourceProvider* resource_provider =
      Host()->GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);
  if (!resource_provider)
    return false;

  if (Host()->LowLatencyEnabled() &&
      resource_provider->SupportsSingleBuffering()) {
    // It's possible single buffering isn't enabled yet because we haven't
    // finished the first frame e.g. this gets called first due to drawImage.
    resource_provider->TryEnableSingleBuffering();
    DCHECK(resource_provider->IsSingleBuffered());
    // Single buffered passthrough resource provider doesn't have backing
    // texture. We need to export the backbuffer mailbox directly without
    // copying.
    if (!resource_provider->ImportResource(
            GetDrawingBuffer()->ExportLowLatencyCanvasResource(
                resource_provider->CreateWeakPtr()))) {
      // This isn't expected to fail for single buffered resource provider.
      NOTREACHED_IN_MIGRATION();
      return false;
    }
    return true;
  }

  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(this);
  // TODO(sunnyps): Why is a texture restorer needed? See if it can be removed.
  ScopedTexture2DRestorer restorer(this);
  ScopedFramebufferRestorer fbo_restorer(this);

  // In rare situations on macOS the drawing buffer can be destroyed
  // during the resolve process, specifically during automatic
  // graphics switching. Guard against this.
  if (!GetDrawingBuffer()->ResolveAndBindForReadAndDraw())
    return false;
  if (!CopyRenderingResultsFromDrawingBuffer(Host()->ResourceProvider(),
                                             source_buffer)) {
    // CopyRenderingResultsFromDrawingBuffer handles both the
    // hardware-accelerated and software cases, so there is no
    // possible additional fallback for failures seen at this point.
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::CopyRenderingResultsFromDrawingBuffer(
    CanvasResourceProvider* resource_provider,
    SourceDrawingBuffer source_buffer) {
  DCHECK(resource_provider);
  DCHECK(!resource_provider->IsSingleBuffered());

  // Early-out if the context has been lost.
  if (!GetDrawingBuffer())
    return false;

  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(this);
  ScopedFramebufferRestorer fbo_restorer(this);
  // In rare situations on macOS the drawing buffer can be destroyed
  // during the resolve process, specifically during automatic
  // graphics switching. Guard against this.
  // This is a no-op if already called higher up the stack from here.
  if (!GetDrawingBuffer()->ResolveAndBindForReadAndDraw())
    return false;

  const bool flip_y = IsOriginTopLeft() != resource_provider->IsOriginTopLeft();
  if (resource_provider->IsAccelerated()) {
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> shared_context_wrapper =
        SharedGpuContext::ContextProviderWrapper();
    if (!shared_context_wrapper || !shared_context_wrapper->ContextProvider())
      return false;
    gpu::raster::RasterInterface* raster_interface =
        shared_context_wrapper->ContextProvider()->RasterInterface();
    auto client_si =
        resource_provider->GetBackingClientSharedImageForOverwrite();
    if (!client_si) {
      return false;
    }

    // TODO(xlai): Flush should not be necessary if the synchronization in
    // CopyToPlatformTexture is done correctly. See crbug.com/794706.
    raster_interface->Flush();

    return GetDrawingBuffer()->CopyToPlatformMailbox(
        raster_interface, client_si->mailbox(), client_si->GetTextureTarget(),
        flip_y, gfx::Point(0, 0), gfx::Rect(drawing_buffer_->Size()),
        source_buffer);
  }

  // As the resource provider is not accelerated, we don't need an accelerated
  // image.
  scoped_refptr<StaticBitmapImage> image =
      GetDrawingBuffer()->GetUnacceleratedStaticBitmapImage(flip_y);

  if (!image || !image->PaintImageForCurrentFrame())
    return false;

  gfx::Rect src_rect(image->Size());
  gfx::Rect dest_rect(resource_provider->Size());
  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kSrc);
  // We use this draw helper as we need to take into account the
  // ImageOrientation of the UnacceleratedStaticBitmapImage.
  ImageDrawOptions draw_options;
  draw_options.clamping_mode = Image::kDoNotClampImageToSourceRect;
  image->Draw(&resource_provider->Canvas(), flags, gfx::RectF(dest_rect),
              gfx::RectF(src_rect), draw_options);
  return true;
}

bool WebGLRenderingContextBase::CopyRenderingResultsToVideoFrame(
    WebGraphicsContext3DVideoFramePool* frame_pool,
    SourceDrawingBuffer src_buffer,
    const gfx::ColorSpace& dst_color_space,
    VideoFrameCopyCompletedCallback callback) {
  if (!frame_pool)
    return false;

  auto* drawing_buffer = GetDrawingBuffer();
  if (!drawing_buffer)
    return false;

  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(this);
  ScopedFramebufferRestorer fbo_restorer(this);
  if (!drawing_buffer->ResolveAndBindForReadAndDraw())
    return false;

  return drawing_buffer->CopyToVideoFrame(frame_pool, src_buffer,
                                          is_origin_top_left_, dst_color_space,
                                          std::move(callback));
}

gfx::Size WebGLRenderingContextBase::DrawingBufferSize() const {
  if (isContextLost())
    return gfx::Size(0, 0);
  return GetDrawingBuffer()->Size();
}

sk_sp<SkData> WebGLRenderingContextBase::PaintRenderingResultsToDataArray(
    SourceDrawingBuffer source_buffer) {
  if (isContextLost())
    return nullptr;
  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(this);
  ClearIfComposited(kClearCallerOther);
  // In rare situations on macOS the drawing buffer can be destroyed
  // during the resolve process, specifically during automatic
  // graphics switching. Guard against this.
  if (!GetDrawingBuffer()->ResolveAndBindForReadAndDraw())
    return nullptr;
  ScopedFramebufferRestorer restorer(this);
  return GetDrawingBuffer()->PaintRenderingResultsToDataArray(source_buffer);
}

void WebGLRenderingContextBase::Reshape(int width, int height) {
  if (isContextLost())
    return;

  GLint buffer = 0;
  if (IsWebGL2()) {
    // This query returns client side cached binding, so it's trivial.
    // If it changes in the future, such query is heavy and should be avoided.
    ContextGL()->GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &buffer);
    if (buffer) {
      ContextGL()->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
  }

  // This is an approximation because at WebGLRenderingContextBase level we
  // don't know if the underlying FBO uses textures or renderbuffers.
  GLint max_size = std::min(max_texture_size_, max_renderbuffer_size_);
  GLint max_width = std::min(max_size, max_viewport_dims_[0]);
  GLint max_height = std::min(max_size, max_viewport_dims_[1]);
  width = Clamp(width, 1, max_width);
  height = Clamp(height, 1, max_height);

  // Limit drawing buffer area to the resolution of an 8K monitor to avoid
  // memory exhaustion.  Width or height may be larger than that size as long as
  // it's within the max viewport dimensions and total area remains within the
  // limit. For example: 7680x4320 should be fine.
  const int kMaxArea = 5760 * 5760;
  int current_area = width * height;
  if (current_area > kMaxArea) {
    // If we've exceeded the area limit scale the buffer down, preserving
    // ascpect ratio, until it fits.
    float scale_factor =
        sqrtf(static_cast<float>(kMaxArea) / static_cast<float>(current_area));
    width = std::max(1, static_cast<int>(width * scale_factor));
    height = std::max(1, static_cast<int>(height * scale_factor));
  }

  // We don't have to mark the canvas as dirty, since the newly created image
  // buffer will also start off clear (and this matches what reshape will do).
  GetDrawingBuffer()->set_low_latency_enabled(Host()->LowLatencyEnabled());
  GetDrawingBuffer()->Resize(gfx::Size(width, height));
  GetDrawingBuffer()->MarkContentsChanged();

  if (buffer) {
    ContextGL()->BindBuffer(GL_PIXEL_UNPACK_BUFFER,
                            static_cast<GLuint>(buffer));
  }
}

int WebGLRenderingContextBase::drawingBufferWidth() const {
  return isContextLost() ? 0 : GetDrawingBuffer()->Size().width();
}

int WebGLRenderingContextBase::drawingBufferHeight() const {
  return isContextLost() ? 0 : GetDrawingBuffer()->Size().height();
}

GLenum WebGLRenderingContextBase::drawingBufferFormat() const {
  return isContextLost() ? 0 : GetDrawingBuffer()->StorageFormat();
}

V8PredefinedColorSpace WebGLRenderingContextBase::drawingBufferColorSpace()
    const {
  return PredefinedColorSpaceToV8(drawing_buffer_color_space_);
}

void WebGLRenderingContextBase::setDrawingBufferColorSpace(
    const V8PredefinedColorSpace& v8_color_space,
    ExceptionState& exception_state) {
  // Some values for PredefinedColorSpace are supposed to be guarded behind
  // runtime flags. Use `ValidateAndConvertColorSpace` to throw an exception if
  // `v8_color_space` should not be exposed.
  PredefinedColorSpace color_space = PredefinedColorSpace::kSRGB;
  if (!ValidateAndConvertColorSpace(v8_color_space, color_space,
                                    exception_state)) {
    return;
  }
  if (drawing_buffer_color_space_ == color_space)
    return;
  drawing_buffer_color_space_ = color_space;
  if (GetDrawingBuffer())
    GetDrawingBuffer()->SetColorSpace(drawing_buffer_color_space_);
}

V8PredefinedColorSpace WebGLRenderingContextBase::unpackColorSpace() const {
  return PredefinedColorSpaceToV8(unpack_color_space_);
}

void WebGLRenderingContextBase::setUnpackColorSpace(
    const V8PredefinedColorSpace& v8_color_space,
    ExceptionState& exception_state) {
  PredefinedColorSpace color_space = PredefinedColorSpace::kSRGB;
  if (!ValidateAndConvertColorSpace(v8_color_space, color_space,
                                    exception_state)) {
    return;
  }
  NOTIMPLEMENTED();
  unpack_color_space_ = color_space;
}

void WebGLRenderingContextBase::activeTexture(GLenum texture) {
  if (isContextLost())
    return;
  if (texture - GL_TEXTURE0 >= texture_units_.size()) {
    SynthesizeGLError(GL_INVALID_ENUM, "activeTexture",
                      "texture unit out of range");
    return;
  }
  active_texture_unit_ = texture - GL_TEXTURE0;
  ContextGL()->ActiveTexture(texture);
}

void WebGLRenderingContextBase::attachShader(WebGLProgram* program,
                                             WebGLShader* shader) {
  if (!ValidateWebGLProgramOrShader("attachShader", program) ||
      !ValidateWebGLProgramOrShader("attachShader", shader))
    return;
  if (!program->AttachShader(shader)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "attachShader",
                      "shader attachment already has shader");
    return;
  }
  ContextGL()->AttachShader(ObjectOrZero(program), ObjectOrZero(shader));
  shader->OnAttached();
}

void WebGLRenderingContextBase::bindAttribLocation(WebGLProgram* program,
                                                   GLuint index,
                                                   const String& name) {
  if (!ValidateWebGLProgramOrShader("bindAttribLocation", program))
    return;
  if (!ValidateLocationLength("bindAttribLocation", name))
    return;
  if (IsPrefixReserved(name)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "bindAttribLocation",
                      "reserved prefix");
    return;
  }
  ContextGL()->BindAttribLocation(ObjectOrZero(program), index,
                                  name.Utf8().c_str());
}

bool WebGLRenderingContextBase::ValidateAndUpdateBufferBindTarget(
    const char* function_name,
    GLenum target,
    WebGLBuffer* buffer) {
  if (!ValidateBufferTarget(function_name, target))
    return false;

  if (buffer && buffer->GetInitialTarget() &&
      buffer->GetInitialTarget() != target) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "buffers can not be used with multiple targets");
    return false;
  }

  switch (target) {
    case GL_ARRAY_BUFFER:
      bound_array_buffer_ = buffer;
      break;
    case GL_ELEMENT_ARRAY_BUFFER:
      bound_vertex_array_object_->SetElementArrayBuffer(buffer);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }

  if (buffer && !buffer->GetInitialTarget())
    buffer->SetInitialTarget(target);
  return true;
}

void WebGLRenderingContextBase::bindBuffer(GLenum target, WebGLBuffer* buffer) {
  if (!ValidateNullableWebGLObject("bindBuffer", buffer))
    return;
  if (!ValidateAndUpdateBufferBindTarget("bindBuffer", target, buffer))
    return;
  ContextGL()->BindBuffer(target, ObjectOrZero(buffer));
}

void WebGLRenderingContextBase::bindFramebuffer(GLenum target,
                                                WebGLFramebuffer* buffer) {
  if (!ValidateNullableWebGLObject("bindFramebuffer", buffer))
    return;

  if (target != GL_FRAMEBUFFER) {
    SynthesizeGLError(GL_INVALID_ENUM, "bindFramebuffer", "invalid target");
    return;
  }

  SetFramebuffer(target, buffer);
}

void WebGLRenderingContextBase::bindRenderbuffer(
    GLenum target,
    WebGLRenderbuffer* render_buffer) {
  if (!ValidateNullableWebGLObject("bindRenderbuffer", render_buffer))
    return;
  if (target != GL_RENDERBUFFER) {
    SynthesizeGLError(GL_INVALID_ENUM, "bindRenderbuffer", "invalid target");
    return;
  }
  renderbuffer_binding_ = render_buffer;
  ContextGL()->BindRenderbuffer(target, ObjectOrZero(render_buffer));
  if (render_buffer)
    render_buffer->SetHasEverBeenBound();
}

void WebGLRenderingContextBase::bindTexture(GLenum target,
                                            WebGLTexture* texture) {
  if (!ValidateNullableWebGLObject("bindTexture", texture))
    return;
  if (texture && texture->GetTarget() && texture->GetTarget() != target) {
    SynthesizeGLError(GL_INVALID_OPERATION, "bindTexture",
                      "textures can not be used with multiple targets");
    return;
  }

  if (target == GL_TEXTURE_2D) {
    texture_units_[active_texture_unit_].texture2d_binding_ = texture;
  } else if (target == GL_TEXTURE_CUBE_MAP) {
    texture_units_[active_texture_unit_].texture_cube_map_binding_ = texture;
  } else if (IsWebGL2() && target == GL_TEXTURE_2D_ARRAY) {
    texture_units_[active_texture_unit_].texture2d_array_binding_ = texture;
  } else if (IsWebGL2() && target == GL_TEXTURE_3D) {
    texture_units_[active_texture_unit_].texture3d_binding_ = texture;
  } else if (target == GL_TEXTURE_EXTERNAL_OES) {
    SynthesizeGLError(GL_INVALID_ENUM, "bindTexture",
                      "GL_TEXTURE_EXTERNAL_OES textures not supported");
    return;
  } else if (target == GL_TEXTURE_RECTANGLE_ARB) {
    SynthesizeGLError(GL_INVALID_ENUM, "bindTexture",
                      "GL_TEXTURE_RECTANGLE_ARB textures not supported");
    return;
  } else {
    SynthesizeGLError(GL_INVALID_ENUM, "bindTexture", "invalid target");
    return;
  }

  ContextGL()->BindTexture(target, ObjectOrZero(texture));
  if (texture) {
    texture->SetTarget(target);
    one_plus_max_non_default_texture_unit_ =
        max(active_texture_unit_ + 1, one_plus_max_non_default_texture_unit_);
  } else {
    // If the disabled index is the current maximum, trace backwards to find the
    // new max enabled texture index
    if (one_plus_max_non_default_texture_unit_ == active_texture_unit_ + 1) {
      FindNewMaxNonDefaultTextureUnit();
    }
  }

  // Note: previously we used to automatically set the TEXTURE_WRAP_R
  // repeat mode to CLAMP_TO_EDGE for cube map textures, because OpenGL
  // ES 2.0 doesn't expose this flag (a bug in the specification) and
  // otherwise the application has no control over the seams in this
  // dimension. However, it appears that supporting this properly on all
  // platforms is fairly involved (will require a HashMap from texture ID
  // in all ports), and we have not had any complaints, so the logic has
  // been removed.
}

void WebGLRenderingContextBase::blendColor(GLfloat red,
                                           GLfloat green,
                                           GLfloat blue,
                                           GLfloat alpha) {
  if (isContextLost())
    return;
  ContextGL()->BlendColor(red, green, blue, alpha);
}

void WebGLRenderingContextBase::blendEquation(GLenum mode) {
  if (isContextLost() || !ValidateBlendEquation("blendEquation", mode))
    return;
  ContextGL()->BlendEquation(mode);
}

void WebGLRenderingContextBase::blendEquationSeparate(GLenum mode_rgb,
                                                      GLenum mode_alpha) {
  if (isContextLost() ||
      !ValidateBlendEquation("blendEquationSeparate", mode_rgb) ||
      !ValidateBlendEquation("blendEquationSeparate", mode_alpha))
    return;
  ContextGL()->BlendEquationSeparate(mode_rgb, mode_alpha);
}

void WebGLRenderingContextBase::blendFunc(GLenum sfactor, GLenum dfactor) {
  if (isContextLost() ||
      !ValidateBlendFuncFactors("blendFunc", sfactor, dfactor))
    return;
  ContextGL()->BlendFunc(sfactor, dfactor);
}

void WebGLRenderingContextBase::blendFuncSeparate(GLenum src_rgb,
                                                  GLenum dst_rgb,
                                                  GLenum src_alpha,
                                                  GLenum dst_alpha) {
  // Note: Alpha does not have the same restrictions as RGB.
  if (isContextLost() ||
      !ValidateBlendFuncFactors("blendFuncSeparate", src_rgb, dst_rgb))
    return;

  if (!ValidateBlendFuncExtendedFactors("blendFuncSeparate", src_alpha,
                                        dst_alpha)) {
    return;
  }

  ContextGL()->BlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
}

bool WebGLRenderingContextBase::ValidateBufferDataBufferSize(
    const char* function_name,
    int64_t size) {
  if (size < 0) {
    String error_msg = "data size is invalid";
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      error_msg.Ascii().c_str());
    return false;
  }
  if (static_cast<size_t>(size) > kMaximumSupportedArrayBufferSize) {
    String error_msg = "data size exceeds the maximum supported size";
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      error_msg.Ascii().c_str());
    return false;
  }
  return true;
}

void WebGLRenderingContextBase::BufferDataImpl(GLenum target,
                                               int64_t size,
                                               const void* data,
                                               GLenum usage) {
  WebGLBuffer* buffer = ValidateBufferDataTarget("bufferData", target);
  if (!buffer)
    return;

  if (!ValidateBufferDataUsage("bufferData", usage))
    return;

  if (!ValidateValueFitNonNegInt32("bufferData", "size", size))
    return;

  if (!ValidateBufferDataBufferSize("bufferData", size))
    return;

  buffer->SetSize(size);

  ContextGL()->BufferData(target, static_cast<GLsizeiptr>(size), data, usage);
}

void WebGLRenderingContextBase::bufferData(GLenum target,
                                           int64_t size,
                                           GLenum usage) {
  if (isContextLost())
    return;
  BufferDataImpl(target, size, nullptr, usage);
}

void WebGLRenderingContextBase::bufferData(GLenum target,
                                           DOMArrayBufferBase* data,
                                           GLenum usage) {
  if (isContextLost())
    return;
  if (!data) {
    SynthesizeGLError(GL_INVALID_VALUE, "bufferData", "no data");
    return;
  }
  BufferDataImpl(target, data->ByteLength(), data->DataMaybeShared(), usage);
}

void WebGLRenderingContextBase::bufferData(GLenum target,
                                           MaybeShared<DOMArrayBufferView> data,
                                           GLenum usage) {
  if (isContextLost())
    return;
  DCHECK(data);
  BufferDataImpl(target, data->byteLength(), data->BaseAddressMaybeShared(),
                 usage);
}

void WebGLRenderingContextBase::BufferSubDataImpl(GLenum target,
                                                  int64_t offset,
                                                  int64_t size,
                                                  const void* data) {
  WebGLBuffer* buffer = ValidateBufferDataTarget("bufferSubData", target);
  if (!buffer)
    return;
  if (!ValidateValueFitNonNegInt32("bufferSubData", "offset", offset))
    return;
  if (!ValidateValueFitNonNegInt32("bufferSubData", "size", size))
    return;
  if (!ValidateBufferDataBufferSize("bufferSubData", size))
    return;

  if (!data)
    return;
  if (offset + static_cast<int64_t>(size) > buffer->GetSize()) {
    SynthesizeGLError(GL_INVALID_VALUE, "bufferSubData", "buffer overflow");
    return;
  }

  ContextGL()->BufferSubData(target, static_cast<GLintptr>(offset),
                             static_cast<GLintptr>(size), data);
}

void WebGLRenderingContextBase::bufferSubData(GLenum target,
                                              int64_t offset,
                                              base::span<const uint8_t> data) {
  if (isContextLost())
    return;
  BufferSubDataImpl(target, offset, data.size(), data.data());
}

bool WebGLRenderingContextBase::ValidateFramebufferTarget(GLenum target) {
  if (target == GL_FRAMEBUFFER)
    return true;
  return false;
}

WebGLFramebuffer* WebGLRenderingContextBase::GetFramebufferBinding(
    GLenum target) {
  if (target == GL_FRAMEBUFFER)
    return framebuffer_binding_.Get();
  return nullptr;
}

WebGLFramebuffer* WebGLRenderingContextBase::GetReadFramebufferBinding() {
  return framebuffer_binding_.Get();
}

GLenum WebGLRenderingContextBase::checkFramebufferStatus(GLenum target) {
  if (isContextLost())
    return GL_FRAMEBUFFER_UNSUPPORTED;
  if (!ValidateFramebufferTarget(target)) {
    SynthesizeGLError(GL_INVALID_ENUM, "checkFramebufferStatus",
                      "invalid target");
    return 0;
  }
  WebGLFramebuffer* framebuffer_binding = GetFramebufferBinding(target);
  if (framebuffer_binding) {
    const char* reason = "framebuffer incomplete";
    GLenum status = framebuffer_binding->CheckDepthStencilStatus(&reason);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      EmitGLWarning("checkFramebufferStatus", reason);
      return status;
    }
  }
  return ContextGL()->CheckFramebufferStatus(target);
}

void WebGLRenderingContextBase::clear(GLbitfield mask) {
  if (isContextLost())
    return;
  if (mask &
      ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) {
    SynthesizeGLError(GL_INVALID_VALUE, "clear", "invalid mask");
    return;
  }
  const char* reason = "framebuffer incomplete";
  if (framebuffer_binding_ && framebuffer_binding_->CheckDepthStencilStatus(
                                  &reason) != GL_FRAMEBUFFER_COMPLETE) {
    SynthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, "clear", reason);
    return;
  }

  if (!mask) {
    // Use OnErrorMessage because it's both rate-limited and obeys the
    // webGLErrorsToConsole setting.
    OnErrorMessage(
        "Performance warning: clear() called with no buffers in bitmask", 0);
    // Don't skip the call to ClearIfComposited below; it has side
    // effects even without the user requesting to clear any buffers.
  }

  ScopedRGBEmulationColorMask emulation_color_mask(this, color_mask_.data(),
                                                   drawing_buffer_.get());

  if (ClearIfComposited(kClearCallerDrawOrClear, mask) != kCombinedClear) {
    // If clearing the default back buffer's depth buffer, also clear the
    // stencil buffer, if one was allocated implicitly. This avoids performance
    // problems on some GPUs.
    if (!framebuffer_binding_ &&
        GetDrawingBuffer()->HasImplicitStencilBuffer() &&
        (mask & GL_DEPTH_BUFFER_BIT)) {
      // It shouldn't matter what value it's cleared to, since in other queries
      // in the API, we claim that the stencil buffer doesn't exist.
      mask |= GL_STENCIL_BUFFER_BIT;
    }
    ContextGL()->Clear(mask);
  }
  MarkContextChanged(kCanvasChanged,
                     CanvasPerformanceMonitor::DrawType::kOther);
}

void WebGLRenderingContextBase::clearColor(GLfloat r,
                                           GLfloat g,
                                           GLfloat b,
                                           GLfloat a) {
  if (isContextLost())
    return;
  if (std::isnan(r))
    r = 0;
  if (std::isnan(g))
    g = 0;
  if (std::isnan(b))
    b = 0;
  if (std::isnan(a))
    a = 1;
  clear_color_[0] = r;
  clear_color_[1] = g;
  clear_color_[2] = b;
  clear_color_[3] = a;
  ContextGL()->ClearColor(r, g, b, a);
}

void WebGLRenderingContextBase::clearDepth(GLfloat depth) {
  if (isContextLost())
    return;
  clear_depth_ = depth;
  ContextGL()->ClearDepthf(depth);
}

void WebGLRenderingContextBase::clearStencil(GLint s) {
  if (isContextLost())
    return;
  clear_stencil_ = s;
  ContextGL()->ClearStencil(s);
}

void WebGLRenderingContextBase::colorMask(GLboolean red,
                                          GLboolean green,
                                          GLboolean blue,
                                          GLboolean alpha) {
  if (isContextLost())
    return;
  color_mask_[0] = red;
  color_mask_[1] = green;
  color_mask_[2] = blue;
  color_mask_[3] = alpha;
  ContextGL()->ColorMask(red, green, blue, alpha);
}

void WebGLRenderingContextBase::compileShader(WebGLShader* shader) {
  if (!ValidateWebGLProgramOrShader("compileShader", shader))
    return;
  ContextGL()->CompileShader(ObjectOrZero(shader));
}

void WebGLRenderingContextBase::compressedTexImage2D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    MaybeShared<DOMArrayBufferView> data) {
  if (isContextLost())
    return;
  if (!ValidateTexture2DBinding("compressedTexImage2D", target, true))
    return;
  if (!ValidateCompressedTexFormat("compressedTexImage2D", internalformat))
    return;
  GLsizei data_length;
  if (!ExtractDataLengthIfValid("compressedTexImage2D", data, &data_length))
    return;
  if (static_cast<size_t>(data_length) > kMaximumSupportedArrayBufferSize) {
    SynthesizeGLError(GL_INVALID_VALUE, "compressedTexImage2D",
                      "ArrayBufferView size exceeds the supported range");
    return;
  }
  ContextGL()->CompressedTexImage2D(target, level, internalformat, width,
                                    height, border, data_length,
                                    data->BaseAddressMaybeShared());
}

void WebGLRenderingContextBase::compressedTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    MaybeShared<DOMArrayBufferView> data) {
  if (isContextLost())
    return;
  if (!ValidateTexture2DBinding("compressedTexSubImage2D", target))
    return;
  if (!ValidateCompressedTexFormat("compressedTexSubImage2D", format))
    return;
  GLsizei data_length;
  if (!ExtractDataLengthIfValid("compressedTexSubImage2D", data, &data_length))
    return;
  if (static_cast<size_t>(data_length) > kMaximumSupportedArrayBufferSize) {
    SynthesizeGLError(GL_INVALID_VALUE, "compressedTexImage2D",
                      "ArrayBufferView size exceeds the supported range");
    return;
  }
  ContextGL()->CompressedTexSubImage2D(target, level, xoffset, yoffset, width,
                                       height, format, data_length,
                                       data->BaseAddressMaybeShared());
}

bool WebGLRenderingContextBase::ValidateSettableTexFormat(
    const char* function_name,
    GLenum format) {
  if (IsWebGL2())
    return true;

  if (WebGLImageConversion::GetChannelBitsByFormat(format) &
      WebGLImageConversion::kChannelDepthStencil) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "format can not be set, only rendered to");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateCopyTexFormat(const char* function_name,
                                                      GLenum internalformat) {
  if (!is_web_gl2_internal_formats_copy_tex_image_added_ && IsWebGL2()) {
    ADD_VALUES_TO_SET(supported_internal_formats_copy_tex_image_,
                      kSupportedInternalFormatsES3);
    is_web_gl2_internal_formats_copy_tex_image_added_ = true;
  }
  if (!is_ext_color_buffer_float_formats_added_ &&
      ExtensionEnabled(kEXTColorBufferFloatName)) {
    ADD_VALUES_TO_SET(supported_internal_formats_copy_tex_image_,
                      kSupportedInternalFormatsCopyTexImageFloatES3);
    is_ext_color_buffer_float_formats_added_ = true;
  }
  if (!is_ext_color_buffer_half_float_formats_added_ &&
      ExtensionEnabled(kEXTColorBufferHalfFloatName)) {
    ADD_VALUES_TO_SET(supported_internal_formats_copy_tex_image_,
                      kSupportedInternalFormatsCopyTexImageHalfFloatES3);
    is_ext_color_buffer_half_float_formats_added_ = true;
  }

  if (!base::Contains(supported_internal_formats_copy_tex_image_,
                      internalformat)) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid internalformat");
    return false;
  }

  return true;
}

void WebGLRenderingContextBase::copyTexImage2D(GLenum target,
                                               GLint level,
                                               GLenum internalformat,
                                               GLint x,
                                               GLint y,
                                               GLsizei width,
                                               GLsizei height,
                                               GLint border) {
  if (isContextLost())
    return;
  if (!ValidateTexture2DBinding("copyTexImage2D", target, true))
    return;
  if (!ValidateCopyTexFormat("copyTexImage2D", internalformat))
    return;
  if (!ValidateSettableTexFormat("copyTexImage2D", internalformat))
    return;
  WebGLFramebuffer* read_framebuffer_binding = nullptr;
  if (!ValidateReadBufferAndGetInfo("copyTexImage2D", read_framebuffer_binding))
    return;
  ClearIfComposited(kClearCallerOther);
  ScopedDrawingBufferBinder binder(GetDrawingBuffer(),
                                   read_framebuffer_binding);
  if (!binder.Succeeded()) {
    return;
  }
  ContextGL()->CopyTexImage2D(target, level, internalformat, x, y, width,
                              height, border);
}

void WebGLRenderingContextBase::copyTexSubImage2D(GLenum target,
                                                  GLint level,
                                                  GLint xoffset,
                                                  GLint yoffset,
                                                  GLint x,
                                                  GLint y,
                                                  GLsizei width,
                                                  GLsizei height) {
  if (isContextLost())
    return;
  if (!ValidateTexture2DBinding("copyTexSubImage2D", target))
    return;
  WebGLFramebuffer* read_framebuffer_binding = nullptr;
  if (!ValidateReadBufferAndGetInfo("copyTexSubImage2D",
                                    read_framebuffer_binding))
    return;
  ClearIfComposited(kClearCallerOther);
  ScopedDrawingBufferBinder binder(GetDrawingBuffer(),
                                   read_framebuffer_binding);
  if (!binder.Succeeded()) {
    return;
  }
  ContextGL()->CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width,
                                 height);
}

WebGLBuffer* WebGLRenderingContextBase::createBuffer() {
  if (isContextLost())
    return nullptr;
  return MakeGarbageCollected<WebGLBuffer>(this);
}

WebGLFramebuffer* WebGLRenderingContextBase::createFramebuffer() {
  if (isContextLost())
    return nullptr;
  return MakeGarbageCollected<WebGLFramebuffer>(this);
}

WebGLTexture* WebGLRenderingContextBase::createTexture() {
  if (isContextLost())
    return nullptr;
  return MakeGarbageCollected<WebGLTexture>(this);
}

WebGLProgram* WebGLRenderingContextBase::createProgram() {
  if (isContextLost())
    return nullptr;
  return MakeGarbageCollected<WebGLProgram>(this);
}

WebGLRenderbuffer* WebGLRenderingContextBase::createRenderbuffer() {
  if (isContextLost())
    return nullptr;
  return MakeGarbageCollected<WebGLRenderbuffer>(this);
}

void WebGLRenderingContextBase::SetBoundVertexArrayObject(
    WebGLVertexArrayObjectBase* array_object) {
  if (array_object)
    bound_vertex_array_object_ = array_object;
  else
    bound_vertex_array_object_ = default_vertex_array_object_;
}

WebGLShader* WebGLRenderingContextBase::createShader(GLenum type) {
  if (isContextLost())
    return nullptr;
  if (!ValidateShaderType("createShader", type)) {
    return nullptr;
  }

  return MakeGarbageCollected<WebGLShader>(this, type);
}

void WebGLRenderingContextBase::cullFace(GLenum mode) {
  if (isContextLost())
    return;
  ContextGL()->CullFace(mode);
}

bool WebGLRenderingContextBase::DeleteObject(WebGLObject* object) {
  if (isContextLost() || !object)
    return false;
  if (!object->Validate(ContextGroup(), this)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "delete",
                      "object does not belong to this context");
    return false;
  }
  if (object->MarkedForDeletion()) {
    // This is specified to be a no-op, including skipping all unbinding from
    // the context's attachment points that would otherwise happen.
    return false;
  }
  if (object->HasObject()) {
    // We need to pass in context here because we want
    // things in this context unbound.
    object->DeleteObject(ContextGL());
  }
  return true;
}

void WebGLRenderingContextBase::deleteBuffer(WebGLBuffer* buffer) {
  if (!DeleteObject(buffer))
    return;
  RemoveBoundBuffer(buffer);
}

void WebGLRenderingContextBase::deleteFramebuffer(
    WebGLFramebuffer* framebuffer) {
  // Don't allow the application to delete an opaque framebuffer.
  if (framebuffer && framebuffer->Opaque()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "deleteFramebuffer",
                      "cannot delete an opaque framebuffer");
    return;
  }
  if (!DeleteObject(framebuffer))
    return;
  if (framebuffer == framebuffer_binding_) {
    framebuffer_binding_ = nullptr;
    // Have to call drawingBuffer()->bind() here to bind back to internal fbo.
    GetDrawingBuffer()->Bind(GL_FRAMEBUFFER);
  }
}

void WebGLRenderingContextBase::deleteProgram(WebGLProgram* program) {
  DeleteObject(program);
  // We don't reset m_currentProgram to 0 here because the deletion of the
  // current program is delayed.
}

void WebGLRenderingContextBase::deleteRenderbuffer(
    WebGLRenderbuffer* renderbuffer) {
  if (!DeleteObject(renderbuffer))
    return;
  if (renderbuffer == renderbuffer_binding_) {
    renderbuffer_binding_ = nullptr;
  }
  if (framebuffer_binding_)
    framebuffer_binding_->RemoveAttachmentFromBoundFramebuffer(GL_FRAMEBUFFER,
                                                               renderbuffer);
  if (GetFramebufferBinding(GL_READ_FRAMEBUFFER))
    GetFramebufferBinding(GL_READ_FRAMEBUFFER)
        ->RemoveAttachmentFromBoundFramebuffer(GL_READ_FRAMEBUFFER,
                                               renderbuffer);
}

void WebGLRenderingContextBase::deleteShader(WebGLShader* shader) {
  DeleteObject(shader);
}

void WebGLRenderingContextBase::deleteTexture(WebGLTexture* texture) {
  if (texture && texture->IsOpaqueTexture()) {
    // Calling deleteTexture() on opaque textures is not allowed, see
    // https://www.w3.org/TR/webxrlayers-1/#opaque-texture
    SynthesizeGLError(GL_INVALID_OPERATION, "deleteTexture",
                      "opaque textures cannot be deleted");
    return;
  }

  if (!DeleteObject(texture))
    return;

  int max_bound_texture_index = -1;
  for (wtf_size_t i = 0; i < one_plus_max_non_default_texture_unit_; ++i) {
    if (texture == texture_units_[i].texture2d_binding_) {
      texture_units_[i].texture2d_binding_ = nullptr;
      max_bound_texture_index = i;
    }
    if (texture == texture_units_[i].texture_cube_map_binding_) {
      texture_units_[i].texture_cube_map_binding_ = nullptr;
      max_bound_texture_index = i;
    }
    if (IsWebGL2()) {
      if (texture == texture_units_[i].texture3d_binding_) {
        texture_units_[i].texture3d_binding_ = nullptr;
        max_bound_texture_index = i;
      }
      if (texture == texture_units_[i].texture2d_array_binding_) {
        texture_units_[i].texture2d_array_binding_ = nullptr;
        max_bound_texture_index = i;
      }
    }
  }
  if (framebuffer_binding_)
    framebuffer_binding_->RemoveAttachmentFromBoundFramebuffer(GL_FRAMEBUFFER,
                                                               texture);
  if (GetFramebufferBinding(GL_READ_FRAMEBUFFER))
    GetFramebufferBinding(GL_READ_FRAMEBUFFER)
        ->RemoveAttachmentFromBoundFramebuffer(GL_READ_FRAMEBUFFER, texture);

  // If the deleted was bound to the the current maximum index, trace backwards
  // to find the new max texture index.
  if (one_plus_max_non_default_texture_unit_ ==
      static_cast<wtf_size_t>(max_bound_texture_index + 1)) {
    FindNewMaxNonDefaultTextureUnit();
  }
}

void WebGLRenderingContextBase::depthFunc(GLenum func) {
  if (isContextLost())
    return;
  ContextGL()->DepthFunc(func);
}

void WebGLRenderingContextBase::depthMask(GLboolean flag) {
  if (isContextLost())
    return;
  depth_mask_ = flag;
  ContextGL()->DepthMask(flag);
}

void WebGLRenderingContextBase::depthRange(GLfloat z_near, GLfloat z_far) {
  if (isContextLost())
    return;
  // Check required by WebGL spec section 6.12
  if (z_near > z_far) {
    SynthesizeGLError(GL_INVALID_OPERATION, "depthRange", "zNear > zFar");
    return;
  }
  ContextGL()->DepthRangef(z_near, z_far);
}

void WebGLRenderingContextBase::detachShader(WebGLProgram* program,
                                             WebGLShader* shader) {
  if (!ValidateWebGLProgramOrShader("detachShader", program) ||
      !ValidateWebGLProgramOrShader("detachShader", shader))
    return;
  if (!program->DetachShader(shader)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "detachShader",
                      "shader not attached");
    return;
  }
  ContextGL()->DetachShader(ObjectOrZero(program), ObjectOrZero(shader));
  shader->OnDetached(ContextGL());
}

void WebGLRenderingContextBase::disable(GLenum cap) {
  if (isContextLost() || !ValidateCapability("disable", cap))
    return;
  if (cap == GL_STENCIL_TEST) {
    stencil_enabled_ = false;
    ApplyDepthAndStencilTest();
    return;
  }
  if (cap == GL_DEPTH_TEST) {
    depth_enabled_ = false;
    ApplyDepthAndStencilTest();
    return;
  }
  if (cap == GL_SCISSOR_TEST)
    scissor_enabled_ = false;
  if (cap == GL_RASTERIZER_DISCARD)
    rasterizer_discard_enabled_ = false;
  ContextGL()->Disable(cap);
}

void WebGLRenderingContextBase::disableVertexAttribArray(GLuint index) {
  if (isContextLost())
    return;
  if (index >= max_vertex_attribs_) {
    SynthesizeGLError(GL_INVALID_VALUE, "disableVertexAttribArray",
                      "index out of range");
    return;
  }

  bound_vertex_array_object_->SetAttribEnabled(index, false);
  ContextGL()->DisableVertexAttribArray(index);
}

bool WebGLRenderingContextBase::ValidateRenderingState(
    const char* function_name) {
  // Command buffer will not error if no program is bound.
  if (!current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "no valid shader program in use");
    return false;
  }

  return true;
}

bool WebGLRenderingContextBase::ValidateNullableWebGLObject(
    const char* function_name,
    WebGLObject* object) {
  if (isContextLost())
    return false;
  if (!object) {
    // This differs in behavior to ValidateWebGLObject; null objects are allowed
    // in these entry points.
    return true;
  }
  return ValidateWebGLObject(function_name, object);
}

bool WebGLRenderingContextBase::ValidateWebGLObject(const char* function_name,
                                                    WebGLObject* object) {
  if (isContextLost())
    return false;
  DCHECK(object);
  if (object->MarkedForDeletion()) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "attempt to use a deleted object");
    return false;
  }
  if (!object->Validate(ContextGroup(), this)) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "object does not belong to this context");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateWebGLProgramOrShader(
    const char* function_name,
    WebGLObject* object) {
  if (isContextLost())
    return false;
  DCHECK(object);
  // OpenGL ES 3.0.5 p. 45:
  // "Commands that accept shader or program object names will generate the
  // error INVALID_VALUE if the provided name is not the name of either a shader
  // or program object and INVALID_OPERATION if the provided name identifies an
  // object that is not the expected type."
  //
  // Programs and shaders also have slightly different lifetime rules than other
  // objects in the API; they continue to be usable after being marked for
  // deletion.
  if (!object->HasObject()) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      "attempt to use a deleted object");
    return false;
  }
  if (!object->Validate(ContextGroup(), this)) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "object does not belong to this context");
    return false;
  }
  return true;
}

void WebGLRenderingContextBase::drawArrays(GLenum mode,
                                           GLint first,
                                           GLsizei count) {
  if (!ValidateDrawArrays("drawArrays"))
    return;

  DrawWrapper("drawArrays", CanvasPerformanceMonitor::DrawType::kDrawArrays,
              [&]() { ContextGL()->DrawArrays(mode, first, count); });
}

void WebGLRenderingContextBase::drawElements(GLenum mode,
                                             GLsizei count,
                                             GLenum type,
                                             int64_t offset) {
  if (!ValidateDrawElements("drawElements", type, offset))
    return;

  DrawWrapper("drawElements", CanvasPerformanceMonitor::DrawType::kDrawElements,
              [&]() {
                ContextGL()->DrawElements(
                    mode, count, type,
                    reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
              });
}

void WebGLRenderingContextBase::DrawArraysInstancedANGLE(GLenum mode,
                                                         GLint first,
                                                         GLsizei count,
                                                         GLsizei primcount) {
  if (!ValidateDrawArrays("drawArraysInstancedANGLE"))
    return;

  DrawWrapper("drawArraysInstancedANGLE",
              CanvasPerformanceMonitor::DrawType::kDrawArrays, [&]() {
                ContextGL()->DrawArraysInstancedANGLE(mode, first, count,
                                                      primcount);
              });
}

void WebGLRenderingContextBase::DrawElementsInstancedANGLE(GLenum mode,
                                                           GLsizei count,
                                                           GLenum type,
                                                           int64_t offset,
                                                           GLsizei primcount) {
  if (!ValidateDrawElements("drawElementsInstancedANGLE", type, offset))
    return;

  DrawWrapper("drawElementsInstancedANGLE",
              CanvasPerformanceMonitor::DrawType::kDrawElements, [&]() {
                ContextGL()->DrawElementsInstancedANGLE(
                    mode, count, type,
                    reinterpret_cast<void*>(static_cast<intptr_t>(offset)),
                    primcount);
              });
}

void WebGLRenderingContextBase::enable(GLenum cap) {
  if (isContextLost() || !ValidateCapability("enable", cap))
    return;
  if (cap == GL_STENCIL_TEST) {
    stencil_enabled_ = true;
    ApplyDepthAndStencilTest();
    return;
  }
  if (cap == GL_DEPTH_TEST) {
    depth_enabled_ = true;
    ApplyDepthAndStencilTest();
    return;
  }
  if (cap == GL_SCISSOR_TEST)
    scissor_enabled_ = true;
  if (cap == GL_RASTERIZER_DISCARD)
    rasterizer_discard_enabled_ = true;
  ContextGL()->Enable(cap);
}

void WebGLRenderingContextBase::enableVertexAttribArray(GLuint index) {
  if (isContextLost())
    return;
  if (index >= max_vertex_attribs_) {
    SynthesizeGLError(GL_INVALID_VALUE, "enableVertexAttribArray",
                      "index out of range");
    return;
  }

  bound_vertex_array_object_->SetAttribEnabled(index, true);
  ContextGL()->EnableVertexAttribArray(index);
}

void WebGLRenderingContextBase::finish() {
  if (isContextLost())
    return;
  ContextGL()->Flush();  // Intentionally a flush, not a finish.
}

void WebGLRenderingContextBase::flush() {
  if (isContextLost())
    return;
  ContextGL()->Flush();
}

void WebGLRenderingContextBase::framebufferRenderbuffer(
    GLenum target,
    GLenum attachment,
    GLenum renderbuffertarget,
    WebGLRenderbuffer* buffer) {
  if (isContextLost() || !ValidateFramebufferFuncParameters(
                             "framebufferRenderbuffer", target, attachment))
    return;
  if (renderbuffertarget != GL_RENDERBUFFER) {
    SynthesizeGLError(GL_INVALID_ENUM, "framebufferRenderbuffer",
                      "invalid target");
    return;
  }
  if (!ValidateNullableWebGLObject("framebufferRenderbuffer", buffer))
    return;
  if (buffer && (!buffer->HasEverBeenBound())) {
    SynthesizeGLError(GL_INVALID_OPERATION, "framebufferRenderbuffer",
                      "renderbuffer has never been bound");
    return;
  }
  // Don't allow the default framebuffer to be mutated; all current
  // implementations use an FBO internally in place of the default
  // FBO.
  WebGLFramebuffer* framebuffer_binding = GetFramebufferBinding(target);
  if (!framebuffer_binding || !framebuffer_binding->Object()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "framebufferRenderbuffer",
                      "no framebuffer bound");
    return;
  }
  // Don't allow modifications to opaque framebuffer attachements.
  if (framebuffer_binding && framebuffer_binding->Opaque()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "framebufferRenderbuffer",
                      "opaque framebuffer bound");
    return;
  }
  framebuffer_binding->SetAttachmentForBoundFramebuffer(target, attachment,
                                                        buffer);
  ApplyDepthAndStencilTest();
}

void WebGLRenderingContextBase::framebufferTexture2D(GLenum target,
                                                     GLenum attachment,
                                                     GLenum textarget,
                                                     WebGLTexture* texture,
                                                     GLint level) {
  if (isContextLost() || !ValidateFramebufferFuncParameters(
                             "framebufferTexture2D", target, attachment))
    return;
  if (!ValidateNullableWebGLObject("framebufferTexture2D", texture))
    return;
  // TODO(crbug.com/919711): validate texture's target against textarget.

  // Don't allow the default framebuffer to be mutated; all current
  // implementations use an FBO internally in place of the default
  // FBO.
  WebGLFramebuffer* framebuffer_binding = GetFramebufferBinding(target);
  if (!framebuffer_binding || !framebuffer_binding->Object()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "framebufferTexture2D",
                      "no framebuffer bound");
    return;
  }
  // Don't allow modifications to opaque framebuffer attachements.
  if (framebuffer_binding && framebuffer_binding->Opaque()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "framebufferTexture2D",
                      "opaque framebuffer bound");
    return;
  }
  framebuffer_binding->SetAttachmentForBoundFramebuffer(
      target, attachment, textarget, texture, level, 0, 0);
  ApplyDepthAndStencilTest();
}

void WebGLRenderingContextBase::frontFace(GLenum mode) {
  if (isContextLost())
    return;
  ContextGL()->FrontFace(mode);
}

void WebGLRenderingContextBase::generateMipmap(GLenum target) {
  if (isContextLost())
    return;
  if (!ValidateTextureBinding("generateMipmap", target))
    return;
  ContextGL()->GenerateMipmap(target);
}

WebGLActiveInfo* WebGLRenderingContextBase::getActiveAttrib(
    WebGLProgram* program,
    GLuint index) {
  if (!ValidateWebGLProgramOrShader("getActiveAttrib", program))
    return nullptr;
  GLuint program_id = ObjectNonZero(program);
  GLint max_name_length = -1;
  ContextGL()->GetProgramiv(program_id, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                            &max_name_length);
  if (max_name_length < 0)
    return nullptr;
  if (max_name_length == 0) {
    SynthesizeGLError(GL_INVALID_VALUE, "getActiveAttrib",
                      "no active attributes exist");
    return nullptr;
  }
  LChar* name_ptr;
  scoped_refptr<StringImpl> name_impl =
      StringImpl::CreateUninitialized(max_name_length, name_ptr);
  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  ContextGL()->GetActiveAttrib(program_id, index, max_name_length, &length,
                               &size, &type,
                               reinterpret_cast<GLchar*>(name_ptr));
  if (size < 0)
    return nullptr;
  return MakeGarbageCollected<WebGLActiveInfo>(name_impl->Substring(0, length),
                                               type, size);
}

WebGLActiveInfo* WebGLRenderingContextBase::getActiveUniform(
    WebGLProgram* program,
    GLuint index) {
  if (!ValidateWebGLProgramOrShader("getActiveUniform", program))
    return nullptr;
  GLuint program_id = ObjectNonZero(program);
  GLint max_name_length = -1;
  ContextGL()->GetProgramiv(program_id, GL_ACTIVE_UNIFORM_MAX_LENGTH,
                            &max_name_length);
  if (max_name_length < 0)
    return nullptr;
  if (max_name_length == 0) {
    SynthesizeGLError(GL_INVALID_VALUE, "getActiveUniform",
                      "no active uniforms exist");
    return nullptr;
  }
  LChar* name_ptr;
  scoped_refptr<StringImpl> name_impl =
      StringImpl::CreateUninitialized(max_name_length, name_ptr);
  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  ContextGL()->GetActiveUniform(program_id, index, max_name_length, &length,
                                &size, &type,
                                reinterpret_cast<GLchar*>(name_ptr));
  if (size < 0)
    return nullptr;
  return MakeGarbageCollected<WebGLActiveInfo>(name_impl->Substring(0, length),
                                               type, size);
}

std::optional<HeapVector<Member<WebGLShader>>>
WebGLRenderingContextBase::getAttachedShaders(WebGLProgram* program) {
  if (!ValidateWebGLProgramOrShader("getAttachedShaders", program))
    return std::nullopt;

  HeapVector<Member<WebGLShader>> shader_objects;
  for (GLenum shaderType : {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER}) {
    WebGLShader* shader = program->GetAttachedShader(shaderType);
    if (shader)
      shader_objects.push_back(shader);
  }
  return shader_objects;
}

GLint WebGLRenderingContextBase::getAttribLocation(WebGLProgram* program,
                                                   const String& name) {
  if (!ValidateWebGLProgramOrShader("getAttribLocation", program))
    return -1;
  if (!ValidateLocationLength("getAttribLocation", name))
    return -1;
  if (!ValidateString("getAttribLocation", name))
    return -1;
  if (IsPrefixReserved(name))
    return -1;
  if (!program->LinkStatus(this)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "getAttribLocation",
                      "program not linked");
    return 0;
  }
  return ContextGL()->GetAttribLocation(ObjectOrZero(program),
                                        name.Utf8().c_str());
}

bool WebGLRenderingContextBase::ValidateBufferTarget(const char* function_name,
                                                     GLenum target) {
  switch (target) {
    case GL_ARRAY_BUFFER:
    case GL_ELEMENT_ARRAY_BUFFER:
      return true;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid target");
      return false;
  }
}

ScriptValue WebGLRenderingContextBase::getBufferParameter(
    ScriptState* script_state,
    GLenum target,
    GLenum pname) {
  if (isContextLost() || !ValidateBufferTarget("getBufferParameter", target))
    return ScriptValue::CreateNull(script_state->GetIsolate());

  switch (pname) {
    case GL_BUFFER_USAGE: {
      GLint value = 0;
      ContextGL()->GetBufferParameteriv(target, pname, &value);
      return WebGLAny(script_state, static_cast<unsigned>(value));
    }
    case GL_BUFFER_SIZE: {
      GLint value = 0;
      ContextGL()->GetBufferParameteriv(target, pname, &value);
      if (!IsWebGL2())
        return WebGLAny(script_state, value);
      return WebGLAny(script_state, static_cast<GLint64>(value));
    }
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getBufferParameter",
                        "invalid parameter name");
      return ScriptValue::CreateNull(script_state->GetIsolate());
  }
}

WebGLContextAttributes* WebGLRenderingContextBase::getContextAttributes()
    const {
  if (isContextLost())
    return nullptr;

  WebGLContextAttributes* result =
      ToWebGLContextAttributes(CreationAttributes());

  // Some requested attributes may not be honored, so we need to query the
  // underlying context/drawing buffer and adjust accordingly.
  if (CreationAttributes().depth && !GetDrawingBuffer()->HasDepthBuffer())
    result->setDepth(false);
  if (CreationAttributes().stencil && !GetDrawingBuffer()->HasStencilBuffer())
    result->setStencil(false);
  result->setAntialias(GetDrawingBuffer()->Multisample());
  result->setXrCompatible(xr_compatible_);
  result->setDesynchronized(Host()->LowLatencyEnabled());
  return result;
}

GLenum WebGLRenderingContextBase::getError() {
  if (!lost_context_errors_.empty()) {
    GLenum error = lost_context_errors_.front();
    lost_context_errors_.EraseAt(0);
    return error;
  }

  if (isContextLost())
    return GL_NO_ERROR;

  if (!synthetic_errors_.empty()) {
    GLenum error = synthetic_errors_.front();
    synthetic_errors_.EraseAt(0);
    return error;
  }

  return ContextGL()->GetError();
}

bool WebGLRenderingContextBase::ExtensionTracker::MatchesName(
    const String& name) const {
  if (DeprecatedEqualIgnoringCase(ExtensionName(), name)) {
    return true;
  }
  return false;
}

bool WebGLRenderingContextBase::ExtensionSupportedAndAllowed(
    const ExtensionTracker* tracker) {
  if (tracker->Draft() &&
      !RuntimeEnabledFeatures::WebGLDraftExtensionsEnabled())
    return false;
  if (tracker->Developer() &&
      !RuntimeEnabledFeatures::WebGLDeveloperExtensionsEnabled())
    return false;
  if (!tracker->Supported(this))
    return false;
  if (disabled_extensions_.Contains(String(tracker->ExtensionName())))
    return false;
  return true;
}

WebGLExtension* WebGLRenderingContextBase::EnableExtensionIfSupported(
    const String& name) {
  WebGLExtension* extension = nullptr;

  if (!isContextLost()) {
    for (ExtensionTracker* tracker : extensions_) {
      if (tracker->MatchesName(name)) {
        if (ExtensionSupportedAndAllowed(tracker)) {
          extension = tracker->GetExtension(this);
          if (extension) {
            if (!extension_enabled_[extension->GetName()]) {
              extension_enabled_[extension->GetName()] = true;
            }
          }
        }
        break;
      }
    }
  }

  return extension;
}

bool WebGLRenderingContextBase::TimerQueryExtensionsEnabled() {
  return (drawing_buffer_ && drawing_buffer_->ContextProvider() &&
          drawing_buffer_->ContextProvider()
              ->GetGpuFeatureInfo()
              .IsWorkaroundEnabled(gpu::ENABLE_WEBGL_TIMER_QUERY_EXTENSIONS));
}

ScriptValue WebGLRenderingContextBase::getExtension(ScriptState* script_state,
                                                    const String& name) {
  if (name == WebGLDebugRendererInfo::ExtensionName()) {
    ExecutionContext* context = ExecutionContext::From(script_state);
    UseCounter::Count(context, WebFeature::kWebGLDebugRendererInfo);
  }

  WebGLExtension* extension = EnableExtensionIfSupported(name);
  return ScriptValue(
      script_state->GetIsolate(),
      ToV8Traits<IDLNullable<WebGLExtension>>::ToV8(script_state, extension));
}

ScriptValue WebGLRenderingContextBase::getFramebufferAttachmentParameter(
    ScriptState* script_state,
    GLenum target,
    GLenum attachment,
    GLenum pname) {
  const char kFunctionName[] = "getFramebufferAttachmentParameter";
  if (isContextLost() ||
      !ValidateFramebufferFuncParameters(kFunctionName, target, attachment)) {
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  if (!framebuffer_binding_ || !framebuffer_binding_->Object()) {
    SynthesizeGLError(GL_INVALID_OPERATION, kFunctionName,
                      "no framebuffer bound");
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  if (framebuffer_binding_ && framebuffer_binding_->Opaque()) {
    SynthesizeGLError(GL_INVALID_OPERATION, kFunctionName,
                      "cannot query parameters of an opaque framebuffer");
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  WebGLSharedObject* attachment_object =
      framebuffer_binding_->GetAttachmentObject(attachment);
  if (!attachment_object) {
    if (pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
      return WebGLAny(script_state, GL_NONE);
    // OpenGL ES 2.0 specifies INVALID_ENUM in this case, while desktop GL
    // specifies INVALID_OPERATION.
    SynthesizeGLError(GL_INVALID_ENUM, kFunctionName, "invalid parameter name");
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  DCHECK(attachment_object->IsTexture() || attachment_object->IsRenderbuffer());
  switch (pname) {
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
      if (attachment_object->IsTexture()) {
        return WebGLAny(script_state, GL_TEXTURE);
      }
      return WebGLAny(script_state, GL_RENDERBUFFER);
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
      return WebGLAny(script_state, attachment_object);
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
      if (attachment_object->IsTexture()) {
        GLint value = 0;
        ContextGL()->GetFramebufferAttachmentParameteriv(target, attachment,
                                                         pname, &value);
        return WebGLAny(script_state, value);
      }
      break;
    case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT:
      if (ExtensionEnabled(kEXTsRGBName)) {
        GLint value = 0;
        ContextGL()->GetFramebufferAttachmentParameteriv(target, attachment,
                                                         pname, &value);
        return WebGLAny(script_state, static_cast<unsigned>(value));
      }
      SynthesizeGLError(GL_INVALID_ENUM, kFunctionName,
                        "invalid parameter name, EXT_sRGB not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE_EXT:
      if (ExtensionEnabled(kEXTColorBufferHalfFloatName) ||
          ExtensionEnabled(kWebGLColorBufferFloatName)) {
        if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
          SynthesizeGLError(
              GL_INVALID_OPERATION, kFunctionName,
              "component type cannot be queried for DEPTH_STENCIL_ATTACHMENT");
          return ScriptValue::CreateNull(script_state->GetIsolate());
        }
        GLint value = 0;
        ContextGL()->GetFramebufferAttachmentParameteriv(target, attachment,
                                                         pname, &value);
        return WebGLAny(script_state, static_cast<unsigned>(value));
      }
      SynthesizeGLError(
          GL_INVALID_ENUM, kFunctionName,
          "invalid parameter name, EXT_color_buffer_half_float or "
          "WEBGL_color_buffer_float not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
  }
  SynthesizeGLError(GL_INVALID_ENUM, kFunctionName, "invalid parameter name");
  return ScriptValue::CreateNull(script_state->GetIsolate());
}

namespace {

// WebGL parameters which can be used to identify users.
// These parameters should each be uniquely defined,
// see third_party/khronos/GLES2/gl2.h for their definitions.
static const GLenum kIdentifiableGLParams[] = {
    // getParameter()
    GL_ALIASED_LINE_WIDTH_RANGE,          // GetWebGLFloatArrayParameter
    GL_ALIASED_POINT_SIZE_RANGE,          // GetWebGLFloatArrayParameter
    GL_ALPHA_BITS,                        // GetIntParameter
    GL_BLUE_BITS,                         // GetIntParameter
    GL_DEPTH_BITS,                        // GetIntParameter
    GL_GREEN_BITS,                        // GetIntParameter
    GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,  // GetIntParameter
    GL_MAX_CUBE_MAP_TEXTURE_SIZE,         // GetIntParameter
    GL_MAX_FRAGMENT_UNIFORM_VECTORS,      // GetIntParameter
    GL_MAX_RENDERBUFFER_SIZE,             // GetIntParameter
    GL_MAX_TEXTURE_IMAGE_UNITS,           // GetIntParameter
    GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,    // GetFloatParameter
    GL_MAX_TEXTURE_SIZE,                  // GetIntParameter
    GL_MAX_VARYING_VECTORS,               // GetIntParameter
    GL_MAX_VERTEX_ATTRIBS,                // GetIntParameter
    GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,    // GetIntParameter
    GL_MAX_VERTEX_UNIFORM_VECTORS,        // GetIntParameter
    GL_MAX_VIEWPORT_DIMS,                 // GetWebGLIntArrayParameter
    GL_RED_BITS,                          // GetIntParameter
    GL_SHADING_LANGUAGE_VERSION,
    GL_STENCIL_BITS,  // GetIntParameter
    GL_VERSION,
    WebGLDebugRendererInfo::kUnmaskedRendererWebgl,
    WebGLDebugRendererInfo::kUnmaskedVendorWebgl,

    // getRenderBufferParameter()
    GL_RENDERBUFFER_GREEN_SIZE,
    GL_RENDERBUFFER_BLUE_SIZE,
    GL_RENDERBUFFER_RED_SIZE,
    GL_RENDERBUFFER_ALPHA_SIZE,
    GL_RENDERBUFFER_DEPTH_SIZE,
    GL_RENDERBUFFER_STENCIL_SIZE,
    GL_RENDERBUFFER_SAMPLES,
};

bool ShouldMeasureGLParam(GLenum pname) {
  return IdentifiabilityStudySettings::Get()->ShouldSampleType(
             blink::IdentifiableSurface::Type::kWebGLParameter) &&
         base::Contains(kIdentifiableGLParams, pname);
}

}  // namespace

void WebGLRenderingContextBase::RecordIdentifiableGLParameterDigest(
    GLenum pname,
    IdentifiableToken value) {
  DCHECK(IdentifiabilityStudySettings::Get()->ShouldSampleType(
      blink::IdentifiableSurface::Type::kWebGLParameter));
  const auto ukm_params = GetUkmParameters();
  blink::IdentifiabilityMetricBuilder(ukm_params.source_id)
      .Add(blink::IdentifiableSurface::FromTypeAndToken(
               blink::IdentifiableSurface::Type::kWebGLParameter, pname),
           value)
      .Record(ukm_params.ukm_recorder);
}

void WebGLRenderingContextBase::RecordShaderPrecisionFormatForStudy(
    GLenum shader_type,
    GLenum precision_type,
    WebGLShaderPrecisionFormat* format) {
  DCHECK(IdentifiabilityStudySettings::Get()->ShouldSampleType(
      blink::IdentifiableSurface::Type::kWebGLShaderPrecisionFormat));

  const auto& ukm_params = GetUkmParameters();
  IdentifiableTokenBuilder builder;
  auto surface_token =
      builder.AddValue(shader_type).AddValue(precision_type).GetToken();
  auto sample_token = builder.AddValue(format->rangeMin())
                          .AddValue(format->rangeMax())
                          .AddValue(format->precision())
                          .GetToken();

  blink::IdentifiabilityMetricBuilder(ukm_params.source_id)
      .Add(blink::IdentifiableSurface::FromTypeAndToken(
               blink::IdentifiableSurface::Type::kWebGLShaderPrecisionFormat,
               surface_token),
           sample_token)
      .Record(ukm_params.ukm_recorder);
}

void WebGLRenderingContextBase::RecordANGLEImplementation() {
  DCHECK(drawing_buffer_.get());
  const Platform::GraphicsInfo& graphics_info =
      drawing_buffer_->GetGraphicsInfo();
  // For mapping mathematics, see WebGLANGLEImplementation definition above.
  int webgl_version_multiplier =
      (context_type_ == Platform::kWebGL2ContextType ? 2 : 0);
  WebGLANGLEImplementation webgl_angle_implementation =
      static_cast<WebGLANGLEImplementation>(
          webgl_version_multiplier * 10 +
          static_cast<int>(graphics_info.angle_implementation));
  UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.WebGLANGLEImplementation",
                            webgl_angle_implementation);
}

ScriptValue WebGLRenderingContextBase::getParameter(ScriptState* script_state,
                                                    GLenum pname) {
  if (isContextLost())
    return ScriptValue::CreateNull(script_state->GetIsolate());
  const int kIntZero = 0;
  switch (pname) {
    case GL_ACTIVE_TEXTURE:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_ALIASED_LINE_WIDTH_RANGE:
      return GetWebGLFloatArrayParameter(script_state, pname);
    case GL_ALIASED_POINT_SIZE_RANGE:
      return GetWebGLFloatArrayParameter(script_state, pname);
    case GL_ALPHA_BITS:
      if (drawing_buffer_->RequiresAlphaChannelToBePreserved())
        return WebGLAny(script_state, 0);
      return GetIntParameter(script_state, pname);
    case GL_ARRAY_BUFFER_BINDING:
      return WebGLAny(script_state, bound_array_buffer_.Get());
    case GL_BLEND:
      return GetBooleanParameter(script_state, pname);
    case GL_BLEND_COLOR:
      return GetWebGLFloatArrayParameter(script_state, pname);
    case GL_BLEND_DST_ALPHA:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_BLEND_DST_RGB:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_BLEND_EQUATION_ALPHA:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_BLEND_EQUATION_RGB:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_BLEND_SRC_ALPHA:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_BLEND_SRC_RGB:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_BLUE_BITS:
      return GetIntParameter(script_state, pname);
    case GL_COLOR_CLEAR_VALUE:
      return GetWebGLFloatArrayParameter(script_state, pname);
    case GL_COLOR_WRITEMASK:
      return GetBooleanArrayParameter(script_state, pname);
    case GL_COMPRESSED_TEXTURE_FORMATS:
      return WebGLAny(script_state,
                      DOMUint32Array::Create(compressed_texture_formats_));
    case GL_CULL_FACE:
      return GetBooleanParameter(script_state, pname);
    case GL_CULL_FACE_MODE:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_CURRENT_PROGRAM:
      return WebGLAny(script_state, current_program_.Get());
    case GL_DEPTH_BITS:
      if (!framebuffer_binding_ && !CreationAttributes().depth)
        return WebGLAny(script_state, kIntZero);
      return GetIntParameter(script_state, pname);
    case GL_DEPTH_CLEAR_VALUE:
      return GetFloatParameter(script_state, pname);
    case GL_DEPTH_FUNC:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_DEPTH_RANGE:
      return GetWebGLFloatArrayParameter(script_state, pname);
    case GL_DEPTH_TEST:
      return WebGLAny(script_state, depth_enabled_);
    case GL_DEPTH_WRITEMASK:
      return GetBooleanParameter(script_state, pname);
    case GL_DITHER:
      return GetBooleanParameter(script_state, pname);
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
      return WebGLAny(script_state,
                      bound_vertex_array_object_->BoundElementArrayBuffer());
    case GL_FRAMEBUFFER_BINDING:
      return WebGLAny(script_state, framebuffer_binding_.Get());
    case GL_FRONT_FACE:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_GENERATE_MIPMAP_HINT:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_GREEN_BITS:
      return GetIntParameter(script_state, pname);
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
      return GetIntParameter(script_state, pname);
    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      return GetIntParameter(script_state, pname);
    case GL_LINE_WIDTH:
      return GetFloatParameter(script_state, pname);
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      return GetIntParameter(script_state, pname);
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_RENDERBUFFER_SIZE:
      return GetIntParameter(script_state, pname);
    case GL_MAX_TEXTURE_IMAGE_UNITS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_TEXTURE_SIZE:
      return GetIntParameter(script_state, pname);
    case GL_MAX_VARYING_VECTORS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_VERTEX_ATTRIBS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_VIEWPORT_DIMS:
      return GetWebGLIntArrayParameter(script_state, pname);
    case GL_NUM_SHADER_BINARY_FORMATS:
      // FIXME: should we always return 0 for this?
      return GetIntParameter(script_state, pname);
    case GL_PACK_ALIGNMENT:
      return GetIntParameter(script_state, pname);
    case GL_POLYGON_OFFSET_FACTOR:
      return GetFloatParameter(script_state, pname);
    case GL_POLYGON_OFFSET_FILL:
      return GetBooleanParameter(script_state, pname);
    case GL_POLYGON_OFFSET_UNITS:
      return GetFloatParameter(script_state, pname);
    case GL_RED_BITS:
      return GetIntParameter(script_state, pname);
    case GL_RENDERBUFFER_BINDING:
      return WebGLAny(script_state, renderbuffer_binding_.Get());
    case GL_RENDERER:
      return WebGLAny(script_state, String("WebKit WebGL"));
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
      return GetBooleanParameter(script_state, pname);
    case GL_SAMPLE_BUFFERS:
      return GetIntParameter(script_state, pname);
    case GL_SAMPLE_COVERAGE:
      return GetBooleanParameter(script_state, pname);
    case GL_SAMPLE_COVERAGE_INVERT:
      return GetBooleanParameter(script_state, pname);
    case GL_SAMPLE_COVERAGE_VALUE:
      return GetFloatParameter(script_state, pname);
    case GL_SAMPLES:
      return GetIntParameter(script_state, pname);
    case GL_SCISSOR_BOX:
      return GetWebGLIntArrayParameter(script_state, pname);
    case GL_SCISSOR_TEST:
      return GetBooleanParameter(script_state, pname);
    case GL_SHADING_LANGUAGE_VERSION:
      if (IdentifiabilityStudySettings::Get()->ShouldSampleType(
              blink::IdentifiableSurface::Type::kWebGLParameter)) {
        RecordIdentifiableGLParameterDigest(
            pname, IdentifiabilityBenignStringToken(String(
                       ContextGL()->GetString(GL_SHADING_LANGUAGE_VERSION))));
      }
      return WebGLAny(
          script_state,
          "WebGL GLSL ES 1.0 (" +
              String(ContextGL()->GetString(GL_SHADING_LANGUAGE_VERSION)) +
              ")");
    case GL_STENCIL_BACK_FAIL:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_BACK_FUNC:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_BACK_PASS_DEPTH_PASS:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_BACK_REF:
      return GetIntParameter(script_state, pname);
    case GL_STENCIL_BACK_VALUE_MASK:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_BACK_WRITEMASK:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_BITS:
      if (!framebuffer_binding_ && !CreationAttributes().stencil)
        return WebGLAny(script_state, kIntZero);
      return GetIntParameter(script_state, pname);
    case GL_STENCIL_CLEAR_VALUE:
      return GetIntParameter(script_state, pname);
    case GL_STENCIL_FAIL:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_FUNC:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_PASS_DEPTH_FAIL:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_PASS_DEPTH_PASS:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_REF:
      return GetIntParameter(script_state, pname);
    case GL_STENCIL_TEST:
      return WebGLAny(script_state, stencil_enabled_);
    case GL_STENCIL_VALUE_MASK:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_STENCIL_WRITEMASK:
      return GetUnsignedIntParameter(script_state, pname);
    case GL_SUBPIXEL_BITS:
      return GetIntParameter(script_state, pname);
    case GL_TEXTURE_BINDING_2D:
      return WebGLAny(
          script_state,
          texture_units_[active_texture_unit_].texture2d_binding_.Get());
    case GL_TEXTURE_BINDING_CUBE_MAP:
      return WebGLAny(
          script_state,
          texture_units_[active_texture_unit_].texture_cube_map_binding_.Get());
    case GL_UNPACK_ALIGNMENT:
      return GetIntParameter(script_state, pname);
    case GC3D_UNPACK_FLIP_Y_WEBGL:
      return WebGLAny(script_state, unpack_flip_y_);
    case GC3D_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
      return WebGLAny(script_state, unpack_premultiply_alpha_);
    case GC3D_UNPACK_COLORSPACE_CONVERSION_WEBGL:
      return WebGLAny(script_state, unpack_colorspace_conversion_);
    case GL_VENDOR:
      return WebGLAny(script_state, String("WebKit"));
    case GL_VERSION:
      if (IdentifiabilityStudySettings::Get()->ShouldSampleType(
              blink::IdentifiableSurface::Type::kWebGLParameter)) {
        RecordIdentifiableGLParameterDigest(
            pname, IdentifiabilityBenignStringToken(
                       String(ContextGL()->GetString(GL_VERSION))));
      }
      return WebGLAny(
          script_state,
          "WebGL 1.0 (" + String(ContextGL()->GetString(GL_VERSION)) + ")");
    case GL_VIEWPORT:
      return GetWebGLIntArrayParameter(script_state, pname);
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:  // OES_standard_derivatives
      if (ExtensionEnabled(kOESStandardDerivativesName) || IsWebGL2())
        return GetUnsignedIntParameter(script_state,
                                       GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, OES_standard_derivatives not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    case WebGLDebugRendererInfo::kUnmaskedRendererWebgl:
      if (ExtensionEnabled(kWebGLDebugRendererInfoName)) {
        if (IdentifiabilityStudySettings::Get()->ShouldSampleType(
                blink::IdentifiableSurface::Type::kWebGLParameter)) {
          RecordIdentifiableGLParameterDigest(
              pname, IdentifiabilityBenignStringToken(
                         String(ContextGL()->GetString(GL_RENDERER))));
        }
        return WebGLAny(script_state,
                        String(ContextGL()->GetString(GL_RENDERER)));
      }
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, WEBGL_debug_renderer_info not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    case WebGLDebugRendererInfo::kUnmaskedVendorWebgl:
      if (ExtensionEnabled(kWebGLDebugRendererInfoName)) {
        if (IdentifiabilityStudySettings::Get()->ShouldSampleType(
                blink::IdentifiableSurface::Type::kWebGLParameter)) {
          RecordIdentifiableGLParameterDigest(
              pname, IdentifiabilityBenignStringToken(
                         String(ContextGL()->GetString(GL_VENDOR))));
        }
        return WebGLAny(script_state,
                        String(ContextGL()->GetString(GL_VENDOR)));
      }
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, WEBGL_debug_renderer_info not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    case GL_VERTEX_ARRAY_BINDING_OES:  // OES_vertex_array_object
      if (ExtensionEnabled(kOESVertexArrayObjectName) || IsWebGL2()) {
        if (!bound_vertex_array_object_->IsDefaultObject())
          return WebGLAny(script_state, bound_vertex_array_object_.Get());
        return ScriptValue::CreateNull(script_state->GetIsolate());
      }
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, OES_vertex_array_object not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:  // EXT_texture_filter_anisotropic
      if (ExtensionEnabled(kEXTTextureFilterAnisotropicName)) {
        return GetFloatParameter(script_state,
                                 GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT);
      }
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, EXT_texture_filter_anisotropic not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    case GL_DEPTH_CLAMP_EXT:  // EXT_depth_clamp
      if (ExtensionEnabled(kEXTDepthClampName)) {
        return GetBooleanParameter(script_state, pname);
      }
      SynthesizeGLError(GL_INVALID_ENUM, "getParameter",
                        "invalid parameter name, EXT_depth_clamp not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    case GL_POLYGON_MODE_ANGLE:  // WEBGL_polygon_mode
    case GL_POLYGON_OFFSET_LINE_ANGLE:
      if (ExtensionEnabled(kWebGLPolygonModeName)) {
        if (pname == GL_POLYGON_OFFSET_LINE_ANGLE) {
          return GetBooleanParameter(script_state, pname);
        }
        return GetUnsignedIntParameter(script_state, pname);
      }
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, WEBGL_polygon_mode not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    case GL_POLYGON_OFFSET_CLAMP_EXT:  // EXT_polygon_offset_clamp
      if (ExtensionEnabled(kEXTPolygonOffsetClampName)) {
        return GetFloatParameter(script_state, pname);
      }
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, EXT_polygon_offset_clamp not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    case GL_CLIP_ORIGIN_EXT:  // EXT_clip_control
    case GL_CLIP_DEPTH_MODE_EXT:
      if (ExtensionEnabled(kEXTClipControlName)) {
        return GetUnsignedIntParameter(script_state, pname);
      }
      SynthesizeGLError(GL_INVALID_ENUM, "getParameter",
                        "invalid parameter name, EXT_clip_control not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    case GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT:  // WEBGL_blend_func_extended
      if (ExtensionEnabled(kWebGLBlendFuncExtendedName)) {
        return GetUnsignedIntParameter(script_state, pname);
      }
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, WEBGL_blend_func_extended not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    case GL_MAX_COLOR_ATTACHMENTS_EXT:  // EXT_draw_buffers BEGIN
      if (ExtensionEnabled(kWebGLDrawBuffersName) || IsWebGL2())
        return WebGLAny(script_state, MaxColorAttachments());
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, WEBGL_draw_buffers not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    case GL_MAX_DRAW_BUFFERS_EXT:
      if (ExtensionEnabled(kWebGLDrawBuffersName) || IsWebGL2())
        return WebGLAny(script_state, MaxDrawBuffers());
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, WEBGL_draw_buffers not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    case GL_TIMESTAMP_EXT:
      if (ExtensionEnabled(kEXTDisjointTimerQueryName))
        return WebGLAny(script_state, 0);
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, EXT_disjoint_timer_query not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    case GL_GPU_DISJOINT_EXT:
      if (ExtensionEnabled(kEXTDisjointTimerQueryName))
        return GetBooleanParameter(script_state, GL_GPU_DISJOINT_EXT);
      SynthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, EXT_disjoint_timer_query not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    case GL_MAX_VIEWS_OVR:
      if (ExtensionEnabled(kOVRMultiview2Name))
        return GetIntParameter(script_state, pname);
      SynthesizeGLError(GL_INVALID_ENUM, "getParameter",
                        "invalid parameter name, OVR_multiview2 not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    default:
      if ((ExtensionEnabled(kWebGLDrawBuffersName) || IsWebGL2()) &&
          pname >= GL_DRAW_BUFFER0_EXT &&
          pname < static_cast<GLenum>(GL_DRAW_BUFFER0_EXT + MaxDrawBuffers())) {
        GLint value = GL_NONE;
        if (framebuffer_binding_)
          value = framebuffer_binding_->GetDrawBuffer(pname);
        else  // emulated backbuffer
          value = back_draw_buffer_;
        return WebGLAny(script_state, value);
      }
      SynthesizeGLError(GL_INVALID_ENUM, "getParameter",
                        "invalid parameter name");
      return ScriptValue::CreateNull(script_state->GetIsolate());
  }
}

ScriptValue WebGLRenderingContextBase::getProgramParameter(
    ScriptState* script_state,
    WebGLProgram* program,
    GLenum pname) {
  // Completion status queries always return true on a lost context. This is
  // intended to prevent applications from entering an infinite polling loop.
  if (isContextLost() && pname == GL_COMPLETION_STATUS_KHR)
    return WebGLAny(script_state, true);
  if (!ValidateWebGLProgramOrShader("getProgramParamter", program)) {
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  GLint value = 0;
  switch (pname) {
    case GL_DELETE_STATUS:
      return WebGLAny(script_state, program->MarkedForDeletion());
    case GL_VALIDATE_STATUS:
      ContextGL()->GetProgramiv(ObjectOrZero(program), pname, &value);
      return WebGLAny(script_state, static_cast<bool>(value));
    case GL_LINK_STATUS:
      return WebGLAny(script_state, program->LinkStatus(this));
    case GL_COMPLETION_STATUS_KHR:
      if (!ExtensionEnabled(kKHRParallelShaderCompileName)) {
        SynthesizeGLError(GL_INVALID_ENUM, "getProgramParameter",
                          "invalid parameter name");
        return ScriptValue::CreateNull(script_state->GetIsolate());
      }
      bool completed;
      if (checkProgramCompletionQueryAvailable(program, &completed)) {
        return WebGLAny(script_state, completed);
      }
      return WebGLAny(script_state, program->CompletionStatus(this));
    case GL_ACTIVE_UNIFORM_BLOCKS:
    case GL_TRANSFORM_FEEDBACK_VARYINGS:
      if (!IsWebGL2()) {
        SynthesizeGLError(GL_INVALID_ENUM, "getProgramParameter",
                          "invalid parameter name");
        return ScriptValue::CreateNull(script_state->GetIsolate());
      }
      [[fallthrough]];
    case GL_ATTACHED_SHADERS:
    case GL_ACTIVE_ATTRIBUTES:
    case GL_ACTIVE_UNIFORMS:
      ContextGL()->GetProgramiv(ObjectOrZero(program), pname, &value);
      return WebGLAny(script_state, value);
    case GL_TRANSFORM_FEEDBACK_BUFFER_MODE:
      if (!IsWebGL2()) {
        SynthesizeGLError(GL_INVALID_ENUM, "getProgramParameter",
                          "invalid parameter name");
        return ScriptValue::CreateNull(script_state->GetIsolate());
      }
      ContextGL()->GetProgramiv(ObjectOrZero(program), pname, &value);
      return WebGLAny(script_state, static_cast<unsigned>(value));
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getProgramParameter",
                        "invalid parameter name");
      return ScriptValue::CreateNull(script_state->GetIsolate());
  }
}

String WebGLRenderingContextBase::getProgramInfoLog(WebGLProgram* program) {
  if (!ValidateWebGLProgramOrShader("getProgramInfoLog", program))
    return String();
  GLStringQuery query(ContextGL());
  return query.Run<GLStringQuery::ProgramInfoLog>(ObjectNonZero(program));
}

ScriptValue WebGLRenderingContextBase::getRenderbufferParameter(
    ScriptState* script_state,
    GLenum target,
    GLenum pname) {
  if (isContextLost())
    return ScriptValue::CreateNull(script_state->GetIsolate());
  if (target != GL_RENDERBUFFER) {
    SynthesizeGLError(GL_INVALID_ENUM, "getRenderbufferParameter",
                      "invalid target");
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }
  if (!renderbuffer_binding_ || !renderbuffer_binding_->Object()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "getRenderbufferParameter",
                      "no renderbuffer bound");
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  GLint value = 0;
  switch (pname) {
    case GL_RENDERBUFFER_SAMPLES:
      if (!IsWebGL2()) {
        SynthesizeGLError(GL_INVALID_ENUM, "getRenderbufferParameter",
                          "invalid parameter name");
        return ScriptValue::CreateNull(script_state->GetIsolate());
      }
      [[fallthrough]];
    case GL_RENDERBUFFER_WIDTH:
    case GL_RENDERBUFFER_HEIGHT:
    case GL_RENDERBUFFER_RED_SIZE:
    case GL_RENDERBUFFER_GREEN_SIZE:
    case GL_RENDERBUFFER_BLUE_SIZE:
    case GL_RENDERBUFFER_ALPHA_SIZE:
    case GL_RENDERBUFFER_DEPTH_SIZE:
    case GL_RENDERBUFFER_STENCIL_SIZE:
      ContextGL()->GetRenderbufferParameteriv(target, pname, &value);
      if (IdentifiabilityStudySettings::Get()->ShouldSampleType(
              blink::IdentifiableSurface::Type::kWebGLParameter)) {
        RecordIdentifiableGLParameterDigest(pname, value);
      }
      return WebGLAny(script_state, value);
    case GL_RENDERBUFFER_INTERNAL_FORMAT:
      return WebGLAny(script_state, renderbuffer_binding_->InternalFormat());
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getRenderbufferParameter",
                        "invalid parameter name");
      return ScriptValue::CreateNull(script_state->GetIsolate());
  }
}

ScriptValue WebGLRenderingContextBase::getShaderParameter(
    ScriptState* script_state,
    WebGLShader* shader,
    GLenum pname) {
  // Completion status queries always return true on a lost context. This is
  // intended to prevent applications from entering an infinite polling loop.
  if (isContextLost() && pname == GL_COMPLETION_STATUS_KHR)
    return WebGLAny(script_state, true);
  if (!ValidateWebGLProgramOrShader("getShaderParameter", shader)) {
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }
  GLint value = 0;
  switch (pname) {
    case GL_DELETE_STATUS:
      return WebGLAny(script_state, shader->MarkedForDeletion());
    case GL_COMPILE_STATUS:
      ContextGL()->GetShaderiv(ObjectOrZero(shader), pname, &value);
      return WebGLAny(script_state, static_cast<bool>(value));
    case GL_COMPLETION_STATUS_KHR:
      if (!ExtensionEnabled(kKHRParallelShaderCompileName)) {
        SynthesizeGLError(GL_INVALID_ENUM, "getShaderParameter",
                          "invalid parameter name");
        return ScriptValue::CreateNull(script_state->GetIsolate());
      }
      ContextGL()->GetShaderiv(ObjectOrZero(shader), pname, &value);
      return WebGLAny(script_state, static_cast<bool>(value));
    case GL_SHADER_TYPE:
      ContextGL()->GetShaderiv(ObjectOrZero(shader), pname, &value);
      return WebGLAny(script_state, static_cast<unsigned>(value));
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getShaderParameter",
                        "invalid parameter name");
      return ScriptValue::CreateNull(script_state->GetIsolate());
  }
}

String WebGLRenderingContextBase::getShaderInfoLog(WebGLShader* shader) {
  if (!ValidateWebGLProgramOrShader("getShaderInfoLog", shader))
    return String();
  GLStringQuery query(ContextGL());
  return query.Run<GLStringQuery::ShaderInfoLog>(ObjectNonZero(shader));
}

WebGLShaderPrecisionFormat* WebGLRenderingContextBase::getShaderPrecisionFormat(
    GLenum shader_type,
    GLenum precision_type) {
  if (isContextLost())
    return nullptr;
  if (!ValidateShaderType("getShaderPrecisionFormat", shader_type)) {
    return nullptr;
  }
  switch (precision_type) {
    case GL_LOW_FLOAT:
    case GL_MEDIUM_FLOAT:
    case GL_HIGH_FLOAT:
    case GL_LOW_INT:
    case GL_MEDIUM_INT:
    case GL_HIGH_INT:
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getShaderPrecisionFormat",
                        "invalid precision type");
      return nullptr;
  }

  GLint range[2] = {0, 0};
  GLint precision = 0;
  ContextGL()->GetShaderPrecisionFormat(shader_type, precision_type, range,
                                        &precision);
  auto* result = MakeGarbageCollected<WebGLShaderPrecisionFormat>(
      range[0], range[1], precision);
  if (IdentifiabilityStudySettings::Get()->ShouldSampleType(
          blink::IdentifiableSurface::Type::kWebGLShaderPrecisionFormat)) {
    RecordShaderPrecisionFormatForStudy(shader_type, precision_type, result);
  }
  return result;
}

String WebGLRenderingContextBase::getShaderSource(WebGLShader* shader) {
  if (!ValidateWebGLProgramOrShader("getShaderSource", shader))
    return String();
  return EnsureNotNull(shader->Source());
}

std::optional<Vector<String>>
WebGLRenderingContextBase::getSupportedExtensions() {
  if (isContextLost())
    return std::nullopt;

  Vector<String> result;

  for (ExtensionTracker* tracker : extensions_) {
    if (ExtensionSupportedAndAllowed(tracker)) {
      result.push_back(tracker->ExtensionName());
    }
  }

  return result;
}

ScriptValue WebGLRenderingContextBase::getTexParameter(
    ScriptState* script_state,
    GLenum target,
    GLenum pname) {
  if (isContextLost())
    return ScriptValue::CreateNull(script_state->GetIsolate());
  if (!ValidateTextureBinding("getTexParameter", target))
    return ScriptValue::CreateNull(script_state->GetIsolate());
  switch (pname) {
    case GL_TEXTURE_MAG_FILTER:
    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T: {
      GLint value = 0;
      ContextGL()->GetTexParameteriv(target, pname, &value);
      return WebGLAny(script_state, static_cast<unsigned>(value));
    }
    case GL_TEXTURE_MAX_ANISOTROPY_EXT:  // EXT_texture_filter_anisotropic
      if (ExtensionEnabled(kEXTTextureFilterAnisotropicName)) {
        GLfloat value = 0.f;
        ContextGL()->GetTexParameterfv(target, pname, &value);
        return WebGLAny(script_state, value);
      }
      SynthesizeGLError(
          GL_INVALID_ENUM, "getTexParameter",
          "invalid parameter name, EXT_texture_filter_anisotropic not enabled");
      return ScriptValue::CreateNull(script_state->GetIsolate());
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getTexParameter",
                        "invalid parameter name");
      return ScriptValue::CreateNull(script_state->GetIsolate());
  }
}

ScriptValue WebGLRenderingContextBase::getUniform(
    ScriptState* script_state,
    WebGLProgram* program,
    const WebGLUniformLocation* uniform_location) {
  if (!ValidateWebGLProgramOrShader("getUniform", program))
    return ScriptValue::CreateNull(script_state->GetIsolate());
  DCHECK(uniform_location);
  if (!ValidateUniformLocation("getUniform", uniform_location, program)) {
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }
  GLint location = uniform_location->Location();

  GLuint program_id = ObjectNonZero(program);
  GLint max_name_length = -1;
  ContextGL()->GetProgramiv(program_id, GL_ACTIVE_UNIFORM_MAX_LENGTH,
                            &max_name_length);
  if (max_name_length < 0)
    return ScriptValue::CreateNull(script_state->GetIsolate());
  if (max_name_length == 0) {
    SynthesizeGLError(GL_INVALID_VALUE, "getUniform",
                      "no active uniforms exist");
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  // FIXME: make this more efficient using WebGLUniformLocation and caching
  // types in it.
  GLint active_uniforms = 0;
  ContextGL()->GetProgramiv(program_id, GL_ACTIVE_UNIFORMS, &active_uniforms);
  for (GLint i = 0; i < active_uniforms; i++) {
    LChar* name_ptr;
    scoped_refptr<StringImpl> name_impl =
        StringImpl::CreateUninitialized(max_name_length, name_ptr);
    GLsizei name_length = 0;
    GLint size = -1;
    GLenum type = 0;
    ContextGL()->GetActiveUniform(program_id, i, max_name_length, &name_length,
                                  &size, &type,
                                  reinterpret_cast<GLchar*>(name_ptr));
    if (size < 0)
      return ScriptValue::CreateNull(script_state->GetIsolate());
    String name(name_impl->Substring(0, name_length));
    StringBuilder name_builder;
    // Strip "[0]" from the name if it's an array.
    if (size > 1 && name.EndsWith("[0]"))
      name = name.Left(name.length() - 3);
    // If it's an array, we need to iterate through each element, appending
    // "[index]" to the name.
    for (GLint index = 0; index < size; ++index) {
      name_builder.Clear();
      name_builder.Append(name);
      if (size > 1 && index >= 1) {
        name_builder.Append('[');
        name_builder.AppendNumber(index);
        name_builder.Append(']');
      }
      // Now need to look this up by name again to find its location
      GLint loc = ContextGL()->GetUniformLocation(
          ObjectOrZero(program), name_builder.ToString().Utf8().c_str());
      if (loc == location) {
        // Found it. Use the type in the ActiveInfo to determine the return
        // type.
        GLenum base_type;
        unsigned length;
        switch (type) {
          case GL_BOOL:
            base_type = GL_BOOL;
            length = 1;
            break;
          case GL_BOOL_VEC2:
            base_type = GL_BOOL;
            length = 2;
            break;
          case GL_BOOL_VEC3:
            base_type = GL_BOOL;
            length = 3;
            break;
          case GL_BOOL_VEC4:
            base_type = GL_BOOL;
            length = 4;
            break;
          case GL_INT:
            base_type = GL_INT;
            length = 1;
            break;
          case GL_INT_VEC2:
            base_type = GL_INT;
            length = 2;
            break;
          case GL_INT_VEC3:
            base_type = GL_INT;
            length = 3;
            break;
          case GL_INT_VEC4:
            base_type = GL_INT;
            length = 4;
            break;
          case GL_FLOAT:
            base_type = GL_FLOAT;
            length = 1;
            break;
          case GL_FLOAT_VEC2:
            base_type = GL_FLOAT;
            length = 2;
            break;
          case GL_FLOAT_VEC3:
            base_type = GL_FLOAT;
            length = 3;
            break;
          case GL_FLOAT_VEC4:
            base_type = GL_FLOAT;
            length = 4;
            break;
          case GL_FLOAT_MAT2:
            base_type = GL_FLOAT;
            length = 4;
            break;
          case GL_FLOAT_MAT3:
            base_type = GL_FLOAT;
            length = 9;
            break;
          case GL_FLOAT_MAT4:
            base_type = GL_FLOAT;
            length = 16;
            break;
          case GL_SAMPLER_2D:
          case GL_SAMPLER_CUBE:
            base_type = GL_INT;
            length = 1;
            break;
          default:
            if (!IsWebGL2()) {
              // Can't handle this type
              SynthesizeGLError(GL_INVALID_VALUE, "getUniform",
                                "unhandled type");
              return ScriptValue::CreateNull(script_state->GetIsolate());
            }
            // handle GLenums for WebGL 2.0 or higher
            switch (type) {
              case GL_UNSIGNED_INT:
                base_type = GL_UNSIGNED_INT;
                length = 1;
                break;
              case GL_UNSIGNED_INT_VEC2:
                base_type = GL_UNSIGNED_INT;
                length = 2;
                break;
              case GL_UNSIGNED_INT_VEC3:
                base_type = GL_UNSIGNED_INT;
                length = 3;
                break;
              case GL_UNSIGNED_INT_VEC4:
                base_type = GL_UNSIGNED_INT;
                length = 4;
                break;
              case GL_FLOAT_MAT2x3:
                base_type = GL_FLOAT;
                length = 6;
                break;
              case GL_FLOAT_MAT2x4:
                base_type = GL_FLOAT;
                length = 8;
                break;
              case GL_FLOAT_MAT3x2:
                base_type = GL_FLOAT;
                length = 6;
                break;
              case GL_FLOAT_MAT3x4:
                base_type = GL_FLOAT;
                length = 12;
                break;
              case GL_FLOAT_MAT4x2:
                base_type = GL_FLOAT;
                length = 8;
                break;
              case GL_FLOAT_MAT4x3:
                base_type = GL_FLOAT;
                length = 12;
                break;
              case GL_SAMPLER_3D:
              case GL_SAMPLER_2D_ARRAY:
              case GL_SAMPLER_2D_SHADOW:
              case GL_SAMPLER_CUBE_SHADOW:
              case GL_SAMPLER_2D_ARRAY_SHADOW:
              case GL_INT_SAMPLER_2D:
              case GL_INT_SAMPLER_CUBE:
              case GL_INT_SAMPLER_3D:
              case GL_INT_SAMPLER_2D_ARRAY:
              case GL_UNSIGNED_INT_SAMPLER_2D:
              case GL_UNSIGNED_INT_SAMPLER_CUBE:
              case GL_UNSIGNED_INT_SAMPLER_3D:
              case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
                base_type = GL_INT;
                length = 1;
                break;
              default:
                // Can't handle this type
                SynthesizeGLError(GL_INVALID_VALUE, "getUniform",
                                  "unhandled type");
                return ScriptValue::CreateNull(script_state->GetIsolate());
            }
        }
        switch (base_type) {
          case GL_FLOAT: {
            GLfloat value[16] = {0};
            ContextGL()->GetUniformfv(ObjectOrZero(program), location, value);
            if (length == 1)
              return WebGLAny(script_state, value[0]);
            return WebGLAny(script_state, DOMFloat32Array::Create(
                                              base::span(value).first(length)));
          }
          case GL_INT: {
            GLint value[4] = {0};
            ContextGL()->GetUniformiv(ObjectOrZero(program), location, value);
            if (length == 1)
              return WebGLAny(script_state, value[0]);
            return WebGLAny(script_state, DOMInt32Array::Create(
                                              base::span(value).first(length)));
          }
          case GL_UNSIGNED_INT: {
            GLuint value[4] = {0};
            ContextGL()->GetUniformuiv(ObjectOrZero(program), location, value);
            if (length == 1)
              return WebGLAny(script_state, value[0]);
            return WebGLAny(script_state, DOMUint32Array::Create(
                                              base::span(value).first(length)));
          }
          case GL_BOOL: {
            std::array<GLint, 4> value = {0};
            ContextGL()->GetUniformiv(ObjectOrZero(program), location,
                                      value.data());

            if (length > 1) {
              std::array<bool, 4> bool_value = {};
              for (unsigned j = 0; j < length; j++)
                bool_value[j] = static_cast<bool>(value[j]);
              return WebGLAny(script_state, bool_value.data(), length);
            }

            return WebGLAny(script_state, static_cast<bool>(value[0]));
          }
          default:
            NOTIMPLEMENTED();
        }
      }
    }
  }
  // If we get here, something went wrong in our unfortunately complex logic
  // above
  SynthesizeGLError(GL_INVALID_VALUE, "getUniform", "unknown error");
  return ScriptValue::CreateNull(script_state->GetIsolate());
}

WebGLUniformLocation* WebGLRenderingContextBase::getUniformLocation(
    WebGLProgram* program,
    const String& name) {
  if (!ValidateWebGLProgramOrShader("getUniformLocation", program))
    return nullptr;
  if (!ValidateLocationLength("getUniformLocation", name))
    return nullptr;
  if (!ValidateString("getUniformLocation", name))
    return nullptr;
  if (IsPrefixReserved(name))
    return nullptr;
  if (!program->LinkStatus(this)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "getUniformLocation",
                      "program not linked");
    return nullptr;
  }
  GLint uniform_location = ContextGL()->GetUniformLocation(
      ObjectOrZero(program), name.Utf8().c_str());
  if (uniform_location == -1)
    return nullptr;
  return MakeGarbageCollected<WebGLUniformLocation>(program, uniform_location);
}

ScriptValue WebGLRenderingContextBase::getVertexAttrib(
    ScriptState* script_state,
    GLuint index,
    GLenum pname) {
  if (isContextLost())
    return ScriptValue::CreateNull(script_state->GetIsolate());
  if (index >= max_vertex_attribs_) {
    SynthesizeGLError(GL_INVALID_VALUE, "getVertexAttrib",
                      "index out of range");
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  if ((ExtensionEnabled(kANGLEInstancedArraysName) || IsWebGL2()) &&
      pname == GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE) {
    GLint value = 0;
    ContextGL()->GetVertexAttribiv(index, pname, &value);
    return WebGLAny(script_state, value);
  }

  switch (pname) {
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
      return WebGLAny(
          script_state,
          bound_vertex_array_object_->GetArrayBufferForAttrib(index));
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED: {
      GLint value = 0;
      ContextGL()->GetVertexAttribiv(index, pname, &value);
      return WebGLAny(script_state, static_cast<bool>(value));
    }
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE: {
      GLint value = 0;
      ContextGL()->GetVertexAttribiv(index, pname, &value);
      return WebGLAny(script_state, value);
    }
    case GL_VERTEX_ATTRIB_ARRAY_TYPE: {
      GLint value = 0;
      ContextGL()->GetVertexAttribiv(index, pname, &value);
      return WebGLAny(script_state, static_cast<GLenum>(value));
    }
    case GL_CURRENT_VERTEX_ATTRIB: {
      switch (vertex_attrib_type_[index]) {
        case kFloat32ArrayType: {
          GLfloat float_value[4];
          ContextGL()->GetVertexAttribfv(index, pname, float_value);
          return WebGLAny(script_state, DOMFloat32Array::Create(float_value));
        }
        case kInt32ArrayType: {
          GLint int_value[4];
          ContextGL()->GetVertexAttribIiv(index, pname, int_value);
          return WebGLAny(script_state, DOMInt32Array::Create(int_value));
        }
        case kUint32ArrayType: {
          GLuint uint_value[4];
          ContextGL()->GetVertexAttribIuiv(index, pname, uint_value);
          return WebGLAny(script_state, DOMUint32Array::Create(uint_value));
        }
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
      return ScriptValue::CreateNull(script_state->GetIsolate());
    }
    case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
      if (IsWebGL2()) {
        GLint value = 0;
        ContextGL()->GetVertexAttribiv(index, pname, &value);
        return WebGLAny(script_state, static_cast<bool>(value));
      }
      [[fallthrough]];
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getVertexAttrib",
                        "invalid parameter name");
      return ScriptValue::CreateNull(script_state->GetIsolate());
  }
}

int64_t WebGLRenderingContextBase::getVertexAttribOffset(GLuint index,
                                                         GLenum pname) {
  if (isContextLost())
    return 0;
  GLvoid* result = nullptr;
  // NOTE: If pname is ever a value that returns more than 1 element
  // this will corrupt memory.
  ContextGL()->GetVertexAttribPointerv(index, pname, &result);
  return static_cast<int64_t>(reinterpret_cast<intptr_t>(result));
}

void WebGLRenderingContextBase::hint(GLenum target, GLenum mode) {
  if (isContextLost())
    return;
  bool is_valid = false;
  switch (target) {
    case GL_GENERATE_MIPMAP_HINT:
      is_valid = true;
      break;
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:  // OES_standard_derivatives
      if (ExtensionEnabled(kOESStandardDerivativesName) || IsWebGL2())
        is_valid = true;
      break;
  }
  if (!is_valid) {
    SynthesizeGLError(GL_INVALID_ENUM, "hint", "invalid target");
    return;
  }
  ContextGL()->Hint(target, mode);
}

bool WebGLRenderingContextBase::isBuffer(WebGLBuffer* buffer) {
  if (!buffer || isContextLost() || !buffer->Validate(ContextGroup(), this))
    return false;

  if (!buffer->HasEverBeenBound())
    return false;
  if (buffer->MarkedForDeletion())
    return false;

  return ContextGL()->IsBuffer(buffer->Object());
}

bool WebGLRenderingContextBase::isContextLost() const {
  return context_lost_mode_ != kNotLostContext;
}

bool WebGLRenderingContextBase::isEnabled(GLenum cap) {
  if (isContextLost() || !ValidateCapability("isEnabled", cap))
    return false;
  if (cap == GL_DEPTH_TEST) {
    return depth_enabled_;
  }
  if (cap == GL_STENCIL_TEST) {
    return stencil_enabled_;
  }
  return ContextGL()->IsEnabled(cap);
}

bool WebGLRenderingContextBase::isFramebuffer(WebGLFramebuffer* framebuffer) {
  if (!framebuffer || isContextLost() ||
      !framebuffer->Validate(ContextGroup(), this))
    return false;

  if (!framebuffer->HasEverBeenBound())
    return false;
  if (framebuffer->MarkedForDeletion())
    return false;

  return ContextGL()->IsFramebuffer(framebuffer->Object());
}

bool WebGLRenderingContextBase::isProgram(WebGLProgram* program) {
  if (!program || isContextLost() || !program->Validate(ContextGroup(), this))
    return false;

  // OpenGL ES special-cases the behavior of program objects; if they're deleted
  // while attached to the current context state, glIsProgram is supposed to
  // still return true. For this reason, MarkedForDeletion is not checked here.

  return ContextGL()->IsProgram(program->Object());
}

bool WebGLRenderingContextBase::isRenderbuffer(
    WebGLRenderbuffer* renderbuffer) {
  if (!renderbuffer || isContextLost() ||
      !renderbuffer->Validate(ContextGroup(), this))
    return false;

  if (!renderbuffer->HasEverBeenBound())
    return false;
  if (renderbuffer->MarkedForDeletion())
    return false;

  return ContextGL()->IsRenderbuffer(renderbuffer->Object());
}

bool WebGLRenderingContextBase::isShader(WebGLShader* shader) {
  if (!shader || isContextLost() || !shader->Validate(ContextGroup(), this))
    return false;

  // OpenGL ES special-cases the behavior of shader objects; if they're deleted
  // while attached to a program, glIsShader is supposed to still return true.
  // For this reason, MarkedForDeletion is not checked here.

  return ContextGL()->IsShader(shader->Object());
}

bool WebGLRenderingContextBase::isTexture(WebGLTexture* texture) {
  if (!texture || isContextLost() || !texture->Validate(ContextGroup(), this))
    return false;

  if (!texture->HasEverBeenBound())
    return false;
  if (texture->MarkedForDeletion())
    return false;

  return ContextGL()->IsTexture(texture->Object());
}

void WebGLRenderingContextBase::lineWidth(GLfloat width) {
  if (isContextLost())
    return;
  ContextGL()->LineWidth(width);
}

void WebGLRenderingContextBase::linkProgram(WebGLProgram* program) {
  if (!ValidateWebGLProgramOrShader("linkProgram", program))
    return;

  if (program->ActiveTransformFeedbackCount() > 0) {
    SynthesizeGLError(
        GL_INVALID_OPERATION, "linkProgram",
        "program being used by one or more active transform feedback objects");
    return;
  }

  GLuint query = 0u;
  if (ExtensionEnabled(kKHRParallelShaderCompileName)) {
    ContextGL()->GenQueriesEXT(1, &query);
    ContextGL()->BeginQueryEXT(GL_PROGRAM_COMPLETION_QUERY_CHROMIUM, query);
  }
  ContextGL()->LinkProgram(ObjectOrZero(program));
  if (ExtensionEnabled(kKHRParallelShaderCompileName)) {
    ContextGL()->EndQueryEXT(GL_PROGRAM_COMPLETION_QUERY_CHROMIUM);
    addProgramCompletionQuery(program, query);
  }

  program->IncreaseLinkCount();
}

void WebGLRenderingContextBase::pixelStorei(GLenum pname, GLint param) {
  if (isContextLost())
    return;
  switch (pname) {
    case GC3D_UNPACK_FLIP_Y_WEBGL:
      unpack_flip_y_ = param;
      break;
    case GC3D_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
      unpack_premultiply_alpha_ = param;
      break;
    case GC3D_UNPACK_COLORSPACE_CONVERSION_WEBGL:
      if (static_cast<GLenum>(param) == GC3D_BROWSER_DEFAULT_WEBGL ||
          param == GL_NONE) {
        unpack_colorspace_conversion_ = static_cast<GLenum>(param);
      } else {
        SynthesizeGLError(
            GL_INVALID_VALUE, "pixelStorei",
            "invalid parameter for UNPACK_COLORSPACE_CONVERSION_WEBGL");
        return;
      }
      break;
    case GL_PACK_ALIGNMENT:
    case GL_UNPACK_ALIGNMENT:
      if (param == 1 || param == 2 || param == 4 || param == 8) {
        if (pname == GL_PACK_ALIGNMENT) {
          pack_alignment_ = param;
        } else {  // GL_UNPACK_ALIGNMENT:
          unpack_alignment_ = param;
        }
        ContextGL()->PixelStorei(pname, param);
      } else {
        SynthesizeGLError(GL_INVALID_VALUE, "pixelStorei",
                          "invalid parameter for alignment");
        return;
      }
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "pixelStorei",
                        "invalid parameter name");
      return;
  }
}

void WebGLRenderingContextBase::polygonOffset(GLfloat factor, GLfloat units) {
  if (isContextLost())
    return;
  ContextGL()->PolygonOffset(factor, units);
}

bool WebGLRenderingContextBase::ValidateReadBufferAndGetInfo(
    const char* function_name,
    WebGLFramebuffer*& read_framebuffer_binding) {
  read_framebuffer_binding = GetReadFramebufferBinding();
  if (read_framebuffer_binding) {
    const char* reason = "framebuffer incomplete";
    if (read_framebuffer_binding->CheckDepthStencilStatus(&reason) !=
        GL_FRAMEBUFFER_COMPLETE) {
      SynthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, function_name,
                        reason);
      return false;
    }
  } else {
    if (read_buffer_of_default_framebuffer_ == GL_NONE) {
      DCHECK(IsWebGL2());
      SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                        "no image to read from");
      return false;
    }
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateReadPixelsFormatAndType(
    GLenum format,
    GLenum type,
    DOMArrayBufferView* buffer) {
  switch (format) {
    case GL_ALPHA:
    case GL_RGB:
    case GL_RGBA:
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "readPixels", "invalid format");
      return false;
  }

  switch (type) {
    case GL_UNSIGNED_BYTE:
      if (buffer) {
        auto bufferType = buffer->GetType();
        if (bufferType != DOMArrayBufferView::kTypeUint8 &&
            bufferType != DOMArrayBufferView::kTypeUint8Clamped) {
          SynthesizeGLError(
              GL_INVALID_OPERATION, "readPixels",
              "type UNSIGNED_BYTE but ArrayBufferView not Uint8Array or "
              "Uint8ClampedArray");
          return false;
        }
      }
      return true;
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
      if (buffer && buffer->GetType() != DOMArrayBufferView::kTypeUint16) {
        SynthesizeGLError(
            GL_INVALID_OPERATION, "readPixels",
            "type UNSIGNED_SHORT but ArrayBufferView not Uint16Array");
        return false;
      }
      return true;
    case GL_FLOAT:
      if (ExtensionEnabled(kOESTextureFloatName) ||
          ExtensionEnabled(kOESTextureHalfFloatName)) {
        if (buffer && buffer->GetType() != DOMArrayBufferView::kTypeFloat32) {
          SynthesizeGLError(GL_INVALID_OPERATION, "readPixels",
                            "type FLOAT but ArrayBufferView not Float32Array");
          return false;
        }
        return true;
      }
      SynthesizeGLError(GL_INVALID_ENUM, "readPixels", "invalid type");
      return false;
    case GL_HALF_FLOAT_OES:
      if (ExtensionEnabled(kOESTextureHalfFloatName)) {
        if (buffer && buffer->GetType() != DOMArrayBufferView::kTypeUint16) {
          SynthesizeGLError(
              GL_INVALID_OPERATION, "readPixels",
              "type HALF_FLOAT_OES but ArrayBufferView not Uint16Array");
          return false;
        }
        return true;
      }
      SynthesizeGLError(GL_INVALID_ENUM, "readPixels", "invalid type");
      return false;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "readPixels", "invalid type");
      return false;
  }
}

WebGLImageConversion::PixelStoreParams
WebGLRenderingContextBase::GetPackPixelStoreParams() {
  WebGLImageConversion::PixelStoreParams params;
  params.alignment = pack_alignment_;
  return params;
}

WebGLImageConversion::PixelStoreParams
WebGLRenderingContextBase::GetUnpackPixelStoreParams(TexImageDimension) {
  WebGLImageConversion::PixelStoreParams params;
  params.alignment = unpack_alignment_;
  return params;
}

bool WebGLRenderingContextBase::ValidateReadPixelsFuncParameters(
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    DOMArrayBufferView* buffer,
    int64_t buffer_size) {
  if (!ValidateReadPixelsFormatAndType(format, type, buffer))
    return false;

  // Calculate array size, taking into consideration of pack parameters.
  unsigned bytes_required = 0;
  unsigned skip_bytes = 0;
  GLenum error = WebGLImageConversion::ComputeImageSizeInBytes(
      format, type, width, height, 1, GetPackPixelStoreParams(),
      &bytes_required, nullptr, &skip_bytes);
  if (error != GL_NO_ERROR) {
    SynthesizeGLError(error, "readPixels", "invalid dimensions");
    return false;
  }
  int64_t total_bytes_required =
      static_cast<int64_t>(bytes_required) + static_cast<int64_t>(skip_bytes);
  if (buffer_size < total_bytes_required) {
    SynthesizeGLError(GL_INVALID_OPERATION, "readPixels",
                      "buffer is not large enough for dimensions");
    return false;
  }
  if (kMaximumSupportedArrayBufferSize <
      static_cast<size_t>(total_bytes_required)) {
    SynthesizeGLError(GL_INVALID_VALUE, "readPixels",
                      "amount of read pixels is too high");
    return false;
  }
  return true;
}

void WebGLRenderingContextBase::readPixels(
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> pixels) {
  ReadPixelsHelper(x, y, width, height, format, type, pixels.Get(), 0);
}

void WebGLRenderingContextBase::ReadPixelsHelper(GLint x,
                                                 GLint y,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLenum format,
                                                 GLenum type,
                                                 DOMArrayBufferView* pixels,
                                                 int64_t offset) {
  if (isContextLost())
    return;
  // Due to WebGL's same-origin restrictions, it is not possible to
  // taint the origin using the WebGL API.
  DCHECK(Host()->OriginClean());

  // Validate input parameters.
  if (!pixels) {
    SynthesizeGLError(GL_INVALID_VALUE, "readPixels",
                      "no destination ArrayBufferView");
    return;
  }
  base::CheckedNumeric<size_t> offset_in_bytes = offset;
  offset_in_bytes *= pixels->TypeSize();
  if (!offset_in_bytes.IsValid() ||
      offset_in_bytes.ValueOrDie() > pixels->byteLength()) {
    SynthesizeGLError(GL_INVALID_VALUE, "readPixels",
                      "destination offset out of range");
    return;
  }
  const char* reason = "framebuffer incomplete";
  WebGLFramebuffer* framebuffer = GetReadFramebufferBinding();
  if (framebuffer && framebuffer->CheckDepthStencilStatus(&reason) !=
                         GL_FRAMEBUFFER_COMPLETE) {
    SynthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, "readPixels", reason);
    return;
  }
  base::CheckedNumeric<GLuint> buffer_size =
      pixels->byteLength() - offset_in_bytes;
  if (!buffer_size.IsValid()) {
    SynthesizeGLError(GL_INVALID_VALUE, "readPixels",
                      "destination offset out of range");
    return;
  }
  if (!ValidateReadPixelsFuncParameters(width, height, format, type, pixels,
                                        buffer_size.ValueOrDie())) {
    return;
  }
  ClearIfComposited(kClearCallerOther);

  uint8_t* data = static_cast<uint8_t*>(pixels->BaseAddressMaybeShared()) +
                  offset_in_bytes.ValueOrDie();

  // We add special handling here if the 'ArrayBufferView' is size '0' and the
  // backing store is 'nullptr'. 'ReadPixels' creates an error if the provided
  // data is 'nullptr'. However, in the case that we want to read zero pixels,
  // we want to avoid this error. Therefore we provide temporary memory here if
  // 'ArrayBufferView' does not provide a backing store but we actually read
  // zero pixels.
  std::optional<Vector<uint8_t>> buffer;
  if (!data && (width == 0 || height == 0)) {
    buffer.emplace(32);
    data = buffer->data();
  }

  // Last-chance early-out, in case somehow the context was lost during
  // the above ClearIfComposited operation.
  if (isContextLost() || !GetDrawingBuffer())
    return;

  {
    ScopedDrawingBufferBinder binder(GetDrawingBuffer(), framebuffer);
    if (!binder.Succeeded()) {
      return;
    }
    ContextGL()->ReadPixels(x, y, width, height, format, type, data);
  }
}

void WebGLRenderingContextBase::RenderbufferStorageImpl(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    const char* function_name) {
  DCHECK(!samples);     // |samples| > 0 is only valid in WebGL2's
                        // renderbufferStorageMultisample().
  DCHECK(!IsWebGL2());  // Make sure this is overridden in WebGL 2.
  switch (internalformat) {
    case GL_DEPTH_COMPONENT16:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGB565:
    case GL_STENCIL_INDEX8:
    case GL_SRGB8_ALPHA8_EXT:
    case GL_RGB16F_EXT:
    case GL_RGBA16F_EXT:
    case GL_RGBA32F_EXT:
      if (internalformat == GL_SRGB8_ALPHA8_EXT &&
          !ExtensionEnabled(kEXTsRGBName)) {
        SynthesizeGLError(GL_INVALID_ENUM, function_name,
                          "EXT_sRGB not enabled");
        break;
      }
      if ((internalformat == GL_RGB16F_EXT ||
           internalformat == GL_RGBA16F_EXT) &&
          !ExtensionEnabled(kEXTColorBufferHalfFloatName)) {
        SynthesizeGLError(GL_INVALID_ENUM, function_name,
                          "EXT_color_buffer_half_float not enabled");
        break;
      }
      if (internalformat == GL_RGBA32F_EXT &&
          !ExtensionEnabled(kWebGLColorBufferFloatName)) {
        SynthesizeGLError(GL_INVALID_ENUM, function_name,
                          "WEBGL_color_buffer_float not enabled");
        break;
      }
      ContextGL()->RenderbufferStorage(target, internalformat, width, height);
      renderbuffer_binding_->SetInternalFormat(internalformat);
      renderbuffer_binding_->SetSize(width, height);
      break;
    case GL_DEPTH_STENCIL_OES:
      DCHECK(IsDepthStencilSupported());
      ContextGL()->RenderbufferStorage(target, GL_DEPTH24_STENCIL8_OES, width,
                                       height);
      renderbuffer_binding_->SetSize(width, height);
      renderbuffer_binding_->SetInternalFormat(internalformat);
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name,
                        "invalid internalformat");
      break;
  }
  UpdateNumberOfUserAllocatedMultisampledRenderbuffers(
      renderbuffer_binding_->UpdateMultisampleState(false));
}

void WebGLRenderingContextBase::renderbufferStorage(GLenum target,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height) {
  const char* function_name = "renderbufferStorage";
  if (isContextLost())
    return;
  if (target != GL_RENDERBUFFER) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid target");
    return;
  }
  if (!renderbuffer_binding_ || !renderbuffer_binding_->Object()) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "no bound renderbuffer");
    return;
  }
  if (!ValidateSize(function_name, width, height))
    return;
  RenderbufferStorageImpl(target, 0, internalformat, width, height,
                          function_name);
  ApplyDepthAndStencilTest();
}

void WebGLRenderingContextBase::sampleCoverage(GLfloat value,
                                               GLboolean invert) {
  if (isContextLost())
    return;
  ContextGL()->SampleCoverage(value, invert);
}

void WebGLRenderingContextBase::scissor(GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height) {
  if (isContextLost())
    return;
  scissor_box_[0] = x;
  scissor_box_[1] = y;
  scissor_box_[2] = width;
  scissor_box_[3] = height;
  ContextGL()->Scissor(x, y, width, height);
}

void WebGLRenderingContextBase::shaderSource(WebGLShader* shader,
                                             const String& string) {
  if (!ValidateWebGLProgramOrShader("shaderSource", shader))
    return;
  String ascii_string = ReplaceNonASCII(string).Result();
  shader->SetSource(string);
  DCHECK(ascii_string.Is8Bit() && ascii_string.ContainsOnlyASCIIOrEmpty());
  const GLchar* shader_data =
      reinterpret_cast<const GLchar*>(ascii_string.Characters8());
  const GLint shader_length = ascii_string.length();
  ContextGL()->ShaderSource(ObjectOrZero(shader), 1, &shader_data,
                            &shader_length);
}

void WebGLRenderingContextBase::stencilFunc(GLenum func,
                                            GLint ref,
                                            GLuint mask) {
  if (isContextLost())
    return;
  if (!ValidateStencilOrDepthFunc("stencilFunc", func))
    return;
  stencil_func_ref_ = ref;
  stencil_func_ref_back_ = ref;
  stencil_func_mask_ = mask;
  stencil_func_mask_back_ = mask;
  ContextGL()->StencilFunc(func, ref, mask);
}

void WebGLRenderingContextBase::stencilFuncSeparate(GLenum face,
                                                    GLenum func,
                                                    GLint ref,
                                                    GLuint mask) {
  if (isContextLost())
    return;
  if (!ValidateStencilOrDepthFunc("stencilFuncSeparate", func))
    return;
  switch (face) {
    case GL_FRONT_AND_BACK:
      stencil_func_ref_ = ref;
      stencil_func_ref_back_ = ref;
      stencil_func_mask_ = mask;
      stencil_func_mask_back_ = mask;
      break;
    case GL_FRONT:
      stencil_func_ref_ = ref;
      stencil_func_mask_ = mask;
      break;
    case GL_BACK:
      stencil_func_ref_back_ = ref;
      stencil_func_mask_back_ = mask;
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "stencilFuncSeparate", "invalid face");
      return;
  }
  ContextGL()->StencilFuncSeparate(face, func, ref, mask);
}

void WebGLRenderingContextBase::stencilMask(GLuint mask) {
  if (isContextLost())
    return;
  stencil_mask_ = mask;
  stencil_mask_back_ = mask;
  ContextGL()->StencilMask(mask);
}

void WebGLRenderingContextBase::stencilMaskSeparate(GLenum face, GLuint mask) {
  if (isContextLost())
    return;
  switch (face) {
    case GL_FRONT_AND_BACK:
      stencil_mask_ = mask;
      stencil_mask_back_ = mask;
      break;
    case GL_FRONT:
      stencil_mask_ = mask;
      break;
    case GL_BACK:
      stencil_mask_back_ = mask;
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "stencilMaskSeparate", "invalid face");
      return;
  }
  ContextGL()->StencilMaskSeparate(face, mask);
}

void WebGLRenderingContextBase::stencilOp(GLenum fail,
                                          GLenum zfail,
                                          GLenum zpass) {
  if (isContextLost())
    return;
  ContextGL()->StencilOp(fail, zfail, zpass);
}

void WebGLRenderingContextBase::stencilOpSeparate(GLenum face,
                                                  GLenum fail,
                                                  GLenum zfail,
                                                  GLenum zpass) {
  if (isContextLost())
    return;
  ContextGL()->StencilOpSeparate(face, fail, zfail, zpass);
}

GLenum WebGLRenderingContextBase::ConvertTexInternalFormat(
    GLenum internalformat,
    GLenum type) {
  // Convert to sized internal formats that are renderable with
  // GL_CHROMIUM_color_buffer_float_rgb(a).
  if (type == GL_FLOAT && internalformat == GL_RGBA &&
      ExtensionsUtil()->IsExtensionEnabled(
          "GL_CHROMIUM_color_buffer_float_rgba"))
    return GL_RGBA32F_EXT;
  if (type == GL_FLOAT && internalformat == GL_RGB &&
      ExtensionsUtil()->IsExtensionEnabled(
          "GL_CHROMIUM_color_buffer_float_rgb"))
    return GL_RGB32F_EXT;
  return internalformat;
}

void WebGLRenderingContextBase::GetCurrentUnpackState(TexImageParams& params) {
  params.unpack_premultiply_alpha = unpack_premultiply_alpha_;
  params.unpack_flip_y = unpack_flip_y_;
  if (params.source_type == kSourceHTMLImageElement ||
      params.source_type == kSourceHTMLVideoElement ||
      params.source_type == kSourceVideoFrame) {
    params.unpack_colorspace_conversion =
        unpack_colorspace_conversion_ != GL_NONE;
  }
}

void WebGLRenderingContextBase::TexImageSkImage(TexImageParams params,
                                                sk_sp<SkImage> image,
                                                bool image_has_flip_y) {
  const char* func_name = GetTexImageFunctionName(params.function_id);

  bool selecting_sub_rectangle = false;
  if (!ValidateTexImageSubRectangle(params, image.get(),
                                    &selecting_sub_rectangle)) {
    return;
  }

  // Ensure that `image` have a color space, because SkImageInfo::readPixels and
  // SkPixmap::readPixels will fail if the source has no color space but the
  // destination does.
  if (!image->colorSpace())
    image = image->reinterpretColorSpace(SkColorSpace::MakeSRGB());

  // The UNSIGNED_INT_10F_11F_11F_REV type pack/unpack isn't implemented,
  // use GL_FLOAT instead.
  if (params.type == GL_UNSIGNED_INT_10F_11F_11F_REV)
    params.type = GL_FLOAT;

  // We will need to flip vertically if the unpack state for flip Y does not
  // match the source state for flip Y.
  const bool do_flip_y = image_has_flip_y != params.unpack_flip_y;

  // Let `converted_info` be `image`'s info, with adjustments for sub-rect
  // selection, alpha type, color type, and color space. Let `converted_x` and
  // `converted_y` be the origin in `image` at which the data is to be read.
  // We will convert `image` to this format (using SkImage::readPixels), if
  // it is not already in this format.
  SkImageInfo converted_info = image->imageInfo();
  int converted_x = 0;
  int converted_y = 0;
  {
    // Set the size and offset parameters for the readPixels call, so we only
    // convert the portion of `image` that is needed. Do not try this if we are
    // uploading a 3D volume (just convert the full image in that case).
    if (params.width.has_value() && params.height.has_value() &&
        params.depth.value_or(1) == 1) {
      converted_info = converted_info.makeWH(*params.width, *params.height);
      converted_x = params.unpack_skip_pixels;
      converted_y = params.unpack_skip_rows;
      if (do_flip_y) {
        converted_y = image->height() - converted_info.height() - converted_y;
      }
      params.unpack_skip_pixels = 0;
      params.unpack_skip_rows = 0;
      selecting_sub_rectangle = false;
    }

    // Set the alpha type to perform premultiplication or unmultiplication
    // during readPixels, if needed. If the input is opaque, do not change it
    // (readPixels fails if the source is opaque and the destination is not).
    if (converted_info.alphaType() != kOpaque_SkAlphaType) {
      converted_info = converted_info.makeAlphaType(
          params.unpack_premultiply_alpha ? kPremul_SkAlphaType
                                          : kUnpremul_SkAlphaType);
    }

    // Set the color type to perform pixel format conversion during readPixels,
    // if possible.
    converted_info = converted_info.makeColorType(
        WebGLImageConversion::DataFormatToSkColorType(
            WebGLImageConversion::GetDataFormat(params.format, params.type),
            converted_info.colorType()));

    // Set the color space to perform color space conversion to the unpack color
    // space during readPixels, if needed.
    if (params.unpack_colorspace_conversion) {
      converted_info = converted_info.makeColorSpace(
          PredefinedColorSpaceToSkColorSpace(unpack_color_space_));
    }
  }

  // Try to access `image`'s pixels directly. If they already match
  // `converted_info` and `converted_x` and `converted_y` are zero, then use
  // them directly. Otherwise, convert them using SkImage::readPixels.
  SkBitmap converted_bitmap;
  SkPixmap pixmap;
  if (!image->peekPixels(&pixmap) || pixmap.info() != converted_info ||
      pixmap.rowBytes() != converted_info.minRowBytes() || converted_x != 0 ||
      converted_y != 0) {
    converted_bitmap.allocPixels(converted_info);
    pixmap = converted_bitmap.pixmap();
    if (!image->readPixels(pixmap, converted_x, converted_y)) {
      SynthesizeGLError(GL_OUT_OF_MEMORY, func_name, "bad image data");
      return;
    }
  }

  // Let `gl_data` be the data that is passed to the GL upload function.
  const void* gl_data = pixmap.addr();

  // We will premultiply or unpremultiply only if there is a mismatch between
  // the source and the requested premultiplication format.
  WebGLImageConversion::AlphaOp alpha_op =
      WebGLImageConversion::kAlphaDoNothing;
  if (params.unpack_premultiply_alpha &&
      pixmap.alphaType() == kUnpremul_SkAlphaType) {
    alpha_op = WebGLImageConversion::kAlphaDoPremultiply;
  }
  if (!params.unpack_premultiply_alpha &&
      pixmap.alphaType() == kPremul_SkAlphaType) {
    alpha_op = WebGLImageConversion::kAlphaDoUnmultiply;
  }

  // If there are required conversions that Skia could not do above, then use
  // WebGLImageConversion to convert the data, and point `gl_data` at the
  // temporary buffer `image_conversion_data`.
  Vector<uint8_t> image_conversion_data;
  if (WebGLImageConversion::SkColorTypeToDataFormat(pixmap.colorType()) !=
          WebGLImageConversion::GetDataFormat(params.format, params.type) ||
      alpha_op != WebGLImageConversion::kAlphaDoNothing || do_flip_y ||
      selecting_sub_rectangle || params.depth != 1) {
    // Adjust the source image rectangle if doing a y-flip.
    gfx::Rect adjusted_source_rect(params.unpack_skip_pixels,
                                   params.unpack_skip_rows,
                                   params.width.value_or(pixmap.width()),
                                   params.height.value_or(pixmap.height()));
    if (do_flip_y) {
      adjusted_source_rect.set_y(pixmap.height() -
                                 adjusted_source_rect.bottom());
    }
    if (!WebGLImageConversion::PackSkPixmap(
            &pixmap, params.format, params.type, do_flip_y, alpha_op,
            adjusted_source_rect, params.depth.value_or(1),
            /*source_unpack_alignment=*/0, params.unpack_image_height,
            image_conversion_data)) {
      SynthesizeGLError(GL_INVALID_VALUE, func_name, "packImage error");
      return;
    }
    gl_data = image_conversion_data.data();
  }

  // Upload using GL.
  ScopedUnpackParametersResetRestore temporary_reset_unpack(this);
  if (!params.width)
    params.width = pixmap.width();
  if (!params.height)
    params.height = pixmap.height();
  if (!params.depth)
    params.depth = 1;
  TexImageBase(params, gl_data);
}

void WebGLRenderingContextBase::TexImageBase(const TexImageParams& params,
                                             const void* pixels) {
  // All calling functions check isContextLost, so a duplicate check is not
  // needed here.
  DCHECK(params.width && params.height);
  switch (params.function_id) {
    case kTexImage2D:
      ContextGL()->TexImage2D(
          params.target, params.level,
          ConvertTexInternalFormat(params.internalformat, params.type),
          *params.width, *params.height, params.border, params.format,
          params.type, pixels);
      break;
    case kTexSubImage2D:
      ContextGL()->TexSubImage2D(params.target, params.level, params.xoffset,
                                 params.yoffset, *params.width, *params.height,
                                 params.format, params.type, pixels);
      break;
    case kTexImage3D:
      DCHECK(params.depth);
      ContextGL()->TexImage3D(
          params.target, params.level,
          ConvertTexInternalFormat(params.internalformat, params.type),
          *params.width, *params.height, *params.depth, params.border,
          params.format, params.type, pixels);
      break;
    case kTexSubImage3D:
      DCHECK(params.depth);
      ContextGL()->TexSubImage3D(params.target, params.level, params.xoffset,
                                 params.yoffset, params.zoffset, *params.width,
                                 *params.height, *params.depth, params.format,
                                 params.type, pixels);
      break;
  }
}

void WebGLRenderingContextBase::TexImageStaticBitmapImage(
    TexImageParams params,
    StaticBitmapImage* image,
    bool image_has_flip_y,
    bool allow_copy_via_gpu) {
  // All calling functions check isContextLost, so a duplicate check is not
  // needed here.
  const char* func_name = GetTexImageFunctionName(params.function_id);

  // If `image` is accelerated, then convert to the unpack color space while
  // still on the GPU. Unaccelerated images will be converted on the CPU below
  // in TexImageSkImage.
  scoped_refptr<StaticBitmapImage> color_converted_image;
  if (params.unpack_colorspace_conversion && image->IsTextureBacked()) {
    const auto image_color_info = image->GetSkColorInfo();
    const auto image_color_space = image_color_info.colorSpace()
                                       ? image_color_info.refColorSpace()
                                       : SkColorSpace::MakeSRGB();
    const auto unpack_color_space =
        PredefinedColorSpaceToSkColorSpace(unpack_color_space_);
    if (!SkColorSpace::Equals(unpack_color_space.get(),
                              image_color_space.get())) {
      color_converted_image = image->ConvertToColorSpace(
          unpack_color_space, image_color_info.colorType());
      if (!color_converted_image) {
        SynthesizeGLError(
            GL_OUT_OF_MEMORY, func_name,
            "ImageBitmap in unpack color space unexpectedly empty");
        return;
      }
      image = color_converted_image.get();
    }
  }

  // Copy using the GPU, if possible.
  if (allow_copy_via_gpu && image->IsTextureBacked() &&
      CanUseTexImageViaGPU(params)) {
    TexImageViaGPU(params, static_cast<AcceleratedStaticBitmapImage*>(image),
                   nullptr);
    return;
  }

  // Apply orientation if necessary. This should be merged into the
  // transformations performed inside TexImageSkImage.
  PaintImage paint_image = image->PaintImageForCurrentFrame();
  if (!image->HasDefaultOrientation()) {
    paint_image = Image::ResizeAndOrientImage(
        paint_image, image->CurrentFrameOrientation(), gfx::Vector2dF(1, 1), 1,
        kInterpolationNone);
  }

  sk_sp<SkImage> sk_image = paint_image.GetSwSkImage();
  if (!sk_image) {
    SynthesizeGLError(GL_INVALID_VALUE, func_name, "bad image data");
    return;
  }
  DCHECK_EQ(sk_image->width(), image->width());
  DCHECK_EQ(sk_image->height(), image->height());

  TexImageSkImage(params, std::move(sk_image), image_has_flip_y);
}

bool WebGLRenderingContextBase::ValidateTexFunc(
    TexImageParams params,
    std::optional<GLsizei> source_width,
    std::optional<GLsizei> source_height) {
  // Overwrite `params.width` and `params.height` with `source_width` and
  // `source_height`. If `params.depth` is unspecified, set it to 1.
  if (source_width)
    params.width = *source_width;
  if (source_height)
    params.height = *source_height;
  if (!params.depth)
    params.depth = 1;

  const char* function_name = GetTexImageFunctionName(params.function_id);
  if (!ValidateTexFuncLevel(function_name, params.target, params.level))
    return false;

  if (!ValidateTexFuncParameters(params)) {
    return false;
  }

  if (GetTexImageFunctionType(params.function_id) == kTexSubImage) {
    if (!ValidateSettableTexFormat(function_name, params.format))
      return false;
    if (!ValidateSize(function_name, params.xoffset, params.yoffset,
                      params.zoffset))
      return false;
  } else {
    // For SourceArrayBufferView, function ValidateTexFuncData() would handle
    // whether to validate the SettableTexFormat
    // by checking if the ArrayBufferView is null or not.
    if (params.source_type != kSourceArrayBufferView) {
      if (!ValidateSettableTexFormat(function_name, params.format))
        return false;
    }
  }

  return true;
}

bool WebGLRenderingContextBase::ValidateValueFitNonNegInt32(
    const char* function_name,
    const char* param_name,
    int64_t value) {
  if (value < 0) {
    String error_msg = String(param_name) + " < 0";
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      error_msg.Ascii().c_str());
    return false;
  }
  if (value > static_cast<int64_t>(std::numeric_limits<int>::max())) {
    String error_msg = String(param_name) + " more than 32-bit";
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      error_msg.Ascii().c_str());
    return false;
  }
  return true;
}

// TODO(fmalita): figure why ImageExtractor can't handle
// SVG-backed images, and get rid of this intermediate step.
scoped_refptr<Image> WebGLRenderingContextBase::DrawImageIntoBufferForTexImage(
    scoped_refptr<Image> pass_image,
    int width,
    int height,
    const char* function_name) {
  scoped_refptr<Image> image(std::move(pass_image));
  DCHECK(image);

  // TODO(https://crbug.com/1341235): The choice of color type should match the
  // format of the TexImage function. The choice of alpha type should opaque for
  // opaque images. The color space should match the unpack color space.
  const auto resource_provider_info = SkImageInfo::Make(
      width, height, kN32_SkColorType, kPremul_SkAlphaType, nullptr);
  CanvasResourceProvider* resource_provider =
      generated_image_cache_.GetCanvasResourceProvider(resource_provider_info);
  if (!resource_provider) {
    SynthesizeGLError(GL_OUT_OF_MEMORY, function_name, "out of memory");
    return nullptr;
  }

  if (!image->CurrentFrameKnownToBeOpaque())
    resource_provider->Canvas().clear(SkColors::kTransparent);

  gfx::Rect src_rect(image->Size());
  gfx::Rect dest_rect(0, 0, width, height);
  cc::PaintFlags flags;
  // TODO(ccameron): WebGL should produce sRGB images.
  // https://crbug.com/672299
  ImageDrawOptions draw_options;
  draw_options.clamping_mode = Image::kDoNotClampImageToSourceRect;
  image->Draw(&resource_provider->Canvas(), flags, gfx::RectF(dest_rect),
              gfx::RectF(src_rect), draw_options);
  return resource_provider->Snapshot(FlushReason::kWebGLTexImage);
}

WebGLTexture* WebGLRenderingContextBase::ValidateTexImageBinding(
    const TexImageParams& params) {
  const char* func_name = GetTexImageFunctionName(params.function_id);
  return ValidateTexture2DBinding(func_name, params.target, true);
}

const char* WebGLRenderingContextBase::GetTexImageFunctionName(
    TexImageFunctionID func_name) {
  switch (func_name) {
    case kTexImage2D:
      return "texImage2D";
    case kTexSubImage2D:
      return "texSubImage2D";
    case kTexSubImage3D:
      return "texSubImage3D";
    case kTexImage3D:
      return "texImage3D";
    default:  // Adding default to prevent compile error
      return "";
  }
}

WebGLRenderingContextBase::TexImageFunctionType
WebGLRenderingContextBase::GetTexImageFunctionType(
    TexImageFunctionID function_id) {
  switch (function_id) {
    case kTexImage2D:
      return kTexImage;
    case kTexSubImage2D:
      return kTexSubImage;
    case kTexImage3D:
      return kTexImage;
    case kTexSubImage3D:
      return kTexSubImage;
  }
}

gfx::Rect WebGLRenderingContextBase::SafeGetImageSize(Image* image) {
  if (!image)
    return gfx::Rect();

  return GetTextureSourceSize(image);
}

SkColorInfo WebGLRenderingContextBase::CanvasRenderingContextSkColorInfo()
    const {
  // This selection of alpha type disregards whether or not the drawing buffer
  // is premultiplied. This is to match historical behavior that may or may not
  // have been intentional.
  const SkAlphaType alpha_type =
      CreationAttributes().alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType;
  SkColorType color_type = kN32_SkColorType;
  if (drawing_buffer_ && drawing_buffer_->StorageFormat() == GL_RGBA16F) {
    color_type = kRGBA_F16_SkColorType;
  }
  return SkColorInfo(
      color_type, alpha_type,
      PredefinedColorSpaceToSkColorSpace(drawing_buffer_color_space_));
}

gfx::Rect WebGLRenderingContextBase::GetImageDataSize(ImageData* pixels) {
  DCHECK(pixels);
  return GetTextureSourceSize(pixels);
}

void WebGLRenderingContextBase::TexImageHelperDOMArrayBufferView(
    TexImageParams params,
    DOMArrayBufferView* pixels,
    NullDisposition null_disposition,
    int64_t src_offset) {
  const char* func_name = GetTexImageFunctionName(params.function_id);
  if (isContextLost())
    return;
  if (!ValidateTexImageBinding(params))
    return;
  if (!ValidateTexFunc(params, std::nullopt, std::nullopt)) {
    return;
  }
  if (!ValidateTexFuncData(params, pixels, null_disposition, src_offset))
    return;
  // No need to check overflow because validateTexFuncData() already did.
  base::span<const uint8_t> data = pixels
                                       ? pixels->ByteSpanMaybeShared().subspan(
                                             src_offset * pixels->TypeSize())
                                       : base::span<const uint8_t>();
  Vector<uint8_t> temp_data;
  bool change_unpack_params = false;
  if (!data.empty() && *params.width && *params.height &&
      (unpack_flip_y_ || unpack_premultiply_alpha_)) {
    DCHECK(params.function_id == kTexImage2D ||
           params.function_id == kTexSubImage2D);
    // Only enter here if width or height is non-zero. Otherwise, call to the
    // underlying driver to generate appropriate GL errors if needed.
    WebGLImageConversion::PixelStoreParams unpack_params =
        GetUnpackPixelStoreParams(kTex2D);
    GLint data_store_width =
        unpack_params.row_length ? unpack_params.row_length : *params.width;
    if (unpack_params.skip_pixels + *params.width > data_store_width) {
      SynthesizeGLError(GL_INVALID_OPERATION, func_name,
                        "Invalid unpack params combination.");
      return;
    }
    if (!WebGLImageConversion::ExtractTextureData(
            *params.width, *params.height, params.format, params.type,
            unpack_params, unpack_flip_y_, unpack_premultiply_alpha_,
            data.data(), temp_data)) {
      SynthesizeGLError(GL_INVALID_OPERATION, func_name,
                        "Invalid params.format/params.type combination.");
      return;
    }
    data = temp_data;
    change_unpack_params = true;
  }
  if (params.function_id == kTexImage3D ||
      params.function_id == kTexSubImage3D) {
    TexImageBase(params, data.data());
    return;
  }

  ScopedUnpackParametersResetRestore temporary_reset_unpack(
      this, change_unpack_params);
  TexImageBase(params, data.data());
}

void WebGLRenderingContextBase::texImage2D(
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> pixels) {
  TexImageParams params;
  POPULATE_TEX_IMAGE_2D_PARAMS(params, kSourceArrayBufferView);
  params.width = width;
  params.height = height;
  params.depth = 1;
  params.border = border;
  TexImageHelperDOMArrayBufferView(params, pixels.Get(), kNullAllowed, 0);
}

void WebGLRenderingContextBase::TexImageHelperImageData(TexImageParams params,
                                                        ImageData* pixels) {
  const char* func_name = GetTexImageFunctionName(params.function_id);
  if (isContextLost())
    return;
  DCHECK(pixels);
  DCHECK(pixels->data());
  if (pixels->IsBufferBaseDetached()) {
    SynthesizeGLError(GL_INVALID_VALUE, func_name,
                      "The source data has been detached.");
    return;
  }

  if (!ValidateTexImageBinding(params))
    return;
  if (!ValidateTexFunc(params, pixels->width(), pixels->height())) {
    return;
  }

  auto pixmap = pixels->GetSkPixmap();
  auto image = SkImages::RasterFromPixmap(pixmap, nullptr, nullptr);
  TexImageSkImage(params, std::move(image), /*image_has_flip_y=*/false);
}

void WebGLRenderingContextBase::texImage2D(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           ImageData* pixels) {
  TexImageParams params;
  POPULATE_TEX_IMAGE_2D_PARAMS(params, kSourceImageData);
  TexImageHelperImageData(params, pixels);
}

void WebGLRenderingContextBase::TexImageHelperHTMLImageElement(
    const SecurityOrigin* security_origin,
    const TexImageParams& params,
    HTMLImageElement* image,
    ExceptionState& exception_state) {
  const char* func_name = GetTexImageFunctionName(params.function_id);
  if (isContextLost())
    return;

  // TODO(crbug.com/1210718): It may be possible to simplify this code
  // by consolidating on CanvasImageSource::GetSourceImageForCanvas().

  if (!ValidateHTMLImageElement(security_origin, func_name, image,
                                exception_state))
    return;
  if (!ValidateTexImageBinding(params))
    return;

  scoped_refptr<Image> image_for_render = image->CachedImage()->GetImage();
  bool have_svg_image = IsA<SVGImage>(image_for_render.get());
  if (have_svg_image || !image_for_render->HasDefaultOrientation()) {
    if (have_svg_image && canvas()) {
      UseCounter::Count(canvas()->GetDocument(), WebFeature::kSVGInWebGL);
    }
    // DrawImageIntoBuffer always respects orientation
    image_for_render = DrawImageIntoBufferForTexImage(
        std::move(image_for_render), image->width(), image->height(),
        func_name);
  }
  if (!image_for_render || !ValidateTexFunc(params, image_for_render->width(),
                                            image_for_render->height())) {
    return;
  }

  ImageExtractor image_extractor(
      image_for_render.get(), params.unpack_premultiply_alpha,
      params.unpack_colorspace_conversion
          ? PredefinedColorSpaceToSkColorSpace(unpack_color_space_)
          : nullptr);
  auto sk_image = image_extractor.GetSkImage();
  if (!sk_image) {
    SynthesizeGLError(GL_INVALID_VALUE, func_name, "bad image data");
    return;
  }
  TexImageSkImage(params, std::move(sk_image), /*image_has_flip_y=*/false);
}

void WebGLRenderingContextBase::texImage2D(ScriptState* script_state,
                                           GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           HTMLImageElement* image,
                                           ExceptionState& exception_state) {
  TexImageParams params;
  POPULATE_TEX_IMAGE_2D_PARAMS(params, kSourceHTMLImageElement);
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  TexImageHelperHTMLImageElement(execution_context->GetSecurityOrigin(), params,
                                 image, exception_state);
}

bool WebGLRenderingContextBase::CanUseTexImageViaGPU(
    const TexImageParams& params) {
#if BUILDFLAG(IS_MAC)
  // RGB5_A1 is not color-renderable on NVIDIA Mac, see crbug.com/676209.
  // Though, glCopyTextureCHROMIUM can handle RGB5_A1 internalformat by doing a
  // fallback path, but it doesn't know the type info. So, we still cannot do
  // the fallback path in glCopyTextureCHROMIUM for
  // RGBA/RGBA/UNSIGNED_SHORT_5_5_5_1 format and type combination.
  if (params.type == GL_UNSIGNED_SHORT_5_5_5_1)
    return false;
#endif

  // TODO(kbr): continued bugs are seen on Linux with AMD's drivers handling
  // uploads to R8UI textures. crbug.com/710673
  if (params.format == GL_RED_INTEGER)
    return false;

#if BUILDFLAG(IS_ANDROID)
  // TODO(kbr): bugs were seen on Android devices with NVIDIA GPUs
  // when copying hardware-accelerated video textures to
  // floating-point textures. Investigate the root cause of this and
  // fix it. crbug.com/710874
  if (params.type == GL_FLOAT)
    return false;
#endif

  // OES_texture_half_float doesn't support HALF_FLOAT_OES type for
  // CopyTexImage/CopyTexSubImage. And OES_texture_half_float doesn't require
  // HALF_FLOAT_OES type texture to be renderable. So, HALF_FLOAT_OES type
  // texture cannot be copied to or drawn to by glCopyTextureCHROMIUM.
  if (params.type == GL_HALF_FLOAT_OES)
    return false;

  // TODO(https://crbug.com/612542): Implement GPU-to-GPU copy path for more
  // cases, like copying to layers of 3D textures, and elements of 2D texture
  // arrays.
  if (params.function_id != kTexImage2D && params.function_id != kTexSubImage2D)
    return false;

  return true;
}

void WebGLRenderingContextBase::TexImageViaGPU(
    TexImageParams params,
    AcceleratedStaticBitmapImage* source_image,
    WebGLRenderingContextBase* source_canvas_webgl_context) {
  WebGLTexture* texture = ValidateTexImageBinding(params);
  if (!texture)
    return;

  // source in Y-down coordinate space -> is_source_origin_top_left = true
  // source in Y-up coordinate space -> is_source_origin_top_left = false
  bool is_source_origin_top_left = false;
  gfx::Size source_size;
  // Only one of `source_image` and `source_canvas_webgl_context` may be
  // specified.
  if (source_image) {
    DCHECK(source_image->IsTextureBacked());
    DCHECK(!source_canvas_webgl_context);
    source_size = source_image->Size();
    is_source_origin_top_left = source_image->IsOriginTopLeft();
  }
  if (source_canvas_webgl_context) {
    DCHECK(!source_image);
    if (source_canvas_webgl_context->isContextLost()) {
      SynthesizeGLError(GL_INVALID_OPERATION,
                        GetTexImageFunctionName(params.function_id),
                        "Can't upload a texture from a lost WebGL context.");
      return;
    }
    source_size = source_canvas_webgl_context->GetDrawingBuffer()->Size();
    is_source_origin_top_left = source_canvas_webgl_context->IsOriginTopLeft();
  }
  if (!params.width)
    params.width = source_size.width();
  if (!params.height)
    params.height = source_size.height();

  if (params.function_id == kTexImage2D)
    TexImageBase(params, nullptr);

  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(this);
  ScopedTexture2DRestorer restorer(this);

  GLuint target_texture = texture->Object();
  bool possible_direct_copy = false;
  if (params.function_id == kTexImage2D ||
      params.function_id == kTexSubImage2D) {
    possible_direct_copy =
        Extensions3DUtil::CanUseCopyTextureCHROMIUM(params.target);
  }

  // if direct copy is not possible, create a temporary texture and then copy
  // from canvas to temporary texture to target texture.
  if (!possible_direct_copy) {
    ContextGL()->GenTextures(1, &target_texture);
    ContextGL()->BindTexture(GL_TEXTURE_2D, target_texture);
    ContextGL()->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                               GL_NEAREST);
    ContextGL()->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                               GL_NEAREST);
    ContextGL()->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                               GL_CLAMP_TO_EDGE);
    ContextGL()->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                               GL_CLAMP_TO_EDGE);
    ContextGL()->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, *params.width,
                            *params.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                            nullptr);
  }

  {
    // The GPU-GPU copy path uses the Y-up coordinate system.
    gfx::Rect source_sub_rectangle(params.unpack_skip_pixels,
                                   params.unpack_skip_rows, *params.width,
                                   *params.height);

    // source_sub_rectangle is always specified in Y-down coordinate space.
    // Adjust if source is in Y-up coordinate space.
    // If unpack_flip_y is true specified by the caller, adjust it back again.
    // This is equivalent of is_source_origin_top_left == params.unpack_flip_y.
    bool adjust_source_sub_rectangle =
        is_source_origin_top_left == params.unpack_flip_y;
    if (adjust_source_sub_rectangle) {
      source_sub_rectangle.set_y(source_size.height() -
                                 source_sub_rectangle.bottom());
    }

    // The various underlying copy functions require a Y-up rectangle.
    // We need to set flip_y according to source_coordinate system and the
    // unpack_flip_y value specified by the caller.
    // The first transferred pixel should be the upper left corner of the source
    // when params.unpack_flip_y is false. And bottom left corner of the source
    // when params.unpack_flip_y is true.
    bool flip_y = is_source_origin_top_left == params.unpack_flip_y;

    // glCopyTextureCHROMIUM has a DRAW_AND_READBACK path which will call
    // texImage2D. So, reset unpack buffer parameters before that.
    ScopedUnpackParametersResetRestore temporaryResetUnpack(this);
    if (source_image) {
      source_image->CopyToTexture(
          ContextGL(), params.target, target_texture, params.level,
          params.unpack_premultiply_alpha, flip_y,
          gfx::Point(params.xoffset, params.yoffset), source_sub_rectangle);
    } else {
      WebGLRenderingContextBase* gl = source_canvas_webgl_context;
      ScopedTexture2DRestorer inner_restorer(gl);
      if (!gl->GetDrawingBuffer()->CopyToPlatformTexture(
              ContextGL(), params.target, target_texture, params.level,
              params.unpack_premultiply_alpha, flip_y,
              gfx::Point(params.xoffset, params.yoffset), source_sub_rectangle,
              kBackBuffer)) {
        NOTREACHED_IN_MIGRATION();
      }
    }
  }

  if (!possible_direct_copy) {
    GLuint tmp_fbo;
    ContextGL()->GenFramebuffers(1, &tmp_fbo);
    ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, tmp_fbo);
    ContextGL()->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_2D, target_texture, 0);
    ContextGL()->BindTexture(texture->GetTarget(), texture->Object());
    if (params.function_id == kTexImage2D) {
      ContextGL()->CopyTexSubImage2D(params.target, params.level, 0, 0, 0, 0,
                                     *params.width, *params.height);
    } else if (params.function_id == kTexSubImage2D) {
      ContextGL()->CopyTexSubImage2D(params.target, params.level,
                                     params.xoffset, params.yoffset, 0, 0,
                                     *params.width, *params.height);
    } else if (params.function_id == kTexSubImage3D) {
      ContextGL()->CopyTexSubImage3D(
          params.target, params.level, params.xoffset, params.yoffset,
          params.zoffset, 0, 0, *params.width, *params.height);
    }
    ContextGL()->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_2D, 0, 0);
    RestoreCurrentFramebuffer();
    ContextGL()->DeleteFramebuffers(1, &tmp_fbo);
    ContextGL()->DeleteTextures(1, &target_texture);
  }
}

void WebGLRenderingContextBase::TexImageHelperCanvasRenderingContextHost(
    const SecurityOrigin* security_origin,
    TexImageParams params,
    CanvasRenderingContextHost* context_host,
    ExceptionState& exception_state) {
  const char* func_name = GetTexImageFunctionName(params.function_id);
  if (isContextLost())
    return;
  if (!params.width)
    params.width = context_host->width();
  if (!params.height)
    params.height = context_host->height();
  if (!params.depth)
    params.depth = 1;

  // TODO(crbug.com/1210718): It may be possible to simplify this code
  // by consolidating on CanvasImageSource::GetSourceImageForCanvas().

  if (!ValidateCanvasRenderingContextHost(security_origin, func_name,
                                          context_host, exception_state)) {
    return;
  }
  if (!ValidateTexImageBinding(params))
    return;
  if (!ValidateTexFunc(params, *params.width, *params.height)) {
    return;
  }

  // Note that the sub-rectangle validation is needed for the GPU-GPU
  // copy case, but is redundant for the software upload case
  // (texImageImpl).
  bool selecting_sub_rectangle = false;
  if (!ValidateTexImageSubRectangle(params, context_host,
                                    &selecting_sub_rectangle)) {
    return;
  }

  // If the source is a WebGL context, then that context can blit its buffer
  // directly into a texture in this context. This path does not perform color
  // space conversion, so only use it if the source and unpack color spaces are
  // the same.
  if (auto* source_canvas_webgl_context = DynamicTo<WebGLRenderingContextBase>(
          context_host->RenderingContext())) {
    if (CanUseTexImageViaGPU(params) &&
        source_canvas_webgl_context->drawing_buffer_color_space_ ==
            unpack_color_space_) {
      TexImageViaGPU(params, nullptr, source_canvas_webgl_context);
      return;
    }
  }

  SourceImageStatus source_image_status = kInvalidSourceImageStatus;
  scoped_refptr<Image> image = context_host->GetSourceImageForCanvas(
      FlushReason::kWebGLTexImage, &source_image_status,
      gfx::SizeF(*params.width, *params.height), kPremultiplyAlpha);
  if (source_image_status != kNormalSourceImageStatus)
    return;

  // The implementation of GetSourceImageForCanvas for both subclasses of
  // CanvasRenderingContextHost (HTMLCanvasElement and OffscreenCanvas) always
  // return a StaticBitmapImage.
  StaticBitmapImage* static_bitmap_image =
      DynamicTo<StaticBitmapImage>(image.get());
  DCHECK(static_bitmap_image);

  const bool source_has_flip_y = is_origin_top_left_ && context_host->IsWebGL();
  const bool allow_copy_via_gpu = true;
  TexImageStaticBitmapImage(params, static_bitmap_image, source_has_flip_y,
                            allow_copy_via_gpu);
}

void WebGLRenderingContextBase::texImage2D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    CanvasRenderingContextHost* context_host,
    ExceptionState& exception_state) {
  TexImageParams params;
  POPULATE_TEX_IMAGE_2D_PARAMS(params, kSourceHTMLCanvasElement);
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  TexImageHelperCanvasRenderingContextHost(
      execution_context->GetSecurityOrigin(), params, context_host,
      exception_state);
}

void WebGLRenderingContextBase::TexImageHelperHTMLVideoElement(
    const SecurityOrigin* security_origin,
    TexImageParams params,
    HTMLVideoElement* video,
    ExceptionState& exception_state) {
  const char* func_name = GetTexImageFunctionName(params.function_id);
  if (isContextLost())
    return;

  // TODO(crbug.com/1210718): It may be possible to simplify this code
  // by consolidating on CanvasImageSource::GetSourceImageForCanvas().

  if (!ValidateHTMLVideoElement(security_origin, func_name, video,
                                exception_state)) {
    return;
  }

  WebGLTexture* texture = ValidateTexImageBinding(params);
  if (!texture)
    return;
  if (!ValidateTexFunc(params, video->videoWidth(), video->videoHeight())) {
    return;
  }

  media::PaintCanvasVideoRenderer* video_renderer = nullptr;
  scoped_refptr<media::VideoFrame> media_video_frame;
  if (auto* wmp = video->GetWebMediaPlayer()) {
    media_video_frame = wmp->GetCurrentFrameThenUpdate();
    video_renderer = wmp->GetPaintCanvasVideoRenderer();
  }

  if (!media_video_frame || !video_renderer)
    return;

  // This is enforced by ValidateHTMLVideoElement(), but DCHECK to be sure.
  DCHECK(!WouldTaintCanvasOrigin(video));
  TexImageHelperMediaVideoFrame(params, texture, std::move(media_video_frame),
                                video_renderer);
}

void WebGLRenderingContextBase::TexImageHelperVideoFrame(
    const SecurityOrigin* security_origin,
    TexImageParams params,
    VideoFrame* frame,
    ExceptionState& exception_state) {
  const char* func_name = GetTexImageFunctionName(params.function_id);
  if (isContextLost())
    return;

  // TODO(crbug.com/1210718): It may be possible to simplify this code
  // by consolidating on CanvasImageSource::GetSourceImageForCanvas().

  WebGLTexture* texture = ValidateTexImageBinding(params);
  if (!texture)
    return;

  auto local_handle = frame->handle()->CloneForInternalUse();
  if (!local_handle) {
    SynthesizeGLError(GL_INVALID_OPERATION, func_name,
                      "can't texture a closed VideoFrame.");
    return;
  }

  const auto natural_size = local_handle->frame()->natural_size();
  if (!ValidateTexFunc(params, natural_size.width(), natural_size.height())) {
    return;
  }

  // Some blink::VideoFrame objects reference a SkImage which can be used
  // directly instead of making a copy through the VideoFrame.
  if (auto sk_img = local_handle->sk_image()) {
    DCHECK(!sk_img->isTextureBacked());
    auto image = UnacceleratedStaticBitmapImage::Create(std::move(sk_img));
    // Note: kHtmlDomVideo means alpha won't be unmultiplied.
    TexImageStaticBitmapImage(params, image.get(), /*image_has_flip_y=*/false,
                              /*allow_copy_via_gpu=*/false);
    return;
  }

  TexImageHelperMediaVideoFrame(params, texture, local_handle->frame(),
                                nullptr);
}

void WebGLRenderingContextBase::TexImageHelperMediaVideoFrame(
    TexImageParams params,
    WebGLTexture* texture,
    scoped_refptr<media::VideoFrame> media_video_frame,
    media::PaintCanvasVideoRenderer* video_renderer) {
  DCHECK(!isContextLost());
  DCHECK(texture);
  DCHECK(media_video_frame);

  // Paths that use the PaintCanvasVideoRenderer assume the target is sRGB, and
  // produce incorrect results when the unpack color space is not sRGB.
  const bool unpack_color_space_is_srgb =
      unpack_color_space_ == PredefinedColorSpace::kSRGB;

  // The CopyTexImage fast paths can't handle orientation, so if a non-default
  // orientation is provided, we must disable them.
  const auto transform = media_video_frame->metadata().transformation.value_or(
      media::kNoTransformation);
  const GLint adjusted_internalformat =
      ConvertTexInternalFormat(params.internalformat, params.type);
  const bool source_image_rect_is_default =
      params.unpack_skip_pixels == 0 && params.unpack_skip_rows == 0 &&
      (!params.width ||
       *params.width == media_video_frame->natural_size().width()) &&
      (!params.height ||
       *params.height == media_video_frame->natural_size().height());
  const auto& caps = GetDrawingBuffer()->ContextProvider()->GetCapabilities();
  const bool may_need_image_external_essl3 =
      caps.egl_image_external &&
      Extensions3DUtil::CopyTextureCHROMIUMNeedsESSL3(params.internalformat);
  const bool have_image_external_essl3 = caps.egl_image_external_essl3;
  const bool use_copy_texture_chromium =
      params.function_id == kTexImage2D && source_image_rect_is_default &&
      params.depth.value_or(1) == 1 && GL_TEXTURE_2D == params.target &&
      (have_image_external_essl3 || !may_need_image_external_essl3) &&
      CanUseTexImageViaGPU(params) && transform == media::kNoTransformation &&
      unpack_color_space_is_srgb;

  // Callers may chose to provide a renderer which ensures that generated
  // intermediates will be cached across TexImage calls for the same frame.
  std::unique_ptr<media::PaintCanvasVideoRenderer> local_video_renderer;
  if (!video_renderer) {
    local_video_renderer = std::make_unique<media::PaintCanvasVideoRenderer>();
    video_renderer = local_video_renderer.get();
  }

  // Format of source VideoFrame may be 16-bit format, e.g. Y16
  // format. glCopyTextureCHROMIUM requires the source texture to be in
  // 8-bit format. Converting 16-bits formatted source texture to 8-bits
  // formatted texture will cause precision lost. So, uploading such video
  // texture to half float or float texture can not use GPU-GPU path.
  if (use_copy_texture_chromium) {
    DCHECK(Extensions3DUtil::CanUseCopyTextureCHROMIUM(params.target));
    DCHECK_EQ(params.xoffset, 0);
    DCHECK_EQ(params.yoffset, 0);
    DCHECK_EQ(params.zoffset, 0);

    viz::RasterContextProvider* raster_context_provider = nullptr;
    if (auto wrapper = SharedGpuContext::ContextProviderWrapper()) {
      if (auto* context_provider = wrapper->ContextProvider())
        raster_context_provider = context_provider->RasterContextProvider();
    }

    // Go through the fast path doing a GPU-GPU textures copy without a readback
    // to system memory if possible.  Otherwise, it will fall back to the normal
    // SW path.

    if (media_video_frame->HasSharedImage() &&
        video_renderer->CopyVideoFrameTexturesToGLTexture(
            raster_context_provider, ContextGL(), media_video_frame,
            params.target, texture->Object(), adjusted_internalformat,
            params.format, params.type, params.level, unpack_premultiply_alpha_,
            unpack_flip_y_)) {
      return;
    }

    // For certain video frame formats (e.g. I420/YUV), if they start on the CPU
    // (e.g. video camera frames): upload them to the GPU, do a GPU decode, and
    // then copy into the target texture.
    //
    // TODO(crbug.com/1180879): I420A should be supported, but currently fails
    // conformance/textures/misc/texture-video-transparent.html.
    if (!media_video_frame->HasSharedImage() &&
        media::IsOpaque(media_video_frame->format()) &&
        video_renderer->CopyVideoFrameYUVDataToGLTexture(
            raster_context_provider, ContextGL(), media_video_frame,
            params.target, texture->Object(), adjusted_internalformat,
            params.format, params.type, params.level, unpack_premultiply_alpha_,
            unpack_flip_y_)) {
      return;
    }
  }

  if (source_image_rect_is_default && media_video_frame->IsMappable() &&
      media_video_frame->format() == media::PIXEL_FORMAT_Y16 &&
      unpack_color_space_is_srgb) {
    // Try using optimized CPU-GPU path for some formats: e.g. Y16 and Y8. It
    // leaves early for other formats or if frame is stored on GPU.
    ScopedUnpackParametersResetRestore unpack_params(
        this, unpack_flip_y_ || unpack_premultiply_alpha_);

    const bool premultiply_alpha =
        unpack_premultiply_alpha_ && unpack_colorspace_conversion_ == GL_NONE;

    if (params.function_id == kTexImage2D &&
        media::PaintCanvasVideoRenderer::TexImage2D(
            params.target, texture->Object(), ContextGL(), caps,
            media_video_frame.get(), params.level, adjusted_internalformat,
            params.format, params.type, unpack_flip_y_, premultiply_alpha)) {
      return;
    } else if (params.function_id == kTexSubImage2D &&
               media::PaintCanvasVideoRenderer::TexSubImage2D(
                   params.target, ContextGL(), media_video_frame.get(),
                   params.level, params.format, params.type, params.xoffset,
                   params.yoffset, unpack_flip_y_, premultiply_alpha)) {
      return;
    }
  }

  // TODO(crbug.com/1175907): Double check that the premultiply alpha settings
  // are all correct below. When we go through the CanvasResourceProvider for
  // Image creation, SkImageInfo { kPremul_SkAlphaType } is used.
  //
  // We probably need some stronger checks on the accelerated upload path if
  // unmultiply has been requested or we need to never premultiply for Image
  // creation from a VideoFrame.

#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/1180726): Sampling from macOS IOSurfaces requires
  // GL_ARB_texture_rectangle which is not available in the WebGL context.
  constexpr bool kAllowZeroCopyImages = false;
#else
  constexpr bool kAllowZeroCopyImages = true;
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  // TODO(crbug.com/1175907): Only TexImage2D seems to work with the GPU path on
  // Android M -- appears to work fine on R, but to avoid regressions in <video>
  // limit to TexImage2D only for now. Fails conformance test on Nexus 5X:
  // conformance/textures/misc/texture-corner-case-videos.html
  //
  // TODO(crbug.com/1181562): TexSubImage2D via the GPU path performs poorly on
  // Linux when used with ShMem GpuMemoryBuffer backed frames. We don't have a
  // way to differentiate between true texture backed frames and ShMem GMBs, so
  // for now limit GPU texturing to TexImage2D.
  const bool function_supports_gpu_teximage = params.function_id == kTexImage2D;
#else
  const bool function_supports_gpu_teximage =
      params.function_id == kTexImage2D || params.function_id == kTexSubImage2D;
#endif

  const bool can_upload_via_gpu = function_supports_gpu_teximage &&
                                  CanUseTexImageViaGPU(params) &&
                                  source_image_rect_is_default;

  // If we can upload via GPU, try to to use an accelerated resource provider
  // configured appropriately for video. Otherwise use the software cache.
  auto& image_cache =
      can_upload_via_gpu ? generated_video_cache_ : generated_image_cache_;

  // Orient the destination rect based on the frame's transform.
  const auto& visible_rect = media_video_frame->visible_rect();
  auto dest_rect = gfx::Rect(visible_rect.size());
  if (transform.rotation == media::VIDEO_ROTATION_90 ||
      transform.rotation == media::VIDEO_ROTATION_270) {
    dest_rect.Transpose();
  }

  // TODO(https://crbug.com/1341235): The choice of color type will clamp
  // higher precision sources to 8 bit per color.
  const auto resource_provider_info = SkImageInfo::Make(
      gfx::SizeToSkISize(dest_rect.size()), kN32_SkColorType,
      media::IsOpaque(media_video_frame->format()) ? kOpaque_SkAlphaType
                                                   : kPremul_SkAlphaType,
      media_video_frame->CompatRGBColorSpace().ToSkColorSpace());

  // Since TexImageStaticBitmapImage() and TexImageGPU() don't know how to
  // handle tagged orientation, we set |prefer_tagged_orientation| to false.
  scoped_refptr<StaticBitmapImage> image = CreateImageFromVideoFrame(
      std::move(media_video_frame), kAllowZeroCopyImages,
      image_cache.GetCanvasResourceProvider(resource_provider_info),
      video_renderer, dest_rect, /*prefer_tagged_orientation=*/false);
  if (!image)
    return;

  TexImageStaticBitmapImage(params, image.get(), /*image_has_flip_y=*/false,
                            can_upload_via_gpu);
}

void WebGLRenderingContextBase::texImage2D(ScriptState* script_state,
                                           GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           HTMLVideoElement* video,
                                           ExceptionState& exception_state) {
  TexImageParams params;
  POPULATE_TEX_IMAGE_2D_PARAMS(params, kSourceHTMLVideoElement);
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  TexImageHelperHTMLVideoElement(execution_context->GetSecurityOrigin(), params,
                                 video, exception_state);
}

void WebGLRenderingContextBase::texImage2D(ScriptState* script_state,
                                           GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           VideoFrame* frame,
                                           ExceptionState& exception_state) {
  TexImageParams params;
  POPULATE_TEX_IMAGE_2D_PARAMS(params, kSourceVideoFrame);
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  TexImageHelperVideoFrame(execution_context->GetSecurityOrigin(), params,
                           frame, exception_state);
}

void WebGLRenderingContextBase::TexImageHelperImageBitmap(
    TexImageParams params,
    ImageBitmap* bitmap,
    ExceptionState& exception_state) {
  const char* func_name = GetTexImageFunctionName(params.function_id);
  if (isContextLost())
    return;

  // TODO(crbug.com/1210718): It may be possible to simplify this code
  // by consolidating on CanvasImageSource::GetSourceImageForCanvas().

  if (!ValidateImageBitmap(func_name, bitmap, exception_state))
    return;
  if (!ValidateTexImageBinding(params))
    return;

  if (!params.width)
    params.width = bitmap->width();
  if (!params.height)
    params.height = bitmap->height();
  if (!params.depth)
    params.depth = 1;
  bool selecting_sub_rectangle = false;
  if (!ValidateTexImageSubRectangle(params, bitmap, &selecting_sub_rectangle)) {
    return;
  }

  if (!ValidateTexFunc(params, std::nullopt, std::nullopt)) {
    return;
  }

  auto static_bitmap_image = bitmap->BitmapImage();
  DCHECK(static_bitmap_image);

  // When TexImage is called with an ImageBitmap, the values of UNPACK_FLIP_Y,
  // UNPACK_PREMULTIPLY_ALPHA, and UNPACK_COLORSPACE_CONVERSION are to be
  // ignored. Set `adjusted_params` such that no conversions will be made using
  // that state.
  params.unpack_premultiply_alpha =
      static_bitmap_image->GetSkColorInfo().alphaType() == kPremul_SkAlphaType;
  params.unpack_flip_y = false;
  const bool image_has_flip_y = false;
  // TODO(kbr): make this work for sub-rectangles of ImageBitmaps.
  const bool can_copy_via_gpu = !selecting_sub_rectangle;
  TexImageStaticBitmapImage(params, static_bitmap_image.get(), image_has_flip_y,
                            can_copy_via_gpu);
}

void WebGLRenderingContextBase::texImage2D(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           ImageBitmap* bitmap,
                                           ExceptionState& exception_state) {
  TexImageParams params;
  POPULATE_TEX_IMAGE_2D_PARAMS(params, kSourceImageBitmap);
  TexImageHelperImageBitmap(params, bitmap, exception_state);
}

void WebGLRenderingContextBase::TexParameter(GLenum target,
                                             GLenum pname,
                                             GLfloat paramf,
                                             GLint parami,
                                             bool is_float) {
  if (isContextLost())
    return;
  if (!ValidateTextureBinding("texParameter", target))
    return;
  switch (pname) {
    case GL_TEXTURE_MIN_FILTER:
      break;
    case GL_TEXTURE_MAG_FILTER:
      break;
    case GL_TEXTURE_WRAP_R:
      if (!IsWebGL2()) {
        SynthesizeGLError(GL_INVALID_ENUM, "texParameter",
                          "invalid parameter name");
        return;
      }
      [[fallthrough]];
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
      if (paramf == GL_MIRROR_CLAMP_TO_EDGE_EXT ||
          parami == GL_MIRROR_CLAMP_TO_EDGE_EXT) {
        if (!ExtensionEnabled(kEXTTextureMirrorClampToEdgeName)) {
          SynthesizeGLError(GL_INVALID_ENUM, "texParameter",
                            "invalid parameter, "
                            "EXT_texture_mirror_clamp_to_edge not enabled");
          return;
        }
        break;
      }
      if ((is_float && paramf != GL_CLAMP_TO_EDGE &&
           paramf != GL_MIRRORED_REPEAT && paramf != GL_REPEAT) ||
          (!is_float && parami != GL_CLAMP_TO_EDGE &&
           parami != GL_MIRRORED_REPEAT && parami != GL_REPEAT)) {
        SynthesizeGLError(GL_INVALID_ENUM, "texParameter", "invalid parameter");
        return;
      }
      break;
    case GL_TEXTURE_MAX_ANISOTROPY_EXT:  // EXT_texture_filter_anisotropic
      if (!ExtensionEnabled(kEXTTextureFilterAnisotropicName)) {
        SynthesizeGLError(
            GL_INVALID_ENUM, "texParameter",
            "invalid parameter, EXT_texture_filter_anisotropic not enabled");
        return;
      }
      break;
    case GL_TEXTURE_COMPARE_FUNC:
    case GL_TEXTURE_COMPARE_MODE:
    case GL_TEXTURE_BASE_LEVEL:
    case GL_TEXTURE_MAX_LEVEL:
    case GL_TEXTURE_MAX_LOD:
    case GL_TEXTURE_MIN_LOD:
      if (!IsWebGL2()) {
        SynthesizeGLError(GL_INVALID_ENUM, "texParameter",
                          "invalid parameter name");
        return;
      }
      break;
    case GL_DEPTH_STENCIL_TEXTURE_MODE_ANGLE:
      if (!ExtensionEnabled(kWebGLStencilTexturingName)) {
        SynthesizeGLError(
            GL_INVALID_ENUM, "texParameter",
            "invalid parameter name, WEBGL_stencil_texturing not enabled");
        return;
      }
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "texParameter",
                        "invalid parameter name");
      return;
  }
  if (is_float) {
    ContextGL()->TexParameterf(target, pname, paramf);
  } else {
    ContextGL()->TexParameteri(target, pname, parami);
  }
}

void WebGLRenderingContextBase::texParameterf(GLenum target,
                                              GLenum pname,
                                              GLfloat param) {
  TexParameter(target, pname, param, 0, true);
}

void WebGLRenderingContextBase::texParameteri(GLenum target,
                                              GLenum pname,
                                              GLint param) {
  TexParameter(target, pname, 0, param, false);
}

void WebGLRenderingContextBase::texSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    MaybeShared<DOMArrayBufferView> pixels) {
  TexImageParams params;
  POPULATE_TEX_SUB_IMAGE_2D_PARAMS(params, kSourceArrayBufferView);
  params.width = width;
  params.height = height;
  params.depth = 1;
  TexImageHelperDOMArrayBufferView(params, pixels.Get(), kNullNotAllowed, 0);
}

void WebGLRenderingContextBase::texSubImage2D(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLenum format,
                                              GLenum type,
                                              ImageData* pixels) {
  TexImageParams params;
  POPULATE_TEX_SUB_IMAGE_2D_PARAMS(params, kSourceImageData);
  TexImageHelperImageData(params, pixels);
}

void WebGLRenderingContextBase::texSubImage2D(ScriptState* script_state,
                                              GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLenum format,
                                              GLenum type,
                                              HTMLImageElement* image,
                                              ExceptionState& exception_state) {
  TexImageParams params;
  POPULATE_TEX_SUB_IMAGE_2D_PARAMS(params, kSourceHTMLImageElement);
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  TexImageHelperHTMLImageElement(execution_context->GetSecurityOrigin(), params,
                                 image, exception_state);
}

void WebGLRenderingContextBase::texSubImage2D(
    ScriptState* script_state,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLenum format,
    GLenum type,
    CanvasRenderingContextHost* context_host,
    ExceptionState& exception_state) {
  TexImageParams params;
  POPULATE_TEX_SUB_IMAGE_2D_PARAMS(params, kSourceHTMLCanvasElement);
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  TexImageHelperCanvasRenderingContextHost(
      execution_context->GetSecurityOrigin(), params, context_host,
      exception_state);
}

void WebGLRenderingContextBase::texSubImage2D(ScriptState* script_state,
                                              GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLenum format,
                                              GLenum type,
                                              HTMLVideoElement* video,
                                              ExceptionState& exception_state) {
  TexImageParams params;
  POPULATE_TEX_SUB_IMAGE_2D_PARAMS(params, kSourceHTMLVideoElement);
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  TexImageHelperHTMLVideoElement(execution_context->GetSecurityOrigin(), params,
                                 video, exception_state);
}

void WebGLRenderingContextBase::texSubImage2D(ScriptState* script_state,
                                              GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLenum format,
                                              GLenum type,
                                              VideoFrame* frame,
                                              ExceptionState& exception_state) {
  TexImageParams params;
  POPULATE_TEX_SUB_IMAGE_2D_PARAMS(params, kSourceVideoFrame);
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  TexImageHelperVideoFrame(execution_context->GetSecurityOrigin(), params,
                           frame, exception_state);
}

void WebGLRenderingContextBase::texSubImage2D(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLenum format,
                                              GLenum type,
                                              ImageBitmap* bitmap,
                                              ExceptionState& exception_state) {
  TexImageParams params;
  POPULATE_TEX_SUB_IMAGE_2D_PARAMS(params, kSourceImageBitmap);
  TexImageHelperImageBitmap(params, bitmap, exception_state);
}

void WebGLRenderingContextBase::uniform1f(const WebGLUniformLocation* location,
                                          GLfloat x) {
  if (isContextLost() || !location)
    return;

  if (!ValidateUniformLocation("uniform1f", location, current_program_)) {
    return;
  }

  ContextGL()->Uniform1f(location->Location(), x);
}

void WebGLRenderingContextBase::uniform1fv(const WebGLUniformLocation* location,
                                           base::span<const GLfloat> v) {
  const GLfloat* data;
  GLuint length;
  if (isContextLost() ||
      !ValidateUniformParameters("uniform1fv", location, v, 1, 0, v.size(),
                                 &data, &length)) {
    return;
  }

  ContextGL()->Uniform1fv(location->Location(), length, data);
}

void WebGLRenderingContextBase::uniform1i(const WebGLUniformLocation* location,
                                          GLint x) {
  if (isContextLost() || !location)
    return;

  if (!ValidateUniformLocation("uniform1i", location, current_program_)) {
    return;
  }

  ContextGL()->Uniform1i(location->Location(), x);
}

void WebGLRenderingContextBase::uniform1iv(const WebGLUniformLocation* location,
                                           base::span<const GLint> v) {
  const GLint* data;
  GLuint length;
  if (isContextLost() ||
      !ValidateUniformParameters("uniform1iv", location, v, 1, 0, v.size(),
                                 &data, &length)) {
    return;
  }

  ContextGL()->Uniform1iv(location->Location(), length, data);
}

void WebGLRenderingContextBase::uniform2f(const WebGLUniformLocation* location,
                                          GLfloat x,
                                          GLfloat y) {
  if (isContextLost() || !location)
    return;

  if (!ValidateUniformLocation("uniform2f", location, current_program_)) {
    return;
  }

  ContextGL()->Uniform2f(location->Location(), x, y);
}

void WebGLRenderingContextBase::uniform2fv(const WebGLUniformLocation* location,
                                           base::span<const GLfloat> v) {
  const GLfloat* data;
  GLuint length;
  if (isContextLost() ||
      !ValidateUniformParameters("uniform2fv", location, v, 2, 0, v.size(),
                                 &data, &length)) {
    return;
  }

  ContextGL()->Uniform2fv(location->Location(), length, data);
}

void WebGLRenderingContextBase::uniform2i(const WebGLUniformLocation* location,
                                          GLint x,
                                          GLint y) {
  if (isContextLost() || !location)
    return;

  if (!ValidateUniformLocation("uniform2i", location, current_program_)) {
    return;
  }

  ContextGL()->Uniform2i(location->Location(), x, y);
}

void WebGLRenderingContextBase::uniform2iv(const WebGLUniformLocation* location,
                                           base::span<const GLint> v) {
  const GLint* data;
  GLuint length;
  if (isContextLost() ||
      !ValidateUniformParameters("uniform2iv", location, v, 2, 0, v.size(),
                                 &data, &length)) {
    return;
  }

  ContextGL()->Uniform2iv(location->Location(), length, data);
}

void WebGLRenderingContextBase::uniform3f(const WebGLUniformLocation* location,
                                          GLfloat x,
                                          GLfloat y,
                                          GLfloat z) {
  if (isContextLost() || !location)
    return;

  if (!ValidateUniformLocation("uniform3f", location, current_program_)) {
    return;
  }

  ContextGL()->Uniform3f(location->Location(), x, y, z);
}

void WebGLRenderingContextBase::uniform3fv(const WebGLUniformLocation* location,
                                           base::span<const GLfloat> v) {
  const GLfloat* data;
  GLuint length;
  if (isContextLost() ||
      !ValidateUniformParameters("uniform3fv", location, v, 3, 0, v.size(),
                                 &data, &length)) {
    return;
  }

  ContextGL()->Uniform3fv(location->Location(), length, data);
}

void WebGLRenderingContextBase::uniform3i(const WebGLUniformLocation* location,
                                          GLint x,
                                          GLint y,
                                          GLint z) {
  if (isContextLost() || !location)
    return;

  if (!ValidateUniformLocation("uniform3i", location, current_program_)) {
    return;
  }

  ContextGL()->Uniform3i(location->Location(), x, y, z);
}

void WebGLRenderingContextBase::uniform3iv(const WebGLUniformLocation* location,
                                           base::span<const GLint> v) {
  const GLint* data;
  GLuint length;
  if (isContextLost() ||
      !ValidateUniformParameters("uniform3iv", location, v, 3, 0, v.size(),
                                 &data, &length)) {
    return;
  }

  ContextGL()->Uniform3iv(location->Location(), length, data);
}

void WebGLRenderingContextBase::uniform4f(const WebGLUniformLocation* location,
                                          GLfloat x,
                                          GLfloat y,
                                          GLfloat z,
                                          GLfloat w) {
  if (isContextLost() || !location)
    return;

  if (!ValidateUniformLocation("uniform4f", location, current_program_)) {
    return;
  }

  ContextGL()->Uniform4f(location->Location(), x, y, z, w);
}

void WebGLRenderingContextBase::uniform4fv(const WebGLUniformLocation* location,
                                           base::span<const GLfloat> v) {
  const GLfloat* data;
  GLuint length;
  if (isContextLost() ||
      !ValidateUniformParameters("uniform4fv", location, v, 4, 0, v.size(),
                                 &data, &length)) {
    return;
  }

  ContextGL()->Uniform4fv(location->Location(), length, data);
}

void WebGLRenderingContextBase::uniform4i(const WebGLUniformLocation* location,
                                          GLint x,
                                          GLint y,
                                          GLint z,
                                          GLint w) {
  if (isContextLost() || !location)
    return;

  if (!ValidateUniformLocation("uniform4i", location, current_program_)) {
    return;
  }

  ContextGL()->Uniform4i(location->Location(), x, y, z, w);
}

void WebGLRenderingContextBase::uniform4iv(const WebGLUniformLocation* location,
                                           base::span<const GLint> v) {
  const GLint* data;
  GLuint length;
  if (isContextLost() ||
      !ValidateUniformParameters("uniform4iv", location, v, 4, 0, v.size(),
                                 &data, &length)) {
    return;
  }

  ContextGL()->Uniform4iv(location->Location(), length, data);
}

void WebGLRenderingContextBase::uniformMatrix2fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v) {
  const GLfloat* data;
  GLuint length;
  if (isContextLost() ||
      !ValidateUniformMatrixParameters("uniformMatrix2fv", location, transpose,
                                       v, 4, 0, v.size(), &data, &length)) {
    return;
  }
  ContextGL()->UniformMatrix2fv(location->Location(), length, transpose, data);
}

void WebGLRenderingContextBase::uniformMatrix3fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v) {
  const GLfloat* data;
  GLuint length;
  if (isContextLost() ||
      !ValidateUniformMatrixParameters("uniformMatrix3fv", location, transpose,
                                       v, 9, 0, v.size(), &data, &length)) {
    return;
  }
  ContextGL()->UniformMatrix3fv(location->Location(), length, transpose, data);
}

void WebGLRenderingContextBase::uniformMatrix4fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    base::span<const GLfloat> v) {
  const GLfloat* data;
  GLuint length;
  if (isContextLost() ||
      !ValidateUniformMatrixParameters("uniformMatrix4fv", location, transpose,
                                       v, 16, 0, v.size(), &data, &length)) {
    return;
  }
  ContextGL()->UniformMatrix4fv(location->Location(), length, transpose, data);
}

void WebGLRenderingContextBase::useProgram(WebGLProgram* program) {
  if (!ValidateNullableWebGLObject("useProgram", program))
    return;
  if (program && !program->LinkStatus(this)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "useProgram", "program not valid");
    return;
  }

  if (current_program_ != program) {
    if (current_program_)
      current_program_->OnDetached(ContextGL());
    current_program_ = program;
    ContextGL()->UseProgram(ObjectOrZero(program));
    if (program)
      program->OnAttached();
  }
}

void WebGLRenderingContextBase::validateProgram(WebGLProgram* program) {
  if (!ValidateWebGLProgramOrShader("validateProgram", program))
    return;
  ContextGL()->ValidateProgram(ObjectOrZero(program));
}

void WebGLRenderingContextBase::SetVertexAttribType(
    GLuint index,
    VertexAttribValueType type) {
  if (index < max_vertex_attribs_)
    vertex_attrib_type_[index] = type;
}

void WebGLRenderingContextBase::vertexAttrib1f(GLuint index, GLfloat v0) {
  if (isContextLost())
    return;
  ContextGL()->VertexAttrib1f(index, v0);
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib1fv(GLuint index,
                                                base::span<const GLfloat> v) {
  if (isContextLost())
    return;
  if (v.size() < 1) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttrib1fv", "invalid array");
    return;
  }
  ContextGL()->VertexAttrib1fv(index, v.data());
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib2f(GLuint index,
                                               GLfloat v0,
                                               GLfloat v1) {
  if (isContextLost())
    return;
  ContextGL()->VertexAttrib2f(index, v0, v1);
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib2fv(GLuint index,
                                                base::span<const GLfloat> v) {
  if (isContextLost())
    return;
  if (v.size() < 2) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttrib2fv", "invalid array");
    return;
  }
  ContextGL()->VertexAttrib2fv(index, v.data());
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib3f(GLuint index,
                                               GLfloat v0,
                                               GLfloat v1,
                                               GLfloat v2) {
  if (isContextLost())
    return;
  ContextGL()->VertexAttrib3f(index, v0, v1, v2);
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib3fv(GLuint index,
                                                base::span<const GLfloat> v) {
  if (isContextLost())
    return;
  if (v.size() < 3) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttrib3fv", "invalid array");
    return;
  }
  ContextGL()->VertexAttrib3fv(index, v.data());
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib4f(GLuint index,
                                               GLfloat v0,
                                               GLfloat v1,
                                               GLfloat v2,
                                               GLfloat v3) {
  if (isContextLost())
    return;
  ContextGL()->VertexAttrib4f(index, v0, v1, v2, v3);
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib4fv(GLuint index,
                                                base::span<const GLfloat> v) {
  if (isContextLost())
    return;
  if (v.size() < 4) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttrib4fv", "invalid array");
    return;
  }
  ContextGL()->VertexAttrib4fv(index, v.data());
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttribPointer(GLuint index,
                                                    GLint size,
                                                    GLenum type,
                                                    GLboolean normalized,
                                                    GLsizei stride,
                                                    int64_t offset) {
  if (isContextLost())
    return;
  if (index >= max_vertex_attribs_) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttribPointer",
                      "index out of range");
    return;
  }
  if (!ValidateValueFitNonNegInt32("vertexAttribPointer", "offset", offset))
    return;
  if (!bound_array_buffer_ && offset != 0) {
    SynthesizeGLError(GL_INVALID_OPERATION, "vertexAttribPointer",
                      "no ARRAY_BUFFER is bound and offset is non-zero");
    return;
  }

  bound_vertex_array_object_->SetArrayBufferForAttrib(
      index, bound_array_buffer_.Get());
  ContextGL()->VertexAttribPointer(
      index, size, type, normalized, stride,
      reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

void WebGLRenderingContextBase::VertexAttribDivisorANGLE(GLuint index,
                                                         GLuint divisor) {
  if (isContextLost())
    return;

  if (index >= max_vertex_attribs_) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttribDivisorANGLE",
                      "index out of range");
    return;
  }

  ContextGL()->VertexAttribDivisorANGLE(index, divisor);
}

void WebGLRenderingContextBase::viewport(GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height) {
  if (isContextLost())
    return;
  ContextGL()->Viewport(x, y, width, height);
}

// Added to provide a unified interface with CanvasRenderingContext2D. Prefer
// calling forceLostContext instead.
void WebGLRenderingContextBase::LoseContext(LostContextMode mode) {
  ForceLostContext(mode, kManual);
}

void WebGLRenderingContextBase::ForceLostContext(
    LostContextMode mode,
    AutoRecoveryMethod auto_recovery_method) {
  if (isContextLost()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "loseContext",
                      "context already lost");
    return;
  }

  context_group_->LoseContextGroup(mode, auto_recovery_method);
}

void WebGLRenderingContextBase::LoseContextImpl(
    WebGLRenderingContextBase::LostContextMode mode,
    AutoRecoveryMethod auto_recovery_method) {
  if (isContextLost())
    return;

  context_lost_mode_ = mode;
  DCHECK_NE(context_lost_mode_, kNotLostContext);
  auto_recovery_method_ = auto_recovery_method;

  // Lose all the extensions.
  for (ExtensionTracker* tracker : extensions_) {
    tracker->LoseExtension(false);
  }

  for (wtf_size_t i = 0; i < kWebGLExtensionNameCount; ++i)
    extension_enabled_[i] = false;

  // This resolver is non-null during a makeXRCompatible call, while waiting
  // for a response from the browser and XR process. If the WebGL context is
  // lost before we get a response, the resolver has to be rejected to be
  // be properly disposed of.
  xr_compatible_ = false;
  CompleteXrCompatiblePromiseIfPending(DOMExceptionCode::kInvalidStateError);

  RemoveAllCompressedTextureFormats();

  if (mode == kRealLostContext) {
    // If it is a real context loss, the signal needs to be propagated to the
    // context host so that it knows all resources are dropped.  Otherwise,
    // OffscreenCanvases on Workers would wait indefinitely for reources to be
    // returned by the compositor, which would stall requestAnimationFrame.
    Host()->NotifyGpuContextLost();

    // If the DrawingBuffer is destroyed during a real lost context event it
    // causes the CommandBufferProxy that the DrawingBuffer owns, which is what
    // issued the lost context event in the first place, to be destroyed before
    // the event is done being handled. This causes a crash when an outstanding
    // AutoLock goes out of scope. To avoid this, we create a no-op task to hold
    // a reference to the DrawingBuffer until this function is done executing.
    task_runner_->PostTask(
        FROM_HERE,
        WTF::BindOnce(&WebGLRenderingContextBase::HoldReferenceToDrawingBuffer,
                      WrapWeakPersistent(this),
                      WTF::RetainedRef(drawing_buffer_)));
  }

  // Always destroy the context, regardless of context loss mode. This will
  // set drawing_buffer_ to null, but it won't actually be destroyed until the
  // above task is executed. drawing_buffer_ is recreated in the event that the
  // context is restored by MaybeRestoreContext. If this was a real lost context
  // the OpenGL calls done during DrawingBuffer destruction will be ignored.
  DestroyContext();

  ConsoleDisplayPreference display =
      (mode == kRealLostContext) ? kDisplayInConsole : kDontDisplayInConsole;
  SynthesizeGLError(GC3D_CONTEXT_LOST_WEBGL, "loseContext", "context lost",
                    display);

  // Don't allow restoration unless the context lost event has both been
  // dispatched and its default behavior prevented.
  restore_allowed_ = false;
  DeactivateContext(this);
  if (auto_recovery_method_ == kWhenAvailable)
    AddToEvictedList(this);

  // Always defer the dispatch of the context lost event, to implement
  // the spec behavior of queueing a task.
  dispatch_context_lost_event_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void WebGLRenderingContextBase::HoldReferenceToDrawingBuffer(DrawingBuffer*) {
  // This function intentionally left blank.
}

void WebGLRenderingContextBase::ForceRestoreContext() {
  if (!isContextLost()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "restoreContext",
                      "context not lost");
    return;
  }

  if (!restore_allowed_) {
    if (context_lost_mode_ == kWebGLLoseContextLostContext)
      SynthesizeGLError(GL_INVALID_OPERATION, "restoreContext",
                        "context restoration not allowed");
    return;
  }

  if (!restore_timer_.IsActive())
    restore_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

uint32_t WebGLRenderingContextBase::NumberOfContextLosses() const {
  return context_group_->NumberOfContextLosses();
}

cc::Layer* WebGLRenderingContextBase::CcLayer() const {
  return isContextLost() ? nullptr : GetDrawingBuffer()->CcLayer();
}

void WebGLRenderingContextBase::SetHdrMetadata(
    const gfx::HDRMetadata& hdr_metadata) {
  if (!isContextLost() && GetDrawingBuffer()) {
    GetDrawingBuffer()->SetHdrMetadata(hdr_metadata);
  }
}

void WebGLRenderingContextBase::SetFilterQuality(
    cc::PaintFlags::FilterQuality filter_quality) {
  if (!isContextLost() && GetDrawingBuffer()) {
    GetDrawingBuffer()->SetFilterQuality(filter_quality);
  }
}

Extensions3DUtil* WebGLRenderingContextBase::ExtensionsUtil() {
  if (!extensions_util_) {
    gpu::gles2::GLES2Interface* gl = ContextGL();
    extensions_util_ = Extensions3DUtil::Create(gl);
    // The only reason the ExtensionsUtil should be invalid is if the gl context
    // is lost.
    DCHECK(extensions_util_->IsValid() ||
           gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR);
  }
  return extensions_util_.get();
}

void WebGLRenderingContextBase::Stop() {
  if (!isContextLost()) {
    // Never attempt to restore the context because the page is being torn down.
    ForceLostContext(kSyntheticLostContext, kManual);
  }
}

void WebGLRenderingContextBase::
    DrawingBufferClientInterruptPixelLocalStorage() {
  if (destruction_in_progress_) {
    return;
  }
  if (!ContextGL()) {
    return;
  }
  if (!has_activated_pixel_local_storage_) {
    return;
  }
  ContextGL()->FramebufferPixelLocalStorageInterruptANGLE();
}

void WebGLRenderingContextBase::DrawingBufferClientRestorePixelLocalStorage() {
  if (destruction_in_progress_) {
    return;
  }
  if (!ContextGL()) {
    return;
  }
  if (!has_activated_pixel_local_storage_) {
    return;
  }
  ContextGL()->FramebufferPixelLocalStorageRestoreANGLE();
}

bool WebGLRenderingContextBase::DrawingBufferClientIsBoundForDraw() {
  return !framebuffer_binding_;
}

void WebGLRenderingContextBase::DrawingBufferClientRestoreScissorTest() {
  if (destruction_in_progress_)
    return;
  if (!ContextGL())
    return;
  if (scissor_enabled_)
    ContextGL()->Enable(GL_SCISSOR_TEST);
  else
    ContextGL()->Disable(GL_SCISSOR_TEST);
}

void WebGLRenderingContextBase::DrawingBufferClientRestoreMaskAndClearValues() {
  if (destruction_in_progress_)
    return;
  if (!ContextGL())
    return;
  bool color_mask_alpha =
      color_mask_[3] && active_scoped_rgb_emulation_color_masks_ == 0;
  if (ExtensionEnabled(kOESDrawBuffersIndexedName)) {
    ContextGL()->ColorMaskiOES(0, color_mask_[0], color_mask_[1],
                               color_mask_[2], color_mask_alpha);
  } else {
    ContextGL()->ColorMask(color_mask_[0], color_mask_[1], color_mask_[2],
                           color_mask_alpha);
  }
  ContextGL()->DepthMask(depth_mask_);
  ContextGL()->StencilMaskSeparate(GL_FRONT, stencil_mask_);

  ContextGL()->ClearColor(clear_color_[0], clear_color_[1], clear_color_[2],
                          clear_color_[3]);
  ContextGL()->ClearDepthf(clear_depth_);
  ContextGL()->ClearStencil(clear_stencil_);
}

void WebGLRenderingContextBase::
    DrawingBufferClientRestorePixelPackParameters() {
  if (destruction_in_progress_)
    return;
  if (!ContextGL())
    return;
  ContextGL()->PixelStorei(GL_PACK_ALIGNMENT, pack_alignment_);
}

void WebGLRenderingContextBase::DrawingBufferClientRestoreTexture2DBinding() {
  if (destruction_in_progress_)
    return;
  if (!ContextGL())
    return;
  RestoreCurrentTexture2D();
}

void WebGLRenderingContextBase::
    DrawingBufferClientRestoreTextureCubeMapBinding() {
  if (destruction_in_progress_)
    return;
  if (!ContextGL())
    return;
  RestoreCurrentTextureCubeMap();
}

void WebGLRenderingContextBase::
    DrawingBufferClientRestoreRenderbufferBinding() {
  if (destruction_in_progress_)
    return;
  if (!ContextGL())
    return;
  ContextGL()->BindRenderbuffer(GL_RENDERBUFFER,
                                ObjectOrZero(renderbuffer_binding_.Get()));
}

void WebGLRenderingContextBase::DrawingBufferClientRestoreFramebufferBinding() {
  if (destruction_in_progress_)
    return;
  if (!ContextGL())
    return;
  RestoreCurrentFramebuffer();
}

void WebGLRenderingContextBase::
    DrawingBufferClientRestorePixelUnpackBufferBinding() {}
void WebGLRenderingContextBase::
    DrawingBufferClientRestorePixelPackBufferBinding() {}

bool WebGLRenderingContextBase::
    DrawingBufferClientUserAllocatedMultisampledRenderbuffers() {
  return number_of_user_allocated_multisampled_renderbuffers_ > 0;
}

void WebGLRenderingContextBase::
    DrawingBufferClientForceLostContextWithAutoRecovery(const char* reason) {
  ForceLostContext(WebGLRenderingContextBase::kSyntheticLostContext,
                   WebGLRenderingContextBase::kAuto);
  if (reason) {
    PrintWarningToConsole(reason);
  }
}

ScriptValue WebGLRenderingContextBase::GetBooleanParameter(
    ScriptState* script_state,
    GLenum pname) {
  GLboolean value = 0;
  if (!isContextLost())
    ContextGL()->GetBooleanv(pname, &value);
  return WebGLAny(script_state, static_cast<bool>(value));
}

ScriptValue WebGLRenderingContextBase::GetBooleanArrayParameter(
    ScriptState* script_state,
    GLenum pname) {
  if (pname != GL_COLOR_WRITEMASK) {
    NOTIMPLEMENTED();
    return WebGLAny(script_state, nullptr, 0);
  }
  std::array<GLboolean, 4> value = {0};
  if (!isContextLost()) {
    ContextGL()->GetBooleanv(pname, value.data());
  }
  std::array<bool, 4> bool_value = {};
  for (int ii = 0; ii < 4; ++ii)
    bool_value[ii] = static_cast<bool>(value[ii]);
  return WebGLAny(script_state, bool_value.data(), 4);
}

ScriptValue WebGLRenderingContextBase::GetFloatParameter(
    ScriptState* script_state,
    GLenum pname) {
  GLfloat value = 0;
  if (!isContextLost())
    ContextGL()->GetFloatv(pname, &value);
  if (IdentifiabilityStudySettings::Get()->ShouldSampleType(
          blink::IdentifiableSurface::Type::kWebGLParameter)) {
    RecordIdentifiableGLParameterDigest(pname, value);
  }
  return WebGLAny(script_state, value);
}

ScriptValue WebGLRenderingContextBase::GetIntParameter(
    ScriptState* script_state,
    GLenum pname) {
  GLint value = 0;
  if (!isContextLost()) {
    ContextGL()->GetIntegerv(pname, &value);
    switch (pname) {
      case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
      case GL_IMPLEMENTATION_COLOR_READ_TYPE:
        if (value == 0) {
          // This indicates read framebuffer is incomplete and an
          // INVALID_OPERATION has been generated.
          return ScriptValue::CreateNull(script_state->GetIsolate());
        }
        break;
      default:
        break;
    }
  }
  if (IdentifiabilityStudySettings::Get()->ShouldSampleType(
          blink::IdentifiableSurface::Type::kWebGLParameter)) {
    RecordIdentifiableGLParameterDigest(pname, value);
  }
  return WebGLAny(script_state, value);
}

ScriptValue WebGLRenderingContextBase::GetInt64Parameter(
    ScriptState* script_state,
    GLenum pname) {
  GLint64 value = 0;
  if (!isContextLost())
    ContextGL()->GetInteger64v(pname, &value);
  return WebGLAny(script_state, value);
}

ScriptValue WebGLRenderingContextBase::GetUnsignedIntParameter(
    ScriptState* script_state,
    GLenum pname) {
  GLint value = 0;
  if (!isContextLost())
    ContextGL()->GetIntegerv(pname, &value);
  return WebGLAny(script_state, static_cast<unsigned>(value));
}

ScriptValue WebGLRenderingContextBase::GetWebGLFloatArrayParameter(
    ScriptState* script_state,
    GLenum pname) {
  std::array<GLfloat, 4> value = {0};
  if (!isContextLost())
    ContextGL()->GetFloatv(pname, value.data());
  unsigned length = 0;
  switch (pname) {
    case GL_ALIASED_POINT_SIZE_RANGE:
    case GL_ALIASED_LINE_WIDTH_RANGE:
    case GL_DEPTH_RANGE:
      length = 2;
      break;
    case GL_BLEND_COLOR:
    case GL_COLOR_CLEAR_VALUE:
      length = 4;
      break;
    default:
      NOTIMPLEMENTED();
  }
  if (ShouldMeasureGLParam(pname)) {
    blink::IdentifiableTokenBuilder builder;
    for (unsigned i = 0; i < length; i++) {
      builder.AddValue(value[i]);
    }
    RecordIdentifiableGLParameterDigest(pname, builder.GetToken());
  }
  return WebGLAny(script_state,
                  DOMFloat32Array::Create(base::span(value).first(length)));
}

ScriptValue WebGLRenderingContextBase::GetWebGLIntArrayParameter(
    ScriptState* script_state,
    GLenum pname) {
  std::array<GLint, 4> value = {0};
  if (!isContextLost())
    ContextGL()->GetIntegerv(pname, value.data());
  unsigned length = 0;
  switch (pname) {
    case GL_MAX_VIEWPORT_DIMS:
      length = 2;
      break;
    case GL_SCISSOR_BOX:
    case GL_VIEWPORT:
      length = 4;
      break;
    default:
      NOTIMPLEMENTED();
  }
  if (ShouldMeasureGLParam(pname)) {
    blink::IdentifiableTokenBuilder builder;
    for (unsigned i = 0; i < length; i++) {
      builder.AddValue(value[i]);
    }
    RecordIdentifiableGLParameterDigest(pname, builder.GetToken());
  }
  return WebGLAny(script_state,
                  DOMInt32Array::Create(base::span(value).first(length)));
}

WebGLTexture* WebGLRenderingContextBase::ValidateTexture2DBinding(
    const char* function_name,
    GLenum target,
    bool validate_opaque_textures) {
  WebGLTexture* tex = nullptr;
  switch (target) {
    case GL_TEXTURE_2D:
      tex = texture_units_[active_texture_unit_].texture2d_binding_.Get();
      break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      tex =
          texture_units_[active_texture_unit_].texture_cube_map_binding_.Get();
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name,
                        "invalid texture target");
      return nullptr;
  }
  if (!tex) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "no texture bound to target");
  } else if (validate_opaque_textures && tex->IsOpaqueTexture()) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "cannot invoke function with an opaque texture");
    return nullptr;
  }
  return tex;
}

WebGLTexture* WebGLRenderingContextBase::ValidateTextureBinding(
    const char* function_name,
    GLenum target) {
  WebGLTexture* tex = nullptr;
  switch (target) {
    case GL_TEXTURE_2D:
      tex = texture_units_[active_texture_unit_].texture2d_binding_.Get();
      break;
    case GL_TEXTURE_CUBE_MAP:
      tex =
          texture_units_[active_texture_unit_].texture_cube_map_binding_.Get();
      break;
    case GL_TEXTURE_3D:
      if (!IsWebGL2()) {
        SynthesizeGLError(GL_INVALID_ENUM, function_name,
                          "invalid texture target");
        return nullptr;
      }
      tex = texture_units_[active_texture_unit_].texture3d_binding_.Get();
      break;
    case GL_TEXTURE_2D_ARRAY:
      if (!IsWebGL2()) {
        SynthesizeGLError(GL_INVALID_ENUM, function_name,
                          "invalid texture target");
        return nullptr;
      }
      tex = texture_units_[active_texture_unit_].texture2d_array_binding_.Get();
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name,
                        "invalid texture target");
      return nullptr;
  }
  if (!tex) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "no texture bound to target");
  }
  return tex;
}

bool WebGLRenderingContextBase::ValidateLocationLength(
    const char* function_name,
    const String& string) {
  const unsigned max_web_gl_location_length = GetMaxWebGLLocationLength();
  if (string.length() > max_web_gl_location_length) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "location length > 256");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateSize(const char* function_name,
                                             GLint x,
                                             GLint y,
                                             GLint z) {
  if (x < 0 || y < 0 || z < 0) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "size < 0");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateCharacter(unsigned char c) {
  // Printing characters are valid except " $ ` @ \ ' DEL.
  if (c >= 32 && c <= 126 && c != '"' && c != '$' && c != '`' && c != '@' &&
      c != '\\' && c != '\'')
    return true;
  // Horizontal tab, line feed, vertical tab, form feed, carriage return
  // are also valid.
  if (c >= 9 && c <= 13)
    return true;
  return false;
}

bool WebGLRenderingContextBase::ValidateString(const char* function_name,
                                               const String& string) {
  for (wtf_size_t i = 0; i < string.length(); ++i) {
    if (!ValidateCharacter(string[i])) {
      SynthesizeGLError(GL_INVALID_VALUE, function_name, "string not ASCII");
      return false;
    }
  }
  return true;
}

bool WebGLRenderingContextBase::IsPrefixReserved(const String& name) {
  if (name.StartsWith("gl_") || name.StartsWith("webgl_") ||
      name.StartsWith("_webgl_"))
    return true;
  return false;
}

bool WebGLRenderingContextBase::ValidateShaderType(const char* function_name,
                                                   GLenum shader_type) {
  switch (shader_type) {
    case GL_VERTEX_SHADER:
    case GL_FRAGMENT_SHADER:
      return true;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid shader type");
      return false;
  }
}

void WebGLRenderingContextBase::AddExtensionSupportedFormatsTypes() {
  if (!is_oes_texture_float_formats_types_added_ &&
      ExtensionEnabled(kOESTextureFloatName)) {
    ADD_VALUES_TO_SET(supported_types_, kSupportedTypesOESTexFloat);
    ADD_VALUES_TO_SET(supported_tex_image_source_types_,
                      kSupportedTypesOESTexFloat);
    is_oes_texture_float_formats_types_added_ = true;
  }

  if (!is_oes_texture_half_float_formats_types_added_ &&
      ExtensionEnabled(kOESTextureHalfFloatName)) {
    ADD_VALUES_TO_SET(supported_types_, kSupportedTypesOESTexHalfFloat);
    ADD_VALUES_TO_SET(supported_tex_image_source_types_,
                      kSupportedTypesOESTexHalfFloat);
    is_oes_texture_half_float_formats_types_added_ = true;
  }

  if (!is_web_gl_depth_texture_formats_types_added_ &&
      ExtensionEnabled(kWebGLDepthTextureName)) {
    ADD_VALUES_TO_SET(supported_internal_formats_,
                      kSupportedInternalFormatsOESDepthTex);
    ADD_VALUES_TO_SET(supported_tex_image_source_internal_formats_,
                      kSupportedInternalFormatsOESDepthTex);
    ADD_VALUES_TO_SET(supported_formats_, kSupportedFormatsOESDepthTex);
    ADD_VALUES_TO_SET(supported_tex_image_source_formats_,
                      kSupportedFormatsOESDepthTex);
    ADD_VALUES_TO_SET(supported_types_, kSupportedTypesOESDepthTex);
    ADD_VALUES_TO_SET(supported_tex_image_source_types_,
                      kSupportedTypesOESDepthTex);
    is_web_gl_depth_texture_formats_types_added_ = true;
  }

  if (!is_ext_srgb_formats_types_added_ && ExtensionEnabled(kEXTsRGBName)) {
    ADD_VALUES_TO_SET(supported_internal_formats_,
                      kSupportedInternalFormatsEXTsRGB);
    ADD_VALUES_TO_SET(supported_tex_image_source_internal_formats_,
                      kSupportedInternalFormatsEXTsRGB);
    ADD_VALUES_TO_SET(supported_formats_, kSupportedFormatsEXTsRGB);
    ADD_VALUES_TO_SET(supported_tex_image_source_formats_,
                      kSupportedFormatsEXTsRGB);
    is_ext_srgb_formats_types_added_ = true;
  }
}

void WebGLRenderingContextBase::AddExtensionSupportedFormatsTypesWebGL2() {
  if (!is_ext_texture_norm16_added_ &&
      ExtensionEnabled(kEXTTextureNorm16Name)) {
    ADD_VALUES_TO_SET(supported_internal_formats_,
                      kSupportedInternalFormatsEXTTextureNorm16ES3);
    ADD_VALUES_TO_SET(supported_tex_image_source_internal_formats_,
                      kSupportedInternalFormatsEXTTextureNorm16ES3);
    ADD_VALUES_TO_SET(supported_formats_, kSupportedFormatsEXTTextureNorm16ES3);
    ADD_VALUES_TO_SET(supported_types_, kSupportedTypesEXTTextureNorm16ES3);
    is_ext_texture_norm16_added_ = true;
  }
}

bool WebGLRenderingContextBase::ValidateTexImageSourceFormatAndType(
    const TexImageParams& params) {
  const char* function_name = GetTexImageFunctionName(params.function_id);

  if (!is_web_gl2_tex_image_source_formats_types_added_ && IsWebGL2()) {
    ADD_VALUES_TO_SET(supported_tex_image_source_internal_formats_,
                      kSupportedInternalFormatsTexImageSourceES3);
    ADD_VALUES_TO_SET(supported_tex_image_source_formats_,
                      kSupportedFormatsTexImageSourceES3);
    ADD_VALUES_TO_SET(supported_tex_image_source_types_,
                      kSupportedTypesTexImageSourceES3);
    is_web_gl2_tex_image_source_formats_types_added_ = true;
  }

  if (!IsWebGL2()) {
    AddExtensionSupportedFormatsTypes();
  } else {
    AddExtensionSupportedFormatsTypesWebGL2();
  }

  if (params.internalformat != 0 &&
      !base::Contains(supported_tex_image_source_internal_formats_,
                      params.internalformat)) {
    if (GetTexImageFunctionType(params.function_id) == kTexImage) {
      SynthesizeGLError(GL_INVALID_VALUE, function_name,
                        "invalid internalformat");
    } else {
      SynthesizeGLError(GL_INVALID_ENUM, function_name,
                        "invalid internalformat");
    }
    return false;
  }
  if (!base::Contains(supported_tex_image_source_formats_, params.format)) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid format");
    return false;
  }
  if (!base::Contains(supported_tex_image_source_types_, params.type)) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid type");
    return false;
  }

  return true;
}

bool WebGLRenderingContextBase::ValidateTexFuncFormatAndType(
    const TexImageParams& params) {
  const char* function_name = GetTexImageFunctionName(params.function_id);

  if (!is_web_gl2_formats_types_added_ && IsWebGL2()) {
    ADD_VALUES_TO_SET(supported_internal_formats_,
                      kSupportedInternalFormatsES3);
    ADD_VALUES_TO_SET(supported_internal_formats_,
                      kSupportedInternalFormatsTexImageES3);
    ADD_VALUES_TO_SET(supported_formats_, kSupportedFormatsES3);
    ADD_VALUES_TO_SET(supported_types_, kSupportedTypesES3);
    is_web_gl2_formats_types_added_ = true;
  }

  if (!IsWebGL2()) {
    AddExtensionSupportedFormatsTypes();
  } else {
    AddExtensionSupportedFormatsTypesWebGL2();
  }

  if (params.internalformat != 0 &&
      !base::Contains(supported_internal_formats_, params.internalformat)) {
    if (GetTexImageFunctionType(params.function_id) == kTexImage) {
      if (compressed_texture_formats_.Contains(
              static_cast<GLenum>(params.internalformat))) {
        SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                          "compressed texture formats are not accepted");
      } else {
        SynthesizeGLError(GL_INVALID_VALUE, function_name,
                          "invalid internalformat");
      }
    } else {
      SynthesizeGLError(GL_INVALID_ENUM, function_name,
                        "invalid internalformat");
    }
    return false;
  }
  if (!base::Contains(supported_formats_, params.format)) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid format");
    return false;
  }
  if (!base::Contains(supported_types_, params.type)) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid type");
    return false;
  }

  if (params.format == GL_DEPTH_COMPONENT && params.level > 0 && !IsWebGL2()) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "level must be 0 for DEPTH_COMPONENT format");
    return false;
  }
  if (params.format == GL_DEPTH_STENCIL_OES && params.level > 0 &&
      !IsWebGL2()) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "level must be 0 for DEPTH_STENCIL format");
    return false;
  }

  return true;
}

GLint WebGLRenderingContextBase::GetMaxTextureLevelForTarget(GLenum target) {
  switch (target) {
    case GL_TEXTURE_2D:
      return max_texture_level_;
    case GL_TEXTURE_CUBE_MAP:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      return max_cube_map_texture_level_;
  }
  return 0;
}

bool WebGLRenderingContextBase::ValidateTexFuncLevel(const char* function_name,
                                                     GLenum target,
                                                     GLint level) {
  if (level < 0) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "level < 0");
    return false;
  }
  GLint max_level = GetMaxTextureLevelForTarget(target);
  if (max_level && level >= max_level) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "level out of range");
    return false;
  }
  // This function only checks if level is legal, so we return true and don't
  // generate INVALID_ENUM if target is illegal.
  return true;
}

bool WebGLRenderingContextBase::ValidateTexFuncDimensions(
    const char* function_name,
    TexImageFunctionType function_type,
    GLenum target,
    GLint level,
    GLsizei width,
    GLsizei height,
    GLsizei depth) {
  if (width < 0 || height < 0 || depth < 0) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      "width, height or depth < 0");
    return false;
  }

  switch (target) {
    case GL_TEXTURE_2D:
      if (width > (max_texture_size_ >> level) ||
          height > (max_texture_size_ >> level)) {
        SynthesizeGLError(GL_INVALID_VALUE, function_name,
                          "width or height out of range");
        return false;
      }
      break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      if (function_type != kTexSubImage && width != height) {
        SynthesizeGLError(GL_INVALID_VALUE, function_name,
                          "width != height for cube map");
        return false;
      }
      // No need to check height here. For texImage width == height.
      // For texSubImage that will be checked when checking yoffset + height is
      // in range.
      if (width > (max_cube_map_texture_size_ >> level)) {
        SynthesizeGLError(GL_INVALID_VALUE, function_name,
                          "width or height out of range for cube map");
        return false;
      }
      break;
    case GL_TEXTURE_3D:
      if (IsWebGL2()) {
        if (width > (max3d_texture_size_ >> level) ||
            height > (max3d_texture_size_ >> level) ||
            depth > (max3d_texture_size_ >> level)) {
          SynthesizeGLError(GL_INVALID_VALUE, function_name,
                            "width, height or depth out of range");
          return false;
        }
        break;
      }
      [[fallthrough]];
    case GL_TEXTURE_2D_ARRAY:
      if (IsWebGL2()) {
        if (width > (max_texture_size_ >> level) ||
            height > (max_texture_size_ >> level) ||
            depth > max_array_texture_layers_) {
          SynthesizeGLError(GL_INVALID_VALUE, function_name,
                            "width, height or depth out of range");
          return false;
        }
        break;
      }
      [[fallthrough]];
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid target");
      return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateTexFuncParameters(
    const TexImageParams& params) {
  const char* function_name = GetTexImageFunctionName(params.function_id);

  // We absolutely have to validate the format and type combination.
  // The texImage2D entry points taking HTMLImage, etc. will produce
  // temporary data based on this combination, so it must be legal.
  if (params.source_type == kSourceHTMLImageElement ||
      params.source_type == kSourceHTMLCanvasElement ||
      params.source_type == kSourceHTMLVideoElement ||
      params.source_type == kSourceImageData ||
      params.source_type == kSourceImageBitmap ||
      params.source_type == kSourceVideoFrame) {
    if (!ValidateTexImageSourceFormatAndType(params)) {
      return false;
    }
  } else {
    if (!ValidateTexFuncFormatAndType(params)) {
      return false;
    }
  }

  if (!ValidateTexFuncDimensions(function_name,
                                 GetTexImageFunctionType(params.function_id),
                                 params.target, params.level, *params.width,
                                 *params.height, *params.depth)) {
    return false;
  }

  if (params.border) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "border != 0");
    return false;
  }

  return true;
}

bool WebGLRenderingContextBase::ValidateTexFuncData(
    const TexImageParams& params,
    DOMArrayBufferView* pixels,
    NullDisposition disposition,
    int64_t src_offset) {
  const char* function_name = GetTexImageFunctionName(params.function_id);
  TexImageDimension tex_dimension;
  if (params.function_id == kTexImage2D || params.function_id == kTexSubImage2D)
    tex_dimension = kTex2D;
  else
    tex_dimension = kTex3D;

  // All calling functions check isContextLost, so a duplicate check is not
  // needed here.
  if (!pixels) {
    DCHECK_NE(disposition, kNullNotReachable);
    if (disposition == kNullAllowed)
      return true;
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "no pixels");
    return false;
  }

  if (!ValidateSettableTexFormat(function_name, params.format))
    return false;

  auto pixelType = pixels->GetType();

  switch (params.type) {
    case GL_BYTE:
      if (pixelType != DOMArrayBufferView::kTypeInt8) {
        SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                          "type BYTE but ArrayBufferView not Int8Array");
        return false;
      }
      break;
    case GL_UNSIGNED_BYTE:
      if (pixelType != DOMArrayBufferView::kTypeUint8 &&
          pixelType != DOMArrayBufferView::kTypeUint8Clamped) {
        SynthesizeGLError(
            GL_INVALID_OPERATION, function_name,
            "type UNSIGNED_BYTE but ArrayBufferView not Uint8Array or "
            "Uint8ClampedArray");
        return false;
      }
      break;
    case GL_SHORT:
      if (pixelType != DOMArrayBufferView::kTypeInt16) {
        SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                          "type SHORT but ArrayBufferView not Int16Array");
        return false;
      }
      break;
    case GL_UNSIGNED_SHORT:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
      if (pixelType != DOMArrayBufferView::kTypeUint16) {
        SynthesizeGLError(
            GL_INVALID_OPERATION, function_name,
            "type UNSIGNED_SHORT but ArrayBufferView not Uint16Array");
        return false;
      }
      break;
    case GL_INT:
      if (pixelType != DOMArrayBufferView::kTypeInt32) {
        SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                          "type INT but ArrayBufferView not Int32Array");
        return false;
      }
      break;
    case GL_UNSIGNED_INT:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
    case GL_UNSIGNED_INT_5_9_9_9_REV:
    case GL_UNSIGNED_INT_24_8:
      if (pixelType != DOMArrayBufferView::kTypeUint32) {
        SynthesizeGLError(
            GL_INVALID_OPERATION, function_name,
            "type UNSIGNED_INT but ArrayBufferView not Uint32Array");
        return false;
      }
      break;
    case GL_FLOAT:  // OES_texture_float
      if (pixelType != DOMArrayBufferView::kTypeFloat32) {
        SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                          "type FLOAT but ArrayBufferView not Float32Array");
        return false;
      }
      break;
    case GL_HALF_FLOAT:
    case GL_HALF_FLOAT_OES:  // OES_texture_half_float
      // As per the specification, ArrayBufferView should be null or a
      // Uint16Array when OES_texture_half_float is enabled.
      if (pixelType != DOMArrayBufferView::kTypeUint16) {
        SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                          "type HALF_FLOAT_OES but ArrayBufferView is not NULL "
                          "and not Uint16Array");
        return false;
      }
      break;
    case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
      SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                        "type FLOAT_32_UNSIGNED_INT_24_8_REV but "
                        "ArrayBufferView is not NULL");
      return false;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  unsigned total_bytes_required, skip_bytes;
  GLenum error = WebGLImageConversion::ComputeImageSizeInBytes(
      params.format, params.type, *params.width, *params.height, *params.depth,
      GetUnpackPixelStoreParams(tex_dimension), &total_bytes_required, nullptr,
      &skip_bytes);
  if (error != GL_NO_ERROR) {
    SynthesizeGLError(error, function_name, "invalid texture dimensions");
    return false;
  }
  base::CheckedNumeric<size_t> total = src_offset;
  total *= pixels->TypeSize();
  total += total_bytes_required;
  total += skip_bytes;
  size_t total_val;
  if (!total.AssignIfValid(&total_val) || pixels->byteLength() < total_val) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "ArrayBufferView not big enough for request");
    return false;
  }
#if UINTPTR_MAX == UINT32_MAX
  // 32-bit platforms have additional constraints, since src_offset is
  // added to a pointer value in calling code.
  if (total_val > kMaximumSupportedArrayBufferSize) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      "src_offset plus texture data size exceeds the "
                      "supported range");
  }
#endif
  base::CheckedNumeric<uint32_t> data_size = total_bytes_required;
  data_size += skip_bytes;
  uint32_t data_size_val;
  if (!data_size.AssignIfValid(&data_size_val) ||
      data_size_val > kMaximumSupportedArrayBufferSize) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      "texture data size exceeds the supported range");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateCompressedTexFormat(
    const char* function_name,
    GLenum format) {
  if (!compressed_texture_formats_.Contains(format)) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid format");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateStencilOrDepthFunc(
    const char* function_name,
    GLenum func) {
  switch (func) {
    case GL_NEVER:
    case GL_LESS:
    case GL_LEQUAL:
    case GL_GREATER:
    case GL_GEQUAL:
    case GL_EQUAL:
    case GL_NOTEQUAL:
    case GL_ALWAYS:
      return true;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid function");
      return false;
  }
}

void WebGLRenderingContextBase::PrintGLErrorToConsole(const String& message) {
  if (!num_gl_errors_to_console_allowed_)
    return;

  --num_gl_errors_to_console_allowed_;
  PrintWarningToConsole(message);

  if (!num_gl_errors_to_console_allowed_)
    PrintWarningToConsole(
        "WebGL: too many errors, no more errors will be reported to the "
        "console for this context.");

  return;
}

void WebGLRenderingContextBase::PrintWarningToConsole(const String& message) {
  blink::ExecutionContext* context = Host()->GetTopExecutionContext();
  if (context && !context->IsContextDestroyed()) {
    context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning, message));
  }
}

void WebGLRenderingContextBase::NotifyWebGLErrorOrWarning(
    const String& message) {
  probe::DidFireWebGLErrorOrWarning(canvas(), message);
}

void WebGLRenderingContextBase::NotifyWebGLError(const String& error_type) {
  probe::DidFireWebGLError(canvas(), error_type);
}

void WebGLRenderingContextBase::NotifyWebGLWarning() {
  probe::DidFireWebGLWarning(canvas());
}

bool WebGLRenderingContextBase::ValidateFramebufferFuncParameters(
    const char* function_name,
    GLenum target,
    GLenum attachment) {
  if (!ValidateFramebufferTarget(target)) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid target");
    return false;
  }
  switch (attachment) {
    case GL_COLOR_ATTACHMENT0:
    case GL_DEPTH_ATTACHMENT:
    case GL_STENCIL_ATTACHMENT:
    case GL_DEPTH_STENCIL_ATTACHMENT:
      break;
    default:
      if ((ExtensionEnabled(kWebGLDrawBuffersName) || IsWebGL2()) &&
          attachment > GL_COLOR_ATTACHMENT0 &&
          attachment <
              static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + MaxColorAttachments()))
        break;
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid attachment");
      return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateBlendEquation(const char* function_name,
                                                      GLenum mode) {
  switch (mode) {
    case GL_FUNC_ADD:
    case GL_FUNC_SUBTRACT:
    case GL_FUNC_REVERSE_SUBTRACT:
      return true;
    case GL_MIN_EXT:
    case GL_MAX_EXT:
      if (ExtensionEnabled(kEXTBlendMinMaxName) || IsWebGL2())
        return true;
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid mode");
      return false;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid mode");
      return false;
  }
}

bool WebGLRenderingContextBase::ValidateBlendFuncFactors(
    const char* function_name,
    GLenum src,
    GLenum dst) {
  if (((src == GL_CONSTANT_COLOR || src == GL_ONE_MINUS_CONSTANT_COLOR) &&
       (dst == GL_CONSTANT_ALPHA || dst == GL_ONE_MINUS_CONSTANT_ALPHA)) ||
      ((dst == GL_CONSTANT_COLOR || dst == GL_ONE_MINUS_CONSTANT_COLOR) &&
       (src == GL_CONSTANT_ALPHA || src == GL_ONE_MINUS_CONSTANT_ALPHA))) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "incompatible src and dst");
    return false;
  }

  return ValidateBlendFuncExtendedFactors(function_name, src, dst);
}

bool WebGLRenderingContextBase::ValidateBlendFuncExtendedFactors(
    const char* function_name,
    GLenum src,
    GLenum dst) {
  // TODO(crbug.com/882580): this validation is done in the
  // passthrough command decoder; this helper can be removed once the
  // validating command decoder is completely unshipped.
  if (src == GL_SRC1_COLOR_EXT || dst == GL_SRC1_COLOR_EXT ||
      src == GL_SRC1_ALPHA_EXT || dst == GL_SRC1_ALPHA_EXT ||
      src == GL_ONE_MINUS_SRC1_COLOR_EXT ||
      dst == GL_ONE_MINUS_SRC1_COLOR_EXT ||
      src == GL_ONE_MINUS_SRC1_ALPHA_EXT ||
      dst == GL_ONE_MINUS_SRC1_ALPHA_EXT ||
      (dst == GL_SRC_ALPHA_SATURATE && !IsWebGL2())) {
    if (!ExtensionEnabled(kWebGLBlendFuncExtendedName)) {
      SynthesizeGLError(GL_INVALID_ENUM, function_name,
                        "invalid value, WEBGL_blend_func_extended not enabled");
      return false;
    }
  }

  return true;
}

bool WebGLRenderingContextBase::ValidateCapability(const char* function_name,
                                                   GLenum cap) {
  switch (cap) {
    case GL_BLEND:
    case GL_CULL_FACE:
    case GL_DEPTH_TEST:
    case GL_DITHER:
    case GL_POLYGON_OFFSET_FILL:
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
    case GL_SAMPLE_COVERAGE:
    case GL_SCISSOR_TEST:
    case GL_STENCIL_TEST:
      return true;
    case GL_POLYGON_OFFSET_LINE_ANGLE:
      if (ExtensionEnabled(kWebGLPolygonModeName)) {
        return true;
      }
      SynthesizeGLError(GL_INVALID_ENUM, function_name,
                        "invalid capability, WEBGL_polygon_mode not enabled");
      return false;
    case GL_DEPTH_CLAMP_EXT:
      if (ExtensionEnabled(kEXTDepthClampName)) {
        return true;
      }
      SynthesizeGLError(GL_INVALID_ENUM, function_name,
                        "invalid capability, EXT_depth_clamp not enabled");
      return false;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid capability");
      return false;
  }
}

WebGLBuffer* WebGLRenderingContextBase::ValidateBufferDataTarget(
    const char* function_name,
    GLenum target) {
  WebGLBuffer* buffer = nullptr;
  switch (target) {
    case GL_ELEMENT_ARRAY_BUFFER:
      buffer = bound_vertex_array_object_->BoundElementArrayBuffer();
      break;
    case GL_ARRAY_BUFFER:
      buffer = bound_array_buffer_.Get();
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid target");
      return nullptr;
  }
  if (!buffer) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name, "no buffer");
    return nullptr;
  }
  return buffer;
}

bool WebGLRenderingContextBase::ValidateBufferDataUsage(
    const char* function_name,
    GLenum usage) {
  switch (usage) {
    case GL_STREAM_DRAW:
    case GL_STATIC_DRAW:
    case GL_DYNAMIC_DRAW:
      return true;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid usage");
      return false;
  }
}

void WebGLRenderingContextBase::RemoveBoundBuffer(WebGLBuffer* buffer) {
  if (bound_array_buffer_ == buffer)
    bound_array_buffer_ = nullptr;

  bound_vertex_array_object_->UnbindBuffer(buffer);
}

bool WebGLRenderingContextBase::ValidateHTMLImageElement(
    const SecurityOrigin* security_origin,
    const char* function_name,
    HTMLImageElement* image,
    ExceptionState& exception_state) {
  if (!image || !image->CachedImage()) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "no image");
    return false;
  }
  const KURL& url = image->CachedImage()->GetResponse().CurrentRequestUrl();
  if (url.IsNull() || url.IsEmpty() || !url.IsValid()) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "invalid image");
    return false;
  }

  if (WouldTaintCanvasOrigin(image)) {
    exception_state.ThrowSecurityError(
        "The image element contains cross-origin data, and may not be loaded.");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateCanvasRenderingContextHost(
    const SecurityOrigin* security_origin,
    const char* function_name,
    CanvasRenderingContextHost* context_host,
    ExceptionState& exception_state) {
  if (!context_host || !context_host->IsPaintable()) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "no canvas");
    return false;
  }

  if (WouldTaintCanvasOrigin(context_host)) {
    exception_state.ThrowSecurityError("Tainted canvases may not be loaded.");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateHTMLVideoElement(
    const SecurityOrigin* security_origin,
    const char* function_name,
    HTMLVideoElement* video,
    ExceptionState& exception_state) {
  if (!video || !video->videoWidth() || !video->videoHeight()) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "no video");
    return false;
  }

  if (WouldTaintCanvasOrigin(video)) {
    exception_state.ThrowSecurityError(
        "The video element contains cross-origin data, and may not be loaded.");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateImageBitmap(
    const char* function_name,
    ImageBitmap* bitmap,
    ExceptionState& exception_state) {
  if (bitmap->IsNeutered()) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      "The source data has been detached.");
    return false;
  }
  if (!bitmap->OriginClean()) {
    exception_state.ThrowSecurityError(
        "The ImageBitmap contains cross-origin data, and may not be loaded.");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateDrawArrays(const char* function_name) {
  if (isContextLost())
    return false;

  if (!ValidateRenderingState(function_name)) {
    return false;
  }

  const char* reason = "framebuffer incomplete";
  if (framebuffer_binding_ && framebuffer_binding_->CheckDepthStencilStatus(
                                  &reason) != GL_FRAMEBUFFER_COMPLETE) {
    SynthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, function_name, reason);
    return false;
  }

  return true;
}

bool WebGLRenderingContextBase::ValidateDrawElements(const char* function_name,
                                                     GLenum type,
                                                     int64_t offset) {
  if (isContextLost())
    return false;

  if (type == GL_UNSIGNED_INT && !IsWebGL2() &&
      !ExtensionEnabled(kOESElementIndexUintName)) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid type");
    return false;
  }

  if (!ValidateValueFitNonNegInt32(function_name, "offset", offset))
    return false;

  if (!ValidateRenderingState(function_name)) {
    return false;
  }

  const char* reason = "framebuffer incomplete";
  if (framebuffer_binding_ && framebuffer_binding_->CheckDepthStencilStatus(
                                  &reason) != GL_FRAMEBUFFER_COMPLETE) {
    SynthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, function_name, reason);
    return false;
  }

  return true;
}

void WebGLRenderingContextBase::OnBeforeDrawCall(
    CanvasPerformanceMonitor::DrawType draw_type) {
  ClearIfComposited(kClearCallerDrawOrClear);
  MarkContextChanged(kCanvasChanged, draw_type);
}

void WebGLRenderingContextBase::DispatchContextLostEvent(TimerBase*) {
  // WebXR spec: When the WebGL context is lost, set the xr compatible boolean
  // to false prior to firing the webglcontextlost event.
  xr_compatible_ = false;

  WebGLContextEvent* event =
      WebGLContextEvent::Create(event_type_names::kWebglcontextlost, "");
  Host()->HostDispatchEvent(event);
  restore_allowed_ = event->defaultPrevented();
  if (restore_allowed_ && auto_recovery_method_ == kAuto) {
    // Defer the restore timer to give the context loss
    // notifications time to propagate through the system: in
    // particular, to the browser process.
    restore_timer_.StartOneShot(kDurationBetweenRestoreAttempts, FROM_HERE);
  }

  if (!restore_allowed_) {
    // Per WebXR spec, reject the promise with an AbortError if the default
    // behavior wasn't prevented. CompleteXrCompatiblePromiseIfPending rejects
    // the promise if xr_compatible_ is false, which was set at the beginning of
    // this method.
    CompleteXrCompatiblePromiseIfPending(DOMExceptionCode::kAbortError);
  }
}

void WebGLRenderingContextBase::MaybeRestoreContext(TimerBase*) {
  DCHECK(isContextLost());

  // The rendering context is not restored unless the default behavior of the
  // webglcontextlost event was prevented earlier.
  //
  // Because of the way m_restoreTimer is set up for real vs. synthetic lost
  // context events, we don't have to worry about this test short-circuiting
  // the retry loop for real context lost events.
  if (!restore_allowed_)
    return;

  if (canvas()) {
    LocalFrame* frame = canvas()->GetDocument().GetFrame();
    if (!frame)
      return;

    bool blocked = false;
    mojo::Remote<mojom::blink::GpuDataManager> gpu_data_manager;
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        gpu_data_manager.BindNewPipeAndPassReceiver());
    gpu_data_manager->Are3DAPIsBlockedForUrl(canvas()->GetDocument().Url(),
                                             &blocked);
    if (blocked) {
      // Notify the canvas if it wasn't already. This has the side
      // effect of scheduling a compositing update so the "sad canvas"
      // will show up properly.
      canvas()->SetContextCreationWasBlocked();
      return;
    }

    Settings* settings = frame->GetSettings();
    if (settings && ((context_type_ == Platform::kWebGL1ContextType &&
                      !settings->GetWebGL1Enabled()) ||
                     (context_type_ == Platform::kWebGL2ContextType &&
                      !settings->GetWebGL2Enabled()))) {
      return;
    }
  }

  // Drawing buffer should have aready been destroyed during context loss to
  // ensure its resources were freed.
  DCHECK(!GetDrawingBuffer());

  Platform::ContextAttributes attributes =
      ToPlatformContextAttributes(CreationAttributes(), context_type_);
  Platform::GraphicsInfo gl_info;
  const auto& url = Host()->GetExecutionContextUrl();

  std::unique_ptr<WebGraphicsContext3DProvider> context_provider =
      CreateOffscreenGraphicsContext3DProvider(attributes, &gl_info, url);
  scoped_refptr<DrawingBuffer> buffer;
  if (context_provider && context_provider->BindToCurrentSequence()) {
    // Construct a new drawing buffer with the new GL context.
    buffer = CreateDrawingBuffer(std::move(context_provider), gl_info);
    // If DrawingBuffer::create() fails to allocate a fbo, |drawingBuffer| is
    // set to null.
  }
  if (!buffer) {
    if (context_lost_mode_ == kRealLostContext) {
      restore_timer_.StartOneShot(kDurationBetweenRestoreAttempts, FROM_HERE);
    } else {
      // This likely shouldn't happen but is the best way to report it to the
      // WebGL app.
      SynthesizeGLError(GL_INVALID_OPERATION, "", "error restoring context");
    }
    return;
  }

  drawing_buffer_ = std::move(buffer);
  GetDrawingBuffer()->Bind(GL_FRAMEBUFFER);
  lost_context_errors_.clear();
  context_lost_mode_ = kNotLostContext;
  auto_recovery_method_ = kManual;
  restore_allowed_ = false;
  RemoveFromEvictedList(this);

  SetupFlags();
  InitializeNewContext();
  MarkContextChanged(kCanvasContextChanged,
                     CanvasPerformanceMonitor::DrawType::kOther);
  if (canvas()) {
    // The cc::Layer associated with this WebGL rendering context has
    // changed, so tell the canvas that a compositing update is
    // needed.
    //
    // TODO(kbr): more work likely needed for the case of a canvas
    // whose control has transferred to an OffscreenCanvas.
    canvas()->SetNeedsCompositingUpdate();
  }

  WebGLContextEvent* event =
      WebGLContextEvent::Create(event_type_names::kWebglcontextrestored, "");
  Host()->HostDispatchEvent(event);

  if (xr_compatible_) {
    CompleteXrCompatiblePromiseIfPending(DOMExceptionCode::kNoError);
  } else {
    CompleteXrCompatiblePromiseIfPending(DOMExceptionCode::kAbortError);
  }
}

String WebGLRenderingContextBase::EnsureNotNull(const String& text) const {
  if (text.IsNull())
    return WTF::g_empty_string;
  return text;
}

WebGLRenderingContextBase::LRUCanvasResourceProviderCache::
    LRUCanvasResourceProviderCache(wtf_size_t capacity, CacheType type)
    : type_(type), resource_providers_(capacity) {}

CanvasResourceProvider* WebGLRenderingContextBase::
    LRUCanvasResourceProviderCache::GetCanvasResourceProvider(
        const SkImageInfo& info) {
  wtf_size_t i;
  for (i = 0; i < resource_providers_.size(); ++i) {
    CanvasResourceProvider* resource_provider = resource_providers_[i].get();
    if (!resource_provider)
      break;
    if (resource_provider->GetSkImageInfo() != info)
      continue;
    BubbleToFront(i);
    return resource_provider;
  }

  std::unique_ptr<CanvasResourceProvider> temp;
  if (type_ == CacheType::kVideo) {
    viz::RasterContextProvider* raster_context_provider = nullptr;
    if (auto wrapper = SharedGpuContext::ContextProviderWrapper()) {
      if (auto* context_provider = wrapper->ContextProvider())
        raster_context_provider = context_provider->RasterContextProvider();
    }
    temp = CreateResourceProviderForVideoFrame(info, raster_context_provider);
  } else {
    // TODO(fserb): why is this a BITMAP?
    temp = CanvasResourceProvider::CreateBitmapProvider(
        info, cc::PaintFlags::FilterQuality::kLow,
        CanvasResourceProvider::ShouldInitialize::kNo);  // TODO: should this
                                                         // use the canvas's
  }

  if (!temp)
    return nullptr;
  i = std::min(resource_providers_.size() - 1, i);
  resource_providers_[i] = std::move(temp);

  CanvasResourceProvider* resource_provider = resource_providers_[i].get();
  BubbleToFront(i);
  return resource_provider;
}

void WebGLRenderingContextBase::LRUCanvasResourceProviderCache::BubbleToFront(
    wtf_size_t idx) {
  for (wtf_size_t i = idx; i > 0; --i)
    resource_providers_[i].swap(resource_providers_[i - 1]);
}

namespace {

String GetErrorString(GLenum error) {
  switch (error) {
    case GL_INVALID_ENUM:
      return "INVALID_ENUM";
    case GL_INVALID_VALUE:
      return "INVALID_VALUE";
    case GL_INVALID_OPERATION:
      return "INVALID_OPERATION";
    case GL_OUT_OF_MEMORY:
      return "OUT_OF_MEMORY";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      return "INVALID_FRAMEBUFFER_OPERATION";
    case GC3D_CONTEXT_LOST_WEBGL:
      return "CONTEXT_LOST_WEBGL";
    default:
      return String::Format("WebGL ERROR(0x%04X)", error);
  }
}

}  // namespace

void WebGLRenderingContextBase::SynthesizeGLError(
    GLenum error,
    const char* function_name,
    const char* description,
    ConsoleDisplayPreference display) {
  String error_type = GetErrorString(error);
  if (synthesized_errors_to_console_ && display == kDisplayInConsole) {
    String message = String("WebGL: ") + error_type + ": " +
                     String(function_name) + ": " + String(description);
    PrintGLErrorToConsole(message);
  }
  if (!isContextLost()) {
    if (!synthetic_errors_.Contains(error))
      synthetic_errors_.push_back(error);
  } else {
    if (!lost_context_errors_.Contains(error))
      lost_context_errors_.push_back(error);
  }
  NotifyWebGLError(error_type);
}

void WebGLRenderingContextBase::EmitGLWarning(const char* function_name,
                                              const char* description) {
  if (synthesized_errors_to_console_) {
    String message =
        String("WebGL: ") + String(function_name) + ": " + String(description);
    PrintGLErrorToConsole(message);
  }
  NotifyWebGLWarning();
}

void WebGLRenderingContextBase::ApplyDepthAndStencilTest() {
  bool have_stencil_buffer = false;
  bool have_depth_buffer = false;

  if (framebuffer_binding_) {
    have_depth_buffer = framebuffer_binding_->HasDepthBuffer();
    have_stencil_buffer = framebuffer_binding_->HasStencilBuffer();
  } else {
    have_depth_buffer = !isContextLost() && CreationAttributes().depth &&
                        GetDrawingBuffer()->HasDepthBuffer();
    have_stencil_buffer = !isContextLost() && CreationAttributes().stencil &&
                          GetDrawingBuffer()->HasStencilBuffer();
  }
  EnableOrDisable(GL_DEPTH_TEST, depth_enabled_ && have_depth_buffer);
  EnableOrDisable(GL_STENCIL_TEST, stencil_enabled_ && have_stencil_buffer);
}

void WebGLRenderingContextBase::EnableOrDisable(GLenum capability,
                                                bool enable) {
  if (isContextLost())
    return;
  if (enable)
    ContextGL()->Enable(capability);
  else
    ContextGL()->Disable(capability);
}

gfx::Size WebGLRenderingContextBase::ClampedCanvasSize() const {
  int width = Host()->Size().width();
  int height = Host()->Size().height();
  return gfx::Size(Clamp(width, 1, max_viewport_dims_[0]),
                   Clamp(height, 1, max_viewport_dims_[1]));
}

GLint WebGLRenderingContextBase::MaxDrawBuffers() {
  if (isContextLost() ||
      !(ExtensionEnabled(kWebGLDrawBuffersName) || IsWebGL2()))
    return 0;
  if (!max_draw_buffers_)
    ContextGL()->GetIntegerv(GL_MAX_DRAW_BUFFERS_EXT, &max_draw_buffers_);
  if (!max_color_attachments_)
    ContextGL()->GetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT,
                             &max_color_attachments_);
  // WEBGL_draw_buffers requires MAX_COLOR_ATTACHMENTS >= MAX_DRAW_BUFFERS.
  return std::min(max_draw_buffers_, max_color_attachments_);
}

GLint WebGLRenderingContextBase::MaxColorAttachments() {
  if (isContextLost() ||
      !(ExtensionEnabled(kWebGLDrawBuffersName) || IsWebGL2()))
    return 0;
  if (!max_color_attachments_)
    ContextGL()->GetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT,
                             &max_color_attachments_);
  return max_color_attachments_;
}

void WebGLRenderingContextBase::SetBackDrawBuffer(GLenum buf) {
  back_draw_buffer_ = buf;
  if (GetDrawingBuffer()) {
    GetDrawingBuffer()->SetDrawBuffer(buf);
  }
}

void WebGLRenderingContextBase::SetFramebuffer(GLenum target,
                                               WebGLFramebuffer* buffer) {
  if (buffer)
    buffer->SetHasEverBeenBound();

  if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) {
    framebuffer_binding_ = buffer;
    ApplyDepthAndStencilTest();
  }
  if (!buffer) {
    // Instead of binding fb 0, bind the drawing buffer.
    GetDrawingBuffer()->Bind(target);
  } else {
    ContextGL()->BindFramebuffer(target, buffer->Object());
  }
}

void WebGLRenderingContextBase::RestoreCurrentFramebuffer() {
  bindFramebuffer(GL_FRAMEBUFFER, framebuffer_binding_.Get());
}

void WebGLRenderingContextBase::RestoreCurrentTexture2D() {
  bindTexture(GL_TEXTURE_2D,
              texture_units_[active_texture_unit_].texture2d_binding_.Get());
}

void WebGLRenderingContextBase::RestoreCurrentTextureCubeMap() {
  bindTexture(
      GL_TEXTURE_CUBE_MAP,
      texture_units_[active_texture_unit_].texture_cube_map_binding_.Get());
}

void WebGLRenderingContextBase::FindNewMaxNonDefaultTextureUnit() {
  // Trace backwards from the current max to find the new max non-default
  // texture unit
  int start_index = one_plus_max_non_default_texture_unit_ - 1;
  for (int i = start_index; i >= 0; --i) {
    if (texture_units_[i].texture2d_binding_ ||
        texture_units_[i].texture_cube_map_binding_) {
      one_plus_max_non_default_texture_unit_ = i + 1;
      return;
    }
  }
  one_plus_max_non_default_texture_unit_ = 0;
}

void WebGLRenderingContextBase::TextureUnitState::Trace(
    blink::Visitor* visitor) const {
  visitor->Trace(texture2d_binding_);
  visitor->Trace(texture_cube_map_binding_);
  visitor->Trace(texture3d_binding_);
  visitor->Trace(texture2d_array_binding_);
  visitor->Trace(texture_video_image_binding_);
  visitor->Trace(texture_external_oes_binding_);
  visitor->Trace(texture_rectangle_arb_binding_);
}

void WebGLRenderingContextBase::Trace(Visitor* visitor) const {
  visitor->Trace(context_group_);
  visitor->Trace(dispatch_context_lost_event_timer_);
  visitor->Trace(restore_timer_);
  visitor->Trace(bound_array_buffer_);
  visitor->Trace(default_vertex_array_object_);
  visitor->Trace(bound_vertex_array_object_);
  visitor->Trace(current_program_);
  visitor->Trace(framebuffer_binding_);
  visitor->Trace(renderbuffer_binding_);
  visitor->Trace(texture_units_);
  visitor->Trace(extensions_);
  visitor->Trace(make_xr_compatible_resolver_);
  visitor->Trace(program_completion_query_list_);
  visitor->Trace(program_completion_query_map_);
  CanvasRenderingContext::Trace(visitor);
}

int WebGLRenderingContextBase::ExternallyAllocatedBufferCountPerPixel() {
  if (isContextLost())
    return 0;

  int buffer_count = 1;
  buffer_count *= 2;  // WebGL's front and back color buffers.
  int samples = GetDrawingBuffer() ? GetDrawingBuffer()->SampleCount() : 0;
  WebGLContextAttributes* attribs = getContextAttributes();
  if (attribs) {
    // Handle memory from WebGL multisample and depth/stencil buffers.
    // It is enabled only in case of explicit resolve assuming that there
    // is no memory overhead for MSAA on tile-based GPU arch.
    if (attribs->antialias() && samples > 0 &&
        GetDrawingBuffer()->ExplicitResolveOfMultisampleData()) {
      if (attribs->depth() || attribs->stencil())
        buffer_count += samples;  // depth/stencil multisample buffer
      buffer_count += samples;    // color multisample buffer
    } else if (attribs->depth() || attribs->stencil()) {
      buffer_count += 1;  // regular depth/stencil buffer
    }
  }

  return buffer_count;
}

DrawingBuffer* WebGLRenderingContextBase::GetDrawingBuffer() const {
  return drawing_buffer_.get();
}

void WebGLRenderingContextBase::ResetUnpackParameters() {
  if (unpack_alignment_ != 1)
    ContextGL()->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void WebGLRenderingContextBase::RestoreUnpackParameters() {
  if (unpack_alignment_ != 1)
    ContextGL()->PixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment_);
}

V8UnionHTMLCanvasElementOrOffscreenCanvas*
WebGLRenderingContextBase::getHTMLOrOffscreenCanvas() const {
  if (canvas()) {
    return MakeGarbageCollected<V8UnionHTMLCanvasElementOrOffscreenCanvas>(
        static_cast<HTMLCanvasElement*>(Host()));
  }
  return MakeGarbageCollected<V8UnionHTMLCanvasElementOrOffscreenCanvas>(
      static_cast<OffscreenCanvas*>(Host()));
}

void WebGLRenderingContextBase::addProgramCompletionQuery(WebGLProgram* program,
                                                          GLuint query) {
  auto old_query = program_completion_query_map_.find(program);
  if (old_query != program_completion_query_map_.end()) {
    ContextGL()->DeleteQueriesEXT(1, &old_query->value);
    // If this program's been inserted into the map already, then it
    // exists in the list, too. Clear it out from there so that its
    // new addition doesn't introduce a duplicate.
    wtf_size_t old_index = program_completion_query_list_.Find(program);
    DCHECK_NE(old_index, WTF::kNotFound);
    program_completion_query_list_.EraseAt(old_index);
  }
  program_completion_query_map_.Set(program, query);
  program_completion_query_list_.push_back(program);
  if (program_completion_query_map_.size() > kMaxProgramCompletionQueries) {
    DCHECK_GT(program_completion_query_list_.size(), 0u);
    WebGLProgram* program_to_remove = program_completion_query_list_[0];
    auto program_iter = program_completion_query_map_.find(program_to_remove);
    CHECK_NE(program_iter, program_completion_query_map_.end(),
             base::NotFatalUntil::M130);
    ContextGL()->DeleteQueriesEXT(1, &program_iter->value);
    program_completion_query_map_.erase(program_iter);
    program_completion_query_list_.EraseAt(0);
  }
}

void WebGLRenderingContextBase::clearProgramCompletionQueries() {
  if (destruction_in_progress_) {
    // GC has started so we can't touch program_completion_query_{map,list}_.
    // That's OK; we don't need to clean up because the context and object are
    // about to be destroyed anyway.
    return;
  }

  for (auto iter : program_completion_query_map_) {
    ContextGL()->DeleteQueriesEXT(1, &iter.value);
  }
  program_completion_query_map_.clear();
  program_completion_query_list_.clear();
}

bool WebGLRenderingContextBase::checkProgramCompletionQueryAvailable(
    WebGLProgram* program,
    bool* completed) {
  GLuint id = 0;
  auto found = program_completion_query_map_.find(program);
  if (found != program_completion_query_map_.end()) {
    id = found->value;
    GLuint available = 0;
    ContextGL()->GetQueryObjectuivEXT(id, GL_QUERY_RESULT_AVAILABLE,
                                      &available);
    if (available) {
      GLuint result = 0u;
      ContextGL()->GetQueryObjectuivEXT(id, GL_QUERY_RESULT, &result);
      program->setLinkStatus(result);
    }
    *completed = (available == GL_TRUE);
    return true;
  }
  return false;
}
}  // namespace blink
