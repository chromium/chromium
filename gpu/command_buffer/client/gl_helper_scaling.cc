// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/client/gl_helper_scaling.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gpu {
using gles2::GLES2Interface;

namespace {

// Linear translation from RGB to grayscale.
const GLfloat kRGBtoGrayscaleColorWeights[4] = {0.213f, 0.715f, 0.072f, 0.0f};

// Linear translation from RGB to YUV color space.
const GLfloat kRGBtoYColorWeights[4] = {0.257f, 0.504f, 0.098f, 0.0625f};
const GLfloat kRGBtoUColorWeights[4] = {-0.148f, -0.291f, 0.439f, 0.5f};
const GLfloat kRGBtoVColorWeights[4] = {0.439f, -0.368f, -0.071f, 0.5f};

// Returns true iff a_num/a_denom == b_num/b_denom.
bool AreRatiosEqual(int32_t a_num,
                    int32_t a_denom,
                    int32_t b_num,
                    int32_t b_denom) {
  // The math (for each dimension):
  //   If: a_num/a_denom == b_num/b_denom
  //   Then: a_num*b_denom == b_num*a_denom
  //
  // ...and cast to int64_t to guarantee no overflow from the multiplications.
  return (static_cast<int64_t>(a_num) * b_denom) ==
         (static_cast<int64_t>(b_num) * a_denom);
}

}  // namespace

GLHelperScaling::GLHelperScaling(GLES2Interface* gl, GLHelper* helper)
    : gl_(gl), helper_(helper), vertex_attributes_buffer_(gl_) {
  InitBuffer();
}

GLHelperScaling::~GLHelperScaling() {}

// Used to keep track of a generated shader program. The program
// is passed in as text through Setup and is used by calling
// UseProgram() with the right parameters. Note that |gl_|
// and |helper_| are assumed to live longer than this program.
class ShaderProgram : public base::RefCounted<ShaderProgram> {
 public:
  ShaderProgram(GLES2Interface* gl,
                GLHelper* helper,
                GLHelperScaling::ShaderType shader)
      : gl_(gl),
        helper_(helper),
        shader_(shader),
        program_(gl_->CreateProgram()),
        position_location_(-1),
        texcoord_location_(-1),
        src_rect_location_(-1),
        src_pixelsize_location_(-1),
        scaling_vector_location_(-1),
        rgb_to_plane0_location_(-1),
        rgb_to_plane1_location_(-1),
        rgb_to_plane2_location_(-1) {}

  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;

  // Compile shader program.
  void Setup(const GLchar* vertex_shader_text,
             const GLchar* fragment_shader_text);

  // UseProgram must be called with GL_ARRAY_BUFFER bound to a vertex attribute
  // buffer. |src_texture_size| is the size of the entire source texture,
  // regardless of which region is to be sampled. |src_rect| is the source
  // region not including overscan pixels past the edges. The program produces a
  // scaled image placed at Rect(0, 0, dst_size.width(), dst_size.height()) in
  // the destination texture(s).
  void UseProgram(const gfx::Size& src_texture_size,
                  const gfx::RectF& src_rect,
                  const gfx::Size& dst_size,
                  bool scale_x,
                  bool flip_y,
                  const GLfloat color_weights[3][4]);

  bool Initialized() const { return position_location_ != -1; }

 private:
  friend class base::RefCounted<ShaderProgram>;
  ~ShaderProgram() { gl_->DeleteProgram(program_); }

  raw_ptr<GLES2Interface> gl_;
  raw_ptr<GLHelper> helper_;
  const GLHelperScaling::ShaderType shader_;

  // A program for copying a source texture into a destination texture.
  GLuint program_;

  // The location of the position in the program.
  GLint position_location_;
  // The location of the texture coordinate in the program.
  GLint texcoord_location_;
  // The location of the source texture in the program.
  GLint texture_location_;
  // The location of the texture coordinate of the source rectangle in the
  // program.
  GLint src_rect_location_;
  // Location of size of source image in pixels.
  GLint src_pixelsize_location_;
  // Location of vector for scaling ratio between source and dest textures.
  GLint scaling_vector_location_;
  // Location of color weights, for programs that convert from interleaved to
  // planar pixel orderings/formats.
  GLint rgb_to_plane0_location_;
  GLint rgb_to_plane1_location_;
  GLint rgb_to_plane2_location_;
};

// Implementation of a single stage in a scaler pipeline. If the pipeline has
// multiple stages, it calls Scale() on the subscaler, then further scales the
// output. Caches textures and framebuffers to avoid allocating/deleting
// them once per frame, which can be expensive on some drivers.
class ScalerImpl : public GLHelper::ScalerInterface {
 public:
  // |gl| and |scaler_helper| are expected to live longer than this object.
  ScalerImpl(GLES2Interface* gl,
             GLHelperScaling* scaler_helper,
             const GLHelperScaling::ScalerStage& scaler_stage,
             std::unique_ptr<ScalerImpl> subscaler)
      : gl_(gl),
        scaler_helper_(scaler_helper),
        spec_(scaler_stage),
        intermediate_texture_(0),
        dst_framebuffer_(gl),
        subscaler_(std::move(subscaler)) {
    shader_program_ =
        scaler_helper_->GetShaderProgram(spec_.shader, spec_.swizzle);
  }

  ~ScalerImpl() override {
    if (intermediate_texture_) {
      gl_->DeleteTextures(1, &intermediate_texture_);
    }
  }

  void SetColorWeights(int plane, const GLfloat color_weights[4]) {
    DCHECK(plane >= 0 && plane < 3);
    color_weights_[plane][0] = color_weights[0];
    color_weights_[plane][1] = color_weights[1];
    color_weights_[plane][2] = color_weights[2];
    color_weights_[plane][3] = color_weights[3];
  }

  void ScaleToMultipleOutputs(GLuint src_texture,
                              const gfx::Size& src_texture_size,
                              const gfx::Vector2dF& src_offset,
                              GLuint dest_texture_0,
                              GLuint dest_texture_1,
                              const gfx::Rect& output_rect) override {
    // TODO(crbug.com/41350322): Do not accept non-whole-numbered offsets
    // until the shader programs produce the correct output for them.
    DCHECK_EQ(src_offset.x(), std::floor(src_offset.x()));
    DCHECK_EQ(src_offset.y(), std::floor(src_offset.y()));

    if (output_rect.IsEmpty())
      return;  // No work to do.
    gfx::RectF src_rect = ToSourceRect(output_rect);

    // Ensure conflicting GL capabilities are disabled. The following explicity
    // disables those known to possibly be enabled in GL compositing code, while
    // the helper method call will DCHECK a wider set.
    gl_->Disable(GL_SCISSOR_TEST);
    gl_->Disable(GL_STENCIL_TEST);
    gl_->Disable(GL_BLEND);
    DCheckNoConflictingCapabilitiesAreEnabled();

    if (subscaler_) {
      gfx::RectF overscan_rect = src_rect;
      PadForOverscan(&overscan_rect);
      const auto intermediate = subscaler_->GenerateIntermediateTexture(
          src_texture, src_texture_size, src_offset,
          gfx::ToEnclosingRect(overscan_rect));
      src_rect -= intermediate.second.OffsetFromOrigin();
      Execute(intermediate.first, intermediate.second.size(), src_rect,
              dest_texture_0, dest_texture_1, output_rect.size());
    } else {
      if (spec_.flipped_source) {
        src_rect.set_x(src_rect.x() + src_offset.x());
        src_rect.set_y(src_texture_size.height() - src_rect.bottom() -
                       src_offset.y());
      } else {
        src_rect += src_offset;
      }
      Execute(src_texture, src_texture_size, src_rect, dest_texture_0,
              dest_texture_1, output_rect.size());
    }
  }

  void ComputeRegionOfInfluence(const gfx::Size& src_texture_size,
                                const gfx::Vector2dF& src_offset,
                                const gfx::Rect& output_rect,
                                gfx::Rect* sampling_rect,
                                gfx::Vector2dF* offset) const override {
    // This mimics the recursive behavior of GenerateIntermediateTexture(),
    // computing the size of the intermediate texture required by each scaler
    // in the chain.
    gfx::Rect intermediate_rect = output_rect;
    const ScalerImpl* scaler = this;
    while (scaler->subscaler_) {
      gfx::RectF overscan_rect = scaler->ToSourceRect(intermediate_rect);
      scaler->PadForOverscan(&overscan_rect);
      intermediate_rect = gfx::ToEnclosingRect(overscan_rect);
      scaler = scaler->subscaler_.get();
    }

    // At this point, |scaler| points to the first scaler in the chain. Compute
    // the source rect that would have been used with the shader program, and
    // then pad that to account for the shader program's overscan pixels.
    const auto rects = scaler->ComputeBaseCaseRects(
        src_texture_size, src_offset, intermediate_rect);
    gfx::RectF src_overscan_rect = rects.first;
    scaler->PadForOverscan(&src_overscan_rect);

    // Provide a whole-numbered Rect result along with the offset to the origin
    // point.
    *sampling_rect = gfx::ToEnclosingRect(src_overscan_rect);
    sampling_rect->Intersect(gfx::Rect(src_texture_size));
    *offset = gfx::ScaleVector2d(
        output_rect.OffsetFromOrigin(),
        static_cast<float>(chain_properties_->scale_from.x()) /
            chain_properties_->scale_to.x(),
        static_cast<float>(chain_properties_->scale_from.y()) /
            chain_properties_->scale_to.y());
    if (scaler->spec_.flipped_source) {
      offset->set_x(offset->x() - sampling_rect->x());
      offset->set_y(offset->y() -
                    (src_texture_size.height() - sampling_rect->bottom()));
    } else {
      *offset -= sampling_rect->OffsetFromOrigin();
    }
  }

  // Sets the overall scale ratio and swizzle for the entire chain of Scalers.
  void SetChainProperties(const gfx::Vector2d& from,
                          const gfx::Vector2d& to,
                          bool swizzle) {
    chain_properties_.emplace(ChainProperties{
        from, to, static_cast<GLenum>(swizzle ? GL_BGRA_EXT : GL_RGBA)});
  }

  // WARNING: This method should only be called by external clients, since they
  // are using it compare against the overall scale ratio (of the entire chain
  // of Scalers).
  bool IsSameScaleRatio(const gfx::Vector2d& from,
                        const gfx::Vector2d& to) const override {
    const gfx::Vector2d& overall_from = chain_properties_->scale_from;
    const gfx::Vector2d& overall_to = chain_properties_->scale_to;
    return AreRatiosEqual(overall_from.x(), overall_to.x(), from.x(), to.x()) &&
           AreRatiosEqual(overall_from.y(), overall_to.y(), from.y(), to.y());
  }

  bool IsSamplingFlippedSource() const override {
    const ScalerImpl* scaler = this;
    while (scaler->subscaler_) {
      DCHECK(!scaler->spec_.flipped_source);
      scaler = scaler->subscaler_.get();
    }
    return scaler->spec_.flipped_source;
  }

  bool IsFlippingOutput() const override {
    bool flipped_overall = false;
    const ScalerImpl* scaler = this;
    while (scaler) {
      flipped_overall = (flipped_overall != scaler->spec_.flip_output);
      scaler = scaler->subscaler_.get();
    }
    return flipped_overall;
  }

  GLenum GetReadbackFormat() const override {
    return chain_properties_->readback_format;
  }

 private:
  // In DCHECK-enabled builds, this checks that no conflicting GL capability is
  // currently enabled in the GL context. Any of these might cause problems when
  // the shader draw operations are executed.
  void DCheckNoConflictingCapabilitiesAreEnabled() const {
    DCHECK_NE(gl_->IsEnabled(GL_BLEND), GL_TRUE);
    DCHECK_NE(gl_->IsEnabled(GL_CULL_FACE), GL_TRUE);
    DCHECK_NE(gl_->IsEnabled(GL_DEPTH_TEST), GL_TRUE);
    DCHECK_NE(gl_->IsEnabled(GL_POLYGON_OFFSET_FILL), GL_TRUE);
    DCHECK_NE(gl_->IsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE), GL_TRUE);
    DCHECK_NE(gl_->IsEnabled(GL_SAMPLE_COVERAGE), GL_TRUE);
    DCHECK_NE(gl_->IsEnabled(GL_SCISSOR_TEST), GL_TRUE);
    DCHECK_NE(gl_->IsEnabled(GL_STENCIL_TEST), GL_TRUE);
  }

  // Expands the given |sampling_rect| to account for the extra pixels bordering
  // it that will be sampled by the shaders.
  void PadForOverscan(gfx::RectF* sampling_rect) const {
    // Room for optimization: These are conservative calculations. Some of the
    // shaders actually require fewer overscan pixels.
    float overscan_x = 0;
    float overscan_y = 0;
    switch (spec_.shader) {
      case GLHelperScaling::SHADER_BILINEAR:
      case GLHelperScaling::SHADER_BILINEAR2:
      case GLHelperScaling::SHADER_BILINEAR3:
      case GLHelperScaling::SHADER_BILINEAR4:
      case GLHelperScaling::SHADER_BILINEAR2X2:
      case GLHelperScaling::SHADER_PLANAR:
      case GLHelperScaling::SHADER_YUV_MRT_PASS1:
      case GLHelperScaling::SHADER_YUV_MRT_PASS2:
        overscan_x =
            static_cast<float>(spec_.scale_from.x()) / spec_.scale_to.x();
        overscan_y =
            static_cast<float>(spec_.scale_from.y()) / spec_.scale_to.y();
        break;

      case GLHelperScaling::SHADER_BICUBIC_UPSCALE:
        DCHECK_LE(spec_.scale_from.x(), spec_.scale_to.x());
        DCHECK_LE(spec_.scale_from.y(), spec_.scale_to.y());
        // This shader always reads a radius of 2 pixels about the sampling
        // point.
        overscan_x = 2.0f;
        overscan_y = 2.0f;
        break;

      case GLHelperScaling::SHADER_BICUBIC_HALF_1D: {
        DCHECK_GE(spec_.scale_from.x(), spec_.scale_to.x());
        DCHECK_GE(spec_.scale_from.y(), spec_.scale_to.y());
        // kLobeDist is the largest pixel read offset in the shader program.
        constexpr float kLobeDist = 11.0f / 4.0f;
        overscan_x = kLobeDist * spec_.scale_from.x() / spec_.scale_to.x();
        overscan_y = kLobeDist * spec_.scale_from.y() / spec_.scale_to.y();
        break;
      }
    }
    // Because the texture sampler sometimes reads between pixels, an extra one
    // must be accounted for.
    sampling_rect->Inset(
        -gfx::InsetsF::VH(overscan_y + 1.0f, overscan_x + 1.0f));
  }

  // Returns the given |rect| in source coordinates.
  gfx::RectF ToSourceRect(const gfx::Rect& rect) const {
    return gfx::ScaleRect(
        gfx::RectF(rect),
        static_cast<float>(spec_.scale_from.x()) / spec_.scale_to.x(),
        static_cast<float>(spec_.scale_from.y()) / spec_.scale_to.y());
  }

  // Returns the given |rect| in output coordinates, enlarged to whole-number
  // coordinates.
  gfx::Rect ToOutputRect(const gfx::RectF& rect) const {
    return gfx::ToEnclosingRect(gfx::ScaleRect(
        rect, static_cast<float>(spec_.scale_to.x()) / spec_.scale_from.x(),
        static_cast<float>(spec_.scale_to.y()) / spec_.scale_from.y()));
  }

  // Returns the source and output rects to use with the shader program,
  // assuming this scaler is the "base case" (i.e., it has no subscaler). The
  // returned output rect is clamped according to what the source texture can
  // provide.
  std::pair<gfx::RectF, gfx::Rect> ComputeBaseCaseRects(
      const gfx::Size& src_texture_size,
      const gfx::Vector2dF& src_offset,
      const gfx::Rect& requested_output_rect) const {
    DCHECK(!subscaler_);

    // Determine what the requested source rect is, and clamp to the texture's
    // bounds.
    gfx::RectF src_rect = ToSourceRect(requested_output_rect);
    src_rect += src_offset;
    if (spec_.flipped_source)
      src_rect.set_y(src_texture_size.height() - src_rect.bottom());
    src_rect.Intersect(gfx::RectF(gfx::SizeF(src_texture_size)));

    // From the clamped source rect, re-compute the output rect that will be
    // provided to the next scaler stage. This will either be all of what was
    // requested or a smaller rect. See comments in
    // GenerateIntermediateTexture().
    if (spec_.flipped_source)
      src_rect.set_y(src_texture_size.height() - src_rect.bottom());
    src_rect -= src_offset;
    const gfx::Rect output_rect = ToOutputRect(src_rect);

    // Once again, compute the source rect from the output rect, which might
    // spill-over the texture's bounds slightly (but only by the minimal amount
    // necessary). Apply the |src_offset| and vertically-flip this source rect,
    // if necessary, as this is what will be provided directly to the shader
    // program.
    src_rect = ToSourceRect(output_rect);
    src_rect += src_offset;
    if (spec_.flipped_source)
      src_rect.set_y(src_texture_size.height() - src_rect.bottom());

    return std::make_pair(src_rect, output_rect);
  }

  // Generates the intermediate texture and/or re-defines it if its size has
  // changed.
  void EnsureIntermediateTextureDefined(const gfx::Size& size) {
    // Reallocate a new texture, if needed.
    if (!intermediate_texture_)
      gl_->GenTextures(1, &intermediate_texture_);
    if (intermediate_texture_size_ != size) {
      gl_->BindTexture(GL_TEXTURE_2D, intermediate_texture_);
      gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      intermediate_texture_size_ = size;
    }
  }

  // Returns a texture of this intermediate scaling step. The caller does NOT
  // own the returned texture. The texture may be smaller than the
  // |requested_output_rect.size()|, if that eliminates data redundancy that
  // GL_CLAMP_TO_EDGE will correct for.
  std::pair<GLuint, gfx::Rect> GenerateIntermediateTexture(
      GLuint src_texture,
      const gfx::Size& src_texture_size,
      const gfx::Vector2dF& src_offset,
      const gfx::Rect& requested_output_rect) {
    // Base case: If there is no subscaler, render the intermediate texture from
    // the |src_texture| and return it.
    if (!subscaler_) {
      const auto rects = ComputeBaseCaseRects(src_texture_size, src_offset,
                                              requested_output_rect);
      EnsureIntermediateTextureDefined(rects.second.size());
      Execute(src_texture, src_texture_size, rects.first, intermediate_texture_,
              0, rects.second.size());
      return std::make_pair(intermediate_texture_, rects.second);
    }

    // Recursive case: Output from the subscaler is needed to generate this
    // scaler's intermediate texture. Compute the region of pixels that will be
    // sampled, and request those pixels from the subscaler.
    gfx::RectF sampling_rect = ToSourceRect(requested_output_rect);
    PadForOverscan(&sampling_rect);
    const auto intermediate = subscaler_->GenerateIntermediateTexture(
        src_texture, src_texture_size, src_offset,
        gfx::ToEnclosingRect(sampling_rect));
    const GLuint& sampling_texture = intermediate.first;
    const gfx::Rect& sampling_bounds = intermediate.second;

    // The subscaler might not have provided pixels for the entire requested
    // |sampling_rect| because they would be redundant (i.e., GL_CLAMP_TO_EDGE
    // behavior will generate the redundant pixel values in the rendering step,
    // below). Thus, re-compute |requested_output_rect| and |sampling_rect| when
    // this has occurred.
    gfx::Rect output_rect;
    if (sampling_bounds.Contains(gfx::ToEnclosingRect(sampling_rect))) {
      output_rect = requested_output_rect;
    } else {
      sampling_rect.Intersect(gfx::RectF(sampling_bounds));
      output_rect = ToOutputRect(sampling_rect);
      // The new sampling rect might exceed the bounds slightly, but only by the
      // minimal amount necessary to populate the entire output.
      sampling_rect = ToSourceRect(output_rect);
    }

    // Render the output, but do not account for |src_offset| nor vertical
    // flipping because that should have been handled in the base case.
    EnsureIntermediateTextureDefined(output_rect.size());
    DCHECK(!spec_.flipped_source);
    Execute(sampling_texture, sampling_bounds.size(),
            sampling_rect - sampling_bounds.OffsetFromOrigin(),
            intermediate_texture_, 0, output_rect.size());
    return std::make_pair(intermediate_texture_, output_rect);
  }

  // Executes the scale, mapping pixels from |src_texture| to one or two
  // outputs, transforming the source pixels in |src_rect| to produce a
  // result of the given size. |src_texture_size| is the size of the entire
  // |src_texture|, regardless of the sampled region.
  void Execute(GLuint src_texture,
               const gfx::Size& src_texture_size,
               const gfx::RectF& src_rect,
               GLuint dest_texture_0,
               GLuint dest_texture_1,
               const gfx::Size& result_size) {
    // Attach output texture(s) to the framebuffer.
    ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(
        gl_, dst_framebuffer_);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, dest_texture_0, 0);
    if (dest_texture_1 > 0) {
      gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 1,
                                GL_TEXTURE_2D, dest_texture_1, 0);
    }

    // Use GL_NEAREST for copies between exactly same size of rectangles to
    // reduce errors on low-precision GPUs. Use bilinear filtering otherwise.
    //
    // This is a workaround for Mali-G72 GPU (b/141898654) that uses lower
    // precision than expected for interpolation.
    GLint filter = (src_rect.IsExpressibleAsRect() &&
                    src_rect.size() == gfx::SizeF(result_size))
                       ? GL_NEAREST
                       : GL_LINEAR;

    // Set the active texture unit to 0 for the ScopedTextureBinder below, then
    // restore the original value when done. (crbug.com/1103385)
    GLint oldActiveTexture = 0;
    gl_->GetIntegerv(GL_ACTIVE_TEXTURE, &oldActiveTexture);
    gl_->ActiveTexture(GL_TEXTURE0);
    {
      // Bind to the source texture and set the filitering and clamp to the
      // edge, as required by all shader programs.
      ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, src_texture);
      gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
      gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
      gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

      // Prepare the shader program for drawing.
      ScopedBufferBinder<GL_ARRAY_BUFFER> buffer_binder(
          gl_, scaler_helper_->vertex_attributes_buffer_);
      shader_program_->UseProgram(src_texture_size, src_rect, result_size,
                                  spec_.scale_x, spec_.flip_output,
                                  color_weights_);

      // Execute the draw.
      gl_->Viewport(0, 0, result_size.width(), result_size.height());
      const GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT0 + 1};
      if (dest_texture_1 > 0) {
        DCHECK_LE(2, scaler_helper_->helper_->MaxDrawBuffers());
        gl_->DrawBuffersEXT(2, buffers);
      }
      gl_->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      if (dest_texture_1 > 0) {
        // Set the draw buffers back to not disrupt external operations.
        gl_->DrawBuffersEXT(1, buffers);
      }

      // ScopedTextureBinder ends here, before restoring ActiveTexture state.
    }
    gl_->ActiveTexture(oldActiveTexture);
  }

  raw_ptr<GLES2Interface> gl_;
  raw_ptr<GLHelperScaling> scaler_helper_;
  GLHelperScaling::ScalerStage spec_;
  GLfloat color_weights_[3][4];  // A vec4 for each plane.
  GLuint intermediate_texture_;
  gfx::Size intermediate_texture_size_;
  scoped_refptr<ShaderProgram> shader_program_;
  ScopedFramebuffer dst_framebuffer_;
  std::unique_ptr<ScalerImpl> subscaler_;

  // This last member is only set on ScalerImpls that are exposed to external
  // modules. This is so the client can query the overall scale ratio and
  // swizzle provided by a chain of ScalerImpls.
  struct ChainProperties {
    gfx::Vector2d scale_from;
    gfx::Vector2d scale_to;
    GLenum readback_format;
  };
  std::optional<ChainProperties> chain_properties_;
};

// The important inputs for this function is |x_ops| and |y_ops|. They represent
// scaling operations to be done on a source image of relative size
// |scale_from|. If |quality| is SCALER_QUALITY_BEST, then interpret these scale
// operations literally and create one scaler stage for each ScaleOp. However,
// if |quality| is SCALER_QUALITY_GOOD, then enable some optimizations that
// combine two or more ScaleOps in to a single scaler stage. Normally first
// ScaleOps from |y_ops| are processed first and |x_ops| after all the |y_ops|,
// but sometimes it's possible to  combine one or more operation from both
// queues essentially for free. This is the reason why |x_ops| and |y_ops|
// aren't just one single queue.
// static
void GLHelperScaling::ConvertScalerOpsToScalerStages(
    GLHelper::ScalerQuality quality,
    gfx::Vector2d scale_from,
    base::circular_deque<GLHelperScaling::ScaleOp>* x_ops,
    base::circular_deque<GLHelperScaling::ScaleOp>* y_ops,
    std::vector<ScalerStage>* scaler_stages) {
  while (!x_ops->empty() || !y_ops->empty()) {
    gfx::Vector2d intermediate_scale = scale_from;
    base::circular_deque<ScaleOp>* current_queue = nullptr;

    if (!y_ops->empty()) {
      current_queue = y_ops;
    } else {
      current_queue = x_ops;
    }

    ShaderType current_shader = SHADER_BILINEAR;
    switch (current_queue->front().scale_factor) {
      case 0:
        if (quality == GLHelper::SCALER_QUALITY_BEST) {
          current_shader = SHADER_BICUBIC_UPSCALE;
        }
        break;
      case 2:
        if (quality == GLHelper::SCALER_QUALITY_BEST) {
          current_shader = SHADER_BICUBIC_HALF_1D;
        }
        break;
      case 3:
        DCHECK(quality != GLHelper::SCALER_QUALITY_BEST);
        current_shader = SHADER_BILINEAR3;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    bool scale_x = current_queue->front().scale_x;
    current_queue->front().UpdateScale(&intermediate_scale);
    current_queue->pop_front();

    // Optimization: Sometimes we can combine 2-4 scaling operations into
    // one operation.
    if (quality == GLHelper::SCALER_QUALITY_GOOD) {
      if (!current_queue->empty() && current_shader == SHADER_BILINEAR) {
        // Combine two steps in the same dimension.
        current_queue->front().UpdateScale(&intermediate_scale);
        current_queue->pop_front();
        current_shader = SHADER_BILINEAR2;
        if (!current_queue->empty()) {
          // Combine three steps in the same dimension.
          current_queue->front().UpdateScale(&intermediate_scale);
          current_queue->pop_front();
          current_shader = SHADER_BILINEAR4;
        }
      }
      // Check if we can combine some steps in the other dimension as well.
      // Since all shaders currently use GL_LINEAR, we can easily scale up
      // or scale down by exactly 2x at the same time as we do another
      // operation. Currently, the following mergers are supported:
      // * 1 bilinear Y-pass with 1 bilinear X-pass (up or down)
      // * 2 bilinear Y-passes with 2 bilinear X-passes
      // * 1 bilinear Y-pass with N bilinear X-pass
      // * N bilinear Y-passes with 1 bilinear X-pass (down only)
      // Measurements indicate that generalizing this for 3x3 and 4x4
      // makes it slower on some platforms, such as the Pixel.
      if (!scale_x && x_ops->size() > 0 && x_ops->front().scale_factor <= 2) {
        int x_passes = 0;
        if (current_shader == SHADER_BILINEAR2 && x_ops->size() >= 2) {
          // 2y + 2x passes
          x_passes = 2;
          current_shader = SHADER_BILINEAR2X2;
        } else if (current_shader == SHADER_BILINEAR) {
          // 1y + Nx passes
          scale_x = true;
          switch (x_ops->size()) {
            case 0:
              NOTREACHED_IN_MIGRATION();
              break;
            case 1:
              if (x_ops->front().scale_factor == 3) {
                current_shader = SHADER_BILINEAR3;
              }
              x_passes = 1;
              break;
            case 2:
              x_passes = 2;
              current_shader = SHADER_BILINEAR2;
              break;
            default:
              x_passes = 3;
              current_shader = SHADER_BILINEAR4;
              break;
          }
        } else if (x_ops->front().scale_factor == 2) {
          // Ny + 1x-downscale
          x_passes = 1;
        }

        for (int i = 0; i < x_passes; i++) {
          x_ops->front().UpdateScale(&intermediate_scale);
          x_ops->pop_front();
        }
      }
    }

    scaler_stages->emplace_back(ScalerStage{current_shader, scale_from,
                                            intermediate_scale, scale_x, false,
                                            false, false});
    scale_from = intermediate_scale;
  }
}

// static
void GLHelperScaling::ComputeScalerStages(
    GLHelper::ScalerQuality quality,
    const gfx::Vector2d& scale_from,
    const gfx::Vector2d& scale_to,
    bool flipped_source,
    bool flip_output,
    bool swizzle,
    std::vector<ScalerStage>* scaler_stages) {
  if (quality == GLHelper::SCALER_QUALITY_FAST || scale_from == scale_to) {
    scaler_stages->emplace_back(ScalerStage{SHADER_BILINEAR, scale_from,
                                            scale_to, false, flipped_source,
                                            flip_output, swizzle});
    return;
  }

  base::circular_deque<GLHelperScaling::ScaleOp> x_ops, y_ops;
  GLHelperScaling::ScaleOp::AddOps(scale_from.x(), scale_to.x(), true,
                                   quality == GLHelper::SCALER_QUALITY_GOOD,
                                   &x_ops);
  GLHelperScaling::ScaleOp::AddOps(scale_from.y(), scale_to.y(), false,
                                   quality == GLHelper::SCALER_QUALITY_GOOD,
                                   &y_ops);
  DCHECK_GT(x_ops.size() + y_ops.size(), 0u);
  ConvertScalerOpsToScalerStages(quality, scale_from, &x_ops, &y_ops,
                                 scaler_stages);
  DCHECK_EQ(x_ops.size() + y_ops.size(), 0u);
  DCHECK(!scaler_stages->empty());

  // If the source content is flipped, the first scaler stage will perform math
  // to account for this. It also will flip the content during scaling so that
  // all following stages may assume the content is not flipped. Then, the final
  // stage must ensure the final output is correctly flipped-back (or not) based
  // on what the first stage did PLUS what is being requested by the client
  // code.
  if (flipped_source) {
    scaler_stages->front().flipped_source = true;
    scaler_stages->front().flip_output = true;
  }
  if (flipped_source != flip_output) {
    scaler_stages->back().flip_output = !scaler_stages->back().flip_output;
  }

  scaler_stages->back().swizzle = swizzle;
}

std::unique_ptr<GLHelper::ScalerInterface> GLHelperScaling::CreateScaler(
    GLHelper::ScalerQuality quality,
    const gfx::Vector2d& scale_from,
    const gfx::Vector2d& scale_to,
    bool flipped_source,
    bool flip_output,
    bool swizzle) {
  if (scale_from.x() == 0 || scale_from.y() == 0 || scale_to.x() == 0 ||
      scale_to.y() == 0) {
    // Invalid arguments: Cannot scale from or to a relative size of 0.
    return nullptr;
  }

  std::vector<ScalerStage> scaler_stages;
  ComputeScalerStages(quality, scale_from, scale_to, flipped_source,
                      flip_output, swizzle, &scaler_stages);

  std::unique_ptr<ScalerImpl> ret;
  for (unsigned int i = 0; i < scaler_stages.size(); i++) {
    ret = std::make_unique<ScalerImpl>(gl_, this, scaler_stages[i],
                                       std::move(ret));
  }
  ret->SetChainProperties(scale_from, scale_to, swizzle);
  return std::move(ret);
}

std::unique_ptr<GLHelper::ScalerInterface>
GLHelperScaling::CreateGrayscalePlanerizer(bool flipped_source,
                                           bool flip_output,
                                           bool swizzle) {
  const ScalerStage stage = {
      SHADER_PLANAR, gfx::Vector2d(4, 1), gfx::Vector2d(1, 1),
      true,          flipped_source,      flip_output,
      swizzle};
  auto result = std::make_unique<ScalerImpl>(gl_, this, stage, nullptr);
  result->SetColorWeights(0, kRGBtoGrayscaleColorWeights);
  result->SetChainProperties(stage.scale_from, stage.scale_to, swizzle);
  return std::move(result);
}

std::unique_ptr<GLHelper::ScalerInterface>
GLHelperScaling::CreateI420Planerizer(int plane,
                                      bool flipped_source,
                                      bool flip_output,
                                      bool swizzle) {
  const ScalerStage stage = {
      SHADER_PLANAR,
      plane == 0 ? gfx::Vector2d(4, 1) : gfx::Vector2d(8, 2),
      gfx::Vector2d(1, 1),
      true,
      flipped_source,
      flip_output,
      swizzle};
  auto result = std::make_unique<ScalerImpl>(gl_, this, stage, nullptr);
  switch (plane) {
    case 0:
      result->SetColorWeights(0, kRGBtoYColorWeights);
      break;
    case 1:
      result->SetColorWeights(0, kRGBtoUColorWeights);
      break;
    case 2:
      result->SetColorWeights(0, kRGBtoVColorWeights);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  result->SetChainProperties(stage.scale_from, stage.scale_to, swizzle);
  return std::move(result);
}

std::unique_ptr<GLHelper::ScalerInterface>
GLHelperScaling::CreateI420MrtPass1Planerizer(bool flipped_source,
                                              bool flip_output,
                                              bool swizzle) {
  const ScalerStage stage = {SHADER_YUV_MRT_PASS1,
                             gfx::Vector2d(4, 1),
                             gfx::Vector2d(1, 1),
                             true,
                             flipped_source,
                             flip_output,
                             swizzle};
  auto result = std::make_unique<ScalerImpl>(gl_, this, stage, nullptr);
  result->SetColorWeights(0, kRGBtoYColorWeights);
  result->SetColorWeights(1, kRGBtoUColorWeights);
  result->SetColorWeights(2, kRGBtoVColorWeights);
  result->SetChainProperties(stage.scale_from, stage.scale_to, swizzle);
  return std::move(result);
}

std::unique_ptr<GLHelper::ScalerInterface>
GLHelperScaling::CreateI420MrtPass2Planerizer(bool swizzle) {
  const ScalerStage stage = {SHADER_YUV_MRT_PASS2,
                             gfx::Vector2d(2, 2),
                             gfx::Vector2d(1, 1),
                             true,
                             false,
                             false,
                             swizzle};
  auto result = std::make_unique<ScalerImpl>(gl_, this, stage, nullptr);
  result->SetChainProperties(stage.scale_from, stage.scale_to, swizzle);
  return std::move(result);
}

// Triangle strip coordinates, used to sweep the entire source area when
// executing the shader programs. The first two columns correspond to
// values interpolated to produce |a_position| values in the shader programs,
// while the latter two columns relate to the |a_texcoord| values; respectively,
// the first pair are the vertex coordinates in object space, and the second
// pair are the corresponding source texture coordinates.
const GLfloat GLHelperScaling::kVertexAttributes[] = {
    -1.0f, -1.0f, 0.0f, 0.0f,  // vertex 0
    1.0f,  -1.0f, 1.0f, 0.0f,  // vertex 1
    -1.0f, 1.0f,  0.0f, 1.0f,  // vertex 2
    1.0f,  1.0f,  1.0f, 1.0f,
};  // vertex 3

void GLHelperScaling::InitBuffer() {
  ScopedBufferBinder<GL_ARRAY_BUFFER> buffer_binder(gl_,
                                                    vertex_attributes_buffer_);
  gl_->BufferData(GL_ARRAY_BUFFER, sizeof(kVertexAttributes), kVertexAttributes,
                  GL_STATIC_DRAW);
}

scoped_refptr<ShaderProgram> GLHelperScaling::GetShaderProgram(ShaderType type,
                                                               bool swizzle) {
  ShaderProgramKeyType key(type, swizzle);
  scoped_refptr<ShaderProgram>& cache_entry(shader_programs_[key]);
  if (!cache_entry) {
    cache_entry = new ShaderProgram(gl_, helper_, type);
    std::basic_string<GLchar> vertex_program;
    std::basic_string<GLchar> fragment_program;
    std::basic_string<GLchar> vertex_header;
    std::basic_string<GLchar> fragment_directives;
    std::basic_string<GLchar> fragment_header;
    std::basic_string<GLchar> shared_variables;

    vertex_header.append(
        "precision highp float;\n"
        "attribute vec2 a_position;\n"
        "attribute vec2 a_texcoord;\n"
        "uniform vec4 src_rect;\n");

    fragment_header.append(
        "precision mediump float;\n"
        "uniform sampler2D s_texture;\n");

    vertex_program.append(
        "  gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "  vec2 texcoord = src_rect.xy + a_texcoord * src_rect.zw;\n");

    switch (type) {
      case SHADER_BILINEAR:
        shared_variables.append("varying vec2 v_texcoord;\n");
        vertex_program.append("  v_texcoord = texcoord;\n");
        fragment_program.append(
            "  gl_FragColor = texture2D(s_texture, v_texcoord);\n");
        break;

      case SHADER_BILINEAR2:
        // This is equivialent to two passes of the BILINEAR shader above.
        // It can be used to scale an image down 1.0x-2.0x in either dimension,
        // or exactly 4x.
        shared_variables.append(
            "varying vec4 v_texcoords;\n");  // 2 texcoords packed in one quad
        vertex_header.append("uniform vec2 scaling_vector;\n");
        vertex_program.append(
            "  vec2 step = scaling_vector / 4.0;\n"
            "  v_texcoords.xy = texcoord + step;\n"
            "  v_texcoords.zw = texcoord - step;\n");

        fragment_program.append(
            "  gl_FragColor = (texture2D(s_texture, v_texcoords.xy) +\n"
            "                  texture2D(s_texture, v_texcoords.zw)) / 2.0;\n");
        break;

      case SHADER_BILINEAR3:
        // This is kind of like doing 1.5 passes of the BILINEAR shader.
        // It can be used to scale an image down 1.5x-3.0x, or exactly 6x.
        shared_variables.append(
            "varying vec4 v_texcoords1;\n"  // 2 texcoords packed in one quad
            "varying vec2 v_texcoords2;\n");
        vertex_header.append("uniform vec2 scaling_vector;\n");
        vertex_program.append(
            "  vec2 step = scaling_vector / 3.0;\n"
            "  v_texcoords1.xy = texcoord + step;\n"
            "  v_texcoords1.zw = texcoord;\n"
            "  v_texcoords2 = texcoord - step;\n");
        fragment_program.append(
            "  gl_FragColor = (texture2D(s_texture, v_texcoords1.xy) +\n"
            "                  texture2D(s_texture, v_texcoords1.zw) +\n"
            "                  texture2D(s_texture, v_texcoords2)) / 3.0;\n");
        break;

      case SHADER_BILINEAR4:
        // This is equivialent to three passes of the BILINEAR shader above,
        // It can be used to scale an image down 2.0x-4.0x or exactly 8x.
        shared_variables.append("varying vec4 v_texcoords[2];\n");
        vertex_header.append("uniform vec2 scaling_vector;\n");
        vertex_program.append(
            "  vec2 step = scaling_vector / 8.0;\n"
            "  v_texcoords[0].xy = texcoord - step * 3.0;\n"
            "  v_texcoords[0].zw = texcoord - step;\n"
            "  v_texcoords[1].xy = texcoord + step;\n"
            "  v_texcoords[1].zw = texcoord + step * 3.0;\n");
        fragment_program.append(
            "  gl_FragColor = (\n"
            "      texture2D(s_texture, v_texcoords[0].xy) +\n"
            "      texture2D(s_texture, v_texcoords[0].zw) +\n"
            "      texture2D(s_texture, v_texcoords[1].xy) +\n"
            "      texture2D(s_texture, v_texcoords[1].zw)) / 4.0;\n");
        break;

      case SHADER_BILINEAR2X2:
        // This is equivialent to four passes of the BILINEAR shader above.
        // Two in each dimension. It can be used to scale an image down
        // 1.0x-2.0x in both X and Y directions. Or, it could be used to
        // scale an image down by exactly 4x in both dimensions.
        shared_variables.append("varying vec4 v_texcoords[2];\n");
        vertex_header.append("uniform vec2 scaling_vector;\n");
        vertex_program.append(
            "  vec2 step = scaling_vector / 4.0;\n"
            "  v_texcoords[0].xy = texcoord + vec2(step.x, step.y);\n"
            "  v_texcoords[0].zw = texcoord + vec2(step.x, -step.y);\n"
            "  v_texcoords[1].xy = texcoord + vec2(-step.x, step.y);\n"
            "  v_texcoords[1].zw = texcoord + vec2(-step.x, -step.y);\n");
        fragment_program.append(
            "  gl_FragColor = (\n"
            "      texture2D(s_texture, v_texcoords[0].xy) +\n"
            "      texture2D(s_texture, v_texcoords[0].zw) +\n"
            "      texture2D(s_texture, v_texcoords[1].xy) +\n"
            "      texture2D(s_texture, v_texcoords[1].zw)) / 4.0;\n");
        break;

      case SHADER_BICUBIC_HALF_1D:
        // This scales down texture by exactly half in one dimension.
        // directions in one pass. We use bilinear lookup to reduce
        // the number of texture reads from 8 to 4
        shared_variables.append(
            "const float CenterDist = 99.0 / 140.0;\n"
            "const float LobeDist = 11.0 / 4.0;\n"
            "const float CenterWeight = 35.0 / 64.0;\n"
            "const float LobeWeight = -3.0 / 64.0;\n"
            "varying vec4 v_texcoords[2];\n");
        vertex_header.append("uniform vec2 scaling_vector;\n");
        vertex_program.append(
            "  vec2 step = scaling_vector / 2.0;\n"
            "  v_texcoords[0].xy = texcoord - LobeDist * step;\n"
            "  v_texcoords[0].zw = texcoord - CenterDist * step;\n"
            "  v_texcoords[1].xy = texcoord + CenterDist * step;\n"
            "  v_texcoords[1].zw = texcoord + LobeDist * step;\n");
        fragment_program.append(
            "  gl_FragColor = \n"
            // Lobe pixels
            "      (texture2D(s_texture, v_texcoords[0].xy) +\n"
            "       texture2D(s_texture, v_texcoords[1].zw)) *\n"
            "          LobeWeight +\n"
            // Center pixels
            "      (texture2D(s_texture, v_texcoords[0].zw) +\n"
            "       texture2D(s_texture, v_texcoords[1].xy)) *\n"
            "          CenterWeight;\n");
        break;

      case SHADER_BICUBIC_UPSCALE:
        // When scaling up, we need 4 texture reads, but we can
        // save some instructions because will know in which range of
        // the bicubic function each call call to the bicubic function
        // will be in.
        // Also, when sampling the bicubic function like this, the sum
        // is always exactly one, so we can skip normalization as well.
        shared_variables.append("varying vec2 v_texcoord;\n");
        vertex_program.append("  v_texcoord = texcoord;\n");
        fragment_header.append(
            "uniform vec2 src_pixelsize;\n"
            "uniform vec2 scaling_vector;\n"
            "const float a = -0.5;\n"
            // This function is equivialent to calling the bicubic
            // function with x-1, x, 1-x and 2-x
            // (assuming 0 <= x < 1)
            "vec4 filt4(float x) {\n"
            "  return vec4(x * x * x, x * x, x, 1) *\n"
            "         mat4(       a,      -2.0 * a,   a, 0.0,\n"
            "               a + 2.0,      -a - 3.0, 0.0, 1.0,\n"
            "              -a - 2.0, 3.0 + 2.0 * a,  -a, 0.0,\n"
            "                    -a,             a, 0.0, 0.0);\n"
            "}\n"
            "mat4 pixels_x(vec2 pos, vec2 step) {\n"
            "  return mat4(\n"
            "      texture2D(s_texture, pos - step),\n"
            "      texture2D(s_texture, pos),\n"
            "      texture2D(s_texture, pos + step),\n"
            "      texture2D(s_texture, pos + step * 2.0));\n"
            "}\n");
        fragment_program.append(
            "  vec2 pixel_pos = v_texcoord * src_pixelsize - \n"
            "      scaling_vector / 2.0;\n"
            "  float frac = fract(dot(pixel_pos, scaling_vector));\n"
            "  vec2 base = (floor(pixel_pos) + vec2(0.5)) / src_pixelsize;\n"
            "  vec2 step = scaling_vector / src_pixelsize;\n"
            "  gl_FragColor = pixels_x(base, step) * filt4(frac);\n");
        break;

      case SHADER_PLANAR:
        // Converts four RGBA pixels into one pixel. Each RGBA
        // pixel will be dot-multiplied with the color weights and
        // then placed into a component of the output. This is used to
        // convert RGBA textures into Y, U and V textures. We do this
        // because single-component textures are not renderable on all
        // architectures.
        shared_variables.append("varying vec4 v_texcoords[2];\n");
        vertex_header.append("uniform vec2 scaling_vector;\n");
        vertex_program.append(
            "  vec2 step = scaling_vector / 4.0;\n"
            "  v_texcoords[0].xy = texcoord - step * 1.5;\n"
            "  v_texcoords[0].zw = texcoord - step * 0.5;\n"
            "  v_texcoords[1].xy = texcoord + step * 0.5;\n"
            "  v_texcoords[1].zw = texcoord + step * 1.5;\n");
        fragment_header.append("uniform vec4 rgb_to_plane0;\n");
        fragment_program.append(
            "  gl_FragColor = rgb_to_plane0 * mat4(\n"
            "    vec4(texture2D(s_texture, v_texcoords[0].xy).rgb, 1.0),\n"
            "    vec4(texture2D(s_texture, v_texcoords[0].zw).rgb, 1.0),\n"
            "    vec4(texture2D(s_texture, v_texcoords[1].xy).rgb, 1.0),\n"
            "    vec4(texture2D(s_texture, v_texcoords[1].zw).rgb, 1.0));\n");
        break;

      case SHADER_YUV_MRT_PASS1:
        // RGB24 to YV12 in two passes; writing two 8888 targets each pass.
        //
        // YV12 is full-resolution luma and half-resolution blue/red chroma.
        //
        //                  (original)
        //    RGBX RGBX RGBX RGBX RGBX RGBX RGBX RGBX
        //    RGBX RGBX RGBX RGBX RGBX RGBX RGBX RGBX
        //    RGBX RGBX RGBX RGBX RGBX RGBX RGBX RGBX
        //    RGBX RGBX RGBX RGBX RGBX RGBX RGBX RGBX
        //    RGBX RGBX RGBX RGBX RGBX RGBX RGBX RGBX
        //    RGBX RGBX RGBX RGBX RGBX RGBX RGBX RGBX
        //      |
        //      |      (y plane)    (temporary)
        //      |      YYYY YYYY     UUVV UUVV
        //      +--> { YYYY YYYY  +  UUVV UUVV }
        //             YYYY YYYY     UUVV UUVV
        //   First     YYYY YYYY     UUVV UUVV
        //    pass     YYYY YYYY     UUVV UUVV
        //             YYYY YYYY     UUVV UUVV
        //                              |
        //                              |  (u plane) (v plane)
        //   Second                     |      UUUU   VVVV
        //     pass                     +--> { UUUU + VVVV }
        //                                     UUUU   VVVV
        //
        shared_variables.append("varying vec4 v_texcoords[2];\n");
        vertex_header.append("uniform vec2 scaling_vector;\n");
        vertex_program.append(
            "  vec2 step = scaling_vector / 4.0;\n"
            "  v_texcoords[0].xy = texcoord - step * 1.5;\n"
            "  v_texcoords[0].zw = texcoord - step * 0.5;\n"
            "  v_texcoords[1].xy = texcoord + step * 0.5;\n"
            "  v_texcoords[1].zw = texcoord + step * 1.5;\n");
        fragment_directives.append("#extension GL_EXT_draw_buffers : enable\n");
        fragment_header.append(
            "uniform vec4 rgb_to_plane0;\n"    // RGB-to-Y
            "uniform vec4 rgb_to_plane1;\n"    // RGB-to-U
            "uniform vec4 rgb_to_plane2;\n");  // RGB-to-V
        fragment_program.append(
            "  vec4 pixel1 = vec4(texture2D(s_texture, v_texcoords[0].xy).rgb, "
            "                     1.0);\n"
            "  vec4 pixel2 = vec4(texture2D(s_texture, v_texcoords[0].zw).rgb, "
            "                     1.0);\n"
            "  vec4 pixel3 = vec4(texture2D(s_texture, v_texcoords[1].xy).rgb, "
            "                     1.0);\n"
            "  vec4 pixel4 = vec4(texture2D(s_texture, v_texcoords[1].zw).rgb, "
            "                     1.0);\n"
            "  vec4 pixel12 = (pixel1 + pixel2) / 2.0;\n"
            "  vec4 pixel34 = (pixel3 + pixel4) / 2.0;\n"
            "  gl_FragData[0] = vec4(dot(pixel1, rgb_to_plane0),\n"
            "                        dot(pixel2, rgb_to_plane0),\n"
            "                        dot(pixel3, rgb_to_plane0),\n"
            "                        dot(pixel4, rgb_to_plane0));\n"
            "  gl_FragData[1] = vec4(dot(pixel12, rgb_to_plane1),\n"
            "                        dot(pixel34, rgb_to_plane1),\n"
            "                        dot(pixel12, rgb_to_plane2),\n"
            "                        dot(pixel34, rgb_to_plane2));\n");
        break;

      case SHADER_YUV_MRT_PASS2:
        // We're just sampling two pixels and unswizzling them.  There's
        // no need to do vertical scaling with math, since bilinear
        // interpolation in the sampler takes care of that.
        shared_variables.append("varying vec4 v_texcoords;\n");
        vertex_header.append("uniform vec2 scaling_vector;\n");
        vertex_program.append(
            "  vec2 step = scaling_vector / 2.0;\n"
            "  v_texcoords.xy = texcoord - step * 0.5;\n"
            "  v_texcoords.zw = texcoord + step * 0.5;\n");
        fragment_directives.append("#extension GL_EXT_draw_buffers : enable\n");
        fragment_program.append(
            "  vec4 lo_uuvv = texture2D(s_texture, v_texcoords.xy);\n"
            "  vec4 hi_uuvv = texture2D(s_texture, v_texcoords.zw);\n"
            "  gl_FragData[0] = vec4(lo_uuvv.rg, hi_uuvv.rg);\n"
            "  gl_FragData[1] = vec4(lo_uuvv.ba, hi_uuvv.ba);\n");
        break;
    }
    if (swizzle) {
      switch (type) {
        case SHADER_YUV_MRT_PASS1:
          fragment_program.append("  gl_FragData[0] = gl_FragData[0].bgra;\n");
          break;
        case SHADER_YUV_MRT_PASS2:
          fragment_program.append("  gl_FragData[0] = gl_FragData[0].bgra;\n");
          fragment_program.append("  gl_FragData[1] = gl_FragData[1].bgra;\n");
          break;
        default:
          fragment_program.append("  gl_FragColor = gl_FragColor.bgra;\n");
          break;
      }
    }

    vertex_program = vertex_header + shared_variables + "void main() {\n" +
                     vertex_program + "}\n";

    fragment_program = fragment_directives + fragment_header +
                       shared_variables + "void main() {\n" + fragment_program +
                       "}\n";

    cache_entry->Setup(vertex_program.c_str(), fragment_program.c_str());
  }
  return cache_entry;
}

namespace {
GLuint CompileShaderFromSource(GLES2Interface* gl,
                               const GLchar* source,
                               GLenum type) {
  GLuint shader = gl->CreateShader(type);
  GLint length = base::checked_cast<GLint>(strlen(source));
  gl->ShaderSource(shader, 1, &source, &length);
  gl->CompileShader(shader);
  GLint compile_status = 0;
  gl->GetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
  if (!compile_status) {
    GLint log_length = 0;
    gl->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length) {
      auto log = base::HeapArray<GLchar>::Uninit(log_length);
      GLsizei returned_log_length = 0;
      gl->GetShaderInfoLog(shader, log_length, &returned_log_length,
                           log.data());
      LOG(ERROR) << std::string(log.begin(), log.begin() + returned_log_length);
    }
    gl->DeleteShader(shader);
    return 0;
  }
  return shader;
}
}  // namespace

void ShaderProgram::Setup(const GLchar* vertex_shader_text,
                          const GLchar* fragment_shader_text) {
  // Shaders to map the source texture to |dst_texture_|.
  const GLuint vertex_shader =
      CompileShaderFromSource(gl_, vertex_shader_text, GL_VERTEX_SHADER);
  if (vertex_shader == 0)
    return;

  gl_->AttachShader(program_, vertex_shader);
  gl_->DeleteShader(vertex_shader);

  const GLuint fragment_shader =
      CompileShaderFromSource(gl_, fragment_shader_text, GL_FRAGMENT_SHADER);
  if (fragment_shader == 0)
    return;
  gl_->AttachShader(program_, fragment_shader);
  gl_->DeleteShader(fragment_shader);

  gl_->LinkProgram(program_);

  GLint link_status = 0;
  gl_->GetProgramiv(program_, GL_LINK_STATUS, &link_status);
  if (!link_status)
    return;

  position_location_ = gl_->GetAttribLocation(program_, "a_position");
  texcoord_location_ = gl_->GetAttribLocation(program_, "a_texcoord");
  texture_location_ = gl_->GetUniformLocation(program_, "s_texture");
  src_rect_location_ = gl_->GetUniformLocation(program_, "src_rect");
  src_pixelsize_location_ = gl_->GetUniformLocation(program_, "src_pixelsize");
  scaling_vector_location_ =
      gl_->GetUniformLocation(program_, "scaling_vector");
  rgb_to_plane0_location_ = gl_->GetUniformLocation(program_, "rgb_to_plane0");
  rgb_to_plane1_location_ = gl_->GetUniformLocation(program_, "rgb_to_plane1");
  rgb_to_plane2_location_ = gl_->GetUniformLocation(program_, "rgb_to_plane2");
  // The only reason fetching these attribute locations should fail is
  // if the context was spontaneously lost (i.e., because the GPU
  // process crashed, perhaps deliberately for testing).
  DCHECK(Initialized() || gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR);
}

void ShaderProgram::UseProgram(const gfx::Size& src_texture_size,
                               const gfx::RectF& src_rect,
                               const gfx::Size& dst_size,
                               bool scale_x,
                               bool flip_y,
                               const GLfloat color_weights[3][4]) {
  gl_->UseProgram(program_);

  // OpenGL defines the last parameter to VertexAttribPointer as type
  // "const GLvoid*" even though it is actually an offset into the buffer
  // object's data store and not a pointer to the client's address space.
  const void* offsets[2] = {nullptr,
                            reinterpret_cast<const void*>(2 * sizeof(GLfloat))};

  gl_->VertexAttribPointer(position_location_, 2, GL_FLOAT, GL_FALSE,
                           4 * sizeof(GLfloat), offsets[0]);
  gl_->EnableVertexAttribArray(position_location_);

  gl_->VertexAttribPointer(texcoord_location_, 2, GL_FLOAT, GL_FALSE,
                           4 * sizeof(GLfloat), offsets[1]);
  gl_->EnableVertexAttribArray(texcoord_location_);

  gl_->Uniform1i(texture_location_, 0);

  // Convert |src_rect| from pixel coordinates to texture coordinates. The
  // source texture coordinates are in the range [0.0,1.0] for each dimension,
  // but the sampling rect may slightly "spill" outside that range (e.g., for
  // scaler overscan).
  GLfloat src_rect_texcoord[4] = {
      src_rect.x() / src_texture_size.width(),
      src_rect.y() / src_texture_size.height(),
      src_rect.width() / src_texture_size.width(),
      src_rect.height() / src_texture_size.height(),
  };
  if (flip_y) {
    src_rect_texcoord[1] += src_rect_texcoord[3];
    src_rect_texcoord[3] *= -1.0f;
  }
  gl_->Uniform4fv(src_rect_location_, 1, src_rect_texcoord);

  // Set shader-specific uniform inputs. The |scaling_vector| is the ratio of
  // the number of source pixels sampled per dest pixels output. It is used by
  // the shader programs to locate distinct texels from the source texture, and
  // sample them at the appropriate offset to produce each output texel. In many
  // cases, |scaling_vector| also selects whether scaling will happen only in
  // the X or the Y dimension.
  switch (shader_) {
    case GLHelperScaling::SHADER_BILINEAR:
      break;

    case GLHelperScaling::SHADER_BILINEAR2:
    case GLHelperScaling::SHADER_BILINEAR3:
    case GLHelperScaling::SHADER_BILINEAR4:
    case GLHelperScaling::SHADER_BICUBIC_HALF_1D:
    case GLHelperScaling::SHADER_PLANAR:
    case GLHelperScaling::SHADER_YUV_MRT_PASS1:
    case GLHelperScaling::SHADER_YUV_MRT_PASS2:
      if (scale_x) {
        gl_->Uniform2f(scaling_vector_location_,
                       src_rect_texcoord[2] / dst_size.width(), 0.0);
      } else {
        gl_->Uniform2f(scaling_vector_location_, 0.0,
                       src_rect_texcoord[3] / dst_size.height());
      }
      break;

    case GLHelperScaling::SHADER_BILINEAR2X2:
      gl_->Uniform2f(scaling_vector_location_,
                     src_rect_texcoord[2] / dst_size.width(),
                     src_rect_texcoord[3] / dst_size.height());
      break;

    case GLHelperScaling::SHADER_BICUBIC_UPSCALE:
      gl_->Uniform2f(src_pixelsize_location_, src_texture_size.width(),
                     src_texture_size.height());
      // For this shader program, the |scaling_vector| has an alternate meaning:
      // It is only being used to select whether sampling is stepped in the X or
      // the Y direction.
      gl_->Uniform2f(scaling_vector_location_, scale_x ? 1.0 : 0.0,
                     scale_x ? 0.0 : 1.0);
      break;
  }

  if (rgb_to_plane0_location_ != -1) {
    gl_->Uniform4fv(rgb_to_plane0_location_, 1, &color_weights[0][0]);
    if (rgb_to_plane1_location_ != -1) {
      DCHECK_NE(rgb_to_plane2_location_, -1);
      gl_->Uniform4fv(rgb_to_plane1_location_, 1, &color_weights[1][0]);
      gl_->Uniform4fv(rgb_to_plane2_location_, 1, &color_weights[2][0]);
    }
  }
}

}  // namespace gpu
