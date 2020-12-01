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

#include "base/feature_list.h"
#include "base/numerics/checked_math.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/gpu/gpu.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/html_canvas_element_or_offscreen_canvas.h"
#include "third_party/blink/renderer/bindings/modules/v8/webgl_any.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/dactyloscoper.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/core/typed_arrays/flexible_array_buffer_view.h"
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
#include "third_party/blink/renderer/modules/webgl/webgl_video_texture.h"
#include "third_party/blink/renderer/modules/webgl/webgl_video_texture_enum.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

bool WebGLRenderingContextBase::webgl_context_limits_initialized_ = false;
unsigned WebGLRenderingContextBase::max_active_webgl_contexts_ = 0;
unsigned WebGLRenderingContextBase::max_active_webgl_contexts_on_worker_ = 0;

namespace {

constexpr base::TimeDelta kDurationBetweenRestoreAttempts =
    base::TimeDelta::FromSeconds(1);
const int kMaxGLErrorsAllowedToConsole = 256;

Mutex& WebGLContextLimitMutex() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, ());
  return mutex;
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
    active_contexts_persistent.RegisterAsStaticReference();
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
    forcibly_evicted_contexts_persistent.RegisterAsStaticReference();
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
    memcpy(color_mask_, color_mask, 4 * sizeof(GLboolean));
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
  MutexLocker locker(WebGLContextLimitMutex());
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
  MutexLocker locker(WebGLContextLimitMutex());
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
  if (ActiveContexts().IsEmpty())
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
  if (ForciblyEvictedContexts().IsEmpty())
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

    IntSize desired_size = DrawingBuffer::AdjustSize(
        evicted_context->ClampedCanvasSize(), IntSize(),
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

// Strips comments from shader text. This allows non-ASCII characters
// to be used in comments without potentially breaking OpenGL
// implementations not expecting characters outside the GLSL ES set.
class StripComments {
 public:
  StripComments(const String& str)
      : parse_state_(kBeginningOfLine),
        source_string_(str),
        length_(str.length()),
        position_(0) {
    Parse();
  }

  String Result() { return builder_.ToString(); }

 private:
  bool HasMoreCharacters() const { return (position_ < length_); }

  void Parse() {
    while (HasMoreCharacters()) {
      Process(Current());
      // process() might advance the position.
      if (HasMoreCharacters())
        Advance();
    }
  }

  void Process(UChar);

  bool Peek(UChar& character) const {
    if (position_ + 1 >= length_)
      return false;
    character = source_string_[position_ + 1];
    return true;
  }

  UChar Current() {
    SECURITY_DCHECK(position_ < length_);
    return source_string_[position_];
  }

  void Advance() { ++position_; }

  static bool IsNewline(UChar character) {
    // Don't attempt to canonicalize newline related characters.
    return (character == '\n' || character == '\r');
  }

  void Emit(UChar character) { builder_.Append(character); }

  enum ParseState {
    // Have not seen an ASCII non-whitespace character yet on
    // this line. Possible that we might see a preprocessor
    // directive.
    kBeginningOfLine,

    // Have seen at least one ASCII non-whitespace character
    // on this line.
    kMiddleOfLine,

    // Handling a preprocessor directive. Passes through all
    // characters up to the end of the line. Disables comment
    // processing.
    kInPreprocessorDirective,

    // Handling a single-line comment. The comment text is
    // replaced with a single space.
    kInSingleLineComment,

    // Handling a multi-line comment. Newlines are passed
    // through to preserve line numbers.
    kInMultiLineComment
  };

  ParseState parse_state_;
  String source_string_;
  unsigned length_;
  unsigned position_;
  StringBuilder builder_;
};

void StripComments::Process(UChar c) {
  if (IsNewline(c)) {
    // No matter what state we are in, pass through newlines
    // so we preserve line numbers.
    Emit(c);

    if (parse_state_ != kInMultiLineComment)
      parse_state_ = kBeginningOfLine;

    return;
  }

  UChar temp = 0;
  switch (parse_state_) {
    case kBeginningOfLine:
      if (WTF::IsASCIISpace(c)) {
        Emit(c);
        break;
      }

      if (c == '#') {
        parse_state_ = kInPreprocessorDirective;
        Emit(c);
        break;
      }

      // Transition to normal state and re-handle character.
      parse_state_ = kMiddleOfLine;
      Process(c);
      break;

    case kMiddleOfLine:
    case kInPreprocessorDirective:
      if (c == '/' && Peek(temp)) {
        if (temp == '/') {
          parse_state_ = kInSingleLineComment;
          Emit(' ');
          Advance();
          break;
        }

        if (temp == '*') {
          parse_state_ = kInMultiLineComment;
          // Emit the comment start in case the user has
          // an unclosed comment and we want to later
          // signal an error.
          Emit('/');
          Emit('*');
          Advance();
          break;
        }
      }

      Emit(c);
      break;

    case kInSingleLineComment:
      // Line-continuation characters are processed before comment processing.
      // Advance string if a new line character is immediately behind
      // line-continuation character.
      if (c == '\\') {
        if (Peek(temp) && IsNewline(temp))
          Advance();
      }

      // The newline code at the top of this function takes care
      // of resetting our state when we get out of the
      // single-line comment. Swallow all other characters.
      break;

    case kInMultiLineComment:
      if (c == '*' && Peek(temp) && temp == '/') {
        Emit('*');
        Emit('/');
        parse_state_ = kMiddleOfLine;
        Advance();
        break;
      }

      // Swallow all other characters. Unclear whether we may
      // want or need to just emit a space per character to try
      // to preserve column numbers for debugging purposes.
      break;
  }
}

static bool g_should_fail_context_creation_for_testing = false;

static CanvasRenderingContext::CanvasRenderingAPI GetCanvasRenderingAPIType(
    Platform::ContextType context_type) {
  switch (context_type) {
    case Platform::kWebGL1ContextType:
      return CanvasRenderingContext::CanvasRenderingAPI::kWebgl;
    case Platform::kWebGL2ContextType:
      return CanvasRenderingContext::CanvasRenderingAPI::kWebgl2;
    default:
      NOTREACHED();
      return CanvasRenderingContext::CanvasRenderingAPI::kWebgl;
  }
}

}  // namespace

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
  if (info_string.IsEmpty())
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

bool WebGLRenderingContextBase::SupportOwnOffscreenSurface(
    ExecutionContext* execution_context) {
  // Using an own offscreen surface disables virtualized contexts, and this
  // doesn't currently work properly, see https://crbug.com/691102.
  // TODO(https://crbug.com/791755): Remove this function and related code once
  // the replacement is ready.
  return false;
}

std::unique_ptr<WebGraphicsContext3DProvider>
WebGLRenderingContextBase::CreateContextProviderInternal(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attributes,
    Platform::ContextType context_type,
    bool* using_gpu_compositing) {
  DCHECK(host);
  ExecutionContext* execution_context = host->GetTopExecutionContext();
  DCHECK(execution_context);

  Platform::ContextAttributes context_attributes = ToPlatformContextAttributes(
      attributes, context_type, SupportOwnOffscreenSurface(execution_context));

  Platform::GraphicsInfo gl_info;
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider;
  const auto& url = execution_context->Url();
  if (IsMainThread()) {
    // Ask for gpu compositing mode when making the context. The context will be
    // lost if the mode changes.
    *using_gpu_compositing = !Platform::Current()->IsGpuCompositingDisabled();
    context_provider =
        Platform::Current()->CreateOffscreenGraphicsContext3DProvider(
            context_attributes, url, &gl_info);
  } else {
    context_provider = CreateContextProviderOnWorkerThread(
        context_attributes, &gl_info, using_gpu_compositing, url);
  }
  if (context_provider && !context_provider->BindToCurrentThread()) {
    context_provider = nullptr;
    gl_info.error_message =
        String("bindToCurrentThread failed: " + String(gl_info.error_message));
  }
  if (!context_provider || g_should_fail_context_creation_for_testing) {
    g_should_fail_context_creation_for_testing = false;
    host->HostDispatchEvent(
        WebGLContextEvent::Create(event_type_names::kWebglcontextcreationerror,
                                  ExtractWebGLContextCreationError(gl_info)));
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
    bool* using_gpu_compositing) {
  // The host might block creation of a new WebGL context despite the
  // page settings; in particular, if WebGL contexts were lost one or
  // more times via the GL_ARB_robustness extension.
  if (host->IsWebGLBlocked()) {
    host->SetContextCreationWasBlocked();
    host->HostDispatchEvent(WebGLContextEvent::Create(
        event_type_names::kWebglcontextcreationerror,
        "Web page caused context loss and was blocked"));
    return nullptr;
  }
  if ((context_type == Platform::kWebGL1ContextType &&
       !host->IsWebGL1Enabled()) ||
      (context_type == Platform::kWebGL2ContextType &&
       !host->IsWebGL2Enabled())) {
    host->HostDispatchEvent(WebGLContextEvent::Create(
        event_type_names::kWebglcontextcreationerror,
        "disabled by enterprise policy or commandline switch"));
    return nullptr;
  }

  return CreateContextProviderInternal(host, attributes, context_type,
                                       using_gpu_compositing);
}

void WebGLRenderingContextBase::ForceNextWebGLContextCreationToFail() {
  g_should_fail_context_creation_for_testing = true;
}

ImageBitmap* WebGLRenderingContextBase::TransferToImageBitmapBase(
    ScriptState* script_state) {
  WebFeature feature = WebFeature::kOffscreenCanvasTransferToImageBitmapWebGL;
  UseCounter::Count(ExecutionContext::From(script_state), feature);
  return MakeGarbageCollected<ImageBitmap>(
      GetDrawingBuffer()->TransferToStaticBitmapImage());
}

void WebGLRenderingContextBase::commit() {
  if (!GetDrawingBuffer() || (Host() && Host()->IsOffscreenCanvas()))
    return;

  int width = GetDrawingBuffer()->Size().Width();
  int height = GetDrawingBuffer()->Size().Height();

  if (PaintRenderingResultsToCanvas(kBackBuffer)) {
    if (Host()->GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)) {
      Host()->Commit(Host()->ResourceProvider()->ProduceCanvasResource(),
                     SkIRect::MakeWH(width, height));
    }
  }
  MarkLayerComposited();
}

scoped_refptr<StaticBitmapImage> WebGLRenderingContextBase::GetImage() {
  if (!GetDrawingBuffer())
    return nullptr;

  ScopedFramebufferRestorer fbo_restorer(this);
  GetDrawingBuffer()->ResolveAndBindForReadAndDraw();
  // Use the drawing buffer size here instead of the canvas size to ensure that
  // sizing is consistent. The forced downsizing logic in Reshape() can lead to
  // the drawing buffer being smaller than the canvas size.
  // See https://crbug.com/845742.
  IntSize size = GetDrawingBuffer()->Size();
  // Since we are grabbing a snapshot that is not for compositing, we use a
  // custom resource provider. This avoids consuming compositing-specific
  // resources (e.g. GpuMemoryBuffer)
  auto color_params = CanvasRenderingContextColorParams();
  std::unique_ptr<CanvasResourceProvider> resource_provider =
      CanvasResourceProvider::CreateSharedImageProvider(
          size, GetDrawingBuffer()->FilterQuality(), color_params,
          CanvasResourceProvider::ShouldInitialize::kNo,
          SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
          is_origin_top_left_, 0u /*shared_image_usage_flags*/);
  // todo(bug 1090962) This CPU fallback is needed as it would break
  // webgl_conformance_gles_passthrough_tests on Android FYI for Nexus 5x.
  if (!resource_provider || !resource_provider->IsValid()) {
    resource_provider = CanvasResourceProvider::CreateBitmapProvider(
        size, GetDrawingBuffer()->FilterQuality(), color_params,
        CanvasResourceProvider::ShouldInitialize::kNo);
  }

  if (!resource_provider || !resource_provider->IsValid())
    return nullptr;

  if (!CopyRenderingResultsFromDrawingBuffer(resource_provider.get(),
                                             kBackBuffer)) {
    // CopyRenderingResultsFromDrawingBuffer will handle both CPU and GPU cases.
    NOTREACHED();
    return nullptr;
  }
  return resource_provider->Snapshot();
}

ScriptPromise WebGLRenderingContextBase::makeXRCompatible(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (isContextLost()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Context lost.");
    return ScriptPromise();
  }

  // Return a resolved promise if we're already xr compatible. Once we're
  // compatible, we should always be compatible unless a context lost occurs.
  // DispatchContextLostEvent() resets this flag to false.
  if (xr_compatible_)
    return ScriptPromise::CastUndefined(script_state);

  if (!RuntimeEnabledFeatures::WebXRMultiGpuEnabled()) {
    xr_compatible_ = true;
    return ScriptPromise::CastUndefined(script_state);
  }

  // If there's a request currently in progress, return the same promise.
  if (make_xr_compatible_resolver_)
    return make_xr_compatible_resolver_->Promise();

  make_xr_compatible_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = make_xr_compatible_resolver_->Promise();

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

bool WebGLRenderingContextBase::MakeXrCompatibleSync(
    CanvasRenderingContextHost* host) {
  if (!RuntimeEnabledFeatures::WebXRMultiGpuEnabled())
    return true;

  device::mojom::blink::XrCompatibleResult xr_compatible_result =
      device::mojom::blink::XrCompatibleResult::kNoDeviceAvailable;

  if (!host->IsOffscreenCanvas()) {
    HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(host);
    if (XRSystem* xr = XRSystem::From(canvas->GetDocument()))
      xr->MakeXrCompatibleSync(&xr_compatible_result);
  }

  return IsXrCompatibleFromResult(xr_compatible_result);
}

void WebGLRenderingContextBase::MakeXrCompatibleAsync() {
  if (!canvas()) {
    xr_compatible_ = false;
    CompleteXrCompatiblePromiseIfPending(DOMExceptionCode::kAbortError);
    return;
  }

  XRSystem* xr = XRSystem::From(canvas()->GetDocument());
  if (!xr) {
    xr_compatible_ = false;
    CompleteXrCompatiblePromiseIfPending(DOMExceptionCode::kAbortError);
    return;
  }

  // The promise will be completed on the callback.
  xr->MakeXrCompatibleAsync(
      WTF::Bind(&WebGLRenderingContextBase::OnMakeXrCompatibleFinished,
                WrapWeakPersistent(this)));
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
        NOTREACHED();
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

    if (IdentifiabilityStudySettings::Get()->ShouldSample(
            IdentifiableSurface::FromTypeAndToken(
                IdentifiableSurface::Type::kWebFeature,
                WebFeature::kWebGLRenderingContextMakeXRCompatible))) {
      const auto& ukm_params = GetUkmParameters();
      IdentifiabilityMetricBuilder(ukm_params.source_id)
          .SetWebfeature(WebFeature::kWebGLRenderingContextMakeXRCompatible,
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
static const GLenum kSupportedInternalFormatsOESDepthTex[] = {
    GL_DEPTH_COMPONENT,
    GL_DEPTH_STENCIL,
};

// Exposed by GL_EXT_sRGB
static const GLenum kSupportedInternalFormatsEXTsRGB[] = {
    GL_SRGB,
    GL_SRGB_ALPHA_EXT,
};

// ES3 enums supported by both CopyTexImage and TexImage.
static const GLenum kSupportedInternalFormatsES3[] = {
    GL_R8,           GL_RG8,      GL_RGB565,   GL_RGB8,       GL_RGBA4,
    GL_RGB5_A1,      GL_RGBA8,    GL_RGB10_A2, GL_RGB10_A2UI, GL_SRGB8,
    GL_SRGB8_ALPHA8, GL_R8I,      GL_R8UI,     GL_R16I,       GL_R16UI,
    GL_R32I,         GL_R32UI,    GL_RG8I,     GL_RG8UI,      GL_RG16I,
    GL_RG16UI,       GL_RG32I,    GL_RG32UI,   GL_RGBA8I,     GL_RGBA8UI,
    GL_RGBA16I,      GL_RGBA16UI, GL_RGBA32I,  GL_RGBA32UI,   GL_RGB32I,
    GL_RGB32UI,      GL_RGB8I,    GL_RGB8UI,   GL_RGB16I,     GL_RGB16UI,
};

// ES3 enums only supported by TexImage
static const GLenum kSupportedInternalFormatsTexImageES3[] = {
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
};

// Exposed by EXT_texture_norm16
static constexpr GLenum kSupportedInternalFormatsEXTTextureNorm16ES3[] = {
    GL_R16_EXT,         GL_RG16_EXT,        GL_RGB16_EXT,
    GL_RGBA16_EXT,      GL_R16_SNORM_EXT,   GL_RG16_SNORM_EXT,
    GL_RGB16_SNORM_EXT, GL_RGBA16_SNORM_EXT};

static constexpr GLenum kSupportedFormatsEXTTextureNorm16ES3[] = {GL_RED,
                                                                  GL_RG};

static constexpr GLenum kSupportedTypesEXTTextureNorm16ES3[] = {
    GL_SHORT, GL_UNSIGNED_SHORT};

// Exposed by EXT_color_buffer_float
static const GLenum kSupportedInternalFormatsCopyTexImageFloatES3[] = {
    GL_R16F,   GL_R32F,    GL_RG16F,   GL_RG32F,         GL_RGB16F,
    GL_RGB32F, GL_RGBA16F, GL_RGBA32F, GL_R11F_G11F_B10F};

// Exposed by EXT_color_buffer_half_float
static const GLenum kSupportedInternalFormatsCopyTexImageHalfFloatES3[] = {
    GL_R16F,
    GL_RG16F,
    GL_RGB16F,
    GL_RGBA16F,
};

// ES3 enums supported by TexImageSource
static const GLenum kSupportedInternalFormatsTexImageSourceES3[] = {
    GL_R8,      GL_R16F,           GL_R32F,         GL_R8UI,     GL_RG8,
    GL_RG16F,   GL_RG32F,          GL_RG8UI,        GL_RGB8,     GL_SRGB8,
    GL_RGB565,  GL_R11F_G11F_B10F, GL_RGB9_E5,      GL_RGB16F,   GL_RGB32F,
    GL_RGB8UI,  GL_RGBA8,          GL_SRGB8_ALPHA8, GL_RGB5_A1,  GL_RGBA4,
    GL_RGBA16F, GL_RGBA32F,        GL_RGBA8UI,      GL_RGB10_A2,
};

// ES2 enums
// Internalformat must equal format in ES2.
static const GLenum kSupportedFormatsES2[] = {
    GL_RGB, GL_RGBA, GL_LUMINANCE_ALPHA, GL_LUMINANCE, GL_ALPHA,
};

// Exposed by GL_ANGLE_depth_texture
static const GLenum kSupportedFormatsOESDepthTex[] = {
    GL_DEPTH_COMPONENT,
    GL_DEPTH_STENCIL,
};

// Exposed by GL_EXT_sRGB
static const GLenum kSupportedFormatsEXTsRGB[] = {
    GL_SRGB,
    GL_SRGB_ALPHA_EXT,
};

// ES3 enums
static const GLenum kSupportedFormatsES3[] = {
    GL_RED,           GL_RED_INTEGER,  GL_RG,
    GL_RG_INTEGER,    GL_RGB,          GL_RGB_INTEGER,
    GL_RGBA,          GL_RGBA_INTEGER, GL_DEPTH_COMPONENT,
    GL_DEPTH_STENCIL,
};

// ES3 enums supported by TexImageSource
static const GLenum kSupportedFormatsTexImageSourceES3[] = {
    GL_RED, GL_RED_INTEGER, GL_RG,   GL_RG_INTEGER,
    GL_RGB, GL_RGB_INTEGER, GL_RGBA, GL_RGBA_INTEGER,
};

// ES2 enums
static const GLenum kSupportedTypesES2[] = {
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_SHORT_5_6_5,
    GL_UNSIGNED_SHORT_4_4_4_4,
    GL_UNSIGNED_SHORT_5_5_5_1,
};

// Exposed by GL_OES_texture_float
static const GLenum kSupportedTypesOESTexFloat[] = {
    GL_FLOAT,
};

// Exposed by GL_OES_texture_half_float
static const GLenum kSupportedTypesOESTexHalfFloat[] = {
    GL_HALF_FLOAT_OES,
};

// Exposed by GL_ANGLE_depth_texture
static const GLenum kSupportedTypesOESDepthTex[] = {
    GL_UNSIGNED_SHORT,
    GL_UNSIGNED_INT,
    GL_UNSIGNED_INT_24_8,
};

// ES3 enums
static const GLenum kSupportedTypesES3[] = {
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
};

// ES3 enums supported by TexImageSource
static const GLenum kSupportedTypesTexImageSourceES3[] = {
    GL_HALF_FLOAT,
    GL_FLOAT,
    GL_UNSIGNED_INT_10F_11F_11F_REV,
    GL_UNSIGNED_INT_2_10_10_10_REV,
};

}  // namespace

WebGLRenderingContextBase::WebGLRenderingContextBase(
    CanvasRenderingContextHost* host,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    bool using_gpu_compositing,
    const CanvasContextCreationAttributesCore& requested_attributes,
    Platform::ContextType version)
    : WebGLRenderingContextBase(
          host,
          host->GetTopExecutionContext()->GetTaskRunner(TaskType::kWebGL),
          std::move(context_provider),
          using_gpu_compositing,
          requested_attributes,
          version) {}

WebGLRenderingContextBase::WebGLRenderingContextBase(
    CanvasRenderingContextHost* host,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    bool using_gpu_compositing,
    const CanvasContextCreationAttributesCore& requested_attributes,
    Platform::ContextType context_type)
    : CanvasRenderingContext(host, requested_attributes),
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
      program_completion_queries_(
          base::MRUCache<WebGLProgram*, GLuint>::NO_AUTO_EVICT),
      number_of_user_allocated_multisampled_renderbuffers_(0) {
  DCHECK(context_provider);

  xr_compatible_ = requested_attributes.xr_compatible;

  context_group_->AddContext(this);

  max_viewport_dims_[0] = max_viewport_dims_[1] = 0;
  context_provider->ContextGL()->GetIntegerv(GL_MAX_VIEWPORT_DIMS,
                                             max_viewport_dims_);
  InitializeWebGLContextLimits(context_provider.get());

  scoped_refptr<DrawingBuffer> buffer;
  buffer =
      CreateDrawingBuffer(std::move(context_provider), using_gpu_compositing);
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

#define ADD_VALUES_TO_SET(set, values)              \
  for (size_t i = 0; i < base::size(values); ++i) { \
    set.insert(values[i]);                          \
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
    bool using_gpu_compositing) {
  const CanvasContextCreationAttributesCore& attrs = CreationAttributes();
  bool premultiplied_alpha = attrs.premultiplied_alpha;
  bool want_alpha_channel = attrs.alpha;
  bool want_depth_buffer = attrs.depth;
  bool want_stencil_buffer = attrs.stencil;
  bool want_antialiasing = attrs.antialias;
  DrawingBuffer::PreserveDrawingBuffer preserve = attrs.preserve_drawing_buffer
                                                      ? DrawingBuffer::kPreserve
                                                      : DrawingBuffer::kDiscard;
  DrawingBuffer::WebGLVersion web_gl_version = DrawingBuffer::kWebGL1;
  if (context_type_ == Platform::kWebGL1ContextType) {
    web_gl_version = DrawingBuffer::kWebGL1;
  } else if (context_type_ == Platform::kWebGL2ContextType) {
    web_gl_version = DrawingBuffer::kWebGL2;
  } else {
    NOTREACHED();
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

  bool using_swap_chain =
      base::FeatureList::IsEnabled(features::kLowLatencyWebGLSwapChain) &&
      context_provider->GetCapabilities().shared_image_swap_chain &&
      attrs.desynchronized;

  return DrawingBuffer::Create(
      std::move(context_provider), using_gpu_compositing, using_swap_chain,
      this, ClampedCanvasSize(), premultiplied_alpha, want_alpha_channel,
      want_depth_buffer, want_stencil_buffer, want_antialiasing, preserve,
      web_gl_version, chromium_image_usage, Host()->FilterQuality(),
      CanvasRenderingContextColorParams(),
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
  if (!context->Is3d())
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
    ContentChangeType change_type) {
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
    DidDraw();
    return;
  }

  if (!canvas())
    return;

  if (!marked_canvas_dirty_) {
    marked_canvas_dirty_ = true;
    LayoutBox* layout_box = canvas()->GetLayoutBox();
    auto* settings = canvas()->GetDocument().GetSettings();
    if (layout_box && settings->GetAcceleratedCompositingEnabled())
      layout_box->ContentChanged(change_type);
    IntSize canvas_size = ClampedCanvasSize();
    DidDraw(SkIRect::MakeXYWH(0, 0, canvas_size.Width(), canvas_size.Height()));
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
WebGLRenderingContextBase::GetContextTaskRunner() {
  return task_runner_;
}

void WebGLRenderingContextBase::DidDraw(const SkIRect& dirty_rect) {
  MarkContextChanged(kCanvasChanged);
  CanvasRenderingContext::DidDraw(dirty_rect);
}

void WebGLRenderingContextBase::DidDraw() {
  MarkContextChanged(kCanvasChanged);
  CanvasRenderingContext::DidDraw();
}

bool WebGLRenderingContextBase::PushFrame() {
  int submitted_frame = false;
  if (PaintRenderingResultsToCanvas(kBackBuffer)) {
    if (Host()->GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)) {
      int width = GetDrawingBuffer()->Size().Width();
      int height = GetDrawingBuffer()->Size().Height();
      submitted_frame =
          Host()->PushFrame(Host()->ResourceProvider()->ProduceCanvasResource(),
                            SkIRect::MakeWH(width, height));
    }
  }
  MarkLayerComposited();
  return submitted_frame;
}

void WebGLRenderingContextBase::FinalizeFrame() {
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

  GLbitfield buffers_needing_clearing =
      GetDrawingBuffer()->GetBuffersToAutoClear();

  if (buffers_needing_clearing == 0 || (mask && framebuffer_binding_) ||
      (rasterizer_discard_enabled_ && caller == kClearCallerDrawOrClear))
    return kSkipped;

  if (isContextLost()) {
    // Unlikely, but context was lost.
    return kSkipped;
  }

  // Determine if it's possible to combine the clear the user asked for and this
  // clear.
  bool combined_clear = mask && !scissor_enabled_;

  ContextGL()->Disable(GL_SCISSOR_TEST);
  if (combined_clear && (mask & GL_COLOR_BUFFER_BIT)) {
    ContextGL()->ClearColor(color_mask_[0] ? clear_color_[0] : 0,
                            color_mask_[1] ? clear_color_[1] : 0,
                            color_mask_[2] ? clear_color_[2] : 0,
                            color_mask_[3] ? clear_color_[3] : 0);
  } else {
    ContextGL()->ClearColor(0, 0, 0, 0);
  }
  ContextGL()->ColorMask(
      true, true, true,
      !GetDrawingBuffer()->RequiresAlphaChannelToBePreserved());
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

  ContextGL()->ColorMask(
      true, true, true,
      !GetDrawingBuffer()->DefaultBufferRequiresAlphaChannelToBePreserved());
  {
    ScopedDisableRasterizerDiscard scoped_disable(this,
                                                  rasterizer_discard_enabled_);
    // If the WebGL 2.0 clearBuffer APIs already have been used to
    // selectively clear some of the buffers, don't destroy those
    // results.
    GetDrawingBuffer()->ClearFramebuffers(clear_mask &
                                          buffers_needing_clearing);
  }

  // Call the DrawingBufferClient method to restore scissor test, mask, and
  // clear values, because we dirtied them above.
  DrawingBufferClientRestoreScissorTest();
  DrawingBufferClientRestoreMaskAndClearValues();

  GetDrawingBuffer()->SetBuffersToAutoClear(0);

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
    GetDrawingBuffer()->ResetBuffersToAutoClear();
}

bool WebGLRenderingContextBase::UsingSwapChain() const {
  return GetDrawingBuffer() && GetDrawingBuffer()->UsingSwapChain();
}

bool WebGLRenderingContextBase::IsOriginTopLeft() const {
  if (isContextLost())
    return false;
  return is_origin_top_left_;
}

void WebGLRenderingContextBase::SetIsInHiddenPage(bool hidden) {
  is_hidden_ = hidden;
  if (GetDrawingBuffer())
    GetDrawingBuffer()->SetIsInHiddenPage(hidden);

  if (!hidden && isContextLost() && restore_allowed_ &&
      auto_recovery_method_ == kAuto && !restore_timer_.IsActive()) {
    restore_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
  }
}

bool WebGLRenderingContextBase::PaintRenderingResultsToCanvas(
    SourceDrawingBuffer source_buffer) {
  if (isContextLost() || !GetDrawingBuffer())
    return false;

  bool must_clear_now = ClearIfComposited(kClearCallerOther) != kSkipped;
  if (!must_paint_to_canvas_ && !must_clear_now)
    return false;

  must_paint_to_canvas_ = false;

  if (Host()->ResourceProvider() &&
      Host()->ResourceProvider()->Size() != GetDrawingBuffer()->Size()) {
    Host()->DiscardResourceProvider();
  }

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
    if (!resource_provider->ImportResource(GetDrawingBuffer()->AsCanvasResource(
            resource_provider->CreateWeakPtr()))) {
      // This isn't expected to fail for single buffered resource provider.
      NOTREACHED();
      return false;
    }
    return true;
  }

  ScopedTexture2DRestorer restorer(this);
  ScopedFramebufferRestorer fbo_restorer(this);

  GetDrawingBuffer()->ResolveAndBindForReadAndDraw();
  if (!CopyRenderingResultsFromDrawingBuffer(Host()->ResourceProvider(),
                                             source_buffer)) {
    // Currently, CopyRenderingResultsFromDrawingBuffer is expected to always
    // succeed because cases where canvas()-buffer() is not accelerated are
    // handled before reaching this point.  If that assumption ever stops
    // holding true, we may need to implement a fallback right here.
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::CopyRenderingResultsFromDrawingBuffer(
    CanvasResourceProvider* resource_provider,
    SourceDrawingBuffer source_buffer) {
  DCHECK(drawing_buffer_);
  DCHECK(resource_provider);
  DCHECK(!resource_provider->IsSingleBuffered());
  if (resource_provider->IsAccelerated()) {
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> shared_context_wrapper =
        SharedGpuContext::ContextProviderWrapper();
    if (!shared_context_wrapper || !shared_context_wrapper->ContextProvider())
      return false;
    gpu::raster::RasterInterface* raster_interface =
        shared_context_wrapper->ContextProvider()->RasterInterface();
    const gpu::Mailbox& mailbox =
        resource_provider->GetBackingMailboxForOverwrite(
            MailboxSyncMode::kOrderingBarrier);
    GLenum texture_target = resource_provider->GetBackingTextureTarget();
    if (mailbox.IsZero())
      return false;

    // TODO(xlai): Flush should not be necessary if the synchronization in
    // CopyToPlatformTexture is done correctly. See crbug.com/794706.
    raster_interface->Flush();

    bool flip_y = IsOriginTopLeft() != resource_provider->IsOriginTopLeft();
    return drawing_buffer_->CopyToPlatformMailbox(
        raster_interface, mailbox, texture_target, flip_y, IntPoint(0, 0),
        IntRect(IntPoint(0, 0), drawing_buffer_->Size()), source_buffer);
  }

  // Note: This code path could work for all cases. The only reason there
  // is a separate path for the accelerated case is that we assume texture
  // copying is faster than drawImage.
  scoped_refptr<StaticBitmapImage> image = GetImage();
  if (!image || !image->PaintImageForCurrentFrame())
    return false;
  cc::PaintFlags paint_flags;
  paint_flags.setBlendMode(SkBlendMode::kSrc);
  resource_provider->Canvas()->drawImage(image->PaintImageForCurrentFrame(), 0,
                                         0, &paint_flags);
  return true;
}

IntSize WebGLRenderingContextBase::DrawingBufferSize() const {
  if (isContextLost())
    return IntSize(0, 0);
  return GetDrawingBuffer()->Size();
}

sk_sp<SkData> WebGLRenderingContextBase::PaintRenderingResultsToDataArray(
    SourceDrawingBuffer source_buffer) {
  if (isContextLost())
    return nullptr;
  ClearIfComposited(kClearCallerOther);
  GetDrawingBuffer()->ResolveAndBindForReadAndDraw();
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
  GetDrawingBuffer()->Resize(IntSize(width, height));

  if (buffer) {
    ContextGL()->BindBuffer(GL_PIXEL_UNPACK_BUFFER,
                            static_cast<GLuint>(buffer));
  }
}

int WebGLRenderingContextBase::drawingBufferWidth() const {
  return isContextLost() ? 0 : GetDrawingBuffer()->Size().Width();
}

int WebGLRenderingContextBase::drawingBufferHeight() const {
  return isContextLost() ? 0 : GetDrawingBuffer()->Size().Height();
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
      NOTREACHED();
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
  } else if (target == GL_TEXTURE_VIDEO_IMAGE_WEBGL) {
    if (!ExtensionEnabled(kWebGLVideoTextureName)) {
      SynthesizeGLError(
          GL_INVALID_VALUE, "bindTexture",
          "unhandled type, WEBGL_video_texture extension not enabled");
      return;
    }
    texture_units_[active_texture_unit_].texture_video_image_binding_ = texture;
  } else {
    SynthesizeGLError(GL_INVALID_ENUM, "bindTexture", "invalid target");
    return;
  }

  // We use TEXTURE_EXTERNAL_OES to implement video texture on Android platform
  if (target == GL_TEXTURE_VIDEO_IMAGE_WEBGL) {
#if defined(OS_ANDROID)
    // TODO(crbug.com/776222): Support extension on Android
    NOTIMPLEMENTED();
    return;
#else
    // TODO(crbug.com/776222): Using GL_TEXTURE_VIDEO_IMAGE_WEBGL in blink
    ContextGL()->BindTexture(GL_TEXTURE_2D, ObjectOrZero(texture));
    if (texture && !texture->GetTarget()) {
      ContextGL()->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                 GL_LINEAR);
      ContextGL()->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                 GL_LINEAR);
      ContextGL()->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                 GL_CLAMP_TO_EDGE);
      ContextGL()->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                 GL_CLAMP_TO_EDGE);
    }
#endif  // defined OS_ANDROID
  } else {
    ContextGL()->BindTexture(target, ObjectOrZero(texture));
  }
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
  ContextGL()->BlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
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
                                           DOMArrayBuffer* data,
                                           GLenum usage) {
  if (isContextLost())
    return;
  if (!data) {
    SynthesizeGLError(GL_INVALID_VALUE, "bufferData", "no data");
    return;
  }
  BufferDataImpl(target, data->ByteLength(), data->Data(), usage);
}

void WebGLRenderingContextBase::bufferData(GLenum target,
                                           MaybeShared<DOMArrayBufferView> data,
                                           GLenum usage) {
  if (isContextLost())
    return;
  DCHECK(data);
  BufferDataImpl(target, data.View()->byteLength(),
                 data.View()->BaseAddressMaybeShared(), usage);
}

void WebGLRenderingContextBase::BufferSubDataImpl(GLenum target,
                                                  int64_t offset,
                                                  GLsizeiptr size,
                                                  const void* data) {
  WebGLBuffer* buffer = ValidateBufferDataTarget("bufferSubData", target);
  if (!buffer)
    return;
  if (!ValidateValueFitNonNegInt32("bufferSubData", "offset", offset))
    return;
  if (!data)
    return;
  if (offset + static_cast<int64_t>(size) > buffer->GetSize()) {
    SynthesizeGLError(GL_INVALID_VALUE, "bufferSubData", "buffer overflow");
    return;
  }

  ContextGL()->BufferSubData(target, static_cast<GLintptr>(offset), size, data);
}

void WebGLRenderingContextBase::bufferSubData(GLenum target,
                                              int64_t offset,
                                              DOMArrayBuffer* data) {
  if (isContextLost())
    return;
  DCHECK(data);
  BufferSubDataImpl(target, offset, data->ByteLength(), data->Data());
}

void WebGLRenderingContextBase::bufferSubData(
    GLenum target,
    int64_t offset,
    const FlexibleArrayBufferView& data) {
  if (isContextLost())
    return;
  DCHECK(!data.IsNull());
  BufferSubDataImpl(target, offset, data.ByteLength(),
                    data.BaseAddressMaybeOnStack());
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

  ScopedRGBEmulationColorMask emulation_color_mask(this, color_mask_,
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
  MarkContextChanged(kCanvasChanged);
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
  if (!ValidateTexture2DBinding("compressedTexImage2D", target))
    return;
  if (!ValidateCompressedTexFormat("compressedTexImage2D", internalformat))
    return;
  GLsizei data_length;
  if (!ExtractDataLengthIfValid("compressedTexImage2D", data, &data_length))
    return;
  ContextGL()->CompressedTexImage2D(target, level, internalformat, width,
                                    height, border, data_length,
                                    data.View()->BaseAddressMaybeShared());
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
  ContextGL()->CompressedTexSubImage2D(target, level, xoffset, yoffset, width,
                                       height, format, data_length,
                                       data.View()->BaseAddressMaybeShared());
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

  if (supported_internal_formats_copy_tex_image_.find(internalformat) ==
      supported_internal_formats_copy_tex_image_.end()) {
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
  if (!ValidateTexture2DBinding("copyTexImage2D", target))
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
    ApplyStencilTest();
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

void WebGLRenderingContextBase::drawArraysImpl(GLenum mode,
                                               GLint first,
                                               GLsizei count) {
  if (!ValidateDrawArrays("drawArrays"))
    return;

  if (!bound_vertex_array_object_->IsAllEnabledAttribBufferBound()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "drawArrays",
                      "no buffer is bound to enabled attribute");
    return;
  }

  ScopedRGBEmulationColorMask emulation_color_mask(this, color_mask_,
                                                   drawing_buffer_.get());
  OnBeforeDrawCall();
  ContextGL()->DrawArrays(mode, first, count);
  RecordUKMCanvasDrawnToAtFirstDrawCall();
}

void WebGLRenderingContextBase::drawElementsImpl(GLenum mode,
                                                 GLsizei count,
                                                 GLenum type,
                                                 int64_t offset) {
  if (!ValidateDrawElements("drawElements", type, offset))
    return;

  if (!bound_vertex_array_object_->IsAllEnabledAttribBufferBound()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "drawElements",
                      "no buffer is bound to enabled attribute");
    return;
  }

  ScopedRGBEmulationColorMask emulation_color_mask(this, color_mask_,
                                                   drawing_buffer_.get());
  OnBeforeDrawCall();
  ContextGL()->DrawElements(
      mode, count, type,
      reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
  RecordUKMCanvasDrawnToAtFirstDrawCall();
}

void WebGLRenderingContextBase::DrawArraysInstancedANGLE(GLenum mode,
                                                         GLint first,
                                                         GLsizei count,
                                                         GLsizei primcount) {
  if (!ValidateDrawArrays("drawArraysInstancedANGLE"))
    return;

  if (!bound_vertex_array_object_->IsAllEnabledAttribBufferBound()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "drawArraysInstancedANGLE",
                      "no buffer is bound to enabled attribute");
    return;
  }

  ScopedRGBEmulationColorMask emulation_color_mask(this, color_mask_,
                                                   drawing_buffer_.get());
  OnBeforeDrawCall();
  ContextGL()->DrawArraysInstancedANGLE(mode, first, count, primcount);
  RecordUKMCanvasDrawnToAtFirstDrawCall();
}

void WebGLRenderingContextBase::DrawElementsInstancedANGLE(GLenum mode,
                                                           GLsizei count,
                                                           GLenum type,
                                                           int64_t offset,
                                                           GLsizei primcount) {
  if (!ValidateDrawElements("drawElementsInstancedANGLE", type, offset))
    return;

  if (!bound_vertex_array_object_->IsAllEnabledAttribBufferBound()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "drawElementsInstancedANGLE",
                      "no buffer is bound to enabled attribute");
    return;
  }

  ScopedRGBEmulationColorMask emulation_color_mask(this, color_mask_,
                                                   drawing_buffer_.get());
  OnBeforeDrawCall();
  ContextGL()->DrawElementsInstancedANGLE(
      mode, count, type, reinterpret_cast<void*>(static_cast<intptr_t>(offset)),
      primcount);
  RecordUKMCanvasDrawnToAtFirstDrawCall();
}

void WebGLRenderingContextBase::enable(GLenum cap) {
  if (isContextLost() || !ValidateCapability("enable", cap))
    return;
  if (cap == GL_STENCIL_TEST) {
    stencil_enabled_ = true;
    ApplyStencilTest();
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
  ApplyStencilTest();
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
  ApplyStencilTest();
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

base::Optional<HeapVector<Member<WebGLShader>>>
WebGLRenderingContextBase::getAttachedShaders(WebGLProgram* program) {
  if (!ValidateWebGLProgramOrShader("getAttachedShaders", program))
    return base::nullopt;

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
  if (!lost_context_errors_.IsEmpty()) {
    GLenum error = lost_context_errors_.front();
    lost_context_errors_.EraseAt(0);
    return error;
  }

  if (isContextLost())
    return GL_NO_ERROR;

  if (!synthetic_errors_.IsEmpty()) {
    GLenum error = synthetic_errors_.front();
    synthetic_errors_.EraseAt(0);
    return error;
  }

  return ContextGL()->GetError();
}

const char* const* WebGLRenderingContextBase::ExtensionTracker::Prefixes()
    const {
  static const char* const kUnprefixed[] = {
      "",
      nullptr,
  };
  return prefixes_ ? prefixes_ : kUnprefixed;
}

bool WebGLRenderingContextBase::ExtensionTracker::MatchesNameWithPrefixes(
    const String& name) const {
  const char* const* prefix_set = Prefixes();
  for (; *prefix_set; ++prefix_set) {
    String prefixed_name = String(*prefix_set) + ExtensionName();
    if (DeprecatedEqualIgnoringCase(prefixed_name, name)) {
      return true;
    }
  }
  return false;
}

bool WebGLRenderingContextBase::ExtensionSupportedAndAllowed(
    const ExtensionTracker* tracker) {
  if (tracker->Draft() &&
      !RuntimeEnabledFeatures::WebGLDraftExtensionsEnabled())
    return false;
  if (!tracker->Supported(this))
    return false;
  if (disabled_extensions_.Contains(String(tracker->ExtensionName())))
    return false;
  return true;
}

ScriptValue WebGLRenderingContextBase::getExtension(ScriptState* script_state,
                                                    const String& name) {
  WebGLExtension* extension = nullptr;

  if (name == WebGLDebugRendererInfo::ExtensionName()) {
    ExecutionContext* context = ExecutionContext::From(script_state);
    UseCounter::Count(context, WebFeature::kWebGLDebugRendererInfo);
    Dactyloscoper::Record(context, WebFeature::kWebGLDebugRendererInfo);
  }

  if (!isContextLost()) {
    for (ExtensionTracker* tracker : extensions_) {
      if (tracker->MatchesNameWithPrefixes(name)) {
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

  v8::Local<v8::Value> wrapped_extension =
      ToV8(extension, script_state->GetContext()->Global(),
           script_state->GetIsolate());

  return ScriptValue(script_state->GetIsolate(), wrapped_extension);
}

ScriptValue WebGLRenderingContextBase::getFramebufferAttachmentParameter(
    ScriptState* script_state,
    GLenum target,
    GLenum attachment,
    GLenum pname) {
  if (isContextLost() ||
      !ValidateFramebufferFuncParameters("getFramebufferAttachmentParameter",
                                         target, attachment))
    return ScriptValue::CreateNull(script_state->GetIsolate());

  if (!framebuffer_binding_ || !framebuffer_binding_->Object()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "getFramebufferAttachmentParameter",
                      "no framebuffer bound");
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  if (framebuffer_binding_ && framebuffer_binding_->Opaque()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "getFramebufferAttachmentParameter",
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
    SynthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter",
                      "invalid parameter name");
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  DCHECK(attachment_object->IsTexture() || attachment_object->IsRenderbuffer());
  if (attachment_object->IsTexture()) {
    switch (pname) {
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        return WebGLAny(script_state, GL_TEXTURE);
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        return WebGLAny(script_state, attachment_object);
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE: {
        GLint value = 0;
        ContextGL()->GetFramebufferAttachmentParameteriv(target, attachment,
                                                         pname, &value);
        return WebGLAny(script_state, value);
      }
      case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT:
        if (ExtensionEnabled(kEXTsRGBName)) {
          GLint value = 0;
          ContextGL()->GetFramebufferAttachmentParameteriv(target, attachment,
                                                           pname, &value);
          return WebGLAny(script_state, static_cast<unsigned>(value));
        }
        SynthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter",
                          "invalid parameter name for renderbuffer attachment");
        return ScriptValue::CreateNull(script_state->GetIsolate());
      default:
        SynthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter",
                          "invalid parameter name for texture attachment");
        return ScriptValue::CreateNull(script_state->GetIsolate());
    }
  } else {
    switch (pname) {
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        return WebGLAny(script_state, GL_RENDERBUFFER);
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        return WebGLAny(script_state, attachment_object);
      case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT:
        if (ExtensionEnabled(kEXTsRGBName)) {
          GLint value = 0;
          ContextGL()->GetFramebufferAttachmentParameteriv(target, attachment,
                                                           pname, &value);
          return WebGLAny(script_state, value);
        }
        SynthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter",
                          "invalid parameter name for renderbuffer attachment");
        return ScriptValue::CreateNull(script_state->GetIsolate());
      default:
        SynthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter",
                          "invalid parameter name for renderbuffer attachment");
        return ScriptValue::CreateNull(script_state->GetIsolate());
    }
  }
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
  return IdentifiabilityStudySettings::Get()->ShouldSample(
             blink::IdentifiableSurface::Type::kWebGLParameter) &&
         std::find(std::begin(kIdentifiableGLParams),
                   std::end(kIdentifiableGLParams),
                   pname) != std::end(kIdentifiableGLParams);
}

}  // namespace

void WebGLRenderingContextBase::RecordIdentifiableGLParameterDigest(
    GLenum pname,
    IdentifiableToken value) {
  DCHECK(IdentifiabilityStudySettings::Get()->IsTypeAllowed(
      blink::IdentifiableSurface::Type::kWebGLParameter));
  const auto ukm_params = GetUkmParameters();
  blink::IdentifiabilityMetricBuilder(ukm_params.source_id)
      .Set(blink::IdentifiableSurface::FromTypeAndToken(
               blink::IdentifiableSurface::Type::kWebGLParameter, pname),
           value)
      .Record(ukm_params.ukm_recorder);
}

void WebGLRenderingContextBase::RecordShaderPrecisionFormatForStudy(
    GLenum shader_type,
    GLenum precision_type,
    WebGLShaderPrecisionFormat* format) {
  DCHECK(IdentifiabilityStudySettings::Get()->IsTypeAllowed(
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
      .Set(blink::IdentifiableSurface::FromTypeAndToken(
               blink::IdentifiableSurface::Type::kWebGLShaderPrecisionFormat,
               surface_token),
           sample_token)
      .Record(ukm_params.ukm_recorder);
}

void WebGLRenderingContextBase::RecordUKMCanvasDrawnToAtFirstDrawCall() {
  if (!has_been_drawn_to_) {
    has_been_drawn_to_ = true;
    RecordUKMCanvasDrawnToRenderingAPI(
        GetCanvasRenderingAPIType(context_type_));
  }
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
      return WebGLAny(script_state, DOMUint32Array::Create(
                                        compressed_texture_formats_.data(),
                                        compressed_texture_formats_.size()));
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
      return GetBooleanParameter(script_state, pname);
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
      if (IdentifiabilityStudySettings::Get()->ShouldSample(
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
      if (IdentifiabilityStudySettings::Get()->ShouldSample(
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
        if (IdentifiabilityStudySettings::Get()->ShouldSample(
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
        if (IdentifiabilityStudySettings::Get()->ShouldSample(
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
      FALLTHROUGH;
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
      FALLTHROUGH;
    case GL_RENDERBUFFER_WIDTH:
    case GL_RENDERBUFFER_HEIGHT:
    case GL_RENDERBUFFER_RED_SIZE:
    case GL_RENDERBUFFER_GREEN_SIZE:
    case GL_RENDERBUFFER_BLUE_SIZE:
    case GL_RENDERBUFFER_ALPHA_SIZE:
    case GL_RENDERBUFFER_DEPTH_SIZE:
    case GL_RENDERBUFFER_STENCIL_SIZE:
      ContextGL()->GetRenderbufferParameteriv(target, pname, &value);
      if (IdentifiabilityStudySettings::Get()->ShouldSample(
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
  if (IdentifiabilityStudySettings::Get()->ShouldSample(
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

base::Optional<Vector<String>>
WebGLRenderingContextBase::getSupportedExtensions() {
  if (isContextLost())
    return base::nullopt;

  Vector<String> result;

  for (ExtensionTracker* tracker : extensions_) {
    if (ExtensionSupportedAndAllowed(tracker)) {
      const char* const* prefixes = tracker->Prefixes();
      for (; *prefixes; ++prefixes) {
        String prefixed_name = String(*prefixes) + tracker->ExtensionName();
        result.push_back(prefixed_name);
      }
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
  if (uniform_location->Program() != program) {
    SynthesizeGLError(GL_INVALID_OPERATION, "getUniform",
                      "no uniformlocation or not valid for this program");
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
          case GL_SAMPLER_VIDEO_IMAGE_WEBGL:
            if (!ExtensionEnabled(kWebGLVideoTextureName)) {
              SynthesizeGLError(
                  GL_INVALID_VALUE, "getUniform",
                  "unhandled type, WEBGL_video_texture extension not enabled");
              return ScriptValue::CreateNull(script_state->GetIsolate());
            }
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
            return WebGLAny(script_state,
                            DOMFloat32Array::Create(value, length));
          }
          case GL_INT: {
            GLint value[4] = {0};
            ContextGL()->GetUniformiv(ObjectOrZero(program), location, value);
            if (length == 1)
              return WebGLAny(script_state, value[0]);
            return WebGLAny(script_state, DOMInt32Array::Create(value, length));
          }
          case GL_UNSIGNED_INT: {
            GLuint value[4] = {0};
            ContextGL()->GetUniformuiv(ObjectOrZero(program), location, value);
            if (length == 1)
              return WebGLAny(script_state, value[0]);
            return WebGLAny(script_state,
                            DOMUint32Array::Create(value, length));
          }
          case GL_BOOL: {
            GLint value[4] = {0};
            ContextGL()->GetUniformiv(ObjectOrZero(program), location, value);
            if (length > 1) {
              bool bool_value[4] = {0};
              for (unsigned j = 0; j < length; j++)
                bool_value[j] = static_cast<bool>(value[j]);
              return WebGLAny(script_state, bool_value, length);
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
          return WebGLAny(script_state,
                          DOMFloat32Array::Create(float_value, 4));
        }
        case kInt32ArrayType: {
          GLint int_value[4];
          ContextGL()->GetVertexAttribIiv(index, pname, int_value);
          return WebGLAny(script_state, DOMInt32Array::Create(int_value, 4));
        }
        case kUint32ArrayType: {
          GLuint uint_value[4];
          ContextGL()->GetVertexAttribIuiv(index, pname, uint_value);
          return WebGLAny(script_state, DOMUint32Array::Create(uint_value, 4));
        }
        default:
          NOTREACHED();
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
      FALLTHROUGH;
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

GLboolean WebGLRenderingContextBase::isBuffer(WebGLBuffer* buffer) {
  if (!buffer || isContextLost() || !buffer->Validate(ContextGroup(), this))
    return 0;

  if (!buffer->HasEverBeenBound())
    return 0;
  if (buffer->MarkedForDeletion())
    return 0;

  return ContextGL()->IsBuffer(buffer->Object());
}

bool WebGLRenderingContextBase::isContextLost() const {
  return context_lost_mode_ != kNotLostContext;
}

GLboolean WebGLRenderingContextBase::isEnabled(GLenum cap) {
  if (isContextLost() || !ValidateCapability("isEnabled", cap))
    return 0;
  if (cap == GL_STENCIL_TEST)
    return stencil_enabled_;
  return ContextGL()->IsEnabled(cap);
}

GLboolean WebGLRenderingContextBase::isFramebuffer(
    WebGLFramebuffer* framebuffer) {
  if (!framebuffer || isContextLost() ||
      !framebuffer->Validate(ContextGroup(), this))
    return 0;

  if (!framebuffer->HasEverBeenBound())
    return 0;
  if (framebuffer->MarkedForDeletion())
    return 0;

  return ContextGL()->IsFramebuffer(framebuffer->Object());
}

GLboolean WebGLRenderingContextBase::isProgram(WebGLProgram* program) {
  if (!program || isContextLost() || !program->Validate(ContextGroup(), this))
    return 0;

  // OpenGL ES special-cases the behavior of program objects; if they're deleted
  // while attached to the current context state, glIsProgram is supposed to
  // still return true. For this reason, MarkedForDeletion is not checked here.

  return ContextGL()->IsProgram(program->Object());
}

GLboolean WebGLRenderingContextBase::isRenderbuffer(
    WebGLRenderbuffer* renderbuffer) {
  if (!renderbuffer || isContextLost() ||
      !renderbuffer->Validate(ContextGroup(), this))
    return 0;

  if (!renderbuffer->HasEverBeenBound())
    return 0;
  if (renderbuffer->MarkedForDeletion())
    return 0;

  return ContextGL()->IsRenderbuffer(renderbuffer->Object());
}

GLboolean WebGLRenderingContextBase::isShader(WebGLShader* shader) {
  if (!shader || isContextLost() || !shader->Validate(ContextGroup(), this))
    return 0;

  // OpenGL ES special-cases the behavior of shader objects; if they're deleted
  // while attached to a program, glIsShader is supposed to still return true.
  // For this reason, MarkedForDeletion is not checked here.

  return ContextGL()->IsShader(shader->Object());
}

GLboolean WebGLRenderingContextBase::isTexture(WebGLTexture* texture) {
  if (!texture || isContextLost() || !texture->Validate(ContextGroup(), this))
    return 0;

  if (!texture->HasEverBeenBound())
    return 0;
  if (texture->MarkedForDeletion())
    return 0;

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
  unsigned total_bytes_required = 0, total_skip_bytes = 0;
  GLenum error = WebGLImageConversion::ComputeImageSizeInBytes(
      format, type, width, height, 1, GetPackPixelStoreParams(),
      &total_bytes_required, nullptr, &total_skip_bytes);
  if (error != GL_NO_ERROR) {
    SynthesizeGLError(error, "readPixels", "invalid dimensions");
    return false;
  }
  if (buffer_size <
      static_cast<int64_t>(total_bytes_required + total_skip_bytes)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "readPixels",
                      "buffer is not large enough for dimensions");
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
  if (IdentifiabilityStudySettings::Get()->ShouldSample(
          blink::IdentifiableSurface::Type::kCanvasReadback)) {
    const auto& ukm_params = GetUkmParameters();
    blink::IdentifiabilityMetricBuilder(ukm_params.source_id)
        .Set(blink::IdentifiableSurface::FromTypeAndToken(
                 blink::IdentifiableSurface::Type::kCanvasReadback,
                 GetContextType()),
             0)
        .Record(ukm_params.ukm_recorder);
  }
  ReadPixelsHelper(x, y, width, height, format, type, pixels.View(), 0);
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
  base::CheckedNumeric<GLuint> offset_in_bytes = offset;
  offset_in_bytes *= pixels->TypeSize();
  if (!offset_in_bytes.IsValid() ||
      static_cast<size_t>(offset_in_bytes.ValueOrDie()) >
          pixels->byteLength()) {
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
  base::Optional<Vector<uint8_t>> buffer;
  if (!data && (width == 0 || height == 0)) {
    buffer.emplace(32);
    data = buffer->data();
  }
  {
    ScopedDrawingBufferBinder binder(GetDrawingBuffer(), framebuffer);
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
  DCHECK(!samples);             // |samples| > 0 is only valid in WebGL2's
                                // renderbufferStorageMultisample().
  DCHECK(!IsWebGL2());          // Make sure this is overridden in WebGL 2.
  switch (internalformat) {
    case GL_DEPTH_COMPONENT16:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGB565:
    case GL_STENCIL_INDEX8:
      ContextGL()->RenderbufferStorage(target, internalformat, width, height);
      renderbuffer_binding_->SetInternalFormat(internalformat);
      renderbuffer_binding_->SetSize(width, height);
      break;
    case GL_SRGB8_ALPHA8_EXT:
      if (!ExtensionEnabled(kEXTsRGBName)) {
        SynthesizeGLError(GL_INVALID_ENUM, function_name, "sRGB not enabled");
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
  ApplyStencilTest();
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
  String string_without_comments = StripComments(string).Result();
  shader->SetSource(string);
  if (!string_without_comments.Is8Bit() ||
      !string_without_comments.ContainsOnlyASCIIOrEmpty()) {
    SynthesizeGLError(
        GL_INVALID_VALUE, "shaderSource",
        "Non ASCII character detected after comments are stripped.");
    return;
  }
  const GLchar* shader_data =
      reinterpret_cast<const GLchar*>(string_without_comments.Characters8());
  const GLint shader_length = string_without_comments.length();
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

void WebGLRenderingContextBase::TexImage2DBase(GLenum target,
                                               GLint level,
                                               GLint internalformat,
                                               GLsizei width,
                                               GLsizei height,
                                               GLint border,
                                               GLenum format,
                                               GLenum type,
                                               const void* pixels) {
  // All calling functions check isContextLost, so a duplicate check is not
  // needed here.
  ContextGL()->TexImage2D(target, level,
                          ConvertTexInternalFormat(internalformat, type), width,
                          height, border, format, type, pixels);
}

// Software-based upload of Image* to WebGL texture.
void WebGLRenderingContextBase::TexImageImpl(
    TexImageFunctionID function_id,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLenum format,
    GLenum type,
    Image* image,
    WebGLImageConversion::ImageHtmlDomSource dom_source,
    bool flip_y,
    bool premultiply_alpha,
    const IntRect& source_image_rect,
    GLsizei depth,
    GLint unpack_image_height) {
  const char* func_name = GetTexImageFunctionName(function_id);
  // All calling functions check isContextLost, so a duplicate check is not
  // needed here.
  if (type == GL_UNSIGNED_INT_10F_11F_11F_REV) {
    // The UNSIGNED_INT_10F_11F_11F_REV type pack/unpack isn't implemented.
    type = GL_FLOAT;
  }
  Vector<uint8_t> data;

  IntRect sub_rect = source_image_rect;
  if (sub_rect.IsValid() && sub_rect == SentinelEmptyRect()) {
    // Recalculate based on the size of the Image.
    sub_rect = SafeGetImageSize(image);
  }

  bool selecting_sub_rectangle = false;
  if (!ValidateTexImageSubRectangle(func_name, function_id, image, sub_rect,
                                    depth, unpack_image_height,
                                    &selecting_sub_rectangle)) {
    return;
  }

  // Adjust the source image rectangle if doing a y-flip.
  IntRect adjusted_source_image_rect = sub_rect;
  if (flip_y) {
    adjusted_source_image_rect.SetY(image->height() -
                                    adjusted_source_image_rect.MaxY());
  }

  WebGLImageConversion::ImageExtractor image_extractor(
      image, dom_source, premultiply_alpha,
      unpack_colorspace_conversion_ == GL_NONE);
  if (!image_extractor.ImagePixelData()) {
    SynthesizeGLError(GL_INVALID_VALUE, func_name, "bad image data");
    return;
  }

  WebGLImageConversion::DataFormat source_data_format =
      image_extractor.ImageSourceFormat();
  WebGLImageConversion::AlphaOp alpha_op = image_extractor.ImageAlphaOp();
  const void* image_pixel_data = image_extractor.ImagePixelData();

  bool need_conversion = true;
  if (type == GL_UNSIGNED_BYTE &&
      source_data_format == WebGLImageConversion::kDataFormatRGBA8 &&
      format == GL_RGBA && alpha_op == WebGLImageConversion::kAlphaDoNothing &&
      !flip_y && !selecting_sub_rectangle && depth == 1) {
    need_conversion = false;
  } else {
    if (!WebGLImageConversion::PackImageData(
            image, image_pixel_data, format, type, flip_y, alpha_op,
            source_data_format, image_extractor.ImageWidth(),
            image_extractor.ImageHeight(), adjusted_source_image_rect, depth,
            image_extractor.ImageSourceUnpackAlignment(), unpack_image_height,
            data)) {
      SynthesizeGLError(GL_INVALID_VALUE, func_name, "packImage error");
      return;
    }
  }

  ScopedUnpackParametersResetRestore temporary_reset_unpack(this);
  if (function_id == kTexImage2D) {
    TexImage2DBase(target, level, internalformat,
                   adjusted_source_image_rect.Width(),
                   adjusted_source_image_rect.Height(), 0, format, type,
                   need_conversion ? data.data() : image_pixel_data);
  } else if (function_id == kTexSubImage2D) {
    ContextGL()->TexSubImage2D(
        target, level, xoffset, yoffset, adjusted_source_image_rect.Width(),
        adjusted_source_image_rect.Height(), format, type,
        need_conversion ? data.data() : image_pixel_data);
  } else {
    // 3D functions.
    if (function_id == kTexImage3D) {
      ContextGL()->TexImage3D(
          target, level, internalformat, adjusted_source_image_rect.Width(),
          adjusted_source_image_rect.Height(), depth, 0, format, type,
          need_conversion ? data.data() : image_pixel_data);
    } else {
      DCHECK_EQ(function_id, kTexSubImage3D);
      ContextGL()->TexSubImage3D(
          target, level, xoffset, yoffset, zoffset,
          adjusted_source_image_rect.Width(),
          adjusted_source_image_rect.Height(), depth, format, type,
          need_conversion ? data.data() : image_pixel_data);
    }
  }
}

bool WebGLRenderingContextBase::ValidateTexFunc(
    const char* function_name,
    TexImageFunctionType function_type,
    TexFuncValidationSourceType source_type,
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset) {
  if (!ValidateTexFuncLevel(function_name, target, level))
    return false;

  if (!ValidateTexFuncParameters(function_name, function_type, source_type,
                                 target, level, internalformat, width, height,
                                 depth, border, format, type))
    return false;

  if (function_type == kTexSubImage) {
    if (!ValidateSettableTexFormat(function_name, format))
      return false;
    if (!ValidateSize(function_name, xoffset, yoffset, zoffset))
      return false;
  } else {
    // For SourceArrayBufferView, function validateTexFuncData() would handle
    // whether to validate the SettableTexFormat
    // by checking if the ArrayBufferView is null or not.
    if (source_type != kSourceArrayBufferView) {
      if (!ValidateSettableTexFormat(function_name, format))
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

// TODO(fmalita): figure why WebGLImageConversion::ImageExtractor can't handle
// SVG-backed images, and get rid of this intermediate step.
scoped_refptr<Image> WebGLRenderingContextBase::DrawImageIntoBuffer(
    scoped_refptr<Image> pass_image,
    int width,
    int height,
    const char* function_name) {
  scoped_refptr<Image> image(std::move(pass_image));
  DCHECK(image);

  IntSize size(width, height);
  CanvasResourceProvider* resource_provider =
      generated_image_cache_.GetCanvasResourceProvider(size);
  if (!resource_provider) {
    SynthesizeGLError(GL_OUT_OF_MEMORY, function_name, "out of memory");
    return nullptr;
  }

  if (!image->CurrentFrameKnownToBeOpaque())
    resource_provider->Canvas()->clear(SK_ColorTRANSPARENT);

  IntRect src_rect(IntPoint(), image->Size());
  IntRect dest_rect(0, 0, size.Width(), size.Height());
  PaintFlags flags;
  // TODO(ccameron): WebGL should produce sRGB images.
  // https://crbug.com/672299
  image->Draw(resource_provider->Canvas(), flags, FloatRect(dest_rect),
              FloatRect(src_rect), kRespectImageOrientation,
              Image::kDoNotClampImageToSourceRect, Image::kSyncDecode);
  return resource_provider->Snapshot();
}

WebGLTexture* WebGLRenderingContextBase::ValidateTexImageBinding(
    const char* func_name,
    TexImageFunctionID function_id,
    GLenum target) {
  return ValidateTexture2DBinding(func_name, target);
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

IntRect WebGLRenderingContextBase::SentinelEmptyRect() {
  // Return a rectangle with -1 width and height so we can recognize
  // it later and recalculate it based on the Image whose data we'll
  // upload. It's important that there be no possible differences in
  // the logic which computes the image's size.
  return IntRect(0, 0, -1, -1);
}

IntRect WebGLRenderingContextBase::SafeGetImageSize(Image* image) {
  if (!image)
    return IntRect();

  return GetTextureSourceSize(image);
}

IntRect WebGLRenderingContextBase::GetImageDataSize(ImageData* pixels) {
  DCHECK(pixels);
  return GetTextureSourceSize(pixels);
}

void WebGLRenderingContextBase::TexImageHelperDOMArrayBufferView(
    TexImageFunctionID function_id,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    DOMArrayBufferView* pixels,
    NullDisposition null_disposition,
    GLuint src_offset) {
  const char* func_name = GetTexImageFunctionName(function_id);
  if (isContextLost())
    return;
  if (!ValidateTexImageBinding(func_name, function_id, target))
    return;
  TexImageFunctionType function_type;
  if (function_id == kTexImage2D || function_id == kTexImage3D)
    function_type = kTexImage;
  else
    function_type = kTexSubImage;
  if (!ValidateTexFunc(func_name, function_type, kSourceArrayBufferView, target,
                       level, internalformat, width, height, depth, border,
                       format, type, xoffset, yoffset, zoffset))
    return;
  TexImageDimension source_type;
  if (function_id == kTexImage2D || function_id == kTexSubImage2D)
    source_type = kTex2D;
  else
    source_type = kTex3D;
  if (!ValidateTexFuncData(func_name, source_type, level, width, height, depth,
                           format, type, pixels, null_disposition, src_offset))
    return;
  uint8_t* data = reinterpret_cast<uint8_t*>(
      pixels ? pixels->BaseAddressMaybeShared() : nullptr);
  if (src_offset) {
    DCHECK(pixels);
    // No need to check overflow because validateTexFuncData() already did.
    data += src_offset * pixels->TypeSize();
  }
  Vector<uint8_t> temp_data;
  bool change_unpack_params = false;
  if (data && width && height &&
      (unpack_flip_y_ || unpack_premultiply_alpha_)) {
    DCHECK_EQ(kTex2D, source_type);
    // Only enter here if width or height is non-zero. Otherwise, call to the
    // underlying driver to generate appropriate GL errors if needed.
    WebGLImageConversion::PixelStoreParams unpack_params =
        GetUnpackPixelStoreParams(kTex2D);
    GLint data_store_width =
        unpack_params.row_length ? unpack_params.row_length : width;
    if (unpack_params.skip_pixels + width > data_store_width) {
      SynthesizeGLError(GL_INVALID_OPERATION, func_name,
                        "Invalid unpack params combination.");
      return;
    }
    if (!WebGLImageConversion::ExtractTextureData(
            width, height, format, type, unpack_params, unpack_flip_y_,
            unpack_premultiply_alpha_, data, temp_data)) {
      SynthesizeGLError(GL_INVALID_OPERATION, func_name,
                        "Invalid format/type combination.");
      return;
    }
    data = temp_data.data();
    change_unpack_params = true;
  }
  if (function_id == kTexImage3D) {
    ContextGL()->TexImage3D(target, level,
                            ConvertTexInternalFormat(internalformat, type),
                            width, height, depth, border, format, type, data);
    return;
  }
  if (function_id == kTexSubImage3D) {
    ContextGL()->TexSubImage3D(target, level, xoffset, yoffset, zoffset, width,
                               height, depth, format, type, data);
    return;
  }

  ScopedUnpackParametersResetRestore temporary_reset_unpack(
      this, change_unpack_params);
  if (function_id == kTexImage2D)
    TexImage2DBase(target, level, internalformat, width, height, border, format,
                   type, data);
  else if (function_id == kTexSubImage2D)
    ContextGL()->TexSubImage2D(target, level, xoffset, yoffset, width, height,
                               format, type, data);
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
  TexImageHelperDOMArrayBufferView(kTexImage2D, target, level, internalformat,
                                   width, height, 1, border, format, type, 0, 0,
                                   0, pixels.View(), kNullAllowed, 0);
}

void WebGLRenderingContextBase::TexImageHelperImageData(
    TexImageFunctionID function_id,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLint border,
    GLenum format,
    GLenum type,
    GLsizei depth,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    ImageData* pixels,
    const IntRect& source_image_rect,
    GLint unpack_image_height) {
  const char* func_name = GetTexImageFunctionName(function_id);
  if (isContextLost())
    return;
  DCHECK(pixels);
  DCHECK(!pixels->data().IsNull());
  if (pixels->BufferBase()->IsDetached()) {
    SynthesizeGLError(GL_INVALID_VALUE, func_name,
                      "The source data has been detached.");
    return;
  }

  if (!ValidateTexImageBinding(func_name, function_id, target))
    return;
  TexImageFunctionType function_type;
  if (function_id == kTexImage2D || function_id == kTexImage3D)
    function_type = kTexImage;
  else
    function_type = kTexSubImage;
  if (!ValidateTexFunc(func_name, function_type, kSourceImageData, target,
                       level, internalformat, pixels->width(), pixels->height(),
                       depth, border, format, type, xoffset, yoffset, zoffset))
    return;

  bool selecting_sub_rectangle = false;
  if (!ValidateTexImageSubRectangle(
          func_name, function_id, pixels, source_image_rect, depth,
          unpack_image_height, &selecting_sub_rectangle)) {
    return;
  }
  // Adjust the source image rectangle if doing a y-flip.
  IntRect adjusted_source_image_rect = source_image_rect;
  if (unpack_flip_y_) {
    adjusted_source_image_rect.SetY(pixels->height() -
                                    adjusted_source_image_rect.MaxY());
  }

  // TODO(crbug.com/1115317): Should be compatible with uint_8, float16 and
  // float32.
  Vector<uint8_t> data;
  bool need_conversion = true;
  // The data from ImageData is always of format RGBA8.
  // No conversion is needed if destination format is RGBA and type is
  // UNSIGNED_BYTE and no Flip or Premultiply operation is required.
  if (!unpack_flip_y_ && !unpack_premultiply_alpha_ && format == GL_RGBA &&
      type == GL_UNSIGNED_BYTE && !selecting_sub_rectangle && depth == 1) {
    need_conversion = false;
  } else {
    if (type == GL_UNSIGNED_INT_10F_11F_11F_REV) {
      // The UNSIGNED_INT_10F_11F_11F_REV type pack/unpack isn't implemented.
      type = GL_FLOAT;
    }
    if (!WebGLImageConversion::ExtractImageData(
            pixels->data().GetAsUint8ClampedArray()->Data(),
            WebGLImageConversion::DataFormat::kDataFormatRGBA8, pixels->Size(),
            adjusted_source_image_rect, depth, unpack_image_height, format,
            type, unpack_flip_y_, unpack_premultiply_alpha_, data)) {
      SynthesizeGLError(GL_INVALID_VALUE, func_name, "bad image data");
      return;
    }
  }
  ScopedUnpackParametersResetRestore temporary_reset_unpack(this);
  const uint8_t* bytes = need_conversion
                             ? data.data()
                             : pixels->data().GetAsUint8ClampedArray()->Data();
  if (function_id == kTexImage2D) {
    DCHECK_EQ(unpack_image_height, 0);
    TexImage2DBase(
        target, level, internalformat, adjusted_source_image_rect.Width(),
        adjusted_source_image_rect.Height(), border, format, type, bytes);
  } else if (function_id == kTexSubImage2D) {
    DCHECK_EQ(unpack_image_height, 0);
    ContextGL()->TexSubImage2D(
        target, level, xoffset, yoffset, adjusted_source_image_rect.Width(),
        adjusted_source_image_rect.Height(), format, type, bytes);
  } else {
    GLint upload_height = adjusted_source_image_rect.Height();
    if (function_id == kTexImage3D) {
      ContextGL()->TexImage3D(target, level, internalformat,
                              adjusted_source_image_rect.Width(), upload_height,
                              depth, border, format, type, bytes);
    } else {
      DCHECK_EQ(function_id, kTexSubImage3D);
      ContextGL()->TexSubImage3D(target, level, xoffset, yoffset, zoffset,
                                 adjusted_source_image_rect.Width(),
                                 upload_height, depth, format, type, bytes);
    }
  }
}

void WebGLRenderingContextBase::texImage2D(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           ImageData* pixels) {
  TexImageHelperImageData(kTexImage2D, target, level, internalformat, 0, format,
                          type, 1, 0, 0, 0, pixels, GetImageDataSize(pixels),
                          0);
}

void WebGLRenderingContextBase::TexImageHelperHTMLImageElement(
    const SecurityOrigin* security_origin,
    TexImageFunctionID function_id,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    HTMLImageElement* image,
    const IntRect& source_image_rect,
    GLsizei depth,
    GLint unpack_image_height,
    ExceptionState& exception_state) {
  const char* func_name = GetTexImageFunctionName(function_id);
  if (isContextLost())
    return;

  if (!ValidateHTMLImageElement(security_origin, func_name, image,
                                exception_state))
    return;
  if (!ValidateTexImageBinding(func_name, function_id, target))
    return;

  scoped_refptr<Image> image_for_render = image->CachedImage()->GetImage();
  bool have_svg_image = IsA<SVGImage>(image_for_render.get());
  if (have_svg_image || !image_for_render->HasDefaultOrientation()) {
    if (have_svg_image && canvas()) {
      UseCounter::Count(canvas()->GetDocument(), WebFeature::kSVGInWebGL);
    }
    // DrawImageIntoBuffer always respects orientation
    image_for_render =
        DrawImageIntoBuffer(std::move(image_for_render), image->width(),
                            image->height(), func_name);
  }

  TexImageFunctionType function_type;
  if (function_id == kTexImage2D || function_id == kTexImage3D)
    function_type = kTexImage;
  else
    function_type = kTexSubImage;
  if (!image_for_render ||
      !ValidateTexFunc(func_name, function_type, kSourceHTMLImageElement,
                       target, level, internalformat, image_for_render->width(),
                       image_for_render->height(), depth, 0, format, type,
                       xoffset, yoffset, zoffset))
    return;

  TexImageImpl(function_id, target, level, internalformat, xoffset, yoffset,
               zoffset, format, type, image_for_render.get(),
               WebGLImageConversion::kHtmlDomImage, unpack_flip_y_,
               unpack_premultiply_alpha_, source_image_rect, depth,
               unpack_image_height);
}

void WebGLRenderingContextBase::texImage2D(ExecutionContext* execution_context,
                                           GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           HTMLImageElement* image,
                                           ExceptionState& exception_state) {
  TexImageHelperHTMLImageElement(execution_context->GetSecurityOrigin(),
                                 kTexImage2D, target, level, internalformat,
                                 format, type, 0, 0, 0, image,
                                 SentinelEmptyRect(), 1, 0, exception_state);
}

bool WebGLRenderingContextBase::CanUseTexImageViaGPU(GLenum format,
                                                     GLenum type) {
#if defined(OS_MAC)
  // RGB5_A1 is not color-renderable on NVIDIA Mac, see crbug.com/676209.
  // Though, glCopyTextureCHROMIUM can handle RGB5_A1 internalformat by doing a
  // fallback path, but it doesn't know the type info. So, we still cannot do
  // the fallback path in glCopyTextureCHROMIUM for
  // RGBA/RGBA/UNSIGNED_SHORT_5_5_5_1 format and type combination.
  if (type == GL_UNSIGNED_SHORT_5_5_5_1)
    return false;
#endif

  // TODO(kbr): continued bugs are seen on Linux with AMD's drivers handling
  // uploads to R8UI textures. crbug.com/710673
  if (format == GL_RED_INTEGER)
    return false;

#if defined(OS_ANDROID)
  // TODO(kbr): bugs were seen on Android devices with NVIDIA GPUs
  // when copying hardware-accelerated video textures to
  // floating-point textures. Investigate the root cause of this and
  // fix it. crbug.com/710874
  if (type == GL_FLOAT)
    return false;
#endif

  // OES_texture_half_float doesn't support HALF_FLOAT_OES type for
  // CopyTexImage/CopyTexSubImage. And OES_texture_half_float doesn't require
  // HALF_FLOAT_OES type texture to be renderable. So, HALF_FLOAT_OES type
  // texture cannot be copied to or drawn to by glCopyTextureCHROMIUM.
  if (type == GL_HALF_FLOAT_OES)
    return false;

  return true;
}

void WebGLRenderingContextBase::TexImageViaGPU(
    TexImageFunctionID function_id,
    WebGLTexture* texture,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    AcceleratedStaticBitmapImage* source_image,
    WebGLRenderingContextBase* source_canvas_webgl_context,
    const IntRect& source_sub_rectangle,
    bool premultiply_alpha,
    bool flip_y) {
  bool have_source_image = source_image;
  bool have_source_canvas_webgl_context = source_canvas_webgl_context;
  DCHECK(have_source_image ^ have_source_canvas_webgl_context);

  int width = source_sub_rectangle.Width();
  int height = source_sub_rectangle.Height();

  ScopedTexture2DRestorer restorer(this);

  GLuint target_texture = texture->Object();
  bool possible_direct_copy = false;
  if (function_id == kTexImage2D || function_id == kTexSubImage2D) {
    possible_direct_copy = Extensions3DUtil::CanUseCopyTextureCHROMIUM(target);
  }

  GLint copy_x_offset = xoffset;
  GLint copy_y_offset = yoffset;
  GLenum copy_target = target;

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
    ContextGL()->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                            GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    copy_x_offset = 0;
    copy_y_offset = 0;
    copy_target = GL_TEXTURE_2D;
  }

  {
    // glCopyTextureCHROMIUM has a DRAW_AND_READBACK path which will call
    // texImage2D. So, reset unpack buffer parameters before that.
    ScopedUnpackParametersResetRestore temporaryResetUnpack(this);
    if (source_image) {
      source_image->CopyToTexture(
          ContextGL(), target, target_texture, level, premultiply_alpha, flip_y,
          IntPoint(xoffset, yoffset), source_sub_rectangle);
    } else {
      WebGLRenderingContextBase* gl = source_canvas_webgl_context;
      if (gl->is_origin_top_left_ && !canvas()->LowLatencyEnabled())
        flip_y = !flip_y;
      ScopedTexture2DRestorer inner_restorer(gl);
      if (!gl->GetDrawingBuffer()->CopyToPlatformTexture(
              ContextGL(), target, target_texture, level,
              unpack_premultiply_alpha_, !flip_y, IntPoint(xoffset, yoffset),
              source_sub_rectangle, kBackBuffer)) {
        NOTREACHED();
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
    if (function_id == kTexImage2D) {
      ContextGL()->CopyTexSubImage2D(target, level, 0, 0, 0, 0, width, height);
    } else if (function_id == kTexSubImage2D) {
      ContextGL()->CopyTexSubImage2D(target, level, xoffset, yoffset, 0, 0,
                                     width, height);
    } else if (function_id == kTexSubImage3D) {
      ContextGL()->CopyTexSubImage3D(target, level, xoffset, yoffset, zoffset,
                                     0, 0, width, height);
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
    TexImageFunctionID function_id,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    CanvasRenderingContextHost* context_host,
    const IntRect& source_sub_rectangle,
    GLsizei depth,
    GLint unpack_image_height,
    ExceptionState& exception_state) {
  const char* func_name = GetTexImageFunctionName(function_id);
  if (isContextLost())
    return;

  if (!ValidateCanvasRenderingContextHost(security_origin, func_name,
                                          context_host, exception_state))
    return;
  WebGLTexture* texture =
      ValidateTexImageBinding(func_name, function_id, target);
  if (!texture)
    return;
  TexImageFunctionType function_type;
  if (function_id == kTexImage2D)
    function_type = kTexImage;
  else
    function_type = kTexSubImage;
  if (!ValidateTexFunc(func_name, function_type, kSourceHTMLCanvasElement,
                       target, level, internalformat,
                       source_sub_rectangle.Width(),
                       source_sub_rectangle.Height(), depth, 0, format, type,
                       xoffset, yoffset, zoffset))
    return;

  // Note that the sub-rectangle validation is needed for the GPU-GPU
  // copy case, but is redundant for the software upload case
  // (texImageImpl).
  bool selecting_sub_rectangle = false;
  if (!ValidateTexImageSubRectangle(
          func_name, function_id, context_host, source_sub_rectangle, depth,
          unpack_image_height, &selecting_sub_rectangle)) {
    return;
  }

  bool is_webgl_canvas = context_host->Is3d();
  WebGLRenderingContextBase* source_canvas_webgl_context = nullptr;
  SourceImageStatus source_image_status = kInvalidSourceImageStatus;
  scoped_refptr<Image> image;

  bool upload_via_gpu =
      (function_id == kTexImage2D || function_id == kTexSubImage2D) &&
      CanUseTexImageViaGPU(format, type);

  // The Image-based upload path may still be used for WebGL-rendered
  // canvases in the case of driver bug workarounds
  // (e.g. CanUseTexImageViaGPU returning false).
  if (is_webgl_canvas && upload_via_gpu) {
    source_canvas_webgl_context =
        To<WebGLRenderingContextBase>(context_host->RenderingContext());
  } else {
    image = context_host->GetSourceImageForCanvas(
        &source_image_status,
        FloatSize(source_sub_rectangle.Width(), source_sub_rectangle.Height()));
    if (source_image_status != kNormalSourceImageStatus)
      return;
  }

  // Still not clear whether we will take the accelerated upload path
  // at this point; it depends on what came back from
  // CanUseTexImageViaGPU, for example.
  auto* static_bitmap_image = DynamicTo<StaticBitmapImage>(image.get());
  upload_via_gpu &= source_canvas_webgl_context ||
                    (static_bitmap_image && image->IsTextureBacked());

  if (upload_via_gpu) {
    AcceleratedStaticBitmapImage* accel_image = nullptr;
    if (image) {
      accel_image =
          static_cast<AcceleratedStaticBitmapImage*>(static_bitmap_image);
    }

    // The GPU-GPU copy path uses the Y-up coordinate system.
    IntRect adjusted_source_sub_rectangle = source_sub_rectangle;

    bool should_adjust_source_sub_rectangle = !unpack_flip_y_;
    if (is_origin_top_left_ && source_canvas_webgl_context)
      should_adjust_source_sub_rectangle = !should_adjust_source_sub_rectangle;

    if (should_adjust_source_sub_rectangle) {
      adjusted_source_sub_rectangle.SetY(context_host->Size().Height() -
                                         adjusted_source_sub_rectangle.MaxY());
    }

    if (function_id == kTexImage2D) {
      TexImage2DBase(target, level, internalformat,
                     source_sub_rectangle.Width(),
                     source_sub_rectangle.Height(), 0, format, type, nullptr);
      TexImageViaGPU(function_id, texture, target, level, 0, 0, 0, accel_image,
                     source_canvas_webgl_context, adjusted_source_sub_rectangle,
                     unpack_premultiply_alpha_, unpack_flip_y_);
    } else {
      TexImageViaGPU(function_id, texture, target, level, xoffset, yoffset, 0,
                     accel_image, source_canvas_webgl_context,
                     adjusted_source_sub_rectangle, unpack_premultiply_alpha_,
                     unpack_flip_y_);
    }
  } else {
    // If these are the 2D functions, the caller must have passed in 1
    // for the depth and 0 for the unpack_image_height.
    DCHECK(!(function_id == kTexSubImage2D || function_id == kTexSubImage2D) ||
           (depth == 1 && unpack_image_height == 0));
    // We expect an Image at this point, not a WebGL-rendered canvas.
    DCHECK(image);
    // TODO(crbug.com/612542): Implement GPU-to-GPU copy path for more
    // cases, like copying to layers of 3D textures, and elements of
    // 2D texture arrays.
    bool flip_y = unpack_flip_y_;
    if (is_origin_top_left_ && is_webgl_canvas)
      flip_y = !flip_y;

    TexImageImpl(function_id, target, level, internalformat, xoffset, yoffset,
                 zoffset, format, type, image.get(),
                 WebGLImageConversion::kHtmlDomCanvas, flip_y,
                 unpack_premultiply_alpha_, source_sub_rectangle, depth,
                 unpack_image_height);
  }
}

void WebGLRenderingContextBase::texImage2D(
    ExecutionContext* execution_context,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    CanvasRenderingContextHost* context_host,
    ExceptionState& exception_state) {
  TexImageHelperCanvasRenderingContextHost(
      execution_context->GetSecurityOrigin(), kTexImage2D, target, level,
      internalformat, format, type, 0, 0, 0, context_host,
      GetTextureSourceSize(context_host), 1, 0, exception_state);
}

scoped_refptr<Image> WebGLRenderingContextBase::VideoFrameToImage(
    HTMLVideoElement* video,
    int already_uploaded_id,
    WebMediaPlayer::VideoFrameUploadMetadata* out_metadata) {
  const IntSize& visible_size = video->videoVisibleSize();
  if (visible_size.IsEmpty()) {
    SynthesizeGLError(GL_INVALID_VALUE, "tex(Sub)Image2D",
                      "video visible size is empty");
    return nullptr;
  }
  CanvasResourceProvider* resource_provider =
      generated_image_cache_.GetCanvasResourceProvider(visible_size);
  if (!resource_provider) {
    SynthesizeGLError(GL_OUT_OF_MEMORY, "texImage2D", "out of memory");
    return nullptr;
  }
  IntRect dest_rect(0, 0, visible_size.Width(), visible_size.Height());
  video->PaintCurrentFrame(resource_provider->Canvas(), dest_rect, nullptr,
                           already_uploaded_id, out_metadata);
  return resource_provider->Snapshot();
}

void WebGLRenderingContextBase::TexImageHelperHTMLVideoElement(
    const SecurityOrigin* security_origin,
    TexImageFunctionID function_id,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    HTMLVideoElement* video,
    const IntRect& source_image_rect,
    GLsizei depth,
    GLint unpack_image_height,
    ExceptionState& exception_state) {
  const char* func_name = GetTexImageFunctionName(function_id);
  if (isContextLost())
    return;

  if (!ValidateHTMLVideoElement(security_origin, func_name, video,
                                exception_state))
    return;
  WebGLTexture* texture =
      ValidateTexImageBinding(func_name, function_id, target);
  if (!texture)
    return;
  TexImageFunctionType function_type;
  if (function_id == kTexImage2D || function_id == kTexImage3D)
    function_type = kTexImage;
  else
    function_type = kTexSubImage;
  if (!ValidateTexFunc(func_name, function_type, kSourceHTMLVideoElement,
                       target, level, internalformat, video->videoWidth(),
                       video->videoHeight(), 1, 0, format, type, xoffset,
                       yoffset, zoffset))
    return;

  GLint adjusted_internalformat =
      ConvertTexInternalFormat(internalformat, type);

  // For WebGL last-uploaded-frame-metadata API. https://crbug.com/639174
  WebMediaPlayer::VideoFrameUploadMetadata frame_metadata = {};
  int already_uploaded_id = -1;
  WebMediaPlayer::VideoFrameUploadMetadata* frame_metadata_ptr = nullptr;
  if (RuntimeEnabledFeatures::ExtraWebGLVideoTextureMetadataEnabled()) {
    already_uploaded_id = texture->GetLastUploadedVideoFrameId();
    frame_metadata_ptr = &frame_metadata;
  }

  if (!source_image_rect.IsValid()) {
    SynthesizeGLError(GL_INVALID_OPERATION, func_name,
                      "source sub-rectangle specified via pixel unpack "
                      "parameters is invalid");
    return;
  }
  bool source_image_rect_is_default =
      source_image_rect == SentinelEmptyRect() ||
      source_image_rect ==
          IntRect(0, 0, video->videoWidth(), video->videoHeight());

  const auto& caps = GetDrawingBuffer()->ContextProvider()->GetCapabilities();
  const bool may_need_image_external_essl3 =
      caps.egl_image_external &&
      Extensions3DUtil::CopyTextureCHROMIUMNeedsESSL3(internalformat);
  const bool have_image_external_essl3 = caps.egl_image_external_essl3;
  const bool use_copyTextureCHROMIUM =
      function_id == kTexImage2D && source_image_rect_is_default &&
      depth == 1 && GL_TEXTURE_2D == target &&
      (have_image_external_essl3 || !may_need_image_external_essl3) &&
      CanUseTexImageViaGPU(format, type);

  // Format of source video may be 16-bit format, e.g. Y16 format.
  // glCopyTextureCHROMIUM requires the source texture to be in 8-bit format.
  // Converting 16-bits formated source texture to 8-bits formated texture will
  // cause precision lost. So, uploading such video texture to half float or
  // float texture can not use GPU-GPU path.
  if (use_copyTextureCHROMIUM) {
    DCHECK(Extensions3DUtil::CanUseCopyTextureCHROMIUM(target));
    DCHECK_EQ(xoffset, 0);
    DCHECK_EQ(yoffset, 0);
    DCHECK_EQ(zoffset, 0);
    // Go through the fast path doing a GPU-GPU textures copy without a readback
    // to system memory if possible.  Otherwise, it will fall back to the normal
    // SW path.

    if (video->CopyVideoTextureToPlatformTexture(
            ContextGL(), target, texture->Object(), adjusted_internalformat,
            format, type, level, unpack_premultiply_alpha_, unpack_flip_y_,
            already_uploaded_id, frame_metadata_ptr)) {
      texture->UpdateLastUploadedFrame(frame_metadata);
      return;
    }

    // For certain video frame formats (e.g. I420/YUV), if they start on the CPU
    // (e.g. video camera frames): upload them to the GPU, do a GPU decode, and
    // then copy into the target texture.
    if (video->CopyVideoYUVDataToPlatformTexture(
            ContextGL(), target, texture->Object(), adjusted_internalformat,
            format, type, level, unpack_premultiply_alpha_, unpack_flip_y_,
            already_uploaded_id, frame_metadata_ptr)) {
      texture->UpdateLastUploadedFrame(frame_metadata);
      return;
    }
  }

  if (source_image_rect_is_default) {
    // Try using optimized CPU-GPU path for some formats: e.g. Y16 and Y8. It
    // leaves early for other formats or if frame is stored on GPU.
    ScopedUnpackParametersResetRestore unpack_params(
        this, unpack_flip_y_ || unpack_premultiply_alpha_);
    if (video->TexImageImpl(
            static_cast<WebMediaPlayer::TexImageFunctionID>(function_id),
            target, ContextGL(), texture->Object(), level,
            adjusted_internalformat, format, type, xoffset, yoffset, zoffset,
            unpack_flip_y_,
            unpack_premultiply_alpha_ &&
                unpack_colorspace_conversion_ == GL_NONE)) {
      texture->ClearLastUploadedFrame();
      return;
    }
  }

  scoped_refptr<Image> image =
      VideoFrameToImage(video, already_uploaded_id, frame_metadata_ptr);
  if (!image)
    return;
  TexImageImpl(function_id, target, level, adjusted_internalformat, xoffset,
               yoffset, zoffset, format, type, image.get(),
               WebGLImageConversion::kHtmlDomVideo, unpack_flip_y_,
               unpack_premultiply_alpha_, source_image_rect, depth,
               unpack_image_height);
  texture->UpdateLastUploadedFrame(frame_metadata);
}

void WebGLRenderingContextBase::texImage2D(ExecutionContext* execution_context,
                                           GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           HTMLVideoElement* video,
                                           ExceptionState& exception_state) {
  TexImageHelperHTMLVideoElement(execution_context->GetSecurityOrigin(),
                                 kTexImage2D, target, level, internalformat,
                                 format, type, 0, 0, 0, video,
                                 SentinelEmptyRect(), 1, 0, exception_state);
}

void WebGLRenderingContextBase::TexImageHelperImageBitmap(
    TexImageFunctionID function_id,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    ImageBitmap* bitmap,
    const IntRect& source_sub_rect,
    GLsizei depth,
    GLint unpack_image_height,
    ExceptionState& exception_state) {
  const char* func_name = GetTexImageFunctionName(function_id);
  if (isContextLost())
    return;
  if (!ValidateImageBitmap(func_name, bitmap, exception_state))
    return;
  WebGLTexture* texture =
      ValidateTexImageBinding(func_name, function_id, target);
  if (!texture)
    return;

  bool selecting_sub_rectangle = false;
  if (!ValidateTexImageSubRectangle(func_name, function_id, bitmap,
                                    source_sub_rect, depth, unpack_image_height,
                                    &selecting_sub_rectangle)) {
    return;
  }

  TexImageFunctionType function_type;
  if (function_id == kTexImage2D)
    function_type = kTexImage;
  else
    function_type = kTexSubImage;

  GLsizei width = source_sub_rect.Width();
  GLsizei height = source_sub_rect.Height();
  if (!ValidateTexFunc(func_name, function_type, kSourceImageBitmap, target,
                       level, internalformat, width, height, depth, 0, format,
                       type, xoffset, yoffset, zoffset))
    return;

  scoped_refptr<StaticBitmapImage> image = bitmap->BitmapImage();
  DCHECK(image);

  // TODO(kbr): make this work for sub-rectangles of ImageBitmaps.
  if (function_id != kTexSubImage3D && function_id != kTexImage3D &&
      image->IsTextureBacked() && CanUseTexImageViaGPU(format, type) &&
      !selecting_sub_rectangle) {
    AcceleratedStaticBitmapImage* accel_image =
        static_cast<AcceleratedStaticBitmapImage*>(image.get());
    // We hard-code premultiply_alpha and flip_y values because these should
    // have already been manipulated during construction of the ImageBitmap.
    bool premultiply_alpha = true;  // TODO(kbr): this looks wrong!
    bool flip_y = false;
    if (function_id == kTexImage2D) {
      TexImage2DBase(target, level, internalformat, width, height, 0, format,
                     type, nullptr);
      TexImageViaGPU(function_id, texture, target, level, 0, 0, 0, accel_image,
                     nullptr, source_sub_rect, premultiply_alpha, flip_y);
    } else if (function_id == kTexSubImage2D) {
      TexImageViaGPU(function_id, texture, target, level, xoffset, yoffset, 0,
                     accel_image, nullptr, source_sub_rect, premultiply_alpha,
                     flip_y);
    }
    return;
  }

  // Apply orientation if necessary
  PaintImage paint_image = bitmap->BitmapImage()->PaintImageForCurrentFrame();
  if (!image->HasDefaultOrientation()) {
    paint_image = Image::ResizeAndOrientImage(
        paint_image, image->CurrentFrameOrientation(), FloatSize(1, 1), 1,
        kInterpolationNone);
  }

  // TODO(kbr): refactor this away to use TexImageImpl on image.
  sk_sp<SkImage> sk_image = paint_image.GetSwSkImage();
  if (!sk_image) {
    SynthesizeGLError(GL_OUT_OF_MEMORY, func_name,
                      "ImageBitmap unexpectedly empty");
    return;
  }

  SkPixmap pixmap;
  uint8_t* pixel_data_ptr = nullptr;
  Vector<uint8_t> pixel_data;
  // PaintImage::GetSwSkImage() can return a lazily generated image which will
  // cause peekPixels() to fail. In that case we use CopyBitmapData to force
  // image generation.
  bool peek_succeed = sk_image->peekPixels(&pixmap);
  if (peek_succeed) {
    pixel_data_ptr = static_cast<uint8_t*>(pixmap.writable_addr());
  } else {
    SkImageInfo info = bitmap->GetBitmapSkImageInfo();
    info =
        info.makeAlphaType(bitmap->IsPremultiplied() ? kPremul_SkAlphaType
                                                     : kUnpremul_SkAlphaType);
    if (info.colorType() == kN32_SkColorType)
      info = info.makeColorType(kRGBA_8888_SkColorType);
    pixel_data = bitmap->CopyBitmapData(info, true);
    pixel_data_ptr = pixel_data.data();
  }
  Vector<uint8_t> data;
  bool need_conversion = true;
  bool have_peekable_rgba =
      (peek_succeed &&
       pixmap.colorType() == SkColorType::kRGBA_8888_SkColorType);
  bool is_pixel_data_rgba = (have_peekable_rgba || !peek_succeed);
  if (is_pixel_data_rgba && format == GL_RGBA && type == GL_UNSIGNED_BYTE &&
      !selecting_sub_rectangle && depth == 1) {
    need_conversion = false;
  } else {
    if (type == GL_UNSIGNED_INT_10F_11F_11F_REV) {
      // The UNSIGNED_INT_10F_11F_11F_REV type pack/unpack isn't implemented.
      type = GL_FLOAT;
    }
    WebGLImageConversion::DataFormat data_format;
    if (is_pixel_data_rgba) {
      data_format = WebGLImageConversion::DataFormat::kDataFormatRGBA8;
    } else {
      switch (pixmap.colorType()) {
        case SkColorType::kBGRA_8888_SkColorType:
          data_format = WebGLImageConversion::DataFormat::kDataFormatBGRA8;
          break;
        case SkColorType::kRGBA_F16_SkColorType:
          // Used in ImageBitmap's ApplyColorSpaceConversion.
          data_format = WebGLImageConversion::DataFormat::kDataFormatRGBA16F;
          break;
        default:
          // Can not handle this ImageBitmap's format.
          SynthesizeGLError(GL_INVALID_VALUE, func_name,
                            "unsupported color type / space in ImageBitmap");
          return;
      }
    }
    // In the case of ImageBitmap, we do not need to apply flipY or
    // premultiplyAlpha.
    if (!WebGLImageConversion::ExtractImageData(
            pixel_data_ptr, data_format, bitmap->Size(), source_sub_rect, depth,
            unpack_image_height, format, type, false, false, data)) {
      SynthesizeGLError(GL_INVALID_VALUE, func_name,
                        "error extracting data from ImageBitmap");
      return;
    }
  }
  ScopedUnpackParametersResetRestore temporary_reset_unpack(this);
  if (function_id == kTexImage2D) {
    TexImage2DBase(target, level, internalformat, width, height, 0, format,
                   type, need_conversion ? data.data() : pixel_data_ptr);
  } else if (function_id == kTexSubImage2D) {
    ContextGL()->TexSubImage2D(target, level, xoffset, yoffset, width, height,
                               format, type,
                               need_conversion ? data.data() : pixel_data_ptr);
  } else if (function_id == kTexImage3D) {
    ContextGL()->TexImage3D(target, level, internalformat, width, height, depth,
                            0, format, type,
                            need_conversion ? data.data() : pixel_data_ptr);
  } else {
    DCHECK_EQ(function_id, kTexSubImage3D);
    ContextGL()->TexSubImage3D(target, level, xoffset, yoffset, zoffset, width,
                               height, depth, format, type,
                               need_conversion ? data.data() : pixel_data_ptr);
  }
}

void WebGLRenderingContextBase::texImage2D(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           ImageBitmap* bitmap,
                                           ExceptionState& exception_state) {
  TexImageHelperImageBitmap(kTexImage2D, target, level, internalformat, format,
                            type, 0, 0, 0, bitmap, GetTextureSourceSize(bitmap),
                            1, 0, exception_state);
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
      if (target == GL_TEXTURE_VIDEO_IMAGE_WEBGL) {
        if ((is_float && paramf != GL_NEAREST && paramf != GL_LINEAR) ||
            (!is_float && parami != GL_NEAREST && parami != GL_LINEAR)) {
          SynthesizeGLError(GL_INVALID_ENUM, "texParameter",
                            "invalid parameter name");
          return;
        }
      }
      break;
    case GL_TEXTURE_MAG_FILTER:
      break;
    case GL_TEXTURE_WRAP_R:
      if (!IsWebGL2()) {
        SynthesizeGLError(GL_INVALID_ENUM, "texParameter",
                          "invalid parameter name");
        return;
      }
      FALLTHROUGH;
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
      if ((is_float && paramf != GL_CLAMP_TO_EDGE &&
           paramf != GL_MIRRORED_REPEAT && paramf != GL_REPEAT) ||
          (!is_float && parami != GL_CLAMP_TO_EDGE &&
           parami != GL_MIRRORED_REPEAT && parami != GL_REPEAT)) {
        SynthesizeGLError(GL_INVALID_ENUM, "texParameter", "invalid parameter");
        return;
      }

      if (target == GL_TEXTURE_VIDEO_IMAGE_WEBGL) {
        if ((is_float && paramf != GL_CLAMP_TO_EDGE) ||
            (!is_float && parami != GL_CLAMP_TO_EDGE)) {
          SynthesizeGLError(GL_INVALID_ENUM, "texParameter",
                            "invalid parameter");
          return;
        }
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
  TexImageHelperDOMArrayBufferView(kTexSubImage2D, target, level, 0, width,
                                   height, 1, 0, format, type, xoffset, yoffset,
                                   0, pixels.View(), kNullNotAllowed, 0);
}

void WebGLRenderingContextBase::texSubImage2D(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLenum format,
                                              GLenum type,
                                              ImageData* pixels) {
  TexImageHelperImageData(kTexSubImage2D, target, level, 0, 0, format, type, 1,
                          xoffset, yoffset, 0, pixels, GetImageDataSize(pixels),
                          0);
}

void WebGLRenderingContextBase::texSubImage2D(
    ExecutionContext* execution_context,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLenum format,
    GLenum type,
    HTMLImageElement* image,
    ExceptionState& exception_state) {
  TexImageHelperHTMLImageElement(execution_context->GetSecurityOrigin(),
                                 kTexSubImage2D, target, level, 0, format, type,
                                 xoffset, yoffset, 0, image,
                                 SentinelEmptyRect(), 1, 0, exception_state);
}

void WebGLRenderingContextBase::texSubImage2D(
    ExecutionContext* execution_context,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLenum format,
    GLenum type,
    CanvasRenderingContextHost* context_host,
    ExceptionState& exception_state) {
  TexImageHelperCanvasRenderingContextHost(
      execution_context->GetSecurityOrigin(), kTexSubImage2D, target, level, 0,
      format, type, xoffset, yoffset, 0, context_host,
      GetTextureSourceSize(context_host), 1, 0, exception_state);
}

void WebGLRenderingContextBase::texSubImage2D(
    ExecutionContext* execution_context,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLenum format,
    GLenum type,
    HTMLVideoElement* video,
    ExceptionState& exception_state) {
  TexImageHelperHTMLVideoElement(execution_context->GetSecurityOrigin(),
                                 kTexSubImage2D, target, level, 0, format, type,
                                 xoffset, yoffset, 0, video,
                                 SentinelEmptyRect(), 1, 0, exception_state);
}

void WebGLRenderingContextBase::texSubImage2D(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLenum format,
                                              GLenum type,
                                              ImageBitmap* bitmap,
                                              ExceptionState& exception_state) {
  TexImageHelperImageBitmap(
      kTexSubImage2D, target, level, 0, format, type, xoffset, yoffset, 0,
      bitmap, GetTextureSourceSize(bitmap), 1, 0, exception_state);
}

void WebGLRenderingContextBase::uniform1f(const WebGLUniformLocation* location,
                                          GLfloat x) {
  if (isContextLost() || !location)
    return;

  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, "uniform1f",
                      "location not for current program");
    return;
  }

  ContextGL()->Uniform1f(location->Location(), x);
}

void WebGLRenderingContextBase::uniform1fv(const WebGLUniformLocation* location,
                                           const FlexibleFloat32Array& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform1fv", location, v, 1, 0, v.length()))
    return;

  ContextGL()->Uniform1fv(location->Location(),
                          base::checked_cast<GLuint>(v.length()),
                          v.DataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform1fv(const WebGLUniformLocation* location,
                                           Vector<GLfloat>& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform1fv", location, v.data(), v.size(), 1,
                                 0, v.size()))
    return;

  ContextGL()->Uniform1fv(location->Location(), v.size(), v.data());
}

void WebGLRenderingContextBase::uniform1i(const WebGLUniformLocation* location,
                                          GLint x) {
  if (isContextLost() || !location)
    return;

  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, "uniform1i",
                      "location not for current program");
    return;
  }

  ContextGL()->Uniform1i(location->Location(), x);
}

void WebGLRenderingContextBase::uniform1iv(const WebGLUniformLocation* location,
                                           const FlexibleInt32Array& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform1iv", location, v, 1, 0, v.length()))
    return;

  ContextGL()->Uniform1iv(location->Location(),
                          base::checked_cast<GLuint>(v.length()),
                          v.DataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform1iv(const WebGLUniformLocation* location,
                                           Vector<GLint>& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform1iv", location, v.data(), v.size(), 1,
                                 0, v.size()))
    return;

  ContextGL()->Uniform1iv(location->Location(), v.size(), v.data());
}

void WebGLRenderingContextBase::uniform2f(const WebGLUniformLocation* location,
                                          GLfloat x,
                                          GLfloat y) {
  if (isContextLost() || !location)
    return;

  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, "uniform2f",
                      "location not for current program");
    return;
  }

  ContextGL()->Uniform2f(location->Location(), x, y);
}

void WebGLRenderingContextBase::uniform2fv(const WebGLUniformLocation* location,
                                           const FlexibleFloat32Array& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform2fv", location, v, 2, 0, v.length()))
    return;

  ContextGL()->Uniform2fv(location->Location(),
                          base::checked_cast<GLuint>(v.length()) >> 1,
                          v.DataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform2fv(const WebGLUniformLocation* location,
                                           Vector<GLfloat>& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform2fv", location, v.data(), v.size(), 2,
                                 0, v.size()))
    return;

  ContextGL()->Uniform2fv(location->Location(), v.size() >> 1, v.data());
}

void WebGLRenderingContextBase::uniform2i(const WebGLUniformLocation* location,
                                          GLint x,
                                          GLint y) {
  if (isContextLost() || !location)
    return;

  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, "uniform2i",
                      "location not for current program");
    return;
  }

  ContextGL()->Uniform2i(location->Location(), x, y);
}

void WebGLRenderingContextBase::uniform2iv(const WebGLUniformLocation* location,
                                           const FlexibleInt32Array& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform2iv", location, v, 2, 0, v.length()))
    return;

  ContextGL()->Uniform2iv(location->Location(),
                          base::checked_cast<GLuint>(v.length()) >> 1,
                          v.DataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform2iv(const WebGLUniformLocation* location,
                                           Vector<GLint>& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform2iv", location, v.data(), v.size(), 2,
                                 0, v.size()))
    return;

  ContextGL()->Uniform2iv(location->Location(), v.size() >> 1, v.data());
}

void WebGLRenderingContextBase::uniform3f(const WebGLUniformLocation* location,
                                          GLfloat x,
                                          GLfloat y,
                                          GLfloat z) {
  if (isContextLost() || !location)
    return;

  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, "uniform3f",
                      "location not for current program");
    return;
  }

  ContextGL()->Uniform3f(location->Location(), x, y, z);
}

void WebGLRenderingContextBase::uniform3fv(const WebGLUniformLocation* location,
                                           const FlexibleFloat32Array& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform3fv", location, v, 3, 0, v.length()))
    return;

  ContextGL()->Uniform3fv(location->Location(),
                          base::checked_cast<GLuint>(v.length()) / 3,
                          v.DataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform3fv(const WebGLUniformLocation* location,
                                           Vector<GLfloat>& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform3fv", location, v.data(), v.size(), 3,
                                 0, v.size()))
    return;

  ContextGL()->Uniform3fv(location->Location(), v.size() / 3, v.data());
}

void WebGLRenderingContextBase::uniform3i(const WebGLUniformLocation* location,
                                          GLint x,
                                          GLint y,
                                          GLint z) {
  if (isContextLost() || !location)
    return;

  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, "uniform3i",
                      "location not for current program");
    return;
  }

  ContextGL()->Uniform3i(location->Location(), x, y, z);
}

void WebGLRenderingContextBase::uniform3iv(const WebGLUniformLocation* location,
                                           const FlexibleInt32Array& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform3iv", location, v, 3, 0, v.length()))
    return;

  ContextGL()->Uniform3iv(location->Location(),
                          base::checked_cast<GLuint>(v.length()) / 3,
                          v.DataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform3iv(const WebGLUniformLocation* location,
                                           Vector<GLint>& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform3iv", location, v.data(), v.size(), 3,
                                 0, v.size()))
    return;

  ContextGL()->Uniform3iv(location->Location(), v.size() / 3, v.data());
}

void WebGLRenderingContextBase::uniform4f(const WebGLUniformLocation* location,
                                          GLfloat x,
                                          GLfloat y,
                                          GLfloat z,
                                          GLfloat w) {
  if (isContextLost() || !location)
    return;

  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, "uniform4f",
                      "location not for current program");
    return;
  }

  ContextGL()->Uniform4f(location->Location(), x, y, z, w);
}

void WebGLRenderingContextBase::uniform4fv(const WebGLUniformLocation* location,
                                           const FlexibleFloat32Array& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform4fv", location, v, 4, 0, v.length()))
    return;

  ContextGL()->Uniform4fv(location->Location(),
                          base::checked_cast<GLuint>(v.length()) >> 2,
                          v.DataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform4fv(const WebGLUniformLocation* location,
                                           Vector<GLfloat>& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform4fv", location, v.data(), v.size(), 4,
                                 0, v.size()))
    return;

  ContextGL()->Uniform4fv(location->Location(), v.size() >> 2, v.data());
}

void WebGLRenderingContextBase::uniform4i(const WebGLUniformLocation* location,
                                          GLint x,
                                          GLint y,
                                          GLint z,
                                          GLint w) {
  if (isContextLost() || !location)
    return;

  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, "uniform4i",
                      "location not for current program");
    return;
  }

  ContextGL()->Uniform4i(location->Location(), x, y, z, w);
}

void WebGLRenderingContextBase::uniform4iv(const WebGLUniformLocation* location,
                                           const FlexibleInt32Array& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform4iv", location, v, 4, 0, v.length()))
    return;

  ContextGL()->Uniform4iv(location->Location(),
                          base::checked_cast<GLuint>(v.length()) >> 2,
                          v.DataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform4iv(const WebGLUniformLocation* location,
                                           Vector<GLint>& v) {
  if (isContextLost() ||
      !ValidateUniformParameters("uniform4iv", location, v.data(), v.size(), 4,
                                 0, v.size()))
    return;

  ContextGL()->Uniform4iv(location->Location(), v.size() >> 2, v.data());
}

void WebGLRenderingContextBase::uniformMatrix2fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    MaybeShared<DOMFloat32Array> v) {
  if (isContextLost() ||
      !ValidateUniformMatrixParameters("uniformMatrix2fv", location, transpose,
                                       v.View(), 4, 0, v.View()->length()))
    return;
  ContextGL()->UniformMatrix2fv(
      location->Location(), base::checked_cast<GLuint>(v.View()->length()) >> 2,
      transpose, v.View()->DataMaybeShared());
}

void WebGLRenderingContextBase::uniformMatrix2fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    Vector<GLfloat>& v) {
  if (isContextLost() ||
      !ValidateUniformMatrixParameters("uniformMatrix2fv", location, transpose,
                                       v.data(), v.size(), 4, 0, v.size()))
    return;
  ContextGL()->UniformMatrix2fv(location->Location(), v.size() >> 2, transpose,
                                v.data());
}

void WebGLRenderingContextBase::uniformMatrix3fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    MaybeShared<DOMFloat32Array> v) {
  if (isContextLost() ||
      !ValidateUniformMatrixParameters("uniformMatrix3fv", location, transpose,
                                       v.View(), 9, 0, v.View()->length()))
    return;
  ContextGL()->UniformMatrix3fv(
      location->Location(), base::checked_cast<GLuint>(v.View()->length()) / 9,
      transpose, v.View()->DataMaybeShared());
}

void WebGLRenderingContextBase::uniformMatrix3fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    Vector<GLfloat>& v) {
  if (isContextLost() ||
      !ValidateUniformMatrixParameters("uniformMatrix3fv", location, transpose,
                                       v.data(), v.size(), 9, 0, v.size()))
    return;
  ContextGL()->UniformMatrix3fv(location->Location(), v.size() / 9, transpose,
                                v.data());
}

void WebGLRenderingContextBase::uniformMatrix4fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    MaybeShared<DOMFloat32Array> v) {
  if (isContextLost() ||
      !ValidateUniformMatrixParameters("uniformMatrix4fv", location, transpose,
                                       v.View(), 16, 0, v.View()->length()))
    return;
  ContextGL()->UniformMatrix4fv(
      location->Location(), base::checked_cast<GLuint>(v.View()->length()) >> 4,
      transpose, v.View()->DataMaybeShared());
}

void WebGLRenderingContextBase::uniformMatrix4fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    Vector<GLfloat>& v) {
  if (isContextLost() ||
      !ValidateUniformMatrixParameters("uniformMatrix4fv", location, transpose,
                                       v.data(), v.size(), 16, 0, v.size()))
    return;
  ContextGL()->UniformMatrix4fv(location->Location(), v.size() >> 4, transpose,
                                v.data());
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

void WebGLRenderingContextBase::vertexAttrib1fv(
    GLuint index,
    MaybeShared<const DOMFloat32Array> v) {
  if (isContextLost())
    return;
  if (!v.View() || v.View()->length() < 1) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttrib1fv", "invalid array");
    return;
  }
  ContextGL()->VertexAttrib1fv(index, v.View()->DataMaybeShared());
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib1fv(GLuint index,
                                                const Vector<GLfloat>& v) {
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

void WebGLRenderingContextBase::vertexAttrib2fv(
    GLuint index,
    MaybeShared<const DOMFloat32Array> v) {
  if (isContextLost())
    return;
  if (!v.View() || v.View()->length() < 2) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttrib2fv", "invalid array");
    return;
  }
  ContextGL()->VertexAttrib2fv(index, v.View()->DataMaybeShared());
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib2fv(GLuint index,
                                                const Vector<GLfloat>& v) {
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

void WebGLRenderingContextBase::vertexAttrib3fv(
    GLuint index,
    MaybeShared<const DOMFloat32Array> v) {
  if (isContextLost())
    return;
  if (!v.View() || v.View()->length() < 3) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttrib3fv", "invalid array");
    return;
  }
  ContextGL()->VertexAttrib3fv(index, v.View()->DataMaybeShared());
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib3fv(GLuint index,
                                                const Vector<GLfloat>& v) {
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

void WebGLRenderingContextBase::vertexAttrib4fv(
    GLuint index,
    MaybeShared<const DOMFloat32Array> v) {
  if (isContextLost())
    return;
  if (!v.View() || v.View()->length() < 4) {
    SynthesizeGLError(GL_INVALID_VALUE, "vertexAttrib4fv", "invalid array");
    return;
  }
  ContextGL()->VertexAttrib4fv(index, v.View()->DataMaybeShared());
  SetVertexAttribType(index, kFloat32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib4fv(GLuint index,
                                                const Vector<GLfloat>& v) {
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

  RemoveAllCompressedTextureFormats();

  // If the DrawingBuffer is destroyed during a real lost context event it
  // causes the CommandBufferProxy that the DrawingBuffer owns, which is what
  // issued the lost context event in the first place, to be destroyed before
  // the event is done being handled. This causes a crash when an outstanding
  // AutoLock goes out of scope. To avoid this, we create a no-op task to hold
  // a reference to the DrawingBuffer until this function is done executing.
  if (mode == kRealLostContext) {
    task_runner_->PostTask(
        FROM_HERE,
        WTF::Bind(&WebGLRenderingContextBase::HoldReferenceToDrawingBuffer,
                  WrapWeakPersistent(this), WTF::RetainedRef(drawing_buffer_)));
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

void WebGLRenderingContextBase::SetFilterQuality(
    SkFilterQuality filter_quality) {
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
  ContextGL()->ColorMask(color_mask_[0], color_mask_[1], color_mask_[2],
                         color_mask_alpha);
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
    DrawingBufferClientForceLostContextWithAutoRecovery() {
  ForceLostContext(WebGLRenderingContextBase::kSyntheticLostContext,
                   WebGLRenderingContextBase::kAuto);
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
  GLboolean value[4] = {0};
  if (!isContextLost())
    ContextGL()->GetBooleanv(pname, value);
  bool bool_value[4];
  for (int ii = 0; ii < 4; ++ii)
    bool_value[ii] = static_cast<bool>(value[ii]);
  return WebGLAny(script_state, bool_value, 4);
}

ScriptValue WebGLRenderingContextBase::GetFloatParameter(
    ScriptState* script_state,
    GLenum pname) {
  GLfloat value = 0;
  if (!isContextLost())
    ContextGL()->GetFloatv(pname, &value);
  if (IdentifiabilityStudySettings::Get()->ShouldSample(
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
  if (IdentifiabilityStudySettings::Get()->ShouldSample(
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
  GLfloat value[4] = {0};
  if (!isContextLost())
    ContextGL()->GetFloatv(pname, value);
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
  return WebGLAny(script_state, DOMFloat32Array::Create(value, length));
}

ScriptValue WebGLRenderingContextBase::GetWebGLIntArrayParameter(
    ScriptState* script_state,
    GLenum pname) {
  GLint value[4] = {0};
  if (!isContextLost())
    ContextGL()->GetIntegerv(pname, value);
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
  return WebGLAny(script_state, DOMInt32Array::Create(value, length));
}

WebGLTexture* WebGLRenderingContextBase::ValidateTexture2DBinding(
    const char* function_name,
    GLenum target) {
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
  if (!tex)
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "no texture bound to target");
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
    case GL_TEXTURE_VIDEO_IMAGE_WEBGL:
      if (!ExtensionEnabled(kWebGLVideoTextureName)) {
        SynthesizeGLError(GL_INVALID_ENUM, function_name,
                          "invalid texture target");
        return nullptr;
      }
      tex = texture_units_[active_texture_unit_]
                .texture_video_image_binding_.Get();
      break;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name,
                        "invalid texture target");
      return nullptr;
  }
  if (!tex)
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "no texture bound to target");
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
    const char* function_name,
    TexImageFunctionType function_type,
    GLenum internalformat,
    GLenum format,
    GLenum type) {
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

  if (internalformat != 0 &&
      supported_tex_image_source_internal_formats_.find(internalformat) ==
          supported_tex_image_source_internal_formats_.end()) {
    if (function_type == kTexImage) {
      SynthesizeGLError(GL_INVALID_VALUE, function_name,
                        "invalid internalformat");
    } else {
      SynthesizeGLError(GL_INVALID_ENUM, function_name,
                        "invalid internalformat");
    }
    return false;
  }
  if (supported_tex_image_source_formats_.find(format) ==
      supported_tex_image_source_formats_.end()) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid format");
    return false;
  }
  if (supported_tex_image_source_types_.find(type) ==
      supported_tex_image_source_types_.end()) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid type");
    return false;
  }

  return true;
}

bool WebGLRenderingContextBase::ValidateTexFuncFormatAndType(
    const char* function_name,
    TexImageFunctionType function_type,
    GLenum internalformat,
    GLenum format,
    GLenum type,
    GLint level) {
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

  if (internalformat != 0 && supported_internal_formats_.find(internalformat) ==
                                 supported_internal_formats_.end()) {
    if (function_type == kTexImage) {
      if (compressed_texture_formats_.Contains(internalformat)) {
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
  if (supported_formats_.find(format) == supported_formats_.end()) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid format");
    return false;
  }
  if (supported_types_.find(type) == supported_types_.end()) {
    SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid type");
    return false;
  }

  if (format == GL_DEPTH_COMPONENT && level > 0 && !IsWebGL2()) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "level must be 0 for DEPTH_COMPONENT format");
    return false;
  }
  if (format == GL_DEPTH_STENCIL_OES && level > 0 && !IsWebGL2()) {
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
    case GL_TEXTURE_VIDEO_IMAGE_WEBGL:
      return 1;
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
    case GL_TEXTURE_VIDEO_IMAGE_WEBGL:
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
      FALLTHROUGH;
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
      FALLTHROUGH;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid target");
      return false;
  }
  return true;
}

bool WebGLRenderingContextBase::ValidateTexFuncParameters(
    const char* function_name,
    TexImageFunctionType function_type,
    TexFuncValidationSourceType source_type,
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type) {
  // We absolutely have to validate the format and type combination.
  // The texImage2D entry points taking HTMLImage, etc. will produce
  // temporary data based on this combination, so it must be legal.
  if (source_type == kSourceHTMLImageElement ||
      source_type == kSourceHTMLCanvasElement ||
      source_type == kSourceHTMLVideoElement ||
      source_type == kSourceImageData || source_type == kSourceImageBitmap) {
    if (!ValidateTexImageSourceFormatAndType(function_name, function_type,
                                             internalformat, format, type)) {
      return false;
    }
  } else {
    if (!ValidateTexFuncFormatAndType(function_name, function_type,
                                      internalformat, format, type, level)) {
      return false;
    }
  }

  if (!ValidateTexFuncDimensions(function_name, function_type, target, level,
                                 width, height, depth))
    return false;

  if (border) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "border != 0");
    return false;
  }

  return true;
}

bool WebGLRenderingContextBase::ValidateTexFuncData(
    const char* function_name,
    TexImageDimension tex_dimension,
    GLint level,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    GLenum type,
    DOMArrayBufferView* pixels,
    NullDisposition disposition,
    GLuint src_offset) {
  // All calling functions check isContextLost, so a duplicate check is not
  // needed here.
  if (!pixels) {
    DCHECK_NE(disposition, kNullNotReachable);
    if (disposition == kNullAllowed)
      return true;
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "no pixels");
    return false;
  }

  if (!ValidateSettableTexFormat(function_name, format))
    return false;

  auto pixelType = pixels->GetType();

  switch (type) {
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
      NOTREACHED();
  }

  unsigned total_bytes_required, skip_bytes;
  GLenum error = WebGLImageConversion::ComputeImageSizeInBytes(
      format, type, width, height, depth,
      GetUnpackPixelStoreParams(tex_dimension), &total_bytes_required, nullptr,
      &skip_bytes);
  if (error != GL_NO_ERROR) {
    SynthesizeGLError(error, function_name, "invalid texture dimensions");
    return false;
  }
  base::CheckedNumeric<uint32_t> total = src_offset;
  total *= pixels->TypeSize();
  total += total_bytes_required;
  total += skip_bytes;
  if (!total.IsValid() ||
      pixels->byteLength() < static_cast<size_t>(total.ValueOrDie())) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "ArrayBufferView not big enough for request");
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
  if (fast_call_.InFastCall()) {
    fast_call_.AddDeferredConsoleWarning(message);
    return;
  }

  blink::ExecutionContext* context = Host()->GetTopExecutionContext();
  if (context && !context->IsContextDestroyed()) {
    context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kRendering,
        mojom::ConsoleMessageLevel::kWarning, message));
  }
}

void WebGLRenderingContextBase::NotifyWebGLErrorOrWarning(
    const String& message) {
  if (fast_call_.InFastCall()) {
    fast_call_.AddDeferredErrorOrWarningNotification(message);
    return;
  }
  probe::DidFireWebGLErrorOrWarning(canvas(), message);
}

void WebGLRenderingContextBase::NotifyWebGLError(const String& error_type) {
  if (fast_call_.InFastCall()) {
    fast_call_.AddDeferredErrorNotification(error_type);
    return;
  }
  probe::DidFireWebGLError(canvas(), error_type);
}

void WebGLRenderingContextBase::NotifyWebGLWarning() {
  if (fast_call_.InFastCall()) {
    fast_call_.AddDeferredWarningNotification();
    return;
  }
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
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name, "invalid capability");
      return false;
  }
}

bool WebGLRenderingContextBase::ValidateUniformParameters(
    const char* function_name,
    const WebGLUniformLocation* location,
    void* v,
    GLsizei size,
    GLsizei required_min_size,
    GLuint src_offset,
    GLuint src_length) {
  return ValidateUniformMatrixParameters(function_name, location, false, v,
                                         size, required_min_size, src_offset,
                                         src_length);
}

bool WebGLRenderingContextBase::ValidateUniformMatrixParameters(
    const char* function_name,
    const WebGLUniformLocation* location,
    GLboolean transpose,
    DOMFloat32Array* v,
    GLsizei required_min_size,
    GLuint src_offset,
    size_t src_length) {
  if (!v) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "no array");
    return false;
  }
  if (!base::CheckedNumeric<GLuint>(src_length).IsValid()) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      "src_length exceeds the maximum supported length");
    return false;
  }
  return ValidateUniformMatrixParameters(
      function_name, location, transpose, v->DataMaybeShared(), v->length(),
      required_min_size, src_offset, static_cast<GLuint>(src_length));
}

bool WebGLRenderingContextBase::ValidateUniformMatrixParameters(
    const char* function_name,
    const WebGLUniformLocation* location,
    GLboolean transpose,
    void* v,
    size_t size,
    GLsizei required_min_size,
    GLuint src_offset,
    GLuint src_length) {
  DCHECK(size >= 0 && required_min_size > 0);
  if (!location)
    return false;
  if (location->Program() != current_program_) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "location is not from current program");
    return false;
  }
  if (!v) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "no array");
    return false;
  }
  if (!base::CheckedNumeric<GLsizei>(size).IsValid()) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      "array exceeds the maximum supported size");
    return false;
  }
  if (transpose && !IsWebGL2()) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "transpose not FALSE");
    return false;
  }
  if (src_offset >= static_cast<GLuint>(size)) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "invalid srcOffset");
    return false;
  }
  GLsizei actual_size = static_cast<GLsizei>(size) - src_offset;
  if (src_length > 0) {
    if (src_length > static_cast<GLuint>(actual_size)) {
      SynthesizeGLError(GL_INVALID_VALUE, function_name,
                        "invalid srcOffset + srcLength");
      return false;
    }
    actual_size = src_length;
  }
  if (actual_size < required_min_size || (actual_size % required_min_size)) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name, "invalid size");
    return false;
  }
  return true;
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

  if (WouldTaintOrigin(image)) {
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

  if (WouldTaintOrigin(context_host)) {
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

  if (WouldTaintOrigin(video)) {
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

void WebGLRenderingContextBase::OnBeforeDrawCall() {
  ClearIfComposited(kClearCallerDrawOrClear);
  MarkContextChanged(kCanvasChanged);
}

void WebGLRenderingContextBase::DispatchContextLostEvent(TimerBase*) {
  // WebXR spec: When the WebGL context is lost, set the xr compatible boolean
  // to false prior to firing the webglcontextlost event.
  xr_compatible_ = false;

  WebGLContextEvent* event =
      WebGLContextEvent::Create(event_type_names::kWebglcontextlost, "");
  Host()->HostDispatchEvent(event);
  restore_allowed_ = event->defaultPrevented();
  if (restore_allowed_ && !is_hidden_) {
    if (auto_recovery_method_ == kAuto)
      restore_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
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
    if (blocked)
      return;

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

  auto* execution_context = Host()->GetTopExecutionContext();
  Platform::ContextAttributes attributes = ToPlatformContextAttributes(
      CreationAttributes(), context_type_,
      SupportOwnOffscreenSurface(execution_context));
  Platform::GraphicsInfo gl_info;
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider;
  bool using_gpu_compositing;
  const auto& url = Host()->GetExecutionContextUrl();

  if (IsMainThread()) {
    // Ask for gpu compositing mode when making the context. The context will be
    // lost if the mode changes.
    using_gpu_compositing = !Platform::Current()->IsGpuCompositingDisabled();
    context_provider =
        Platform::Current()->CreateOffscreenGraphicsContext3DProvider(
            attributes, url, &gl_info);
  } else {
    context_provider = CreateContextProviderOnWorkerThread(
        attributes, &gl_info, &using_gpu_compositing, url);
  }
  scoped_refptr<DrawingBuffer> buffer;
  if (context_provider && context_provider->BindToCurrentThread()) {
    // Construct a new drawing buffer with the new GL context.
    buffer =
        CreateDrawingBuffer(std::move(context_provider), using_gpu_compositing);
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
  MarkContextChanged(kCanvasContextChanged);
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
    LRUCanvasResourceProviderCache(wtf_size_t capacity)
    : resource_providers_(capacity) {}

CanvasResourceProvider* WebGLRenderingContextBase::
    LRUCanvasResourceProviderCache::GetCanvasResourceProvider(
        const IntSize& size) {
  wtf_size_t i;
  for (i = 0; i < resource_providers_.size(); ++i) {
    CanvasResourceProvider* resource_provider = resource_providers_[i].get();
    if (!resource_provider)
      break;
    if (resource_provider->Size() != size)
      continue;
    BubbleToFront(i);
    return resource_provider;
  }

  // TODO(fserb): why is this a BITMAP?
  std::unique_ptr<CanvasResourceProvider> temp(
      CanvasResourceProvider::CreateBitmapProvider(
          size, kLow_SkFilterQuality, CanvasColorParams(),
          CanvasResourceProvider::ShouldInitialize::kNo));  // TODO: should this
                                                            // use the canvas's

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

void WebGLRenderingContextBase::ApplyStencilTest() {
  bool have_stencil_buffer = false;

  if (framebuffer_binding_) {
    have_stencil_buffer = framebuffer_binding_->HasStencilBuffer();
  } else {
    have_stencil_buffer = !isContextLost() && CreationAttributes().stencil &&
                          GetDrawingBuffer()->HasStencilBuffer();
  }
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

IntSize WebGLRenderingContextBase::ClampedCanvasSize() const {
  int width = Host()->Size().Width();
  int height = Host()->Size().Height();
  return IntSize(Clamp(width, 1, max_viewport_dims_[0]),
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
}

void WebGLRenderingContextBase::SetFramebuffer(GLenum target,
                                               WebGLFramebuffer* buffer) {
  if (buffer)
    buffer->SetHasEverBeenBound();

  if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) {
    framebuffer_binding_ = buffer;
    ApplyStencilTest();
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
}

void WebGLRenderingContextBase::Trace(Visitor* visitor) const {
  visitor->Trace(context_group_);
  visitor->Trace(bound_array_buffer_);
  visitor->Trace(default_vertex_array_object_);
  visitor->Trace(bound_vertex_array_object_);
  visitor->Trace(current_program_);
  visitor->Trace(framebuffer_binding_);
  visitor->Trace(renderbuffer_binding_);
  visitor->Trace(texture_units_);
  visitor->Trace(extensions_);
  visitor->Trace(make_xr_compatible_resolver_);
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

void WebGLRenderingContextBase::getHTMLOrOffscreenCanvas(
    HTMLCanvasElementOrOffscreenCanvas& result) const {
  if (canvas()) {
    result.SetHTMLCanvasElement(static_cast<HTMLCanvasElement*>(Host()));
  } else {
    result.SetOffscreenCanvas(static_cast<OffscreenCanvas*>(Host()));
  }
}

void WebGLRenderingContextBase::addProgramCompletionQuery(WebGLProgram* program,
                                                          GLuint query) {
  auto old_query = program_completion_queries_.Get(program);
  if (old_query != program_completion_queries_.end()) {
    ContextGL()->DeleteQueriesEXT(1, &old_query->second);
  }
  program_completion_queries_.Put(program, query);
  if (program_completion_queries_.size() > kMaxProgramCompletionQueries) {
    auto oldest = program_completion_queries_.rbegin();
    ContextGL()->DeleteQueriesEXT(1, &oldest->second);
    program_completion_queries_.Erase(oldest);
  }
}

void WebGLRenderingContextBase::clearProgramCompletionQueries() {
  for (auto query : program_completion_queries_) {
    ContextGL()->DeleteQueriesEXT(1, &query.second);
  }
  program_completion_queries_.Clear();
}

bool WebGLRenderingContextBase::checkProgramCompletionQueryAvailable(
    WebGLProgram* program,
    bool* completed) {
  GLuint id = 0;
  auto found = program_completion_queries_.Get(program);
  if (found != program_completion_queries_.end()) {
    id = found->second;
    GLuint available;
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
