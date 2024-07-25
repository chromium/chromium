// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/client/gl_helper.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/gl_helper_scaling.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if !BUILDFLAG(IS_ANDROID)

namespace gpu {

namespace {

GLHelper::ScalerQuality kQualities[] = {
    GLHelper::SCALER_QUALITY_BEST,
    GLHelper::SCALER_QUALITY_GOOD,
    GLHelper::SCALER_QUALITY_FAST,
};

const char* kQualityNames[] = {
    "best",
    "good",
    "fast",
};

}  // namespace

class GLHelperTest : public testing::Test {
 protected:
  void SetUp() override {
    feature_list_.Init();

    ContextCreationAttribs attributes;
    attributes.bind_generates_resource = false;

    context_ = std::make_unique<GLInProcessContext>();
    auto result = context_->Initialize(
        viz::TestGpuServiceHolder::GetInstance()->task_executor(), attributes,
        SharedMemoryLimits());
    DCHECK_EQ(result, ContextResult::kSuccess);
    gl_ = context_->GetImplementation();
    ContextSupport* support = context_->GetImplementation();

    helper_ = std::make_unique<GLHelper>(gl_, support);
    helper_scaling_ = std::make_unique<GLHelperScaling>(gl_, helper_.get());
  }

  void TearDown() override {
    helper_scaling_.reset(nullptr);
    helper_.reset(nullptr);
    gl_ = nullptr;
    context_.reset(nullptr);
  }

  // Bicubic filter kernel function.
  static float Bicubic(float x) {
    const float a = -0.5;
    x = std::abs(x);
    float x2 = x * x;
    float x3 = x2 * x;
    if (x <= 1) {
      return (a + 2) * x3 - (a + 3) * x2 + 1;
    } else if (x < 2) {
      return a * x3 - 5 * a * x2 + 8 * a * x - 4 * a;
    } else {
      return 0.0f;
    }
  }

  // Look up a single channel value. Works for 4-channel and single channel
  // bitmaps.  Clamp x/y.
  int Channel(SkBitmap* pixels, int x, int y, int c) {
    if (pixels->bytesPerPixel() == 4) {
      uint32_t* data =
          pixels->getAddr32(std::clamp(x, 0, pixels->width() - 1),
                            std::clamp(y, 0, pixels->height() - 1));
      return (*data) >> (c * 8) & 0xff;
    } else {
      DCHECK_EQ(pixels->bytesPerPixel(), 1);
      DCHECK_EQ(c, 0);
      return *pixels->getAddr8(std::clamp(x, 0, pixels->width() - 1),
                               std::clamp(y, 0, pixels->height() - 1));
    }
  }

  // Set a single channel value. Works for 4-channel and single channel
  // bitmaps.  Clamp x/y.
  void SetChannel(SkBitmap* pixels, int x, int y, int c, int v) {
    DCHECK_GE(x, 0);
    DCHECK_GE(y, 0);
    DCHECK_LT(x, pixels->width());
    DCHECK_LT(y, pixels->height());
    if (pixels->bytesPerPixel() == 4) {
      uint32_t* data = pixels->getAddr32(x, y);
      v = std::clamp(v, 0, 255);
      *data = (*data & ~(0xffu << (c * 8))) | (v << (c * 8));
    } else {
      DCHECK_EQ(pixels->bytesPerPixel(), 1);
      DCHECK_EQ(c, 0);
      uint8_t* data = pixels->getAddr8(x, y);
      v = std::clamp(v, 0, 255);
      *data = v;
    }
  }

  // Print all the R, G, B or A values from an SkBitmap in a
  // human-readable format.
  void PrintChannel(SkBitmap* pixels, int c) {
    for (int y = 0; y < pixels->height(); y++) {
      std::string formatted;
      for (int x = 0; x < pixels->width(); x++) {
        formatted.append(base::StringPrintf("%3d, ", Channel(pixels, x, y, c)));
      }
      LOG(ERROR) << formatted;
    }
  }

  // Print out the individual steps of a scaler pipeline.
  std::string PrintStages(
      const std::vector<GLHelperScaling::ScalerStage>& scaler_stages) {
    std::string ret;
    for (size_t i = 0; i < scaler_stages.size(); i++) {
      ret.append(base::StringPrintf(
          "%dx%d -> %dx%d ", scaler_stages[i].scale_from.x(),
          scaler_stages[i].scale_from.y(), scaler_stages[i].scale_to.x(),
          scaler_stages[i].scale_to.y()));
      bool xy_matters = false;
      switch (scaler_stages[i].shader) {
        case GLHelperScaling::SHADER_BILINEAR:
          ret.append("bilinear");
          break;
        case GLHelperScaling::SHADER_BILINEAR2:
          ret.append("bilinear2");
          xy_matters = true;
          break;
        case GLHelperScaling::SHADER_BILINEAR3:
          ret.append("bilinear3");
          xy_matters = true;
          break;
        case GLHelperScaling::SHADER_BILINEAR4:
          ret.append("bilinear4");
          xy_matters = true;
          break;
        case GLHelperScaling::SHADER_BILINEAR2X2:
          ret.append("bilinear2x2");
          break;
        case GLHelperScaling::SHADER_BICUBIC_UPSCALE:
          ret.append("bicubic upscale");
          xy_matters = true;
          break;
        case GLHelperScaling::SHADER_BICUBIC_HALF_1D:
          ret.append("bicubic 1/2");
          xy_matters = true;
          break;
        case GLHelperScaling::SHADER_PLANAR:
          ret.append("planar");
          break;
        case GLHelperScaling::SHADER_YUV_MRT_PASS1:
          ret.append("rgb2yuv pass 1");
          break;
        case GLHelperScaling::SHADER_YUV_MRT_PASS2:
          ret.append("rgb2yuv pass 2");
          break;
      }

      if (xy_matters) {
        if (scaler_stages[i].scale_x) {
          ret.append(" X");
        } else {
          ret.append(" Y");
        }
      }
      ret.append("\n");
    }
    return ret;
  }

  bool CheckScale(double scale, int samples, bool already_scaled) {
    // 1:1 is valid if there is one sample.
    if (samples == 1 && scale == 1.0) {
      return true;
    }
    // Is it an exact down-scale (50%, 25%, etc.?)
    if (scale == 2.0 * samples) {
      return true;
    }
    // Upscales, only valid if we haven't already scaled in this dimension.
    if (!already_scaled) {
      // Is it a valid bilinear upscale?
      if (samples == 1 && scale <= 1.0) {
        return true;
      }
      // Multi-sample upscale-downscale combination?
      if (scale > samples / 2.0 && scale < samples) {
        return true;
      }
    }
    return false;
  }

  // Make sure that the stages of the scaler pipeline are sane.
  void ValidateScalerStages(
      GLHelper::ScalerQuality quality,
      const std::vector<GLHelperScaling::ScalerStage>& scaler_stages,
      const gfx::Vector2d& overall_scale_from,
      const gfx::Vector2d& overall_scale_to,
      const std::string& message) {
    bool previous_error = HasFailure();

    // Used to verify that up-scales are not attempted after some
    // other scale.
    bool scaled_x = false;
    bool scaled_y = false;

    double combined_x_scale = 1.0;
    double combined_y_scale = 1.0;
    for (size_t i = 0; i < scaler_stages.size(); i++) {
      // Note: 2.0 means scaling down by 50%
      double x_scale = static_cast<double>(scaler_stages[i].scale_from.x()) /
                       static_cast<double>(scaler_stages[i].scale_to.x());
      combined_x_scale *= x_scale;
      double y_scale = static_cast<double>(scaler_stages[i].scale_from.y()) /
                       static_cast<double>(scaler_stages[i].scale_to.y());
      combined_y_scale *= y_scale;

      int x_samples = 0;
      int y_samples = 0;

      // Codify valid scale operations.
      switch (scaler_stages[i].shader) {
        case GLHelperScaling::SHADER_PLANAR:
        case GLHelperScaling::SHADER_YUV_MRT_PASS1:
        case GLHelperScaling::SHADER_YUV_MRT_PASS2:
          EXPECT_TRUE(false) << "Invalid shader.";
          break;

        case GLHelperScaling::SHADER_BILINEAR:
          if (quality != GLHelper::SCALER_QUALITY_FAST) {
            x_samples = 1;
            y_samples = 1;
          }
          break;
        case GLHelperScaling::SHADER_BILINEAR2:
          x_samples = 2;
          y_samples = 1;
          break;
        case GLHelperScaling::SHADER_BILINEAR3:
          x_samples = 3;
          y_samples = 1;
          break;
        case GLHelperScaling::SHADER_BILINEAR4:
          x_samples = 4;
          y_samples = 1;
          break;
        case GLHelperScaling::SHADER_BILINEAR2X2:
          x_samples = 2;
          y_samples = 2;
          break;
        case GLHelperScaling::SHADER_BICUBIC_UPSCALE:
          if (scaler_stages[i].scale_x) {
            EXPECT_LT(x_scale, 1.0);
            EXPECT_EQ(y_scale, 1.0);
          } else {
            EXPECT_EQ(x_scale, 1.0);
            EXPECT_LT(y_scale, 1.0);
          }
          break;
        case GLHelperScaling::SHADER_BICUBIC_HALF_1D:
          if (scaler_stages[i].scale_x) {
            EXPECT_EQ(x_scale, 2.0);
            EXPECT_EQ(y_scale, 1.0);
          } else {
            EXPECT_EQ(x_scale, 1.0);
            EXPECT_EQ(y_scale, 2.0);
          }
          break;
      }

      if (!scaler_stages[i].scale_x) {
        std::swap(x_samples, y_samples);
      }

      if (x_samples) {
        EXPECT_TRUE(CheckScale(x_scale, x_samples, scaled_x))
            << "x_scale = " << x_scale;
      }
      if (y_samples) {
        EXPECT_TRUE(CheckScale(y_scale, y_samples, scaled_y))
            << "y_scale = " << y_scale;
      }

      if (x_scale != 1.0) {
        scaled_x = true;
      }
      if (y_scale != 1.0) {
        scaled_y = true;
      }
    }

    const double expected_x_scale =
        static_cast<double>(overall_scale_from.x()) /
        static_cast<double>(overall_scale_to.x());
    const double expected_y_scale =
        static_cast<double>(overall_scale_from.y()) /
        static_cast<double>(overall_scale_to.y());
    EXPECT_NEAR(expected_x_scale, combined_x_scale, 1e-9);
    EXPECT_NEAR(expected_y_scale, combined_y_scale, 1e-9);

    if (HasFailure() && !previous_error) {
      LOG(ERROR) << "Invalid scaler stages: " << message;
      LOG(ERROR) << "Scaler stages:";
      LOG(ERROR) << PrintStages(scaler_stages);
    }
  }

  // Compares two bitmaps taking color types into account. Checks whether each
  // component of each pixel is no more than |maxdiff| apart. If bitmaps are not
  // similar enough, prints out |truth|, |other|, |source|, |scaler_stages|
  // and |message|.
  void Compare(SkBitmap* truth,
               SkBitmap* other,
               int maxdiff,
               SkBitmap* source,
               const std::vector<GLHelperScaling::ScalerStage>& scaler_stages,
               std::string message) {
    EXPECT_EQ(truth->width(), other->width());
    EXPECT_EQ(truth->height(), other->height());
    bool swizzle = (truth->colorType() == kRGBA_8888_SkColorType &&
                    other->colorType() == kBGRA_8888_SkColorType) ||
                   (truth->colorType() == kBGRA_8888_SkColorType &&
                    other->colorType() == kRGBA_8888_SkColorType);
    EXPECT_TRUE(swizzle || truth->colorType() == other->colorType());
    int bpp = truth->bytesPerPixel();
    for (int x = 0; x < truth->width(); x++) {
      for (int y = 0; y < truth->height(); y++) {
        for (int c = 0; c < bpp; c++) {
          int a = Channel(truth, x, y, c);
          // swizzle when comparing if needed
          int b = swizzle && (c == 0 || c == 2)
                      ? Channel(other, x, y, (c + 2) & 2)
                      : Channel(other, x, y, c);
          EXPECT_NEAR(a, b, maxdiff)
              << " x=" << x << " y=" << y << " c=" << c << " " << message;
          if (std::abs(a - b) > maxdiff) {
            LOG(ERROR) << "-------expected--------";
            for (int i = 0; i < bpp; i++) {
              LOG(ERROR) << "Channel " << i << ":";
              PrintChannel(truth, i);
            }
            LOG(ERROR) << "-------actual--------";
            for (int i = 0; i < bpp; i++) {
              LOG(ERROR) << "Channel " << i << ":";
              PrintChannel(other, i);
            }
            if (source) {
              LOG(ERROR) << "-------original--------";
              for (int i = 0; i < source->bytesPerPixel(); i++) {
                LOG(ERROR) << "Channel " << i << ":";
                PrintChannel(source, i);
              }
            }
            LOG(ERROR) << "-----Scaler stages------";
            LOG(ERROR) << PrintStages(scaler_stages);
            return;
          }
        }
      }
    }
  }

  // Get a single R, G, B or A value as a float.
  float ChannelAsFloat(SkBitmap* pixels, int x, int y, int c) {
    return Channel(pixels, x, y, c) / 255.0;
  }

  // Works like a GL_LINEAR lookup on an SkBitmap.
  float Bilinear(SkBitmap* pixels, float x, float y, int c) {
    x -= 0.5;
    y -= 0.5;
    int base_x = static_cast<int>(floorf(x));
    int base_y = static_cast<int>(floorf(y));
    x -= base_x;
    y -= base_y;
    return (ChannelAsFloat(pixels, base_x, base_y, c) * (1 - x) * (1 - y) +
            ChannelAsFloat(pixels, base_x + 1, base_y, c) * x * (1 - y) +
            ChannelAsFloat(pixels, base_x, base_y + 1, c) * (1 - x) * y +
            ChannelAsFloat(pixels, base_x + 1, base_y + 1, c) * x * y);
  }

  // Very slow bicubic / bilinear scaler for reference.
  void ScaleSlow(SkBitmap* input,
                 const gfx::Rect& source_rect,
                 GLHelper::ScalerQuality quality,
                 SkBitmap* output) {
    float xscale = static_cast<float>(source_rect.width()) / output->width();
    float yscale = static_cast<float>(source_rect.height()) / output->height();
    float clamped_xscale = xscale < 1.0 ? 1.0 : 1.0 / xscale;
    float clamped_yscale = yscale < 1.0 ? 1.0 : 1.0 / yscale;
    for (int dst_y = 0; dst_y < output->height(); dst_y++) {
      for (int dst_x = 0; dst_x < output->width(); dst_x++) {
        for (int channel = 0; channel < 4; channel++) {
          float dst_x_in_src = source_rect.x() + (dst_x + 0.5f) * xscale;
          float dst_y_in_src = source_rect.y() + (dst_y + 0.5f) * yscale;

          float value = 0.0f;
          float sum = 0.0f;
          switch (quality) {
            case GLHelper::SCALER_QUALITY_BEST:
              for (int src_y = source_rect.y() - 10;
                   src_y < source_rect.bottom() + 10; ++src_y) {
                float coeff_y =
                    Bicubic((src_y + 0.5f - dst_y_in_src) * clamped_yscale);
                if (coeff_y == 0.0f) {
                  continue;
                }
                for (int src_x = source_rect.x() - 10;
                     src_x < source_rect.right() + 10; ++src_x) {
                  float coeff =
                      coeff_y *
                      Bicubic((src_x + 0.5f - dst_x_in_src) * clamped_xscale);
                  if (coeff == 0.0f) {
                    continue;
                  }
                  sum += coeff;
                  float c = ChannelAsFloat(input, src_x, src_y, channel);
                  value += c * coeff;
                }
              }
              break;

            case GLHelper::SCALER_QUALITY_GOOD: {
              int xshift = 0, yshift = 0;
              while ((output->width() << xshift) < source_rect.width()) {
                xshift++;
              }
              while ((output->height() << yshift) < source_rect.height()) {
                yshift++;
              }
              int xmag = 1 << xshift;
              int ymag = 1 << yshift;
              if (xmag == 4 && output->width() * 3 >= source_rect.width()) {
                xmag = 3;
              }
              if (ymag == 4 && output->height() * 3 >= source_rect.height()) {
                ymag = 3;
              }
              for (int x = 0; x < xmag; x++) {
                for (int y = 0; y < ymag; y++) {
                  value += Bilinear(input,
                                    source_rect.x() + (dst_x * xmag + x + 0.5) *
                                                          xscale / xmag,
                                    source_rect.y() + (dst_y * ymag + y + 0.5) *
                                                          yscale / ymag,
                                    channel);
                  sum += 1.0;
                }
              }
              break;
            }

            case GLHelper::SCALER_QUALITY_FAST:
              value = Bilinear(input, dst_x_in_src, dst_y_in_src, channel);
              sum = 1.0;
          }
          value /= sum;
          SetChannel(output, dst_x, dst_y, channel,
                     static_cast<int>(value * 255.0f + 0.5f));
        }
      }
    }
  }

  void FlipSKBitmap(SkBitmap* bitmap) {
    int bpp = bitmap->bytesPerPixel();
    DCHECK(bpp == 4 || bpp == 1);
    int top_line = 0;
    int bottom_line = bitmap->height() - 1;
    while (top_line < bottom_line) {
      for (int x = 0; x < bitmap->width(); x++) {
        bpp == 4 ? std::swap(*bitmap->getAddr32(x, top_line),
                             *bitmap->getAddr32(x, bottom_line))
                 : std::swap(*bitmap->getAddr8(x, top_line),
                             *bitmap->getAddr8(x, bottom_line));
      }
      top_line++;
      bottom_line--;
    }
  }

  // gl_helper scales recursively, so we'll need to do that
  // in the reference implementation too.
  void ScaleSlowRecursive(SkBitmap* input,
                          const gfx::Rect& source_rect,
                          GLHelper::ScalerQuality quality,
                          SkBitmap* output) {
    if (quality == GLHelper::SCALER_QUALITY_FAST ||
        quality == GLHelper::SCALER_QUALITY_GOOD) {
      ScaleSlow(input, source_rect, quality, output);
      return;
    }

    float xscale = static_cast<float>(output->width()) / source_rect.width();

    // This corresponds to all the operations we can do directly.
    float yscale = static_cast<float>(output->height()) / source_rect.height();
    if ((xscale == 1.0f && yscale == 1.0f) ||
        (xscale == 0.5f && yscale == 1.0f) ||
        (xscale == 1.0f && yscale == 0.5f) ||
        (xscale >= 1.0f && yscale == 1.0f) ||
        (xscale == 1.0f && yscale >= 1.0f)) {
      ScaleSlow(input, source_rect, quality, output);
      return;
    }

    // Now we break the problem down into smaller pieces, using the
    // operations available.
    int xtmp = source_rect.width();
    int ytmp = source_rect.height();

    if (output->height() != source_rect.height()) {
      ytmp = output->height();
      while (ytmp < source_rect.height() && ytmp * 2 != source_rect.height()) {
        ytmp += ytmp;
      }
    } else {
      xtmp = output->width();
      while (xtmp < source_rect.width() && xtmp * 2 != source_rect.width()) {
        xtmp += xtmp;
      }
    }

    // Note: The following does not account for scaler overscan. This was
    // attempted, but then unit test run time increased by a factor of 30!
    SkBitmap tmp;
    tmp.allocN32Pixels(xtmp, ytmp);

    ScaleSlowRecursive(input, source_rect, quality, &tmp);
    ScaleSlowRecursive(&tmp, gfx::Rect(0, 0, xtmp, ytmp), quality, output);
  }

  // Creates an RGBA SkBitmap with one of the pre-programmed test patterns. The
  // pattern starts at the given |origin|. For positions to the left or above
  // that point, values are filled-in corresponding to GL_CLAMP_TO_EDGE
  // behavior. This is because the reference scaler does not properly account
  // for overscan.
  std::unique_ptr<SkBitmap> CreateTestBitmap(const gfx::Size& size,
                                             const gfx::Point& origin,
                                             int test_pattern) {
    std::unique_ptr<SkBitmap> bitmap(new SkBitmap);
    bitmap->allocPixels(SkImageInfo::Make(size.width(), size.height(),
                                          kRGBA_8888_SkColorType,
                                          kPremul_SkAlphaType));

    for (int x = 0; x < size.width(); ++x) {
      for (int y = 0; y < size.height(); ++y) {
        const int s = std::max(0, x - origin.x());
        const int t = std::max(0, y - origin.y());
        switch (test_pattern) {
          case 0:  // Smooth test pattern
            SetChannel(bitmap.get(), x, y, 0, s * 10);
            SetChannel(bitmap.get(), x, y, 0, t == 0 ? s * 50 : s * 10);
            SetChannel(bitmap.get(), x, y, 1, t * 10);
            SetChannel(bitmap.get(), x, y, 2, (s + t) * 10);
            SetChannel(bitmap.get(), x, y, 3, 255);
            break;
          case 1:  // Small blocks
            SetChannel(bitmap.get(), x, y, 0, s & 1 ? 255 : 0);
            SetChannel(bitmap.get(), x, y, 1, t & 1 ? 255 : 0);
            SetChannel(bitmap.get(), x, y, 2, (s + t) & 1 ? 255 : 0);
            SetChannel(bitmap.get(), x, y, 3, 255);
            break;
          case 2:  // Medium blocks
            SetChannel(bitmap.get(), x, y, 0, 10 + s / 2 * 50);
            SetChannel(bitmap.get(), x, y, 1, 10 + t / 3 * 50);
            SetChannel(bitmap.get(), x, y, 2, (s + t) / 5 * 50 + 5);
            SetChannel(bitmap.get(), x, y, 3, 255);
            break;
        }
      }
    }
    return bitmap;
  }

  // Scaling test: Create a test image, scale it using GLHelperScaling
  // and a reference implementation and compare the results.
  void TestScale(const gfx::Rect& source_rect,
                 const gfx::Size& scaled_size,
                 int test_pattern,
                 size_t quality_index,
                 bool flip_output) {
    // The source texture is meant to be the contents of a framebuffer. Thus, it
    // includes (0,0), and all the way out to the lower-right corner of the
    // |source_rect|.
    const gfx::Size framebuffer_size(source_rect.right(), source_rect.bottom());
    std::unique_ptr<SkBitmap> input_pixels =
        CreateTestBitmap(framebuffer_size, source_rect.origin(), test_pattern);
    GLuint src_texture;
    gl_->GenTextures(1, &src_texture);
    gl_->BindTexture(GL_TEXTURE_2D, src_texture);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, framebuffer_size.width(),
                    framebuffer_size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    input_pixels->getPixels());

    std::string message = base::StringPrintf(
        "source rect: %s "
        "output size: %s "
        "pattern: %d quality: %s %s",
        source_rect.ToString().c_str(), scaled_size.ToString().c_str(),
        test_pattern, kQualityNames[quality_index],
        flip_output ? "flipout" : "noflipout");

    std::vector<GLHelperScaling::ScalerStage> stages;
    const auto scale_from =
        gfx::Vector2d(source_rect.width(), source_rect.height());
    const auto scale_to =
        gfx::Vector2d(scaled_size.width(), scaled_size.height());
    helper_scaling_->ComputeScalerStages(kQualities[quality_index], scale_from,
                                         scale_to, false, flip_output, false,
                                         &stages);
    ValidateScalerStages(kQualities[quality_index], stages, scale_from,
                         scale_to, message);

    // Scale the source texture, producing the results in a new output
    // texture.
    std::unique_ptr<GLHelper::ScalerInterface> scaler =
        helper_->CreateScaler(kQualities[quality_index], scale_from, scale_to,
                              false, flip_output, false);
    ASSERT_FALSE(scaler->IsSamplingFlippedSource());
    ASSERT_EQ(flip_output, scaler->IsFlippingOutput());
    GLuint dst_texture = 0;
    gl_->GenTextures(1, &dst_texture);
    gl_->BindTexture(GL_TEXTURE_2D, dst_texture);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scaled_size.width(),
                    scaled_size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    nullptr);
    scaler->Scale(src_texture, framebuffer_size, source_rect.OffsetFromOrigin(),
                  dst_texture, gfx::Rect(scaled_size));

    SkBitmap output_pixels;
    output_pixels.allocPixels(
        SkImageInfo::Make(scaled_size.width(), scaled_size.height(),
                          kRGBA_8888_SkColorType, kPremul_SkAlphaType));

    EXPECT_TRUE(ReadBackTexture(
        dst_texture, gfx::Rect(scaled_size),
        static_cast<unsigned char*>(output_pixels.getPixels()),
        output_pixels.rowBytes(), flip_output, kRGBA_8888_SkColorType));

    // If the bitmap shouldn't have changed - compare against input.
    if (source_rect == gfx::Rect(scaled_size)) {
      Compare(input_pixels.get(), &output_pixels, 0, nullptr, stages,
              message + " comparing against input");
      return;
    }

    // Now scale the bitmap using the reference implementation.
    SkBitmap truth_pixels;
    truth_pixels.allocPixels(
        SkImageInfo::Make(scaled_size.width(), scaled_size.height(),
                          kRGBA_8888_SkColorType, kPremul_SkAlphaType));
    ScaleSlowRecursive(input_pixels.get(), source_rect,
                       kQualities[quality_index], &truth_pixels);

    // Compare the results produced by the two implementations. Note that the
    // reference implementation does not fully account for overscan (see
    // comment in ScaleSlowRecursive()), and so the the maxdiff must be
    // increased when the bicubic scaler is being used.
    const int maxdiff = 2 + (quality_index == 0 ? (2 * stages.size()) : 0);
    Compare(&truth_pixels, &output_pixels, maxdiff, input_pixels.get(), stages,
            message + " comparing against scaled");

    gl_->DeleteTextures(1, &src_texture);
    gl_->DeleteTextures(1, &dst_texture);
  }

  // Scaling patching test: Scale an entire source image, and then scale various
  // subsets of the source image; and then confirm that the pixels in the
  // subsets exactly match their corresponding ones in the whole. This is
  // critical for use cases where the scaler only needs to render the changed
  // region of a source image.
  void TestScalePatching(const gfx::Vector2d& scale_from,
                         const gfx::Vector2d& scale_to,
                         int test_pattern,
                         size_t quality_index,
                         bool flipped_source) {
    // Generate a source texture representing copied-from-framebuffer content
    // with a test pattern that is twice the size of the "from" vector.
    const gfx::Size framebuffer_size(scale_from.x() * 2, scale_from.y() * 2);
    std::unique_ptr<SkBitmap> test_bitmap =
        CreateTestBitmap(framebuffer_size, gfx::Point(), test_pattern);
    if (flipped_source)
      FlipSKBitmap(test_bitmap.get());
    GLuint src_texture;
    gl_->GenTextures(1, &src_texture);
    gl_->BindTexture(GL_TEXTURE_2D, src_texture);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, framebuffer_size.width(),
                    framebuffer_size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    test_bitmap->getPixels());

    const std::unique_ptr<GLHelper::ScalerInterface> scaler =
        helper_->CreateScaler(kQualities[quality_index], scale_from, scale_to,
                              flipped_source, false, false);
    ASSERT_EQ(flipped_source, scaler->IsSamplingFlippedSource());
    ASSERT_FALSE(scaler->IsFlippingOutput());
    // Note: These scaler stages are only being computed here for the benefit
    // Compare()'s error output messaging, below.
    std::vector<GLHelperScaling::ScalerStage> stages;
    helper_scaling_->ComputeScalerStages(kQualities[quality_index], scale_from,
                                         scale_to, flipped_source, false, false,
                                         &stages);

    // First, produce the entire output image, a full scan of the source to
    // produce all the output pixels. The output image is twice the size of the
    // "to" vector.
    GLuint dst_texture;
    gl_->GenTextures(1, &dst_texture);
    gl_->BindTexture(GL_TEXTURE_2D, dst_texture);
    const gfx::Size entire_output_size(scale_to.x() * 2, scale_to.y() * 2);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, entire_output_size.width(),
                    entire_output_size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    nullptr);
    scaler->Scale(src_texture, framebuffer_size, gfx::Vector2dF(), dst_texture,
                  gfx::Rect(entire_output_size));
    SkBitmap entire_output;
    entire_output.allocPixels(SkImageInfo::Make(
        entire_output_size.width(), entire_output_size.height(),
        kRGBA_8888_SkColorType, kPremul_SkAlphaType));

    EXPECT_TRUE(ReadBackTexture(
        dst_texture, gfx::Rect(entire_output_size),
        static_cast<unsigned char*>(entire_output.getPixels()),
        entire_output.rowBytes(), /*flip_y=*/false, kRGBA_8888_SkColorType));

    const std::string human_readable_test_params = base::StringPrintf(
        "scale from: %s "
        "scale to: %s "
        "pattern: %d quality: %s %s",
        scale_from.ToString().c_str(), scale_to.ToString().c_str(),
        test_pattern, kQualityNames[quality_index],
        flipped_source ? "flippedsource" : "");

    // Check the entire output image against the reference implementation.
    SkBitmap entire_output_ref;
    entire_output_ref.allocPixels(SkImageInfo::Make(
        entire_output_size.width(), entire_output_size.height(),
        kRGBA_8888_SkColorType, kPremul_SkAlphaType));
    ScaleSlowRecursive(test_bitmap.get(), gfx::Rect(framebuffer_size),
                       kQualities[quality_index], &entire_output_ref);
    Compare(&entire_output_ref, &entire_output, 2, test_bitmap.get(), stages,
            human_readable_test_params + " ENTIRE OUTPUT");
    if (HasFailure())
      return;

    // Now, produce patches at various offsets and compare to the pixels in
    // |entire_output|.
    const gfx::Size patch_size(scale_to.x(), scale_to.y());
    gl_->BindTexture(GL_TEXTURE_2D, dst_texture);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, patch_size.width(),
                    patch_size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    for (int xoffset = 0; xoffset < scale_to.x(); ++xoffset) {
      for (int yoffset = 0; yoffset < scale_to.y(); ++yoffset) {
        const gfx::Rect patch_rect(gfx::Point(xoffset, yoffset), patch_size);

        // First method of producing a patch: Scale from the same source texture
        // and just provide an offset output Rect.
        scaler->Scale(src_texture, framebuffer_size, gfx::Vector2dF(),
                      dst_texture, patch_rect);
        SkBitmap patch_output;
        patch_output.allocPixels(
            SkImageInfo::Make(patch_size.width(), patch_size.height(),
                              kRGBA_8888_SkColorType, kPremul_SkAlphaType));
        EXPECT_TRUE(ReadBackTexture(
            dst_texture, gfx::Rect(patch_size),
            static_cast<unsigned char*>(patch_output.getPixels()),
            patch_output.rowBytes(), /*flip_y=*/false, kRGBA_8888_SkColorType));
        SkBitmap expected;
        SkIRect expected_subrect{patch_rect.x(), patch_rect.y(),
                                 patch_rect.right(), patch_rect.bottom()};
        if (flipped_source) {
          expected_subrect.fTop = entire_output.height() - patch_rect.bottom();
          expected_subrect.fBottom = entire_output.height() - patch_rect.y();
        }
        ASSERT_TRUE(entire_output.extractSubset(&expected, expected_subrect));
        Compare(&expected, &patch_output, 2, test_bitmap.get(), stages,
                "METHOD1 " + human_readable_test_params +
                    " patch rect: " + patch_rect.ToString());
        if (HasFailure())
          return;

        // Second method of producing a patch: First copy just the "region of
        // influence" of the source texture, then produced a scaled image from
        // that.
        gfx::Rect sampling_rect;
        gfx::Vector2dF offset;
        scaler->ComputeRegionOfInfluence(framebuffer_size, gfx::Vector2dF(),
                                         patch_rect, &sampling_rect, &offset);
        // TODO(crbug.com/41350322): Only test offsets having whole-numbered
        // coordinates until the scalers can account for the other case.
        if (offset.x() == std::floor(offset.x()) &&
            offset.y() == std::floor(offset.y())) {
          GLuint src_subset_texture;
          gl_->GenTextures(1, &src_subset_texture);
          gl_->BindTexture(GL_TEXTURE_2D, src_subset_texture);
          gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sampling_rect.width(),
                          sampling_rect.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                          nullptr);
          gl_->CopySubTextureCHROMIUM(
              src_texture, 0 /* source_level */, GL_TEXTURE_2D,
              src_subset_texture, 0 /* dest_level */, 0 /* xoffset */,
              0 /* yoffset */, sampling_rect.x(), sampling_rect.y(),
              sampling_rect.width(), sampling_rect.height(), false, false,
              false);
          scaler->Scale(src_subset_texture, sampling_rect.size(), offset,
                        dst_texture, gfx::Rect(patch_size));
          gl_->DeleteTextures(1, &src_subset_texture);
          EXPECT_TRUE(ReadBackTexture(
              dst_texture, gfx::Rect(patch_size),
              static_cast<unsigned char*>(patch_output.getPixels()),
              patch_output.rowBytes(), /*flip_y=*/false,
              kRGBA_8888_SkColorType));
          Compare(&expected, &patch_output, 2, test_bitmap.get(), stages,
                  "METHOD2 " + human_readable_test_params +
                      " patch rect: " + patch_rect.ToString());
          if (HasFailure())
            return;
        }
      }
    }

    gl_->DeleteTextures(1, &src_texture);
    gl_->DeleteTextures(1, &dst_texture);
  }

  // Create a scaling pipeline and check that it is made up of
  // valid scaling operations.
  void TestScalerPipeline(size_t quality,
                          int xsize,
                          int ysize,
                          int dst_xsize,
                          int dst_ysize) {
    std::vector<GLHelperScaling::ScalerStage> stages;
    helper_scaling_->ComputeScalerStages(
        kQualities[quality], gfx::Vector2d(xsize, ysize),
        gfx::Vector2d(dst_xsize, dst_ysize), false, false, false, &stages);
    ValidateScalerStages(kQualities[quality], stages,
                         gfx::Vector2d(xsize, ysize),
                         gfx::Vector2d(dst_xsize, dst_ysize),
                         base::StringPrintf("input size: %dx%d "
                                            "output size: %dx%d "
                                            "quality: %s",
                                            xsize, ysize, dst_xsize, dst_ysize,
                                            kQualityNames[quality]));
  }

  // Create a scaling pipeline and make sure that the steps
  // are exactly the steps we expect.
  void CheckPipeline(GLHelper::ScalerQuality quality,
                     int xsize,
                     int ysize,
                     int dst_xsize,
                     int dst_ysize,
                     const std::string& description) {
    std::vector<GLHelperScaling::ScalerStage> stages;
    helper_scaling_->ComputeScalerStages(quality, gfx::Vector2d(xsize, ysize),
                                         gfx::Vector2d(dst_xsize, dst_ysize),
                                         false, false, false, &stages);
    ValidateScalerStages(GLHelper::SCALER_QUALITY_GOOD, stages,
                         gfx::Vector2d(xsize, ysize),
                         gfx::Vector2d(dst_xsize, dst_ysize), "");
    EXPECT_EQ(PrintStages(stages), description);
  }

  void DrawGridToBitmap(int w,
                        int h,
                        SkColor background_color,
                        SkColor grid_color,
                        int grid_pitch,
                        int grid_width,
                        const SkBitmap& bmp) {
    ASSERT_GT(grid_pitch, 0);
    ASSERT_GT(grid_width, 0);
    ASSERT_NE(background_color, grid_color);

    for (int y = 0; y < h; ++y) {
      bool y_on_grid = ((y % grid_pitch) < grid_width);

      for (int x = 0; x < w; ++x) {
        bool on_grid = (y_on_grid || ((x % grid_pitch) < grid_width));

        *bmp.getAddr32(x, y) = (on_grid ? grid_color : background_color);
      }
    }
  }

  void DrawCheckerToBitmap(int w,
                           int h,
                           SkColor color1,
                           SkColor color2,
                           int rect_w,
                           int rect_h,
                           const SkBitmap& bmp) {
    ASSERT_GT(rect_w, 0);
    ASSERT_GT(rect_h, 0);
    ASSERT_NE(color1, color2);

    for (int y = 0; y < h; ++y) {
      bool y_bit = (((y / rect_h) & 0x1) == 0);

      for (int x = 0; x < w; ++x) {
        bool x_bit = (((x / rect_w) & 0x1) == 0);

        bool use_color2 = (x_bit != y_bit);  // xor
        *bmp.getAddr32(x, y) = (use_color2 ? color2 : color1);
      }
    }
  }

  bool ColorComponentsClose(SkColor component1, SkColor component2) {
    int c1 = static_cast<int>(component1);
    int c2 = static_cast<int>(component2);
    return (std::abs(c1 - c2) == 0);
  }

  bool ColorsClose(SkColor color1, SkColor color2) {
    bool red = ColorComponentsClose(SkColorGetR(color1), SkColorGetR(color2));
    bool green = ColorComponentsClose(SkColorGetG(color1), SkColorGetG(color2));
    bool blue = ColorComponentsClose(SkColorGetB(color1), SkColorGetB(color2));
    bool alpha = ColorComponentsClose(SkColorGetA(color1), SkColorGetA(color2));
    return red && blue && green && alpha;
  }

  bool IsEqual(const SkBitmap& bmp1, const SkBitmap& bmp2) {
    if (bmp1.isNull() && bmp2.isNull())
      return true;
    if (bmp1.width() != bmp2.width() || bmp1.height() != bmp2.height()) {
      LOG(ERROR) << "Bitmap geometry check failure";
      return false;
    }
    if (bmp1.colorType() != bmp2.colorType())
      return false;

    if (!bmp1.getPixels() || !bmp2.getPixels()) {
      LOG(ERROR) << "Empty Bitmap!";
      return false;
    }
    for (int y = 0; y < bmp1.height(); ++y) {
      for (int x = 0; x < bmp1.width(); ++x) {
        if (!ColorsClose(bmp1.getColor(x, y), bmp2.getColor(x, y))) {
          LOG(ERROR) << "Bitmap color comparison failure";
          return false;
        }
      }
    }
    return true;
  }

  void BindAndAttachTextureWithPixels(GLuint src_texture,
                                      SkColorType color_type,
                                      const gfx::Size& src_size,
                                      const SkBitmap& input_pixels) {
    gl_->BindTexture(GL_TEXTURE_2D, src_texture);
    const GLenum format =
        (color_type == kBGRA_8888_SkColorType) ? GL_BGRA_EXT : GL_RGBA;
    gl_->TexImage2D(GL_TEXTURE_2D, 0, format, src_size.width(),
                    src_size.height(), 0, format, GL_UNSIGNED_BYTE,
                    input_pixels.getPixels());
  }

  bool ReadBackTexture(GLuint src_texture,
                       const gfx::Rect& src_rect,
                       unsigned char* pixels,
                       size_t pixels_stride,
                       bool flip_y,
                       SkColorType color_type) {
    DCHECK(color_type == kRGBA_8888_SkColorType ||
           color_type == kBGRA_8888_SkColorType);
    base::RunLoop run_loop;
    bool success = false;
    GLenum format;
    if (color_type == kRGBA_8888_SkColorType)
      format = GL_RGBA;
    else
      format = GL_BGRA_EXT;

    helper_->ReadbackTextureAsync(
        src_texture, GL_TEXTURE_2D, src_rect.origin(), src_rect.size(), pixels,
        pixels_stride, flip_y, format,
        base::BindOnce(
            [](bool* success, base::OnceClosure callback, bool result) {
              *success = result;
              std::move(callback).Run();
            },
            &success, run_loop.QuitClosure()));
    run_loop.Run();
    return success;
  }

  // Test basic format readback.
  bool TestTextureFormatReadback(const gfx::Size& src_size,
                                 SkColorType color_type) {
    SkImageInfo info = SkImageInfo::Make(src_size.width(), src_size.height(),
                                         color_type, kPremul_SkAlphaType);
    GLuint src_texture;
    gl_->GenTextures(1, &src_texture);
    SkBitmap input_pixels;
    input_pixels.allocPixels(info);
    // Test Pattern-1, Fill with Plain color pattern.
    // Erase the input bitmap with red color.
    input_pixels.eraseColor(SK_ColorRED);
    BindAndAttachTextureWithPixels(src_texture, color_type, src_size,
                                   input_pixels);
    SkBitmap output_pixels;
    output_pixels.allocPixels(info);
    // Initialize the output bitmap with Green color.
    // When the readback is over output bitmap should have the red color.
    output_pixels.eraseColor(SK_ColorGREEN);
    uint8_t* pixels = static_cast<uint8_t*>(output_pixels.getPixels());
    if (!ReadBackTexture(src_texture, gfx::Rect(src_size), pixels,
                         output_pixels.rowBytes(), /*flip_y=*/false,
                         color_type) ||
        !IsEqual(input_pixels, output_pixels)) {
      LOG(ERROR) << "Bitmap comparison failure Pattern-1";
      return false;
    }
    const int rect_w = 10, rect_h = 4, src_grid_pitch = 10, src_grid_width = 4;
    const SkColor color1 = SK_ColorRED, color2 = SK_ColorBLUE;
    // Test Pattern-2, Fill with Grid Pattern.
    DrawGridToBitmap(src_size.width(), src_size.height(), color2, color1,
                     src_grid_pitch, src_grid_width, input_pixels);
    BindAndAttachTextureWithPixels(src_texture, color_type, src_size,
                                   input_pixels);
    if (!ReadBackTexture(src_texture, gfx::Rect(src_size), pixels,
                         output_pixels.rowBytes(), /*flip_y=*/false,
                         color_type) ||
        !IsEqual(input_pixels, output_pixels)) {
      LOG(ERROR) << "Bitmap comparison failure Pattern-2";
      return false;
    }
    // Test Pattern-3, Fill with CheckerBoard Pattern.
    DrawCheckerToBitmap(src_size.width(), src_size.height(), color1, color2,
                        rect_w, rect_h, input_pixels);
    BindAndAttachTextureWithPixels(src_texture, color_type, src_size,
                                   input_pixels);
    if (!ReadBackTexture(src_texture, gfx::Rect(src_size), pixels,
                         output_pixels.rowBytes(), /*flip_y=*/false,
                         color_type) ||
        !IsEqual(input_pixels, output_pixels)) {
      LOG(ERROR) << "Bitmap comparison failure Pattern-3";
      return false;
    }
    gl_->DeleteTextures(1, &src_texture);
    if (HasFailure()) {
      return false;
    }
    return true;
  }

  void TestAddOps(int src, int dst, bool scale_x, bool allow3) {
    base::circular_deque<GLHelperScaling::ScaleOp> ops;
    GLHelperScaling::ScaleOp::AddOps(src, dst, scale_x, allow3, &ops);
    // Scale factor 3 is a special case.
    // It is currently only allowed by itself.
    if (allow3 && dst * 3 >= src && dst * 2 < src) {
      EXPECT_EQ(ops[0].scale_factor, 3);
      EXPECT_EQ(ops.size(), 1U);
      EXPECT_EQ(ops[0].scale_x, scale_x);
      EXPECT_EQ(ops[0].scale_size, dst);
      return;
    }

    for (size_t i = 0; i < ops.size(); i++) {
      EXPECT_EQ(ops[i].scale_x, scale_x);
      if (i == 0) {
        // Only the first op is allowed to be a scale up.
        // (Scaling up *after* scaling down would make it fuzzy.)
        EXPECT_TRUE(ops[0].scale_factor == 0 || ops[0].scale_factor == 2);
      } else {
        // All other operations must be 50% downscales.
        EXPECT_EQ(ops[i].scale_factor, 2);
      }
    }
    // Check that the scale factors make sense and add up.
    int tmp = dst;
    for (int i = static_cast<int>(ops.size() - 1); i >= 0; i--) {
      EXPECT_EQ(tmp, ops[i].scale_size);
      if (ops[i].scale_factor == 0) {
        EXPECT_EQ(i, 0);
        EXPECT_GT(tmp, src);
        tmp = src;
      } else {
        tmp *= ops[i].scale_factor;
      }
    }
    EXPECT_EQ(tmp, src);
  }

  void CheckPipeline2(int xsize,
                      int ysize,
                      int dst_xsize,
                      int dst_ysize,
                      const std::string& description) {
    std::vector<GLHelperScaling::ScalerStage> stages;
    helper_scaling_->ConvertScalerOpsToScalerStages(
        GLHelper::SCALER_QUALITY_GOOD, gfx::Vector2d(xsize, ysize), &x_ops_,
        &y_ops_, &stages);
    EXPECT_EQ(x_ops_.size(), 0U);
    EXPECT_EQ(y_ops_.size(), 0U);
    ValidateScalerStages(GLHelper::SCALER_QUALITY_GOOD, stages,
                         gfx::Vector2d(xsize, ysize),
                         gfx::Vector2d(dst_xsize, dst_ysize), "");
    EXPECT_EQ(PrintStages(stages), description);
  }

  void CheckOptimizationsTest() {
    // Basic upscale. X and Y should be combined into one pass.
    x_ops_.push_back(GLHelperScaling::ScaleOp(0, true, 2000));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 2000));
    CheckPipeline2(1024, 768, 2000, 2000, "1024x768 -> 2000x2000 bilinear\n");

    // X scaled 1/2, Y upscaled, should still be one pass.
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 512));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 2000));
    CheckPipeline2(1024, 768, 512, 2000, "1024x768 -> 512x2000 bilinear\n");

    // X upscaled, Y scaled 1/2, one bilinear pass
    x_ops_.push_back(GLHelperScaling::ScaleOp(0, true, 2000));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 384));
    CheckPipeline2(1024, 768, 2000, 384, "1024x768 -> 2000x384 bilinear\n");

    // X scaled 1/2, Y scaled 1/2, one bilinear pass
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 512));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 384));
    CheckPipeline2(1024, 768, 512, 384, "1024x768 -> 512x384 bilinear\n");

    // X scaled 1/2, Y scaled to 60%, one bilinear2 pass.
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 50));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 120));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 60));
    CheckPipeline2(100, 100, 50, 60, "100x100 -> 50x60 bilinear2 Y\n");

    // X scaled to 60%, Y scaled 1/2, one bilinear2 pass.
    x_ops_.push_back(GLHelperScaling::ScaleOp(0, true, 120));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 60));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 50));
    CheckPipeline2(100, 100, 60, 50, "100x100 -> 60x50 bilinear2 X\n");

    // X scaled to 60%, Y scaled 60%, one bilinear2x2 pass.
    x_ops_.push_back(GLHelperScaling::ScaleOp(0, true, 120));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 60));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 120));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 60));
    CheckPipeline2(100, 100, 60, 60, "100x100 -> 60x60 bilinear2x2\n");

    // X scaled to 40%, Y scaled 40%, two bilinear3 passes.
    x_ops_.push_back(GLHelperScaling::ScaleOp(3, true, 40));
    y_ops_.push_back(GLHelperScaling::ScaleOp(3, false, 40));
    CheckPipeline2(100, 100, 40, 40,
                   "100x100 -> 100x40 bilinear3 Y\n"
                   "100x40 -> 40x40 bilinear3 X\n");

    // X scaled to 60%, Y scaled 40%
    x_ops_.push_back(GLHelperScaling::ScaleOp(0, true, 120));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 60));
    y_ops_.push_back(GLHelperScaling::ScaleOp(3, false, 40));
    CheckPipeline2(100, 100, 60, 40,
                   "100x100 -> 100x40 bilinear3 Y\n"
                   "100x40 -> 60x40 bilinear2 X\n");

    // X scaled to 40%, Y scaled 60%
    x_ops_.push_back(GLHelperScaling::ScaleOp(3, true, 40));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 120));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 60));
    CheckPipeline2(100, 100, 40, 60,
                   "100x100 -> 100x60 bilinear2 Y\n"
                   "100x60 -> 40x60 bilinear3 X\n");

    // X scaled to 30%, Y scaled 30%
    x_ops_.push_back(GLHelperScaling::ScaleOp(0, true, 120));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 60));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 30));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 120));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 60));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 30));
    CheckPipeline2(100, 100, 30, 30,
                   "100x100 -> 100x30 bilinear4 Y\n"
                   "100x30 -> 30x30 bilinear4 X\n");

    // X scaled to 50%, Y scaled 30%
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 50));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 120));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 60));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 30));
    CheckPipeline2(100, 100, 50, 30, "100x100 -> 50x30 bilinear4 Y\n");

    // X scaled to 150%, Y scaled 30%
    // Note that we avoid combinding X and Y passes
    // as that would probably be LESS efficient here.
    x_ops_.push_back(GLHelperScaling::ScaleOp(0, true, 150));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 120));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 60));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 30));
    CheckPipeline2(100, 100, 150, 30,
                   "100x100 -> 100x30 bilinear4 Y\n"
                   "100x30 -> 150x30 bilinear\n");

    // X scaled to 1%, Y scaled 1%
    x_ops_.push_back(GLHelperScaling::ScaleOp(0, true, 128));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 64));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 32));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 16));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 8));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 4));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 2));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 1));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 128));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 64));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 32));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 16));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 8));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 4));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 2));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 1));
    CheckPipeline2(100, 100, 1, 1,
                   "100x100 -> 100x32 bilinear4 Y\n"
                   "100x32 -> 100x4 bilinear4 Y\n"
                   "100x4 -> 64x1 bilinear2x2\n"
                   "64x1 -> 8x1 bilinear4 X\n"
                   "8x1 -> 1x1 bilinear4 X\n");
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<GLInProcessContext> context_;
  raw_ptr<gles2::GLES2Interface> gl_;  // This is owned by |context_|.
  std::unique_ptr<GLHelper> helper_;
  std::unique_ptr<GLHelperScaling> helper_scaling_;
  base::circular_deque<GLHelperScaling::ScaleOp> x_ops_, y_ops_;
};

class GLHelperPixelTest : public GLHelperTest {
 private:
  gl::DisableNullDrawGLBindings enable_pixel_output_;
};

// TODO(crbug.com/40246425): Re-enable this test
TEST_F(GLHelperTest, DISABLED_RGBAASyncReadbackTest) {
  const int kTestSize = 64;
  bool result = TestTextureFormatReadback(gfx::Size(kTestSize, kTestSize),
                                          kRGBA_8888_SkColorType);
  EXPECT_EQ(result, true);
}

TEST_F(GLHelperTest, BGRAASyncReadbackTest) {
  const int kTestSize = 64;
  bool result = TestTextureFormatReadback(gfx::Size(kTestSize, kTestSize),
                                          kBGRA_8888_SkColorType);
  EXPECT_EQ(result, true);
}

int kRGBReadBackSizes[] = {3, 6, 16};

class GLHelperPixelReadbackTest
    : public GLHelperPixelTest,
      public ::testing::WithParamInterface<std::tuple<unsigned int,
                                                      unsigned int,
                                                      unsigned int,
                                                      unsigned int,
                                                      unsigned int>> {};

// Per pixel tests, all sizes are small so that we can print
// out the generated bitmaps.
// TODO(crbug.com/40867694): Very flaky on Linux ASAN.
#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER)
#define MAYBE_ScaleTest DISABLED_ScaleTest
#else
#define MAYBE_ScaleTest ScaleTest
#endif
TEST_P(GLHelperPixelReadbackTest, MAYBE_ScaleTest) {
  unsigned int q_index = std::get<0>(GetParam());
  unsigned int x = std::get<1>(GetParam());
  unsigned int y = std::get<2>(GetParam());
  unsigned int dst_x = std::get<3>(GetParam());
  unsigned int dst_y = std::get<4>(GetParam());

  for (int flip_output = 0; flip_output <= 1; ++flip_output) {
    for (int pattern = 0; pattern < 3; ++pattern) {
      for (int xoffset = 0; xoffset < 4; ++xoffset) {
        for (int yoffset = 0; yoffset < 4; ++yoffset) {
          TestScale(
              gfx::Rect(xoffset, yoffset, kRGBReadBackSizes[x],
                        kRGBReadBackSizes[y]),
              gfx::Size(kRGBReadBackSizes[dst_x], kRGBReadBackSizes[dst_y]),
              pattern, q_index, !!flip_output);
          if (HasFailure()) {
            return;
          }
        }
      }
    }
  }
}

// FLAKY: https://crbug.com/871799
TEST_P(GLHelperPixelReadbackTest, DISABLED_ScalePatching) {
  for (int flipped_source = 0; flipped_source <= 1; ++flipped_source) {
    for (int pattern = 0; pattern < 3; ++pattern) {
      TestScalePatching(
          gfx::Vector2d(kRGBReadBackSizes[std::get<1>(GetParam())],
                        kRGBReadBackSizes[std::get<2>(GetParam())]),
          gfx::Vector2d(kRGBReadBackSizes[std::get<3>(GetParam())],
                        kRGBReadBackSizes[std::get<4>(GetParam())]),
          pattern, std::get<0>(GetParam()), !!flipped_source);
      if (HasFailure()) {
        return;
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GLHelperPixelReadbackTest,
    ::testing::Combine(
        ::testing::Range<unsigned int>(0, std::size(kQualities)),
        ::testing::Range<unsigned int>(0, std::size(kRGBReadBackSizes)),
        ::testing::Range<unsigned int>(0, std::size(kRGBReadBackSizes)),
        ::testing::Range<unsigned int>(0, std::size(kRGBReadBackSizes)),
        ::testing::Range<unsigned int>(0, std::size(kRGBReadBackSizes))));

// Validate that all scaling generates valid pipelines.
TEST_F(GLHelperTest, ValidateScalerPipelines) {
  int sizes[] = {7, 99, 128, 256, 512, 719, 720, 721, 1920, 2011, 3217, 4096};
  for (size_t q = 0; q < std::size(kQualities); q++) {
    for (size_t x = 0; x < std::size(sizes); x++) {
      for (size_t y = 0; y < std::size(sizes); y++) {
        for (size_t dst_x = 0; dst_x < std::size(sizes); dst_x++) {
          for (size_t dst_y = 0; dst_y < std::size(sizes); dst_y++) {
            TestScalerPipeline(q, sizes[x], sizes[y], sizes[dst_x],
                               sizes[dst_y]);
            if (HasFailure()) {
              return;
            }
          }
        }
      }
    }
  }
}

// Make sure we don't create overly complicated pipelines
// for a few common use cases.
TEST_F(GLHelperTest, CheckSpecificPipelines) {
  // Upscale should be single pass.
  CheckPipeline(GLHelper::SCALER_QUALITY_GOOD, 1024, 700, 1280, 720,
                "1024x700 -> 1280x720 bilinear\n");
  // Slight downscale should use BILINEAR2X2.
  CheckPipeline(GLHelper::SCALER_QUALITY_GOOD, 1280, 720, 1024, 700,
                "1280x720 -> 1024x700 bilinear2x2\n");
  // Most common tab capture pipeline on the Pixel.
  // Should be using two BILINEAR3 passes.
  CheckPipeline(GLHelper::SCALER_QUALITY_GOOD, 2560, 1476, 1249, 720,
                "2560x1476 -> 2560x720 bilinear3 Y\n"
                "2560x720 -> 1249x720 bilinear3 X\n");
}

TEST_F(GLHelperTest, ScalerOpTest) {
  for (int allow3 = 0; allow3 <= 1; allow3++) {
    for (int dst = 1; dst < 2049; dst += 1 + (dst >> 3)) {
      for (int src = 1; src < 2049; src++) {
        TestAddOps(src, dst, allow3 == 1, (src & 1) == 1);
        if (HasFailure()) {
          LOG(ERROR) << "Failed for src=" << src << " dst=" << dst
                     << " allow3=" << allow3;
          return;
        }
      }
    }
  }
}

TEST_F(GLHelperTest, CheckOptimizations) {
  // Test in baseclass since it is friends with GLHelperScaling
  CheckOptimizationsTest();
}

}  // namespace gpu

#endif  // BUILDFLAG(IS_ANDROID)
