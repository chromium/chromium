// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GL_HELPER_SCALING_H_
#define GPU_COMMAND_BUFFER_CLIENT_GL_HELPER_SCALING_H_

#include <map>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/client/gl_helper.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/geometry/vector2d.h"

namespace gpu {

class GLHelperTest;
class ScalerImpl;
class ShaderProgram;

// Implements GPU texture scaling methods.
// Note that you should probably not use this class directly.
// See gl_helper.cc::CreateScaler instead.
class GPU_EXPORT GLHelperScaling {
 public:
  enum ShaderType {
    SHADER_BILINEAR,
    SHADER_BILINEAR2,
    SHADER_BILINEAR3,
    SHADER_BILINEAR4,
    SHADER_BILINEAR2X2,
    SHADER_BICUBIC_UPSCALE,
    SHADER_BICUBIC_HALF_1D,
    SHADER_PLANAR,
    SHADER_YUV_MRT_PASS1,
    SHADER_YUV_MRT_PASS2,
  };

  using ShaderProgramKeyType = std::pair<ShaderType, bool>;

  GLHelperScaling(gles2::GLES2Interface* gl, GLHelper* helper);

  GLHelperScaling(const GLHelperScaling&) = delete;
  GLHelperScaling& operator=(const GLHelperScaling&) = delete;

  ~GLHelperScaling();
  void InitBuffer();

  // Returns null on invalid arguments.
  std::unique_ptr<GLHelper::ScalerInterface> CreateScaler(
      GLHelper::ScalerQuality quality,
      const gfx::Vector2d& scale_from,
      const gfx::Vector2d& scale_to,
      bool flipped_source,
      bool flip_output,
      bool swizzle);

  // These convert source textures with RGBA pixel data into a single-color-
  // channel planar format. Used for grayscale and I420 format conversion.
  //
  // While these output RGBA pixels in the destination texture(s), each RGBA
  // pixel is actually a container for 4 consecutive pixels in the result.
  std::unique_ptr<GLHelper::ScalerInterface> CreateGrayscalePlanerizer(
      bool flipped_source,
      bool flip_output,
      bool swizzle);
  std::unique_ptr<GLHelper::ScalerInterface> CreateI420Planerizer(
      int plane,  // 0=Y, 1=U, 2=V
      bool flipped_source,
      bool flip_output,
      bool swizzle);

  // These are a faster path to I420 planerization, if the platform supports
  // it. The first pass draws to two outputs simultaneously: the Y plane and an
  // interim UV plane that is used as the input to the second pass. Then, the
  // second pass splits the UV plane, drawing to two outputs: the final U plane
  // and final V plane. Thus, clients should call ScaleToMultipleOutputs() on
  // the returned instance.
  std::unique_ptr<GLHelper::ScalerInterface> CreateI420MrtPass1Planerizer(
      bool flipped_source,
      bool flip_output,
      bool swizzle);
  std::unique_ptr<GLHelper::ScalerInterface> CreateI420MrtPass2Planerizer(
      bool swizzle);

 private:
  // A ScaleOp represents a pass in a scaler pipeline, in one dimension.
  // Note that when quality is GOOD, multiple scaler passes will be
  // combined into one operation for increased performance.
  // Exposed in the header file for testing purposes.
  struct ScaleOp {
    ScaleOp(int factor, bool x, int size)
        : scale_factor(factor), scale_x(x), scale_size(size) {}

    // Calculates the sequence of ScaleOp needed to convert an image of
    // relative size |src| into an image of relative size |dst|. If |scale_x| is
    // true, then the calculations are for the X axis of the image, otherwise Y.
    // If |allow3| is true, we can use a SHADER_BILINEAR3 to replace
    // a scale up and scale down with a 3-tap bilinear scale.
    // The calculated ScaleOps are added to |ops|.
    static void AddOps(int src,
                       int dst,
                       bool scale_x,
                       bool allow3,
                       base::circular_deque<ScaleOp>* ops) {
      int num_downscales = 0;
      if (allow3 && dst * 3 >= src && dst * 2 < src) {
        // Technically, this should be a scale up and then a
        // scale down, but it makes the optimization code more
        // complicated.
        ops->push_back(ScaleOp(3, scale_x, dst));
        return;
      }
      while ((dst << num_downscales) < src) {
        num_downscales++;
      }
      if ((dst << num_downscales) != src) {
        ops->push_back(ScaleOp(0, scale_x, dst << num_downscales));
      }
      while (num_downscales) {
        num_downscales--;
        ops->push_back(ScaleOp(2, scale_x, dst << num_downscales));
      }
    }

    // Update either the X or Y component of |scale| to the match the relative
    // result size of this ScaleOp.
    void UpdateScale(gfx::Vector2d* scale) {
      if (scale_x) {
        scale->set_x(scale_size);
      } else {
        scale->set_y(scale_size);
      }
    }

    // A scale factor of 0 means upscale
    // 2 means 50% scale
    // 3 means 33% scale, etc.
    int scale_factor;
    bool scale_x;    // Otherwise y
    int scale_size;  // Size to scale to.
  };

  // Full specification for a single scaling stage.
  struct ScalerStage {
    ShaderType shader;
    gfx::Vector2d scale_from;
    gfx::Vector2d scale_to;
    bool scale_x;
    bool flipped_source;
    bool flip_output;
    bool swizzle;
  };

  // Compute a vector of scaler stages for a particular
  // set of input/output parameters.
  static void ComputeScalerStages(GLHelper::ScalerQuality quality,
                                  const gfx::Vector2d& scale_from,
                                  const gfx::Vector2d& scale_to,
                                  bool flipped_source,
                                  bool flip_output,
                                  bool swizzle,
                                  std::vector<ScalerStage>* scaler_stages);

  // Take two queues of ScaleOp structs and generate a
  // vector of scaler stages. This is the second half of
  // ComputeScalerStages.
  static void ConvertScalerOpsToScalerStages(
      GLHelper::ScalerQuality quality,
      gfx::Vector2d scale_from,
      base::circular_deque<GLHelperScaling::ScaleOp>* x_ops,
      base::circular_deque<GLHelperScaling::ScaleOp>* y_ops,
      std::vector<ScalerStage>* scaler_stages);

  scoped_refptr<ShaderProgram> GetShaderProgram(ShaderType type, bool swizzle);

  // Interleaved array of 2-dimentional vertex positions (x, y) and
  // 2-dimentional texture coordinates (s, t).
  static const GLfloat kVertexAttributes[];

  raw_ptr<gles2::GLES2Interface> gl_;
  raw_ptr<GLHelper> helper_;

  // The buffer that holds the vertices and the texture coordinates data for
  // drawing a quad.
  ScopedBuffer vertex_attributes_buffer_;

  std::map<ShaderProgramKeyType, scoped_refptr<ShaderProgram>> shader_programs_;

  friend class ShaderProgram;
  friend class ScalerImpl;
  friend class GLHelperBenchmark;
  friend class GLHelperTest;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GL_HELPER_SCALING_H_
