#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_LAB_COLOR_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_LAB_COLOR_SPACE_H_

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <iterator>

#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

// Class to handle color transformation between RGB and CIE L*a*b* color spaces.
namespace LabColorSpace {

using blink::FloatPoint3D;
using blink::TransformationMatrix;

static constexpr FloatPoint3D kIlluminantD50 =
    FloatPoint3D(0.964212f, 1.0f, 0.825188f);
static constexpr FloatPoint3D kIlluminantD65 =
    FloatPoint3D(0.95042855f, 1.0f, 1.0889004f);

// All matrices here are 3x3 matrices.
// They are stored in blink::TransformationMatrix which is 4x4 matrix in the
// following form.
// |a b c 0|
// |d e f 0|
// |g h i 0|
// |0 0 0 1|

inline TransformationMatrix mul3x3Diag(const FloatPoint3D& lhs,
                                       const TransformationMatrix& rhs) {
  return TransformationMatrix(
      lhs.X() * rhs.M11(), lhs.Y() * rhs.M12(), lhs.Z() * rhs.M13(), 0.0f,
      lhs.X() * rhs.M21(), lhs.Y() * rhs.M22(), lhs.Z() * rhs.M23(), 0.0f,
      lhs.X() * rhs.M31(), lhs.Y() * rhs.M32(), lhs.Z() * rhs.M33(), 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f);
}

template <typename T>
inline constexpr T clamp(T x, T min, T max) {
  return x < min ? min : x > max ? max : x;
}

// See https://en.wikipedia.org/wiki/Chromatic_adaptation#Von_Kries_transform.
inline TransformationMatrix chromaticAdaptation(
    const TransformationMatrix& matrix,
    const FloatPoint3D& srcWhitePoint,
    const FloatPoint3D& dstWhitePoint) {
  FloatPoint3D srcLMS = matrix.MapPoint(srcWhitePoint);
  FloatPoint3D dstLMS = matrix.MapPoint(dstWhitePoint);
  // LMS is a diagonal matrix stored as a float[3]
  FloatPoint3D LMS = {dstLMS.X() / srcLMS.X(), dstLMS.Y() / srcLMS.Y(),
                      dstLMS.Z() / srcLMS.Z()};
  return matrix.Inverse() * mul3x3Diag(LMS, matrix);
}

class sRGBColorSpace {
 public:
  FloatPoint3D toLinear(const FloatPoint3D& v) const {
    auto EOTF = [](float u) {
      return u < 0.04045f
                 ? clamp(u / 12.92f, .0f, 1.0f)
                 : clamp(std::pow((u + 0.055f) / 1.055f, 2.4f), .0f, 1.0f);
    };
    return {EOTF(v.X()), EOTF(v.Y()), EOTF(v.Z())};
  }

  FloatPoint3D fromLinear(const FloatPoint3D& v) const {
    auto OETF = [](float u) {
      return (u < 0.0031308f
                  ? clamp(12.92 * u, .0, 1.0)
                  : clamp(1.055 * std::pow(u, 1.0 / 2.4) - 0.055, .0, 1.0));
    };
    return {OETF(v.X()), OETF(v.Y()), OETF(v.Z())};
  }

  // See https://en.wikipedia.org/wiki/SRGB#The_reverse_transformation.
  FloatPoint3D toXYZ(const FloatPoint3D& rgb) const {
    return transform_.MapPoint(toLinear(rgb));
  }

  // See
  // https://en.wikipedia.org/wiki/SRGB#The_forward_transformation_(CIE_XYZ_to_sRGB).
  FloatPoint3D fromXYZ(const FloatPoint3D& xyz) const {
    return fromLinear(inverseTransform_.MapPoint(xyz));
  }

 private:
  TransformationMatrix kBradford = TransformationMatrix(
       0.8951f, -0.7502f,  0.0389f, 0.0f,
       0.2664f,  1.7135f, -0.0685f, 0.0f,
      -0.1614f,  0.0367f,  1.0296f, 0.0f,
       0.0f,     0.0f,     0.0f,    1.0f);

  TransformationMatrix xyzTransform = TransformationMatrix(
      0.41238642f, 0.21263677f, 0.019330615f, 0.0f,
      0.3575915f,  0.715183f,   0.11919712f,  0.0f,
      0.18045056f, 0.07218022f, 0.95037293f,  0.0f,
      0.0f,        0.0f,        0.0f,         1.0f);

  TransformationMatrix transform_ =
      chromaticAdaptation(kBradford, kIlluminantD65, kIlluminantD50) *
      xyzTransform;
  TransformationMatrix inverseTransform_ = transform_.Inverse();
};

class LABColorSpace {
 public:
  // See
  // https://en.wikipedia.org/wiki/CIELAB_color_space#Reverse_transformation.
  FloatPoint3D fromXYZ(const FloatPoint3D& v) const {
    auto f = [](float x) {
      return x > kSigma3 ? pow(x, 1.0f / 3.0f)
                         : x / (3 * kSigma2) + 4.0f / 29.0f;
    };

    float fx = f(v.X() / kIlluminantD50.X());
    float fy = f(v.Y() / kIlluminantD50.Y());
    float fz = f(v.Z() / kIlluminantD50.Z());

    float L = 116.0f * fy - 16.0f;
    float a = 500.0f * (fx - fy);
    float b = 200.0f * (fy - fz);

    return {clamp(L, 0.0f, 100.0f), clamp(a, -128.0f, 128.0f),
            clamp(b, -128.0f, 128.0f)};
  }

  // See
  // https://en.wikipedia.org/wiki/CIELAB_color_space#Forward_transformation.
  FloatPoint3D toXYZ(const FloatPoint3D& lab) const {
    auto invf = [](float x) {
      return x > kSigma ? pow(x, 3) : 3 * kSigma2 * (x - 4.0f / 29.0f);
    };

    FloatPoint3D v = {clamp(lab.X(), 0.0f, 100.0f),
                      clamp(lab.Y(), -128.0f, 128.0f),
                      clamp(lab.Z(), -128.0f, 128.0f)};

    return {
        invf((v.X() + 16.0f) / 116.0f + (v.Y() * 0.002f)) * kIlluminantD50.X(),
        invf((v.X() + 16.0f) / 116.0f) * kIlluminantD50.Y(),
        invf((v.X() + 16.0f) / 116.0f - (v.Z() * 0.005f)) * kIlluminantD50.Z()};
  }

 private:
  static constexpr float kSigma = 6.0f / 29.0f;
  static constexpr float kSigma2 = 36.0f / 841.0f;
  static constexpr float kSigma3 = 216.0f / 24389.0f;
};

class RGBLABTransformer {
 public:
  FloatPoint3D sRGBToLab(const FloatPoint3D& rgb) const {
    FloatPoint3D xyz = rcs.toXYZ(rgb);
    return lcs.fromXYZ(xyz);
  }

  FloatPoint3D LabToSRGB(const FloatPoint3D& lab) const {
    FloatPoint3D xyz = lcs.toXYZ(lab);
    return rcs.fromXYZ(xyz);
  }

 private:
  sRGBColorSpace rcs = sRGBColorSpace();
  LABColorSpace lcs = LABColorSpace();
};

}  // namespace LabColorSpace

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_LAB_COLOR_SPACE_H_
