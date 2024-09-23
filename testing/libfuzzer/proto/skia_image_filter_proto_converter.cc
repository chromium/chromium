// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Converts an Input protobuf Message to a string that can be successfully read
// by SkImageFilter::Deserialize and used as an image filter. The string
// is essentially a valid flattened skia image filter. Note: We will sometimes
// not use the exact values given to us by LPM in cases where those particular
// values cause issues with OOMs and timeouts. Other times, we may write a value
// that isn't exactly the same as the one given to us by LPM, since we may want
// to write invalid values that the proto definition forbids (eg a number that
// is not in enum).  Also note that the skia unflattening code is necessary to
// apply the output of the converter to a canvas, but it isn't the main target
// of the fuzzer. This means that we will generally try to produce output that
// can be applied to a canvas, even if we will consequently be unable to produce
// outputs that allow us to reach paths in the unflattening code (in particular,
// code that handles invalid input). We make this tradeoff because being applied
// to a canvas makes an image filter more likely to cause bugs than if it were
// just deserialized.  Thus, increasing the chance that a filter is applied is
// more important than hitting all paths in unflattening, particularly if those
// paths return nullptr because they've detected an invalid filter. The mutated
// enum values are a case where we knowingly generate output that may not be
// unflattened successfully, which is why we mutate enums relatively
// infrequently.
// Note that since this is a work in progress and skia serialization is a
// moving target, not everything is finished. Many of these parts of the code
// are #defined out if DEVELOPMENT is not defined.

#include "testing/libfuzzer/proto/skia_image_filter_proto_converter.h"

#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/protobuf/src/google/protobuf/descriptor.h"
#include "third_party/protobuf/src/google/protobuf/message.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkRect.h"

using google::protobuf::Descriptor;
using google::protobuf::EnumDescriptor;
using google::protobuf::EnumValueDescriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::Message;
using google::protobuf::Reflection;

namespace skia_image_filter_proto_converter {

// Visit the skia flattenable that is stored on the oneof FIELD field of MSG if
// not flattenable_visited and MSG.has_FIELD. Sets flattenable_visited to true
// if the MSG.FIELD() is visited. Note that `bool flattenable_visited` must be
// defined false in the same context that this macro is used, before it can be
// used.
#define VISIT_ONEOF_FLATTENABLE(MSG, FIELD)                    \
  if (MSG.has_##FIELD() && !IsBlacklisted(#FIELD)) {           \
    CHECK(!flattenable_visited);                               \
    if (PreVisitFlattenable(FieldToFlattenableName(#FIELD))) { \
      Visit(MSG.FIELD());                                      \
      PostVisitFlattenable();                                  \
    }                                                          \
    flattenable_visited = true;                                \
  }

// Visit FIELD if FIELD is set or if no other field on message was visited
// (this should be used at the end of a series of calls to
// VISIT_ONEOF_FLATTENABLE).
// Note FIELD should not be a message that contains itself by default.
// This is used for messages like ImageFilterChild where we must visit one of
// the fields in a oneof. Even though protobuf doesn't mandate that one of these
// be set, we can still visit one of them if they are not set and protobuf will
// return the default values for each field on that message.
#define VISIT_DEFAULT_FLATTENABLE(MSG, FIELD)                  \
  VISIT_ONEOF_FLATTENABLE(MSG, FIELD);                         \
  if (!flattenable_visited) {                                  \
    flattenable_visited = true;                                \
    if (PreVisitFlattenable(FieldToFlattenableName(#FIELD))) { \
      Visit(MSG.FIELD());                                      \
      PostVisitFlattenable();                                  \
    }                                                          \
  }

// Visit FIELD if it is set on MSG, or write a NULL to indicate it is not
// present.
#define VISIT_OPT_OR_NULL(MSG, FIELD) \
  if (MSG.has_##FIELD()) {            \
    Visit(MSG.FIELD());               \
  } else {                            \
    WriteNum(0);                      \
  }

// Call VisitPictureTag on picture_tag.FIELD() if it is set.
#define VISIT_OPT_TAG(FIELD, TAG)              \
  if (picture_tag.has_##FIELD()) {             \
    VisitPictureTag(picture_tag.FIELD(), TAG); \
  }

// Copied from third_party/skia/include/core/SkTypes.h:SkSetFourByteTag.
#define SET_FOUR_BYTE_TAG(A, B, C, D) \
  (((A) << 24) | ((B) << 16) | ((C) << 8) | (D))

// The following enums and constants are copied from various parts of the skia
// codebase.
enum FlatFlags {
  kHasTypeface_FlatFlag = 0x1,
  kHasEffects_FlatFlag = 0x2,
  kFlatFlagMask = 0x3,
};

enum LightType {
  kDistant_LightType,
  kPoint_LightType,
  kSpot_LightType,
};

// Copied from SkVertices.cpp.
using VerticesConstants = int;
constexpr VerticesConstants kMode_Mask = 0x0FF;
constexpr VerticesConstants kHasTexs_Mask = 0x100;
constexpr VerticesConstants kHasColors_Mask = 0x200;

// Copied from SerializationOffsets in SkPath.h. Named PathSerializationOffsets
// to avoid conflicting with PathRefSerializationOffsets. Both enums were named
// SerializationOffsets in skia.
enum PathSerializationOffsets {
  kType_SerializationShift = 28,
  kDirection_SerializationShift = 26,
  kIsVolatile_SerializationShift = 25,
  kConvexity_SerializationShift = 16,
  kFillType_SerializationShift = 8,
};

// Copied from SerializationOffsets in SkPathRef.h. Named
// PathRefSerializationOffsets to avoid conflicting with
// PathSerializationOffsets. Both enums were named SerializationOffsets in skia.
enum PathRefSerializationOffsets {
  kLegacyRRectOrOvalStartIdx_SerializationShift = 28,
  kLegacyRRectOrOvalIsCCW_SerializationShift = 27,
  kLegacyIsRRect_SerializationShift = 26,
  kIsFinite_SerializationShift = 25,
  kLegacyIsOval_SerializationShift = 24,
  kSegmentMask_SerializationShift = 0
};

const uint32_t Converter::kPictEofTag = SET_FOUR_BYTE_TAG('e', 'o', 'f', ' ');

const uint32_t Converter::kProfileLookupTable[] = {
    SET_FOUR_BYTE_TAG('m', 'n', 't', 'r'),
    SET_FOUR_BYTE_TAG('s', 'c', 'n', 'r'),
    SET_FOUR_BYTE_TAG('p', 'r', 't', 'r'),
    SET_FOUR_BYTE_TAG('s', 'p', 'a', 'c'),
};

const uint32_t Converter::kInputColorSpaceLookupTable[] = {
    SET_FOUR_BYTE_TAG('R', 'G', 'B', ' '),
    SET_FOUR_BYTE_TAG('C', 'M', 'Y', 'K'),
    SET_FOUR_BYTE_TAG('G', 'R', 'A', 'Y'),
};

const uint32_t Converter::kPCSLookupTable[] = {
    SET_FOUR_BYTE_TAG('X', 'Y', 'Z', ' '),
    SET_FOUR_BYTE_TAG('L', 'a', 'b', ' '),
};

const uint32_t Converter::kTagLookupTable[] = {
    SET_FOUR_BYTE_TAG('r', 'X', 'Y', 'Z'),
    SET_FOUR_BYTE_TAG('g', 'X', 'Y', 'Z'),
    SET_FOUR_BYTE_TAG('b', 'X', 'Y', 'Z'),
    SET_FOUR_BYTE_TAG('r', 'T', 'R', 'C'),
    SET_FOUR_BYTE_TAG('g', 'T', 'R', 'C'),
    SET_FOUR_BYTE_TAG('b', 'T', 'R', 'C'),
    SET_FOUR_BYTE_TAG('k', 'T', 'R', 'C'),
    SET_FOUR_BYTE_TAG('A', '2', 'B', '0'),
    SET_FOUR_BYTE_TAG('c', 'u', 'r', 'v'),
    SET_FOUR_BYTE_TAG('p', 'a', 'r', 'a'),
    SET_FOUR_BYTE_TAG('m', 'l', 'u', 'c'),
};

const char Converter::kSkPictReaderTag[] = {'r', 'e', 'a', 'd'};
const char Converter::kPictureMagicString[] = {'s', 'k', 'i', 'a',
                                               'p', 'i', 'c', 't'};

const uint8_t Converter::kCountNibBits[] = {0, 1, 1, 2, 1, 2, 2, 3,
                                            1, 2, 2, 3, 2, 3, 3, 4};

// The rest of the Converter attributes are not copied from skia.
const int Converter::kFlattenableDepthLimit = 3;
const int Converter::kColorTableBufferLength = 256;
uint8_t Converter::kColorTableBuffer[kColorTableBufferLength];
const int Converter::kNumBound = 20;
const uint8_t Converter::kMutateEnumDenominator = 40;

// Does not include SkSumPathEffect, SkComposePathEffect or SkRegion
// since they don't use the VISIT FLATTENABLE macros.
const string_map_t Converter::kFieldToFlattenableName = {
    {"path_1d_path_effect", "SkPath1DPathEffect"},
    {"path_2d_path_effect", "SkPath2DPathEffect"},
    {"alpha_threshold_filter_impl", "SkAlphaThresholdFilterImpl"},
    {"arithmetic_image_filter", "SkArithmeticImageFilter"},
    {"blur_image_filter_impl", "SkBlurImageFilterImpl"},
    {"blur_mask_filter_impl", "SkBlurMaskFilterImpl"},
    {"color_4_shader", "SkColor4Shader"},
    {"color_filter_image_filter", "SkColorFilterImageFilter"},
    {"color_filter_shader", "SkColorFilterShader"},
    {"color_matrix_filter_row_major_255", "SkColorMatrixFilterRowMajor255"},
    {"color_shader", "SkColorShader"},
    {"compose_color_filter", "SkComposeColorFilter"},
    {"compose_image_filter", "SkComposeImageFilter"},
    {"compose_shader", "SkComposeShader"},
    {"corner_path_effect", "SkCornerPathEffect"},
    {"dash_impl", "SkDashImpl"},
    {"diffuse_lighting_image_filter", "SkDiffuseLightingImageFilter"},
    {"dilate_image_filter", "SkDilateImageFilter"},
    {"discrete_path_effect", "SkDiscretePathEffect"},
    {"displacement_map_effect", "SkDisplacementMapEffect"},
    {"drop_shadow_image_filter", "SkDropShadowImageFilter"},
    {"emboss_mask_filter", "SkEmbossMaskFilter"},
    {"empty_shader", "SkEmptyShader"},
    {"image_shader", "SkImageShader"},
    {"image_source", "SkImageSource"},
    {"line_2d_path_effect", "SkLine2DPathEffect"},
    {"linear_gradient", "SkLinearGradient"},
    {"local_matrix_image_filter", "SkLocalMatrixImageFilter"},
    {"local_matrix_shader", "SkLocalMatrixShader"},
    {"luma_color_filter", "SkLumaColorFilter"},
    {"magnifier_image_filter", "SkMagnifierImageFilter"},
    {"matrix_convolution_image_filter", "SkMatrixConvolutionImageFilter"},
    {"matrix_image_filter", "SkMatrixImageFilter"},
    {"merge_image_filter", "SkMergeImageFilter"},
    {"mode_color_filter", "SkModeColorFilter"},
    {"offset_image_filter", "SkOffsetImageFilter"},
    {"overdraw_color_filter", "SkOverdrawColorFilter"},
    {"paint_image_filter", "SkPaintImageFilter"},
    {"picture_image_filter", "SkPictureImageFilter"},
    {"picture_shader", "SkPictureShader"},
    {"radial_gradient", "SkRadialGradient"},
    {"specular_lighting_image_filter", "SkSpecularLightingImageFilter"},
    {"sweep_gradient", "SkSweepGradient"},
    {"tile_image_filter", "SkTileImageFilter"},
    {"two_point_conical_gradient", "SkTwoPointConicalGradient"},
    {"xfermode_image_filter", "SkXfermodeImageFilter"},
    {"xfermode_image_filter__base", "SkXfermodeImageFilter_Base"},
    {"srgb_gamma_color_filter", "SkSRGBGammaColorFilter"},
    {"high_contrast__filter", "SkHighContrast_Filter"},
    {"table__color_filter", "SkTable_ColorFilter"},
    {"to_srgb_color_filter", "SkToSRGBColorFilter"},
    {"layer_draw_looper", "SkLayerDrawLooper"},
    {"perlin_noise_shader_impl", "SkPerlinNoiseShaderImpl"},
    {"erode_image_filter", "SkErodeImageFilter"},
};

const std::set<std::string> Converter::kMisbehavedFlattenableBlacklist = {
    "matrix_image_filter",   // Causes OOMs.
    "discrete_path_effect",  // Causes timeouts.
    "path_1d_path_effect",   // Causes timeouts.
};

// We don't care about default values of attributes because Reset() sets them to
// correct values and is called by Convert(), the only important public
// function.
Converter::Converter() {
  CHECK_GT(kMutateEnumDenominator, 2);
}

Converter::~Converter() {}

Converter::Converter(const Converter& other) {}

std::string Converter::FieldToFlattenableName(
    const std::string& field_name) const {
  CHECK(base::Contains(kFieldToFlattenableName, field_name));

  return kFieldToFlattenableName.at(field_name);
}

void Converter::Reset() {
  output_.clear();
  bound_positive_ = false;
  dont_mutate_enum_ = true;
  pair_path_effect_depth_ = 0;
  flattenable_depth_ = 0;
  stroke_style_used_ = false;
  in_compose_color_filter_ = false;
// In production we don't need attributes used by ICC code since it is not
// built for production code.
#ifdef DEVELOPMENT
  tag_offset_ = 0;
  icc_base_ = 0;
#endif  // DEVELOPMENT
}

std::string Converter::Convert(const Input& input) {
  Reset();
  rand_gen_ = std::mt19937(input.rng_seed());
  enum_mutator_chance_distribution_ =
      std::uniform_int_distribution<>(2, kMutateEnumDenominator);

  // This will recursively call Visit on each proto flattenable until all of
  // them are converted to strings and stored in output_.
  Visit(input.image_filter());
  CheckAlignment();
  return std::string(&output_[0], output_.size());
}

void Converter::Visit(const CropRectangle& crop_rectangle) {
  Visit(crop_rectangle.rectangle());
  WriteNum(BoundNum(crop_rectangle.flags()));
}

void Converter::Visit(const Rectangle& rectangle) {
  WriteRectangle(GetValidRectangle(rectangle.left(), rectangle.top(),
                                   rectangle.right(), rectangle.bottom()));
}

std::tuple<float, float, float, float>
Converter::GetValidRectangle(float left, float top, float right, float bottom) {
  bool initial = bound_positive_;
  bound_positive_ = true;
  left = BoundFloat(left);
  top = BoundFloat(top);
  right = BoundFloat(right);
  bottom = BoundFloat(bottom);

  if (right < left)
    right = left;

  if (bottom < top)
    bottom = top;

  // Inspired by SkValidationUtils.h:SkIsValidRect
  CHECK_LE(left, right);
  CHECK_LE(top, bottom);
  CHECK(IsFinite(right - left));
  CHECK(IsFinite(bottom - top));
  bound_positive_ = initial;
  return std::make_tuple(left, top, right, bottom);
}

std::tuple<int32_t, int32_t, int32_t, int32_t> Converter::GetValidIRect(
    int32_t left,
    int32_t top,
    int32_t right,
    int32_t bottom) {
  auto float_rectangle = GetValidRectangle(left, top, right, bottom);
  return std::make_tuple(static_cast<int32_t>(std::get<0>(float_rectangle)),
                         static_cast<int32_t>(std::get<1>(float_rectangle)),
                         static_cast<int32_t>(std::get<2>(float_rectangle)),
                         static_cast<int32_t>(std::get<3>(float_rectangle)));
}

template <typename T>
void Converter::WriteRectangle(std::tuple<T, T, T, T> rectangle) {
  WriteNum(std::get<0>(rectangle));
  WriteNum(std::get<1>(rectangle));
  WriteNum(std::get<2>(rectangle));
  WriteNum(std::get<3>(rectangle));
}

void Converter::Visit(const LightChild& light_child) {
  if (light_child.has_point_light())
    Visit(light_child.point_light());
  else if (light_child.has_spot_light())
    Visit(light_child.spot_light());
  else
    Visit(light_child.distant_light());
}

void Converter::Visit(const LightParent& light_parent) {
  if (light_parent.light_child().has_point_light())
    WriteNum(kPoint_LightType);
  else if (light_parent.light_child().has_spot_light())
    WriteNum(kSpot_LightType);
  else  // Assume we have distant light
    WriteNum(kDistant_LightType);
  Visit(light_parent.color());
  Visit(light_parent.light_child());
}

void Converter::Visit(const ImageFilterChild& image_filter_child) {
  bool flattenable_visited = false;
  VISIT_ONEOF_FLATTENABLE(image_filter_child, specular_lighting_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, matrix_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, arithmetic_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, alpha_threshold_filter_impl);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, blur_image_filter_impl);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, color_filter_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, compose_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, displacement_map_effect);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, drop_shadow_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, local_matrix_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, magnifier_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, matrix_convolution_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, merge_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, dilate_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, erode_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, offset_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, picture_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, tile_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, xfermode_image_filter__base);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, xfermode_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, diffuse_lighting_image_filter);
  VISIT_ONEOF_FLATTENABLE(image_filter_child, image_source);
  VISIT_DEFAULT_FLATTENABLE(image_filter_child, paint_image_filter);
}

void Converter::Visit(
    const DiffuseLightingImageFilter& diffuse_lighting_image_filter) {
  Visit(diffuse_lighting_image_filter.parent(), 1);
  Visit(diffuse_lighting_image_filter.light());
  WriteNum(diffuse_lighting_image_filter.surface_scale());
  // Can't be negative, see:
  // https://www.w3.org/TR/SVG/filters.html#feDiffuseLightingElement
  const float kd = fabs(BoundFloat(diffuse_lighting_image_filter.kd()));
  WriteNum(kd);
}

void Converter::Visit(const XfermodeImageFilter& xfermode_image_filter) {
  Visit(xfermode_image_filter.parent(), 2);
  WriteNum(xfermode_image_filter.mode());
}

void Converter::Visit(
    const XfermodeImageFilter_Base& xfermode_image_filter__base) {
  Visit(xfermode_image_filter__base.parent(), 2);
  WriteNum(xfermode_image_filter__base.mode());
}

void Converter::Visit(const TileImageFilter& tile_image_filter) {
  Visit(tile_image_filter.parent(), 1);
  Visit(tile_image_filter.src());
  Visit(tile_image_filter.dst());
}

void Converter::Visit(const OffsetImageFilter& offset_image_filter) {
  Visit(offset_image_filter.parent(), 1);
  Visit(offset_image_filter.offset());
}

void Converter::Visit(const HighContrast_Filter& high_contrast__filter) {
  WriteFields(high_contrast__filter, 1, 2);
  // Use contrast as a seed.
  WriteNum(GetRandomFloat(high_contrast__filter.contrast(), -1.0, 1.0));
}

void Converter::Visit(const MergeImageFilter& merge_image_filter) {
  Visit(merge_image_filter.parent(), merge_image_filter.parent().inputs_size());
}

void Converter::Visit(const ErodeImageFilter& erode_image_filter) {
  Visit(erode_image_filter.parent(), 1);
  bool initial = bound_positive_;
  bound_positive_ = true;
  WriteFields(erode_image_filter, 2);
  bound_positive_ = initial;
}

template <typename T>
T Converter::BoundNum(T num, int upper_bound) const {
  if (bound_positive_)
    num = Abs(num);
  if (num >= 0) {
    return num % upper_bound;
  } else {
    // Don't let negative numbers be too negative.
    return num % -upper_bound;
  }
}

template <typename T>
T Converter::BoundNum(T num) {
  return BoundNum(num, kNumBound);
}

float Converter::BoundFloat(float num) {
  return BoundFloat(num, kNumBound);
}

float Converter::BoundFloat(float num, const float num_bound) {
  // Don't allow nans infs, they can cause OOMs.
  if (!IsFinite(num))
    num = GetRandomFloat(&rand_gen_);

  float result;
  if (num >= 0)
    result = fmod(num, num_bound);
  else if (bound_positive_)
    result = fmod(fabsf(num), num_bound);
  else
    // Bound negative numbers.
    result = fmod(num, -num_bound);
  if (!IsFinite(result))
    return BoundFloat(num);
  return result;
}

void Converter::Visit(const DilateImageFilter& dilate_image_filter) {
  Visit(dilate_image_filter.parent(), 1);
  // Make sure WriteFields writes positive values for width and height.
  // Save the value of bound_positive_ and restore it after WriteFields
  // returns.
  bool initial_bound_positive = bound_positive_;
  bound_positive_ = true;
  WriteFields(dilate_image_filter, 2);
  bound_positive_ = initial_bound_positive;
}

void Converter::Visit(
    const MatrixConvolutionImageFilter& matrix_convolution_image_filter) {
  Visit(matrix_convolution_image_filter.parent(), 1);
  // Avoid timeouts from having to generate too many random numbers.
  // TODO(metzman): actually calculate the limit based on this bound (eg 31 x 1
  // probably doesn't need to be bounded).
  const int upper_bound = 30;

  // Use 2 instead of 1 to avoid FPEs in BoundNum.
  int32_t width = std::max(
      2, BoundNum(Abs(matrix_convolution_image_filter.width()), upper_bound));

  WriteNum(width);

  int32_t height = std::max(
      2, BoundNum(Abs(matrix_convolution_image_filter.height()), upper_bound));

  WriteNum(height);

  std::mt19937 rand_gen(matrix_convolution_image_filter.kernel_seed());
  const uint32_t kernel_size = width * height;
  WriteNum(kernel_size);
  // Use rand_gen to ensure we have a large enough kernel.
  for (uint32_t kernel_counter = 0; kernel_counter < kernel_size;
       kernel_counter++) {
    float kernel_element = GetRandomFloat(&rand_gen);
    WriteNum(kernel_element);
  }
  WriteFields(matrix_convolution_image_filter, 5, 6);

  const uint32_t offset_x =
      std::max(0, matrix_convolution_image_filter.offset_x());

  const uint32_t offset_y =
      std::max(0, matrix_convolution_image_filter.offset_y());

  WriteNum(BoundNum(offset_x, width - 1));
  WriteNum(BoundNum(offset_y, height - 1));
  WriteFields(matrix_convolution_image_filter, 9);
}

void Converter::Visit(const MagnifierImageFilter& magnifier_image_filter) {
  Visit(magnifier_image_filter.parent(), 1);
  Visit(magnifier_image_filter.src());
  const float inset = fabs(BoundFloat(magnifier_image_filter.inset()));
  CHECK(IsFinite(inset));
  WriteNum(inset);
}

void Converter::Visit(const LocalMatrixImageFilter& local_matrix_image_filter) {
  // TODO(metzman): Make it so that deserialization always succeeds by ensuring
  // the type isn't kAffine_Mask or KPerspectiveMask (see constructor for
  // SkLocalMatrixImageFilter).
  Visit(local_matrix_image_filter.parent(), 1);
  Visit(local_matrix_image_filter.matrix(), true);
}

void Converter::Visit(const ImageSource& image_source) {
  WriteNum(image_source.filter_quality());
  auto src_rect = GetValidRectangle(
      image_source.src().left(), image_source.src().top(),
      image_source.src().right(), image_source.src().bottom());

  // See SkImageSource::Make for why we mandate width and height be at least
  // .01.  This is such a small difference that we won't bother bounding again.
  float left = std::get<0>(src_rect);
  float* right = &std::get<2>(src_rect);
  if ((*right - left) <= 0.0f)
    *right += .01;

  float top = std::get<1>(src_rect);
  float* bottom = &std::get<3>(src_rect);
  if ((*bottom - top) <= 0.0f)
    *bottom += .01;

  WriteRectangle(src_rect);
  Visit(image_source.dst());
  Visit(image_source.image());
}

void Converter::Visit(const DropShadowImageFilter& drop_shadow_image_filter) {
  Visit(drop_shadow_image_filter.parent(), 1);
  WriteFields(drop_shadow_image_filter, 2);
}

void Converter::Visit(const DisplacementMapEffect& displacement_map_effect) {
  Visit(displacement_map_effect.parent(), 2);
  bool initial = dont_mutate_enum_;
  dont_mutate_enum_ = true;
  WriteFields(displacement_map_effect, 2);
  dont_mutate_enum_ = initial;
}

void Converter::Visit(const ComposeImageFilter& compose_image_filter) {
  Visit(compose_image_filter.parent(), 2);
}

void Converter::Visit(const ColorFilterImageFilter& color_filter_image_filter) {
  Visit(color_filter_image_filter.parent(), 1);
  Visit(color_filter_image_filter.color_filter());
}

void Converter::Visit(const BlurImageFilterImpl& blur_image_filter_impl) {
  Visit(blur_image_filter_impl.parent(), 1);
  WriteFields(blur_image_filter_impl, 2);
}

void Converter::Visit(
    const AlphaThresholdFilterImpl& alpha_threshold_filter_impl) {
  Visit(alpha_threshold_filter_impl.parent(), 1);
  WriteFields(alpha_threshold_filter_impl, 2, 3);
  Visit(alpha_threshold_filter_impl.rgn());
}

std::tuple<int32_t, int32_t, int32_t, int32_t> Converter::WriteNonEmptyIRect(
    const IRect& irect) {
  // Make sure bounds do not specify an empty rectangle.
  // See SkRect.h:202
  auto rectangle =
      GetValidIRect(irect.left(), irect.top(), irect.right(), irect.bottom());

  // Ensure top and right are greater than left and top.
  if (irect.left() >= irect.right() || irect.top() >= irect.bottom()) {
    std::get<2>(rectangle) = std::get<0>(rectangle) + 1;
    std::get<3>(rectangle) = std::get<1>(rectangle) + 1;
  }
  WriteRectangle(rectangle);
  return rectangle;
}

void Converter::Visit(const Region& region) {
  // Write simple region.
  WriteNum(0);
  WriteNonEmptyIRect(region.bounds());

// Complex regions are not finished.
#ifdef DEVELOPMENT
  enum { kRunTypeSentinel = 0x7FFFFFFF };
  auto rectangle = WriteNonEmptyIRect(region.bounds());
  const int32_t bound_left = std::get<0>(rectangle);
  const int32_t bound_top = std::get<1>(rectangle);
  const int32_t bound_right = std::get<2>(rectangle);
  const int32_t bound_bottom = std::get<3>(rectangle);

  const int32_t y_span_count =
      BoundNum(std::max(1, Abs(region.y_span_count())));

  const int32_t interval_count = BoundNum(std::max(1, Abs(region.interval_())));

  WriteNum(run_count);
  WriteNum(y_span_count);
  WriteNum(interval_count);

  // See SkRegion::validate_run.
  // Really this is two less, but we will write the two sentinels
  ourselves const int32_t run_count = 3 * y_span_count + 2 * interval_count;
  CHECK(run_count >= 7);

  WriteNum(run_count + 2);
  // Write runs.

  // Write top.
  Write(bound_top);

  WriteNum(kRunTypeSentinel);
  WriteNum(kRunTypeSentinel);
#endif  // DEVELOPMENT
}

void Converter::Visit(const PictureInfo& picture_info) {
  WriteArray(kPictureMagicString, sizeof(kPictureMagicString));
  WriteNum(picture_info.version());
  Visit(picture_info.rectangle());
  if (picture_info.version() < PictureInfo::kRemoveHeaderFlags_Version)
    WriteNum(picture_info.flags());
}

void Converter::Visit(const ImageFilterParent& image_filter,
                      const int num_inputs_required) {
  CHECK_GE(num_inputs_required, 0);
  if (!num_inputs_required) {
    WriteNum(0);
  } else {
    WriteNum(num_inputs_required);
    WriteBool(true);
    Visit(image_filter.default_input());
    int num_inputs = 1;
    for (const auto& input : image_filter.inputs()) {
      if (num_inputs++ >= num_inputs_required)
        break;
      WriteBool(true);
      Visit(input);
    }
    for (; num_inputs < num_inputs_required; num_inputs++) {
      // Copy default_input until we have enough.
      WriteBool(true);
      Visit(image_filter.default_input());
    }
  }
  Visit(image_filter.crop_rectangle());
}

void Converter::Visit(const ArithmeticImageFilter& arithmetic_image_filter) {
  Visit(arithmetic_image_filter.parent(), 2);

  // This is field is ignored, but write kSrcOver (3) as the flattening code
  // does.
  // TODO(metzman): change to enum value (SkBlendMode::kSrcOver) when it
  // is uncommented, for now just write, its value: 3.
  WriteNum(3);

  WriteFields(arithmetic_image_filter, 2);
}

void Converter::Visit(
    const SpecularLightingImageFilter& specular_lighting_image_filter) {
  Visit(specular_lighting_image_filter.image_filter_parent(), 1);
  Visit(specular_lighting_image_filter.light());
  WriteNum(BoundFloat(specular_lighting_image_filter.surface_scale()) * 255);
  WriteNum(fabs(BoundFloat(specular_lighting_image_filter.ks())));
  WriteNum(BoundFloat(specular_lighting_image_filter.shininess()));
}

void Converter::RecordSize() {
  // Reserve space to overwrite when we are done writing whatever size we are
  // recording.
  WriteNum(0);
  start_sizes_.push_back(output_.size());
}

size_t Converter::PopStartSize() {
  CHECK_GT(start_sizes_.size(), static_cast<size_t>(0));
  const size_t back = start_sizes_.back();
  start_sizes_.pop_back();
  return back;
}

template <typename T>
void Converter::WriteNum(const T num) {
  if (sizeof(T) > 4) {
    CHECK(num <= UINT32_MAX);
    uint32_t four_byte_num = static_cast<uint32_t>(num);
    char num_arr[sizeof(four_byte_num)];
    memcpy(num_arr, &four_byte_num, sizeof(four_byte_num));
    for (size_t idx = 0; idx < sizeof(four_byte_num); idx++)
      output_.push_back(num_arr[idx]);
    return;
  }
  char num_arr[sizeof(T)];
  memcpy(num_arr, &num, sizeof(T));
  for (size_t idx = 0; idx < sizeof(T); idx++)
    output_.push_back(num_arr[idx]);
}

void Converter::InsertSize(const size_t size, const uint32_t position) {
  char size_arr[sizeof(uint32_t)];
  memcpy(size_arr, &size, sizeof(uint32_t));

  for (size_t idx = 0; idx < sizeof(uint32_t); idx++) {
    const size_t output__idx = position + idx - sizeof(uint32_t);
    CHECK_LT(output__idx, output_.size());
    output_[output__idx] = size_arr[idx];
  }
}

void Converter::WriteBytesWritten() {
  const size_t start_size = PopStartSize();
  CHECK_LT(start_size, std::numeric_limits<uint32_t>::max());
  const size_t end_size = output_.size();
  CHECK_LE(start_size, end_size);
  const size_t bytes_written = end_size - start_size;
  CHECK_LT(bytes_written, std::numeric_limits<uint32_t>::max());
  InsertSize(bytes_written, start_size);
}

void Converter::WriteString(const std::string str) {
  WriteNum(str.size());
  const char* c_str = str.c_str();
  for (size_t idx = 0; idx < str.size(); idx++)
    output_.push_back(c_str[idx]);

  output_.push_back('\0');  // Add trailing NULL.

  Pad(str.size() + 1);
}

void Converter::WriteArray(
    const google::protobuf::RepeatedField<uint32_t>& repeated_field,
    const size_t size) {
  WriteNum(size * sizeof(uint32_t));  // Array size.
  for (uint32_t element : repeated_field)
    WriteNum(element);
  // Padding is not a concern because uint32_ts are 4 bytes.
}

void Converter::WriteArray(const char* arr, const size_t size) {
  WriteNum(size);
  for (size_t idx = 0; idx < size; idx++)
    output_.push_back(arr[idx]);

  for (unsigned idx = 0; idx < size % 4; idx++)
    output_.push_back('\0');
}

void Converter::WriteBool(const bool bool_val) {
  // bools are usually written as 32 bit integers in skia flattening.
  WriteNum(static_cast<uint32_t>(bool_val));
}

void Converter::WriteNum(const char (&num_arr)[4]) {
  for (size_t idx = 0; idx < 4; idx++)
    output_.push_back(num_arr[idx]);
}

void Converter::Visit(const PictureShader& picture_shader) {
  // PictureShader cannot be autovisited because matrix cannot be.
  Visit(picture_shader.matrix());
  WriteFields(picture_shader, 2, 3);
  Visit(picture_shader.rect());
  WriteBool(false);
}

void Converter::Visit(const Message& msg) {
  WriteFields(msg);
}

// Visit the Message elements of repeated_field, using the type-specific Visit
// methods (thanks to templating).
template <class T>
void Converter::Visit(
    const google::protobuf::RepeatedPtrField<T>& repeated_field) {
  for (const T& single_field : repeated_field)
    Visit(single_field);
}

void Converter::Visit(const PictureImageFilter& picture_image_filter) {
  WriteBool(picture_image_filter.has_picture());
  if (picture_image_filter.has_picture())
    Visit(picture_image_filter.picture());
  // Allow 0x0 rectangles to sometimes be written even though it will mess up
  // make_localspace_filter.
  Visit(picture_image_filter.crop_rectangle());
  if (picture_image_filter.has_picture()) {
    if (picture_image_filter.picture().info().version() <
        PictureInfo::kRemoveHeaderFlags_Version)

      WriteNum(picture_image_filter.resolution());
  }
}

void Converter::Visit(const PictureData& picture_data) {
  for (auto& tag : picture_data.tags()) {
    Visit(tag);
  }
  Visit(picture_data.reader_tag());

  WriteNum(kPictEofTag);
}

void Converter::VisitPictureTag(const PaintPictureTag& paint_picture_tag,
                                uint32_t tag) {
  WriteNum(tag);
  WriteNum(1);  // Size.
  Visit(paint_picture_tag.paint());
}

void Converter::VisitPictureTag(const PathPictureTag& path_picture_tag,
                                uint32_t tag) {
  WriteNum(tag);
  WriteNum(1);  // Size.
  WriteNum(1);  // Count.
  Visit(path_picture_tag.path());
}

template <class T>
void Converter::VisitPictureTag(const T& picture_tag_child, uint32_t tag) {
  WriteNum(tag);
  WriteNum(1);
  Visit(picture_tag_child);
}

void Converter::Visit(const ReaderPictureTag& reader) {
  WriteNum(SET_FOUR_BYTE_TAG('r', 'e', 'a', 'd'));
  const uint32_t size = sizeof(uint32_t) * (1 + reader.later_bytes_size());
  WriteNum(size);
  WriteNum(size);
  WriteNum(reader.first_bytes());
  for (auto bytes : reader.later_bytes())
    WriteNum(bytes);
}

// Copied from SkPaint.cpp.
static uint32_t pack_4(unsigned a, unsigned b, unsigned c, unsigned d) {
  CHECK_EQ(a, (uint8_t)a);
  CHECK_EQ(b, (uint8_t)b);
  CHECK_EQ(c, (uint8_t)c);
  CHECK_EQ(d, (uint8_t)d);
  return (a << 24) | (b << 16) | (c << 8) | d;
}

// Copied from SkPaint.cpp.
static uint32_t pack_paint_flags(unsigned flags,
                                 unsigned hint,
                                 unsigned align,
                                 unsigned filter,
                                 unsigned flatFlags) {
  // left-align the fields of "known" size, and right-align the last (flatFlags)
  // so it can easily add more bits in the future.
  return (flags << 16) | (hint << 14) | (align << 12) | (filter << 10) |
         flatFlags;
}

bool Converter::IsFinite(float num) const {
  // If num is inf, -inf, nan or -nan then num*0 will be nan.
  return !std::isnan(num * 0);
}

void Converter::Visit(const Paint& paint) {
  WriteFields(paint, 1, 6);

  uint8_t flat_flags = 0;
  if (paint.has_effects())
    flat_flags |= kHasEffects_FlatFlag;

  WriteNum(pack_paint_flags(paint.flags(), paint.hinting(), paint.align(),
                            paint.filter_quality(), flat_flags));

  int style = paint.style();
  Paint::StrokeCap stroke_cap = paint.stroke_cap();

  if (stroke_style_used_) {
    style = Paint::kFill_Style;
  } else if (style == Paint::kStroke_Style) {
    stroke_style_used_ = true;
    // Avoid timeouts.
    stroke_cap = Paint::kButt_Cap;
  }

  uint32_t tmp =
      pack_4(stroke_cap, paint.stroke_join(),
             (style << 4) | paint.text_encoding(), paint.blend_mode());

  WriteNum(tmp);  // See https://goo.gl/nYJfTy

  if (paint.has_effects())
    Visit(paint.effects());
}

void Converter::Visit(const PaintEffects& paint_effects) {
  // There should be a NULL written for every paint_effects field that is not
  // set.
  VISIT_OPT_OR_NULL(paint_effects, path_effect);
  VISIT_OPT_OR_NULL(paint_effects, shader);
  VISIT_OPT_OR_NULL(paint_effects, mask_filter);
  VISIT_OPT_OR_NULL(paint_effects, color_filter);
  WriteNum(0);  // Write ignored number where rasterizer used to be.
  VISIT_OPT_OR_NULL(paint_effects, looper);
  VISIT_OPT_OR_NULL(paint_effects, image_filter);
}

void Converter::Visit(const ColorFilterChild& color_filter_child) {
  bool flattenable_visited = false;
  VISIT_ONEOF_FLATTENABLE(color_filter_child,
                          color_matrix_filter_row_major_255);

  if (!in_compose_color_filter_)
    VISIT_ONEOF_FLATTENABLE(color_filter_child, compose_color_filter);

  VISIT_ONEOF_FLATTENABLE(color_filter_child, srgb_gamma_color_filter);
  VISIT_ONEOF_FLATTENABLE(color_filter_child, high_contrast__filter);
  VISIT_ONEOF_FLATTENABLE(color_filter_child, luma_color_filter);
  VISIT_ONEOF_FLATTENABLE(color_filter_child, overdraw_color_filter);
  VISIT_ONEOF_FLATTENABLE(color_filter_child, table__color_filter);
  VISIT_ONEOF_FLATTENABLE(color_filter_child, to_srgb_color_filter);
  VISIT_DEFAULT_FLATTENABLE(color_filter_child, mode_color_filter);
}

void Converter::Visit(const Color4f& color_4f) {
  WriteFields(color_4f);
}

void Converter::Visit(const GradientDescriptor& gradient_descriptor) {
  // See SkGradientShaderBase::Descriptor::flatten in SkGradientShader.cpp.
  enum GradientSerializationFlags {
    // Bits 29:31 used for various boolean flags
    kHasPosition_GSF = 0x80000000,
    kHasLocalMatrix_GSF = 0x40000000,
    kHasColorSpace_GSF = 0x20000000,

    // Bits 12:28 unused

    // Bits 8:11 for fTileMode
    kTileModeShift_GSF = 8,
    kTileModeMask_GSF = 0xF,

    // Bits 0:7 for fGradFlags (note that kForce4fContext_PrivateFlag is 0x80)
    kGradFlagsShift_GSF = 0,
    kGradFlagsMask_GSF = 0xFF,
  };

  uint32_t flags = 0;
  if (gradient_descriptor.has_pos())
    flags |= kHasPosition_GSF;
  if (gradient_descriptor.has_local_matrix())
    flags |= kHasLocalMatrix_GSF;
  if (gradient_descriptor.has_color_space())
    flags |= kHasColorSpace_GSF;
  flags |= (gradient_descriptor.tile_mode() << kTileModeShift_GSF);
  uint32_t grad_flags =
      (gradient_descriptor.grad_flags() % (kGradFlagsMask_GSF + 1));
  CHECK_LE(grad_flags, static_cast<uint32_t>(kGradFlagsMask_GSF));
  WriteNum(flags);

  const uint32_t count = gradient_descriptor.colors_size();

  WriteNum(count);
  for (auto& color : gradient_descriptor.colors())
    Visit(color);

  Visit(gradient_descriptor.color_space());

  WriteNum(count);
  for (uint32_t counter = 0; counter < count; counter++)
    WriteNum(gradient_descriptor.pos());

  Visit(gradient_descriptor.local_matrix());
}

void Converter::Visit(const GradientParent& gradient_parent) {
  Visit(gradient_parent.gradient_descriptor());
}

void Converter::Visit(const ToSRGBColorFilter& to_srgb_color_filter) {
  Visit(to_srgb_color_filter.color_space());
}

void Converter::Visit(const LooperChild& looper) {
  if (PreVisitFlattenable("SkLayerDrawLooper")) {
    Visit(looper.layer_draw_looper());
    PostVisitFlattenable();
  }
}

// Copied from SkPackBits.cpp.
static uint8_t* flush_diff8(uint8_t* dst, const uint8_t* src, size_t count) {
  while (count > 0) {
    size_t n = count > 128 ? 128 : count;
    *dst++ = (uint8_t)(n + 127);
    memcpy(dst, src, n);
    src += n;
    dst += n;
    count -= n;
  }
  return dst;
}

// Copied from SkPackBits.cpp.
static uint8_t* flush_same8(uint8_t dst[], uint8_t value, size_t count) {
  while (count > 0) {
    size_t n = count > 128 ? 128 : count;
    *dst++ = (uint8_t)(n - 1);
    *dst++ = (uint8_t)value;
    count -= n;
  }
  return dst;
}

// Copied from SkPackBits.cpp.
static size_t compute_max_size8(size_t srcSize) {
  // Worst case is the number of 8bit values + 1 byte per (up to) 128 entries.
  return ((srcSize + 127) >> 7) + srcSize;
}

// Copied from SkPackBits.cpp.
static size_t pack8(const uint8_t* src,
                    size_t srcSize,
                    uint8_t* dst,
                    size_t dstSize) {
  if (dstSize < compute_max_size8(srcSize)) {
    return 0;
  }

  uint8_t* const origDst = dst;
  const uint8_t* stop = src + srcSize;

  for (intptr_t count = stop - src; count > 0; count = stop - src) {
    if (1 == count) {
      *dst++ = 0;
      *dst++ = *src;
      break;
    }

    unsigned value = *src;
    const uint8_t* s = src + 1;

    if (*s == value) {  // accumulate same values...
      do {
        s++;
        if (s == stop) {
          break;
        }
      } while (*s == value);
      dst = flush_same8(dst, value, (size_t)(s - src));
    } else {  // accumulate diff values...
      do {
        if (++s == stop) {
          goto FLUSH_DIFF;
        }
        // only stop if we hit 3 in a row,
        // otherwise we get bigger than compuatemax
      } while (*s != s[-1] || s[-1] != s[-2]);
      s -= 2;  // back up so we don't grab the "same" values that follow
    FLUSH_DIFF:
      dst = flush_diff8(dst, src, (size_t)(s - src));
    }
    src = s;
  }
  return dst - origDst;
}

const uint8_t* Converter::ColorTableToArray(const ColorTable& color_table) {
  float* dst = reinterpret_cast<float*>(kColorTableBuffer);
  const int array_size = 64;
  // Now write the 256 fields.
  const Descriptor* descriptor = color_table.GetDescriptor();
  CHECK(descriptor);
  const Reflection* reflection = color_table.GetReflection();
  CHECK(reflection);
  for (int field_num = 1; field_num <= array_size; field_num++, dst++) {
    const FieldDescriptor* field_descriptor =
        descriptor->FindFieldByNumber(field_num);
    CHECK(field_descriptor);
    *dst = BoundFloat(reflection->GetFloat(color_table, field_descriptor));
  }
  return kColorTableBuffer;
}

void Converter::Visit(const Table_ColorFilter& table__color_filter) {
  // See SkTable_ColorFilter::SkTable_ColorFilter
  enum {
    kA_Flag = 1 << 0,
    kR_Flag = 1 << 1,
    kG_Flag = 1 << 2,
    kB_Flag = 1 << 3,
  };
  unsigned flags = 0;
  uint8_t f_storage[4 * kColorTableBufferLength];
  uint8_t* dst = f_storage;

  if (table__color_filter.has_table_a()) {
    memcpy(dst, ColorTableToArray(table__color_filter.table_a()),
           kColorTableBufferLength);

    dst += kColorTableBufferLength;
    flags |= kA_Flag;
  }
  if (table__color_filter.has_table_r()) {
    memcpy(dst, ColorTableToArray(table__color_filter.table_r()),
           kColorTableBufferLength);

    dst += kColorTableBufferLength;
    flags |= kR_Flag;
  }
  if (table__color_filter.has_table_g()) {
    memcpy(dst, ColorTableToArray(table__color_filter.table_g()),
           kColorTableBufferLength);

    dst += kColorTableBufferLength;
    flags |= kG_Flag;
  }
  if (table__color_filter.has_table_b()) {
    memcpy(dst, ColorTableToArray(table__color_filter.table_b()),
           kColorTableBufferLength);

    dst += kColorTableBufferLength;
    flags |= kB_Flag;
  }
  uint8_t storage[5 * kColorTableBufferLength];
  const int count = kCountNibBits[flags & 0xF];
  const size_t size = pack8(f_storage, count * kColorTableBufferLength, storage,
                            sizeof(storage));

  CHECK_LE(flags, UINT32_MAX);
  const uint32_t flags_32 = (uint32_t)flags;
  WriteNum(flags_32);
  WriteNum((uint32_t)size);
  for (size_t idx = 0; idx < size; idx++)
    output_.push_back(storage[idx]);
  Pad(output_.size());
}

void Converter::Visit(const ComposeColorFilter& compose_color_filter) {
  CHECK(!in_compose_color_filter_);
  in_compose_color_filter_ = true;
  Visit(compose_color_filter.outer());
  Visit(compose_color_filter.inner());
  in_compose_color_filter_ = false;
}

void Converter::Visit(const OverdrawColorFilter& overdraw_color_filter) {
  // This is written as a byte array (length-in-bytes followed by data).
  const uint32_t num_fields = 6;
  const uint32_t arr_size = num_fields * sizeof(uint32_t);
  WriteNum(arr_size);
  WriteFields(overdraw_color_filter);
}

void Converter::Visit(
    const ColorMatrixFilterRowMajor255& color_matrix_filter_row_major_255) {
  Visit(color_matrix_filter_row_major_255.color_filter_matrix());
}

void Converter::Visit(const ColorFilterMatrix& color_filter_matrix) {
  static const int kColorFilterMatrixNumFields = 20;
  WriteNum(kColorFilterMatrixNumFields);
  WriteFields(color_filter_matrix);
}

void Converter::Visit(const LayerDrawLooper& layer_draw_looper) {
  WriteNum(layer_draw_looper.layer_infos_size());
  int n = layer_draw_looper.layer_infos_size();
#ifdef AVOID_MISBEHAVIOR
  n = std::min(n, 1);  // Write at most 1 to avoid timeouts.
#endif
  for (int i = 0; i < n; ++i)
    Visit(layer_draw_looper.layer_infos(i));
}

void Converter::Visit(const LayerInfo& layer_info) {
  WriteNum(0);
  // Don't mutate these enum values or else a crash will be caused
  bool initial = dont_mutate_enum_;
  dont_mutate_enum_ = true;
  WriteFields(layer_info, 1, 4);
  dont_mutate_enum_ = initial;
  Visit(layer_info.paint());
}

void Converter::Visit(const PairPathEffect& pair) {
  // Don't allow nesting of PairPathEffects for performance reasons
  if (pair_path_effect_depth_ >= 1)
    return;
  if (flattenable_depth_ > kFlattenableDepthLimit)
    return;
  pair_path_effect_depth_ += 1;
  flattenable_depth_ += 1;

  std::string name;
  if (pair.type() == PairPathEffect::SUM)
    name = "SkSumPathEffect";
  else
    name = "SkComposePathEffect";
  WriteString(name);
  RecordSize();

  Visit(pair.path_effect_1());
  Visit(pair.path_effect_2());

  WriteBytesWritten();  // Flattenable size.
  CheckAlignment();
  pair_path_effect_depth_ -= 1;
  flattenable_depth_ -= 1;
}

// See SkPathRef::writeToBuffer
void Converter::Visit(const PathRef& path_ref) {
  // Bound segment_mask to avoid timeouts and for proper behavior.
  const int32_t packed =
      (((path_ref.is_finite() & 1) << kIsFinite_SerializationShift) |
       (ToUInt8(path_ref.segment_mask()) << kSegmentMask_SerializationShift));

  WriteNum(packed);
  WriteNum(0);
  std::vector<SkPoint> points;
  if (path_ref.verbs_size()) {
    WriteNum(path_ref.verbs_size() + 1);
    uint32_t num_points = 1;  // The last move will add 1 point.
    uint32_t num_conics = 0;
    for (auto& verb : path_ref.verbs()) {
      switch (verb.value()) {
        case ValidVerb::kMove_Verb:
        case ValidVerb::kLine_Verb:
          num_points += 1;
          break;
        case ValidVerb::kConic_Verb:
          num_conics += 1;
          [[fallthrough]];
        case ValidVerb::kQuad_Verb:
          num_points += 2;
          break;
        case ValidVerb::kCubic_Verb:
          num_points += 3;
          break;
        case ValidVerb::kClose_Verb:
          break;
        default:
          NOTREACHED();
      }
    }
    WriteNum(num_points);
    WriteNum(num_conics);
  } else {
    WriteNum(0);
    WriteNum(0);
    WriteNum(0);
  }

  for (auto& verb : path_ref.verbs()) {
    const uint8_t value = verb.value();
    WriteNum(value);
  }
  // Verbs must start (they are written backwards) with kMove_Verb (0).
  if (path_ref.verbs_size()) {
    uint8_t value = ValidVerb::kMove_Verb;
    WriteNum(value);
  }

  // Write points
  for (auto& verb : path_ref.verbs()) {
    switch (verb.value()) {
      case ValidVerb::kMove_Verb:
      case ValidVerb::kLine_Verb: {
        Visit(verb.point1());
        AppendAsSkPoint(points, verb.point1());
        break;
      }
      case ValidVerb::kConic_Verb:
      case ValidVerb::kQuad_Verb: {
        Visit(verb.point1());
        Visit(verb.point2());
        AppendAsSkPoint(points, verb.point1());
        AppendAsSkPoint(points, verb.point2());
        break;
      }
      case ValidVerb::kCubic_Verb:
        Visit(verb.point1());
        Visit(verb.point2());
        Visit(verb.point3());
        AppendAsSkPoint(points, verb.point1());
        AppendAsSkPoint(points, verb.point2());
        AppendAsSkPoint(points, verb.point3());
        break;
      default:
        break;
    }
  }
  // Write point of the Move Verb we put at the end.
  if (path_ref.verbs_size()) {
    Visit(path_ref.first_verb().point1());
    AppendAsSkPoint(points, path_ref.first_verb().point1());
  }

  // Write conic weights.
  for (auto& verb : path_ref.verbs()) {
    if (verb.value() == ValidVerb::kConic_Verb)
      WriteNum(verb.conic_weight());
  }

  SkRect skrect;
  if (!points.empty()) {
    // Calling `setBoundsCheck()` with an empty array would set `skrect` to the
    // empty rectangle, which it already is after default construction.
    skrect.setBoundsCheck(points.data(), points.size());
  }

  WriteNum(skrect.fLeft);
  WriteNum(skrect.fTop);
  WriteNum(skrect.fRight);
  WriteNum(skrect.fBottom);
}

void Converter::AppendAsSkPoint(std::vector<SkPoint>& sk_points,
                                const Point& proto_point) const {
  SkPoint sk_point;
  sk_point.fX = proto_point.x();
  sk_point.fY = proto_point.y();
  sk_points.push_back(sk_point);
}

void Converter::Visit(const Path& path) {
  enum SerializationVersions {
    kPathPrivFirstDirection_Version = 1,
    kPathPrivLastMoveToIndex_Version = 2,
    kPathPrivTypeEnumVersion = 3,
    kCurrent_Version = 3
  };

  enum FirstDirection {
    kCW_FirstDirection,
    kCCW_FirstDirection,
    kUnknown_FirstDirection,
  };

  int32_t packed = (path.convexity() << kConvexity_SerializationShift) |
                   (path.fill_type() << kFillType_SerializationShift) |
                   (path.first_direction() << kDirection_SerializationShift) |
                   (path.is_volatile() << kIsVolatile_SerializationShift) |
                   kCurrent_Version;

  // TODO(metzman): Allow writing as RRect.
  WriteNum(packed);
  WriteNum(path.last_move_to_index());
  Visit(path.path_ref());
  Pad(output_.size());
  CheckAlignment();
}

void Converter::Visit(const BlurMaskFilter& blur_mask_filter) {
  // Sigma must be a finite number <= 0.
  float sigma = fabs(BoundFloat(blur_mask_filter.sigma()));
  sigma = sigma == 0 ? 1 : sigma;
  WriteNum(sigma);
  const bool old_value = dont_mutate_enum_;
  dont_mutate_enum_ = true;
  WriteFields(blur_mask_filter, 2, 3);
  dont_mutate_enum_ = old_value;
  Visit(blur_mask_filter.occluder());
}

void Converter::CheckAlignment() const {
  CHECK_EQ(output_.size() % 4, static_cast<size_t>(0));
}

void Converter::Visit(const ShaderChild& shader) {
  bool flattenable_visited = false;
  VISIT_ONEOF_FLATTENABLE(shader, color_4_shader);
  VISIT_ONEOF_FLATTENABLE(shader, color_filter_shader);
  VISIT_ONEOF_FLATTENABLE(shader, image_shader);
  VISIT_ONEOF_FLATTENABLE(shader, compose_shader);
  VISIT_ONEOF_FLATTENABLE(shader, empty_shader);
  VISIT_ONEOF_FLATTENABLE(shader, picture_shader);
  VISIT_ONEOF_FLATTENABLE(shader, perlin_noise_shader_impl);
  VISIT_ONEOF_FLATTENABLE(shader, local_matrix_shader);
  VISIT_ONEOF_FLATTENABLE(shader, linear_gradient);
  VISIT_ONEOF_FLATTENABLE(shader, radial_gradient);
  VISIT_ONEOF_FLATTENABLE(shader, sweep_gradient);
  VISIT_ONEOF_FLATTENABLE(shader, two_point_conical_gradient);
  VISIT_DEFAULT_FLATTENABLE(shader, color_shader);
}

void Converter::Visit(
    const TwoPointConicalGradient& two_point_conical_gradient) {
  Visit(two_point_conical_gradient.parent());
  WriteFields(two_point_conical_gradient, 2, 5);
}

void Converter::Visit(const LinearGradient& linear_gradient) {
  Visit(linear_gradient.parent());
  WriteFields(linear_gradient, 2, 3);
}

void Converter::Visit(const SweepGradient& sweep_gradient) {
  Visit(sweep_gradient.parent());
  WriteFields(sweep_gradient, 2, 4);
}

void Converter::Visit(const RadialGradient& radial_gradient) {
  Visit(radial_gradient.parent());
  WriteFields(radial_gradient, 2, 3);
}

// Don't compile unfinished (dead) code in production.
#ifdef DEVELOPMENT
// ICC handling code is unfinished.
// TODO(metzman): Finish implementing ICC.

// Copied from https://goo.gl/j78F6Z
static constexpr uint32_t kTAG_lut8Type = SET_FOUR_BYTE_TAG('m', 'f', 't', '1');
static constexpr uint32_t kTAG_lut16Type =
    SET_FOUR_BYTE_TAG('m', 'f', 't', '2');
void Converter::Visit(const ICC& icc) {
  icc_base_ = output_.size();
  const uint32_t header_size = sizeof(uint8_t) * 4;
  uint32_t tag_count = 0;
  uint32_t tags_size = 0;
  if (icc.color_space().has_a2b0()) {
    if (icc.color_space().a2b0().has_lut8()) {
      tags_size =
          GetLut8Size(icc.color_space().a2b0().lut8()) + kICCTagTableEntrySize;
    } else if (icc.color_space().a2b0().has_lut16()) {
      tags_size = GetLut16Size(icc.color_space().a2b0().lut16()) +
                  kICCTagTableEntrySize;
    } else {
      NOTREACHED();
    }
    tag_count = 1;
  } else {
    NOTREACHED();
  }

  const uint32_t profile_size = sizeof(float) * 33 + tags_size;
  const uint32_t size = profile_size + sizeof(profile_size) + header_size;
  WriteNum(size);

  // Header.
  WriteColorSpaceVersion();
  WriteNum(ToUInt8(icc.named()));
  WriteNum(ToUInt8(GammaNamed::kNonStandard_SkGammaNamed));
  WriteNum(kICC_Flag);

  WriteNum(profile_size);
  WriteBigEndian(profile_size);
  WriteIgnoredFields(1);
  uint32_t version = icc.version() % 5;
  version <<= 24;
  WriteBigEndian(version);
  WriteBigEndian(kProfileLookupTable[icc.profile_class()]);
  WriteBigEndian(kInputColorSpaceLookupTable[icc.input_color_space()]);
  WriteBigEndian(kPCSLookupTable[icc.pcs()]);
  WriteIgnoredFields(3);
  WriteBigEndian(SET_FOUR_BYTE_TAG('a', 'c', 's', 'p'));
  WriteIgnoredFields(6);
  WriteBigEndian(icc.rendering_intent());
  WriteBigEndian(BoundIlluminant(icc.illuminant_x(), 0.96420f));
  WriteBigEndian(BoundIlluminant(icc.illuminant_y(), 1.00000f));
  WriteBigEndian(BoundIlluminant(icc.illuminant_z(), 0.82491f));
  WriteIgnoredFields(12);
  Visit(icc.color_space());
  const unsigned new_size = output_.size();
  CHECK_EQ(static_cast<size_t>(new_size - icc_base_), size + sizeof(size));
}

void Converter::WriteTagSize(const char (&tag)[4], const size_t size) {
  WriteNum(tag);
  WriteNum(size);
}

// Writes num as a big endian number.
void Converter::WriteBigEndian(base::StrictNumeric<uint32_t> num) {
  auto arr = base::numerics::U32ToBigEndian(num);
  output_.insert(output_.end(), arr.begin(), arr.end());
}

void Converter::Visit(const ICCColorSpace& icc_color_space) {
  if (icc_color_space.has_xyz())
    Visit(icc_color_space.xyz());
  else if (icc_color_space.has_gray())
    Visit(icc_color_space.gray());
  else
    Visit(icc_color_space.a2b0());
}

void Converter::Visit(const ICCXYZ& icc_xyz) {}

void Converter::Visit(const ICCGray& icc_gray) {}

void Converter::Visit(const ICCA2B0& icc_a2b0) {
  if (icc_a2b0.has_lut8())
    Visit(icc_a2b0.lut8());
  else if (icc_a2b0.has_lut16())
    Visit(icc_a2b0.lut16());
  else
    Visit(icc_a2b0.atob());
}

void Converter::Visit(const ICCA2B0AToB& icc_a2b0_atob) {}

uint8_t Converter::GetClutGridPoints(const ICCA2B0Lut8& icc_a2b0_lut8) {
  uint8_t clut_grid_points = icc_a2b0_lut8.clut_grid_points();
  return clut_grid_points ? clut_grid_points > 1 : 2;
}

uint32_t Converter::GetLut8Size(const ICCA2B0Lut8& icc_a2b0_lut8) {
  const uint32_t num_entries =
      GetClutGridPoints(icc_a2b0_lut8) * icc_a2b0_lut8.output_channels();

  const uint32_t clut_bytes = kLut8Precision * num_entries * 4;
  const uint32_t gammas_size =
      kOneChannelGammasSize * (3 + icc_a2b0_lut8.input_channels());
  return kLut8InputSize + gammas_size + clut_bytes;
}

uint32_t Converter::GetLut16Size(const ICCA2B0Lut16& icc_a2b0_lut16) {
  return 48;
}

void Converter::Visit(const ICCA2B0Lut8& icc_a2b0_lut8) {
  // Write Header.
  WriteA2B0TagCommon();

  // Write length.
  WriteBigEndian(GetLut8Size(icc_a2b0_lut8));
  // Specify type.
  WriteBigEndian(kTAG_lut8Type);  // Bytes 0-3.
  WriteLut8(icc_a2b0_lut8);
  Visit(icc_a2b0_lut8.input_gammas_1());
  if (icc_a2b0_lut8.input_channels() == 2) {
    Visit(icc_a2b0_lut8.input_gammas_2());
  } else if (icc_a2b0_lut8.input_channels() == 3) {
    Visit(icc_a2b0_lut8.input_gammas_2());
    Visit(icc_a2b0_lut8.input_gammas_3());
  }

  std::mt19937 gen(icc_a2b0_lut8.clut_bytes_seed());
  const uint32_t clut_bytes = GetClutGridPoints(icc_a2b0_lut8) *
                              icc_a2b0_lut8.output_channels() * kLut8Precision *
                              4;
  for (uint32_t i = 0; i < clut_bytes; i++)
    WriteUInt8(static_cast<uint8_t>(gen()));

  Visit(icc_a2b0_lut8.output_gammas());
}

// Write the parts of a lut8 used by a lut16.
void Converter::WriteLut8(const ICCA2B0Lut8& icc_a2b0_lut8) {
  // Bytes 4-7 are ignored.
  WriteUInt8(icc_a2b0_lut8.ignored_byte_4());
  WriteUInt8(icc_a2b0_lut8.ignored_byte_5());
  WriteUInt8(icc_a2b0_lut8.ignored_byte_6());
  WriteUInt8(icc_a2b0_lut8.ignored_byte_7());
  WriteUInt8(icc_a2b0_lut8.input_channels());    // Byte 8.
  WriteUInt8(icc_a2b0_lut8.output_channels());   // Byte 9.
  WriteUInt8(GetClutGridPoints(icc_a2b0_lut8));  // Byte 10.
  WriteUInt8(icc_a2b0_lut8.ignored_byte_11());
  Visit(icc_a2b0_lut8.matrix());
}

void Converter::WriteA2B0TagCommon() {
  WriteBigEndian(1);  // ICC Tag Count
  WriteBigEndian(kTagLookupTable[ICCTag::kTAG_A2B0]);
  WriteBigEndian(GetCurrentICCOffset() - 4);  // Offset.
}

void Converter::WriteIgnoredFields(const int num_fields) {
  CHECK_GE(num_fields, 1);
  for (int counter = 0; counter < num_fields; counter++)
    WriteNum(0);
}

int32_t Converter::BoundIlluminant(float illuminant, const float num) const {
  while (fabs(illuminant) >= 1) {
    illuminant /= 10;
  }
  const float result = num + 0.01f * illuminant;
  CHECK_LT(fabs(num - result), .01f);
  // 1.52587890625e-5f is a hardcoded value from SkFixed.h.
  return round(result / 1.52587890625e-5f);
}

uint32_t Converter::GetCurrentICCOffset() {
  return output_.size() - icc_base_;
}

void Converter::Visit(const ICCA2B0Lut16& icc_a2b0_lut16) {
  // Write Tag Header
  WriteA2B0TagCommon();

  WriteBigEndian(GetLut16Size(icc_a2b0_lut16));
  WriteBigEndian(kTAG_lut16Type);  // Bytes 0-3.
  WriteLut8(icc_a2b0_lut16.lut8());

  uint16_t in_entries =
      icc_a2b0_lut16.in_table_entries() % (kMaxLut16GammaEntries + 1);

  in_entries = in_entries ? in_entries >= 1 : 2;

  uint16_t out_entries =
      icc_a2b0_lut16.out_table_entries() % (kMaxLut16GammaEntries + 1);

  out_entries = out_entries ? out_entries >= 1 : 2;

  WriteUInt16(static_cast<uint16_t>(in_entries));
  WriteUInt16(static_cast<uint16_t>(out_entries));
}

void Converter::WriteTagHeader(const uint32_t tag, const uint32_t len) {
  WriteBigEndian(kTagLookupTable[tag]);
  WriteBigEndian(tag_offset_);
  WriteBigEndian(len);
  tag_offset_ += 12;
}

// ImageInfo related code.
// Copied from SkImageInfo.h
static int SkColorTypeBytesPerPixel(uint8_t ct) {
  static const uint8_t gSize[] = {
      0,  // Unknown
      1,  // Alpha_8
      2,  // RGB_565
      2,  // ARGB_4444
      4,  // RGBA_8888
      4,  // BGRA_8888
      1,  // kGray_8
      8,  // kRGBA_F16
  };
  return gSize[ct];
}

size_t Converter::ComputeMinByteSize(int32_t width,
                                     int32_t height,
                                     ImageInfo::AlphaType alpha_type) const {
  width = Abs(width);
  height = Abs(height);

  if (!height)
    return 0;
  uint32_t bytes_per_pixel = SkColorTypeBytesPerPixel(alpha_type);
  uint64_t bytes_per_row_64 = width * bytes_per_pixel;
  CHECK(bytes_per_row_64 <= INT32_MAX);
  int32_t bytes_per_row = bytes_per_row_64;
  size_t num_bytes = (height - 1) * bytes_per_row + bytes_per_pixel * width;
  return num_bytes;
}

std::tuple<int32_t, int32_t, int32_t> Converter::GetNumPixelBytes(
    const ImageInfo& image_info,
    int32_t width,
    int32_t height) {
  // Returns a value for pixel bytes that is divisible by four by modifying
  // image_info.width() as needed until the computed min byte size is divisible
  // by four.
  size_t num_bytes_64 =
      ComputeMinByteSize(width, height, image_info.alpha_type());
  CHECK(num_bytes_64 <= INT32_MAX);
  int32_t num_bytes = num_bytes_64;
  bool subtract = (num_bytes >= 5);
  while (num_bytes % 4) {
    if (subtract)
      width -= 1;
    else
      width += 1;
    num_bytes_64 = ComputeMinByteSize(width, height, image_info.alpha_type());
    CHECK(num_bytes_64 <= INT32_MAX);
    num_bytes = num_bytes_64;
  }
  return std::make_tuple(num_bytes, width, height);
}

void Converter::Visit(const ImageInfo& image_info,
                      const int32_t width,
                      const int32_t height) {
  WriteNum(width);
  WriteNum(height);
  uint32_t packed = (image_info.alpha_type() << 8) | image_info.color_type();
  WriteNum(packed);
  Visit(image_info.color_space());
}
#endif  // DEVELOPMENT

void Converter::Visit(const ColorSpaceChild& color_space) {
// ICC code is not finished.
#ifdef DEVELOPMENT
  if (color_space.has_icc())
    Visit(color_space.icc());
  else if (color_space.has_transfer_fn())
#else
  if (color_space.has_transfer_fn())
#endif  // DEVELOPMENT
    Visit(color_space.transfer_fn());
  else if (color_space.has_color_space__xyz())
    Visit(color_space.color_space__xyz());
  else
    Visit(color_space.named());
}

template <typename T>
void Converter::WriteUInt8(T num) {
  CHECK_LT(num, 256);
  output_.push_back(static_cast<uint8_t>(num));
}

void Converter::WriteUInt16(uint16_t num) {
  char num_arr[2];
  memcpy(num_arr, &num, 2);
  for (size_t idx = 0; idx < 2; idx++)
    output_.push_back(num_arr[idx]);
}

void Converter::Visit(const TransferFn& transfer_fn) {
  const size_t size_64 =
      (12 * sizeof(float) + 7 * sizeof(float) + 4 * sizeof(uint8_t));
  CHECK_LT(size_64, UINT32_MAX);
  WriteNum((uint32_t)size_64);
  // Header
  WriteColorSpaceVersion();
  WriteNum(ToUInt8(transfer_fn.named()));
  WriteNum(ToUInt8(GammaNamed::kNonStandard_SkGammaNamed));
  WriteNum(ToUInt8(kTransferFn_Flag));

  WriteFields(transfer_fn, 2);
}

void Converter::WriteColorSpaceVersion() {
  // See SkColorSpace::writeToMemory for why this always writes k0_Version.
  // TODO(metzman): Figure out how to keep this up to date.
  WriteNum(k0_Version);
}

void Converter::Visit(const ColorSpace_XYZ& color_space__xyz) {
  const uint32_t size = 12 * sizeof(float) + sizeof(uint8_t) * 4;
  WriteNum(size);
  // Header
  WriteColorSpaceVersion();
  WriteNum(ToUInt8(Named::kSRGB_Named));
  WriteNum(ToUInt8(color_space__xyz.gamma_named()));
  // See SkColorSpace.cpp:Deserialize (around here: https://goo.gl/R9xQ2B)
  WriteNum(ToUInt8(kMatrix_Flag));

  Visit(color_space__xyz.three_by_four());
}

void Converter::Visit(const ColorSpaceNamed& color_space_named) {
  const uint32_t size = sizeof(uint8_t) * 4;
  WriteNum(size);
  // Header
  WriteColorSpaceVersion();
  WriteNum(ToUInt8(color_space_named.named()));
  WriteNum(ToUInt8(color_space_named.gamma_named()));
  WriteNum(ToUInt8(0));
}

void Converter::Visit(const ImageData& image_data) {
  WriteNum(-4 * image_data.data_size());
  for (uint32_t element : image_data.data())
    WriteNum(element);
}

void Converter::Visit(const Image& image) {
  // Width and height must be greater than 0.
  WriteNum(std::max(1, BoundNum(Abs(image.width()))));
  WriteNum(std::max(1, BoundNum(Abs(image.height()))));

  Visit(image.data());
  if (image.data().data_size()) {
    // origin_x and origin_y need to be positive.
    WriteNum(Abs(image.origin_x()));
    WriteNum(Abs(image.origin_y()));
  }
}

void Converter::Visit(const ImageShader& image_shader) {
  WriteFields(image_shader, 1, 3);
  Visit(image_shader.image());
}

void Converter::Visit(const ColorFilterShader& color_filter_shader) {
  Visit(color_filter_shader.shader());
  Visit(color_filter_shader.filter());
}

void Converter::Visit(const ComposeShader& compose_shader) {
  if (flattenable_depth_ > kFlattenableDepthLimit)
    return;
  flattenable_depth_ += 1;
  Visit(compose_shader.dst());
  Visit(compose_shader.src());
  WriteFields(compose_shader, 3, 4);
  flattenable_depth_ -= 1;
}

void Converter::Visit(const LocalMatrixShader& local_matrix_shader) {
  Visit(local_matrix_shader.matrix());
  Visit(local_matrix_shader.proxy_shader());
}

void Converter::Visit(const Color4Shader& color_4_shader) {
  WriteNum(color_4_shader.color());
  // TODO(metzman): Implement ColorSpaces when skia does. See
  // https://goo.gl/c6YAq7
  WriteBool(false);
}

void Converter::Pad(const size_t write_size) {
  if (write_size % 4 == 0)
    return;
  for (size_t padding_count = 0; (padding_count + write_size) % 4 != 0;
       padding_count++)
    output_.push_back('\0');
}

void Converter::Visit(const Path1DPathEffect& path_1d_path_effect) {
  WriteNum(path_1d_path_effect.advance());
  if (path_1d_path_effect.advance()) {
    Visit(path_1d_path_effect.path());
    WriteFields(path_1d_path_effect, 3, 4);
  }
}

bool Converter::PreVisitFlattenable(const std::string& name) {
  if (flattenable_depth_ > kFlattenableDepthLimit)
    return false;
  flattenable_depth_ += 1;
  WriteString(name);
  RecordSize();
  return true;
}

void Converter::PostVisitFlattenable() {
  WriteBytesWritten();  // Flattenable size.
  CheckAlignment();
  flattenable_depth_ -= 1;
}

void Converter::Visit(const DashImpl& dash_impl) {
  WriteNum(BoundFloat(dash_impl.phase()));
  int num_left = dash_impl.intervals_size();
  int size = dash_impl.intervals_size() + 2;
  if (size % 2) {
    num_left = num_left - 1;
    size = size - 1;
  }
  WriteNum(size);
  WriteNum(fabs(BoundFloat(dash_impl.interval_1())));
  WriteNum(fabs(BoundFloat(dash_impl.interval_2())));
  for (int idx = 0; idx < num_left; idx++)
    WriteNum(fabs(BoundFloat(dash_impl.intervals().Get(idx))));
}

void Converter::Visit(const Path2DPathEffect& path_2d_path_effect) {
  Visit(path_2d_path_effect.matrix());
  Visit(path_2d_path_effect.path());
}

void Converter::Visit(const PathEffectChild& path_effect) {
  bool flattenable_visited = false;
  // Visit(pair_path_effect) implements the functionality of
  // VisitFlattenable by writing the correct names itself.
  if (path_effect.has_pair_path_effect()) {
    Visit(path_effect.pair_path_effect());
    flattenable_visited = true;
  }
  VISIT_ONEOF_FLATTENABLE(path_effect, path_2d_path_effect);
  VISIT_ONEOF_FLATTENABLE(path_effect, line_2d_path_effect);
  VISIT_ONEOF_FLATTENABLE(path_effect, corner_path_effect);
  VISIT_ONEOF_FLATTENABLE(path_effect, discrete_path_effect);
  VISIT_ONEOF_FLATTENABLE(path_effect, path_1d_path_effect);
  VISIT_DEFAULT_FLATTENABLE(path_effect, dash_impl);
}

void Converter::Visit(const DiscretePathEffect& discrete_path_effect) {
  // Don't write seg_length because it causes too many timeouts.
  // See SkScalar.h for why this value is picked
  const float SK_ScalarNotNearlyZero = 1.0 / (1 << 11);
  WriteNum(SK_ScalarNotNearlyZero);
  // Found in testing to be a good value that is unlikely to cause timeouts.
  float perterb = discrete_path_effect.perterb();
  // Do this to avoid timeouts.
  if (perterb < 1)
    perterb += 1;
  WriteNum(perterb);
  WriteNum(discrete_path_effect.seed_assist());
}

void Converter::Visit(const MaskFilterChild& mask_filter) {
  bool flattenable_visited = false;
  VISIT_ONEOF_FLATTENABLE(mask_filter, emboss_mask_filter);
  VISIT_DEFAULT_FLATTENABLE(mask_filter, blur_mask_filter_impl);
}

template <typename T>
uint8_t Converter::ToUInt8(const T input_num) const {
  return input_num % (UINT8_MAX + 1);
}

void Converter::Visit(const EmbossMaskFilterLight& emboss_mask_filter_light) {
  // This is written as a byte array, so first write its size, direction_* are
  // floats, fPad is uint16_t and ambient and specular are uint8_ts.
  const uint32_t byte_array_size =
      (3 * sizeof(float) + sizeof(uint16_t) + (2 * sizeof(uint8_t)));
  WriteNum(byte_array_size);
  WriteFields(emboss_mask_filter_light, 1, 3);
  const uint16_t pad = 0;
  WriteNum(pad);  // fPad = 0;
  WriteNum(ToUInt8(emboss_mask_filter_light.ambient()));
  WriteNum(ToUInt8(emboss_mask_filter_light.specular()));
}

void Converter::Visit(const EmbossMaskFilter& emboss_mask_filter) {
  Visit(emboss_mask_filter.light());
  WriteNum(emboss_mask_filter.blur_sigma());
}

void Converter::Visit(const RecordingData& recording_data) {
  WriteNum(kSkPictReaderTag);
  Visit(recording_data.paints());
}

void Converter::Visit(const PictureTagChild& picture_tag) {
  VISIT_OPT_TAG(paint, SET_FOUR_BYTE_TAG('p', 'n', 't', ' '));
  VISIT_OPT_TAG(path, SET_FOUR_BYTE_TAG('p', 't', 'h', ' '));
  VISIT_OPT_TAG(image, SET_FOUR_BYTE_TAG('i', 'm', 'a', 'g'));
  VISIT_OPT_TAG(vertices, SET_FOUR_BYTE_TAG('v', 'e', 'r', 't'));
  VISIT_OPT_TAG(text_blob, SET_FOUR_BYTE_TAG('b', 'l', 'o', 'b'));
}

void Converter::Visit(const Picture& picture) {
  Visit(picture.info());
  WriteNum(1);
  Visit(picture.data());
}

void Converter::Visit(const Matrix& matrix, bool is_local) {
  // Avoid OOMs by making sure that matrix fields aren't tiny fractions.
  WriteMatrixField(matrix.val1());
  WriteMatrixField(matrix.val2());
  WriteMatrixField(matrix.val3());
  WriteMatrixField(matrix.val4());
  WriteMatrixField(matrix.val5());
  WriteMatrixField(matrix.val6());
  // See SkLocalMatrixImageFilter.cpp:20
  if (is_local)
    WriteNum(0.0f);
  else
    WriteMatrixField(matrix.val7());
  if (is_local)
    WriteNum(0.0f);
  else
    WriteMatrixField(matrix.val8());
  if (is_local)
    WriteNum(1.0f);
  else
    WriteMatrixField(matrix.val9());
}

void Converter::WriteMatrixField(float field_value) {
  // Don't let the field values be tiny fractions.
  field_value = BoundFloat(field_value);
  while ((field_value > 0 && field_value < 1e-5) ||
         (field_value < 0 && field_value > -1e-5))
    field_value /= 10.0;
  WriteNum(field_value);
}

void Converter::Visit(const MatrixImageFilter& matrix_image_filter) {
  Visit(matrix_image_filter.image_filter_parent(), 1);
  Visit(matrix_image_filter.transform());
  WriteNum(matrix_image_filter.filter_quality());
}

void Converter::Visit(const PaintImageFilter& paint_image_filter) {
  Visit(paint_image_filter.image_filter_parent(), 0);
  Visit(paint_image_filter.paint());
}

float Converter::GetRandomFloat(std::mt19937* gen_ptr) {
  CHECK(gen_ptr);
  std::mt19937 gen = *gen_ptr;
  const float positive_random_float = gen();
  const bool is_negative = gen() % 2 == 1;
  if (is_negative)
    return -positive_random_float;
  return positive_random_float;
}

float Converter::GetRandomFloat(float seed, float min, float max) {
  std::mt19937 gen(seed);
  auto next_after_max = std::nextafter(max, std::numeric_limits<float>::max());
  std::uniform_real_distribution<> distribution(min, next_after_max);
  float result = distribution(gen);
  CHECK_LE(result, 1.0);
  CHECK_GE(result, -1.0);
  return result;
}

void Converter::WriteFields(const Message& msg,
                            const unsigned start,
                            const unsigned end) {
  // Do basic validation on start and end. If end == 0, then write all
  // fields left in msg (after start).
  CHECK_GE(start, static_cast<unsigned>(1));
  CHECK_GE(end, static_cast<unsigned>(0));
  CHECK(start <= end || end == 0);
  const Descriptor* descriptor = msg.GetDescriptor();
  CHECK(descriptor);
  const Reflection* reflection = msg.GetReflection();
  CHECK(reflection);
  int field_count = descriptor->field_count();
  CHECK_LE(end, static_cast<unsigned>(field_count));
  const bool write_until_last = end == 0;
  const unsigned last_field_to_write = write_until_last ? field_count : end;

  for (auto field_num = start; field_num <= last_field_to_write; field_num++) {
    const FieldDescriptor* field_descriptor =
        descriptor->FindFieldByNumber(field_num);
    CHECK(field_descriptor);
    const auto& tp = field_descriptor->cpp_type();
    if (field_descriptor->is_repeated()) {
      switch (tp) {
        case FieldDescriptor::CPPTYPE_UINT32: {
          const size_t num_elements =
              reflection->FieldSize(msg, field_descriptor);
          for (size_t idx = 0; idx < num_elements; idx++) {
            WriteNum(reflection->GetRepeatedUInt32(msg, field_descriptor, idx));
          }
          break;
        }
        case FieldDescriptor::CPPTYPE_FLOAT: {
          const size_t num_elements =
              reflection->FieldSize(msg, field_descriptor);
          for (size_t idx = 0; idx < num_elements; idx++) {
            WriteNum(reflection->GetRepeatedFloat(msg, field_descriptor, idx));
          }
          break;
        }
        case FieldDescriptor::CPPTYPE_MESSAGE: {
          Visit(reflection->GetRepeatedPtrField<google::protobuf::Message>(
              msg, field_descriptor));
          break;
        }
        default: {
          NOTREACHED();
        }
      }
      continue;
      // Skip field if it is optional and it is unset.
    } else if (!field_descriptor->is_required() &&
               !reflection->HasField(msg, field_descriptor)) {
      continue;
    }

    // Field is either required or it is optional but is set, so write it:
    switch (tp) {
      case FieldDescriptor::CPPTYPE_INT32:
        WriteNum(BoundNum(reflection->GetInt32(msg, field_descriptor)));
        break;
      case FieldDescriptor::CPPTYPE_UINT32:
        WriteNum(BoundNum(reflection->GetUInt32(msg, field_descriptor)));
        break;
      case FieldDescriptor::CPPTYPE_FLOAT:
        WriteNum(BoundFloat(reflection->GetFloat(msg, field_descriptor)));
        break;
      case FieldDescriptor::CPPTYPE_BOOL:
        WriteBool(reflection->GetBool(msg, field_descriptor));
        break;
      case FieldDescriptor::CPPTYPE_ENUM:
        WriteEnum(msg, reflection, field_descriptor);
        break;
      case FieldDescriptor::CPPTYPE_STRING:
        WriteString(reflection->GetString(msg, field_descriptor));
        break;
      case FieldDescriptor::CPPTYPE_MESSAGE:
        Visit(reflection->GetMessage(msg, field_descriptor));
        break;
      default:
        NOTREACHED();
    }
  }
  CHECK(!write_until_last ||
        !descriptor->FindFieldByNumber(last_field_to_write + 1));
}

void Converter::WriteEnum(const Message& msg,
                          const Reflection* reflection,
                          const FieldDescriptor* field_descriptor) {
  enum MutationState {
    MORE = 1,
    LESS = 2,
  };

  const int value = reflection->GetEnumValue(msg, field_descriptor);
  if (dont_mutate_enum_) {
    WriteNum(value);
    return;
  }

  const int should_mutate = enum_mutator_chance_distribution_(rand_gen_);
  if (should_mutate != MORE && should_mutate != LESS) {
    // Don't mutate, just write it.
    WriteNum(value);
    return;
  }

  const EnumDescriptor* enum_descriptor = field_descriptor->enum_type();
  CHECK(enum_descriptor);

  const EnumValueDescriptor* min_value_descriptor = enum_descriptor->value(0);
  CHECK(min_value_descriptor);
  const int min_value = min_value_descriptor->number();

  const int num_values = enum_descriptor->value_count();
  const EnumValueDescriptor* max_value_descriptor =
      enum_descriptor->value(num_values - 1);
  CHECK(max_value_descriptor);
  const int max_value = max_value_descriptor->number();

  // If we are trying to write less than the min value, but it is 0, just write
  // than the max instead.
  if (should_mutate == LESS && min_value != 0) {
    std::uniform_int_distribution<> value_distribution(-min_value,
                                                       min_value - 1);

    const int new_value = value_distribution(rand_gen_);
    CHECK_EQ(enum_descriptor->FindValueByNumber(new_value), nullptr);
    WriteNum(new_value);
    // Don't also write an enum that is larger than it is supposed to be.
    return;
  }
  const int distribution_lower_bound = max_value + 1;
  CHECK_GT(distribution_lower_bound, max_value);
  const int distribution_upper_bound = 2 * max_value;
  CHECK_GE(distribution_upper_bound, distribution_lower_bound);
  std::uniform_int_distribution<> value_distribution(distribution_lower_bound,
                                                     distribution_upper_bound);

  const int new_value = value_distribution(rand_gen_);
  CHECK_EQ(enum_descriptor->FindValueByNumber(new_value), nullptr);
  WriteNum(new_value);
}

int Converter::Abs(const int val) const {
  if (val == INT_MIN)
    return abs(val + 1);
  return abs(val);
}

void Converter::Visit(const Vertices& vertices) {
  // Note that the size is only needed when this is deserialized as part of a
  // picture image filter. Since this the only way our fuzzer can deserialize
  // Vertices, we always write the size.
  RecordSize();
  int32_t packed = vertices.mode() | kMode_Mask;
  packed = packed ? !vertices.has_texs() : packed | kHasTexs_Mask;
  packed = packed ? !vertices.has_colors() : packed | kHasColors_Mask;
  WriteNum(packed);
  WriteNum(vertices.vertex_text_colors_size());
  WriteNum(vertices.indices_size());
  for (auto vertex_text_color : vertices.vertex_text_colors())
    Visit(vertex_text_color.vertex());

  if (vertices.has_texs()) {
    for (auto vertex_text_color : vertices.vertex_text_colors())
      Visit(vertex_text_color.tex());
  }

  if (vertices.has_colors()) {
    for (auto vertex_text_color : vertices.vertex_text_colors())
      Visit(vertex_text_color.color());
  }
  WriteBytesWritten();
}

void Converter::Visit(const TextBlob& text_blob) {
  Visit(text_blob.bounds());
  int num_glyphs = 2 + text_blob.glyph_pos_clusters_size();
  if (num_glyphs % 2 != 0)
    num_glyphs--;
  CHECK_EQ(num_glyphs % 2, 0);

  WriteNum(num_glyphs);
  WriteUInt8(text_blob.glyph_positioning());
  WriteUInt8(text_blob.extended());
  WriteUInt16(0);  // padding

  if (text_blob.extended())
    WriteNum(Abs(text_blob.text_size()));
  Visit(text_blob.offset());

  Paint paint;
  paint.CopyFrom(text_blob.paint());
  paint.set_text_encoding(Paint::kGlyphID_TextEncoding);
  paint.set_text_size(text_blob.text_size());
  Visit(paint);

  // Byte array size.
  WriteNum(sizeof(uint16_t) * num_glyphs);
  WriteUInt16(text_blob.glyph_pos_cluster_1().glyph());
  WriteUInt16(text_blob.glyph_pos_cluster_2().glyph());
  // Ensure 4-byte alignment doesn't get messed up by writing an odd number of
  // glyphs.
  int idx = 2;
  for (auto& glyph_pos_cluster : text_blob.glyph_pos_clusters()) {
    if (idx++ == num_glyphs)
      break;
    WriteUInt16(glyph_pos_cluster.glyph());
  }

  WriteNum(sizeof(float) * num_glyphs * text_blob.glyph_positioning());
  idx = 2;
  if (text_blob.glyph_positioning() == TextBlob::kHorizontal_Positioning) {
    WriteNum(text_blob.glyph_pos_cluster_1().position_1());
    WriteNum(text_blob.glyph_pos_cluster_2().position_1());
  } else if (text_blob.glyph_positioning() == TextBlob::kFull_Positioning) {
    WriteNum(text_blob.glyph_pos_cluster_1().position_1());
    WriteNum(text_blob.glyph_pos_cluster_1().position_2());
    WriteNum(text_blob.glyph_pos_cluster_2().position_1());
    WriteNum(text_blob.glyph_pos_cluster_2().position_2());
  }
  for (auto& glyph_pos_cluster : text_blob.glyph_pos_clusters()) {
    if (idx++ == num_glyphs)
      break;
    if (text_blob.glyph_positioning() == TextBlob::kHorizontal_Positioning) {
      WriteNum(glyph_pos_cluster.position_1());
    } else if (text_blob.glyph_positioning() == TextBlob::kFull_Positioning) {
      WriteNum(glyph_pos_cluster.position_1());
      WriteNum(glyph_pos_cluster.position_2());
    }
  }

  if (text_blob.extended()) {
    // Write clusters.
    WriteNum(text_blob.glyph_pos_cluster_1().cluster());
    WriteNum(text_blob.glyph_pos_cluster_2().cluster());
    WriteNum(sizeof(uint32_t) * num_glyphs);
    idx = 2;
    for (auto& glyph_pos_cluster : text_blob.glyph_pos_clusters()) {
      if (idx++ == num_glyphs)
        break;
      WriteNum(glyph_pos_cluster.cluster());
    }
    WriteArray(text_blob.text(), text_blob.text_size());
  }

  // No more glyphs.
  WriteNum(0);
}

bool Converter::IsBlacklisted(const std::string& field_name) const {
#ifndef AVOID_MISBEHAVIOR
  // Don't blacklist misbehaving flattenables.
  return false;
#else
  return base::Contains(kMisbehavedFlattenableBlacklist, field_name);
#endif  // AVOID_MISBEHAVIOR
}
}  // namespace skia_image_filter_proto_converter
