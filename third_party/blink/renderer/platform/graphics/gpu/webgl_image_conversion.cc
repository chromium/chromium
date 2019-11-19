// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgl_image_conversion.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/graphics/cpu/arm/webgl_image_conversion_neon.h"
#include "third_party/blink/renderer/platform/graphics/cpu/mips/webgl_image_conversion_msa.h"
#include "third_party/blink/renderer/platform/graphics/cpu/x86/webgl_image_conversion_sse.h"
#include "third_party/blink/renderer/platform/graphics/image_observer.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

namespace {

const float kMaxInt8Value = INT8_MAX;
const float kMaxUInt8Value = UINT8_MAX;
const float kMaxInt16Value = INT16_MAX;
const float kMaxUInt16Value = UINT16_MAX;
const double kMaxInt32Value = INT32_MAX;
const double kMaxUInt32Value = UINT32_MAX;

int8_t ClampMin(int8_t value) {
  const static int8_t kMinInt8Value = INT8_MIN + 1;
  return value < kMinInt8Value ? kMinInt8Value : value;
}

int16_t ClampMin(int16_t value) {
  const static int16_t kMinInt16Value = INT16_MIN + 1;
  return value < kMinInt16Value ? kMinInt16Value : value;
}

int32_t ClampMin(int32_t value) {
  const static int32_t kMinInt32Value = INT32_MIN + 1;
  return value < kMinInt32Value ? kMinInt32Value : value;
}

// Return kDataFormatNumFormats if format/type combination is invalid.
WebGLImageConversion::DataFormat GetDataFormat(GLenum destination_format,
                                               GLenum destination_type) {
  WebGLImageConversion::DataFormat dst_format =
      WebGLImageConversion::kDataFormatRGBA8;
  switch (destination_type) {
    case GL_BYTE:
      switch (destination_format) {
        case GL_RED:
        case GL_RED_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatR8_S;
          break;
        case GL_RG:
        case GL_RG_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRG8_S;
          break;
        case GL_RGB:
        case GL_RGB_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGB8_S;
          break;
        case GL_RGBA:
        case GL_RGBA_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGBA8_S;
          break;
        default:
          return WebGLImageConversion::kDataFormatNumFormats;
      }
      break;
    case GL_UNSIGNED_BYTE:
      switch (destination_format) {
        case GL_RGB:
        case GL_RGB_INTEGER:
        case GL_SRGB_EXT:
          dst_format = WebGLImageConversion::kDataFormatRGB8;
          break;
        case GL_RGBA:
        case GL_RGBA_INTEGER:
        case GL_SRGB_ALPHA_EXT:
          dst_format = WebGLImageConversion::kDataFormatRGBA8;
          break;
        case GL_ALPHA:
          dst_format = WebGLImageConversion::kDataFormatA8;
          break;
        case GL_LUMINANCE:
        case GL_RED:
        case GL_RED_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatR8;
          break;
        case GL_RG:
        case GL_RG_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRG8;
          break;
        case GL_LUMINANCE_ALPHA:
          dst_format = WebGLImageConversion::kDataFormatRA8;
          break;
        default:
          return WebGLImageConversion::kDataFormatNumFormats;
      }
      break;
    case GL_SHORT:
      switch (destination_format) {
        case GL_RED_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatR16_S;
          break;
        case GL_RG_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRG16_S;
          break;
        case GL_RGB_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGB16_S;
          break;
        case GL_RGBA_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGBA16_S;
          break;
        default:
          return WebGLImageConversion::kDataFormatNumFormats;
      }
      break;
    case GL_UNSIGNED_SHORT:
      switch (destination_format) {
        case GL_RED_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatR16;
          break;
        case GL_DEPTH_COMPONENT:
          dst_format = WebGLImageConversion::kDataFormatD16;
          break;
        case GL_RG_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRG16;
          break;
        case GL_RGB_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGB16;
          break;
        case GL_RGBA_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGBA16;
          break;
        default:
          return WebGLImageConversion::kDataFormatNumFormats;
      }
      break;
    case GL_INT:
      switch (destination_format) {
        case GL_RED_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatR32_S;
          break;
        case GL_RG_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRG32_S;
          break;
        case GL_RGB_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGB32_S;
          break;
        case GL_RGBA_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGBA32_S;
          break;
        default:
          return WebGLImageConversion::kDataFormatNumFormats;
      }
      break;
    case GL_UNSIGNED_INT:
      switch (destination_format) {
        case GL_RED_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatR32;
          break;
        case GL_DEPTH_COMPONENT:
          dst_format = WebGLImageConversion::kDataFormatD32;
          break;
        case GL_RG_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRG32;
          break;
        case GL_RGB_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGB32;
          break;
        case GL_RGBA_INTEGER:
          dst_format = WebGLImageConversion::kDataFormatRGBA32;
          break;
        default:
          return WebGLImageConversion::kDataFormatNumFormats;
      }
      break;
    case GL_HALF_FLOAT_OES:  // OES_texture_half_float
    case GL_HALF_FLOAT:
      switch (destination_format) {
        case GL_RGBA:
          dst_format = WebGLImageConversion::kDataFormatRGBA16F;
          break;
        case GL_RGB:
          dst_format = WebGLImageConversion::kDataFormatRGB16F;
          break;
        case GL_RG:
          dst_format = WebGLImageConversion::kDataFormatRG16F;
          break;
        case GL_ALPHA:
          dst_format = WebGLImageConversion::kDataFormatA16F;
          break;
        case GL_LUMINANCE:
        case GL_RED:
          dst_format = WebGLImageConversion::kDataFormatR16F;
          break;
        case GL_LUMINANCE_ALPHA:
          dst_format = WebGLImageConversion::kDataFormatRA16F;
          break;
        default:
          return WebGLImageConversion::kDataFormatNumFormats;
      }
      break;
    case GL_FLOAT:  // OES_texture_float
      switch (destination_format) {
        case GL_RGBA:
          dst_format = WebGLImageConversion::kDataFormatRGBA32F;
          break;
        case GL_RGB:
          dst_format = WebGLImageConversion::kDataFormatRGB32F;
          break;
        case GL_RG:
          dst_format = WebGLImageConversion::kDataFormatRG32F;
          break;
        case GL_ALPHA:
          dst_format = WebGLImageConversion::kDataFormatA32F;
          break;
        case GL_LUMINANCE:
        case GL_RED:
          dst_format = WebGLImageConversion::kDataFormatR32F;
          break;
        case GL_DEPTH_COMPONENT:
          dst_format = WebGLImageConversion::kDataFormatD32F;
          break;
        case GL_LUMINANCE_ALPHA:
          dst_format = WebGLImageConversion::kDataFormatRA32F;
          break;
        default:
          return WebGLImageConversion::kDataFormatNumFormats;
      }
      break;
    case GL_UNSIGNED_SHORT_4_4_4_4:
      dst_format = WebGLImageConversion::kDataFormatRGBA4444;
      break;
    case GL_UNSIGNED_SHORT_5_5_5_1:
      dst_format = WebGLImageConversion::kDataFormatRGBA5551;
      break;
    case GL_UNSIGNED_SHORT_5_6_5:
      dst_format = WebGLImageConversion::kDataFormatRGB565;
      break;
    case GL_UNSIGNED_INT_5_9_9_9_REV:
      dst_format = WebGLImageConversion::kDataFormatRGB5999;
      break;
    case GL_UNSIGNED_INT_24_8:
      dst_format = WebGLImageConversion::kDataFormatDS24_8;
      break;
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
      dst_format = WebGLImageConversion::kDataFormatRGB10F11F11F;
      break;
    case GL_UNSIGNED_INT_2_10_10_10_REV:
      dst_format = WebGLImageConversion::kDataFormatRGBA2_10_10_10;
      break;
    default:
      return WebGLImageConversion::kDataFormatNumFormats;
  }
  return dst_format;
}

// The following Float to Half-Float conversion code is from the implementation
// of ftp://www.fox-toolkit.org/pub/fasthalffloatconversion.pdf, "Fast Half
// Float Conversions" by Jeroen van der Zijp, November 2008 (Revised September
// 2010). Specially, the basetable[512] and shifttable[512] are generated as
// follows:
/*
unsigned short basetable[512];
unsigned char shifttable[512];

void generatetables(){
    unsigned int i;
    int e;
    for (i = 0; i < 256; ++i){
        e = i - 127;
        if (e < -24){ // Very small numbers map to zero
            basetable[i | 0x000] = 0x0000;
            basetable[i | 0x100] = 0x8000;
            shifttable[i | 0x000] = 24;
            shifttable[i | 0x100] = 24;
        }
        else if (e < -14) { // Small numbers map to denorms
            basetable[i | 0x000] = (0x0400>>(-e-14));
            basetable[i | 0x100] = (0x0400>>(-e-14)) | 0x8000;
            shifttable[i | 0x000] = -e-1;
            shifttable[i | 0x100] = -e-1;
        }
        else if (e <= 15){ // Normal numbers just lose precision
            basetable[i | 0x000] = ((e+15)<<10);
            basetable[i| 0x100] = ((e+15)<<10) | 0x8000;
            shifttable[i|0x000] = 13;
            shifttable[i|0x100] = 13;
        }
        else if (e<128){ // Large numbers map to Infinity
            basetable[i|0x000] = 0x7C00;
            basetable[i|0x100] = 0xFC00;
            shifttable[i|0x000] = 24;
            shifttable[i|0x100] = 24;
        }
        else { // Infinity and NaN's stay Infinity and NaN's
            basetable[i|0x000] = 0x7C00;
            basetable[i|0x100] = 0xFC00;
            shifttable[i|0x000] = 13;
            shifttable[i|0x100] = 13;
       }
    }
}
*/

uint16_t g_base_table[512] = {
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     1,     2,     4,     8,     16,    32,    64,
    128,   256,   512,   1024,  2048,  3072,  4096,  5120,  6144,  7168,  8192,
    9216,  10240, 11264, 12288, 13312, 14336, 15360, 16384, 17408, 18432, 19456,
    20480, 21504, 22528, 23552, 24576, 25600, 26624, 27648, 28672, 29696, 30720,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744, 31744,
    31744, 31744, 31744, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
    32768, 32768, 32768, 32768, 32768, 32768, 32768, 32769, 32770, 32772, 32776,
    32784, 32800, 32832, 32896, 33024, 33280, 33792, 34816, 35840, 36864, 37888,
    38912, 39936, 40960, 41984, 43008, 44032, 45056, 46080, 47104, 48128, 49152,
    50176, 51200, 52224, 53248, 54272, 55296, 56320, 57344, 58368, 59392, 60416,
    61440, 62464, 63488, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512, 64512,
    64512, 64512, 64512, 64512, 64512, 64512};

unsigned char g_shift_table[512] = {
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 13, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 23, 22,
    21, 20, 19, 18, 17, 16, 15, 14, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 13};

uint16_t ConvertFloatToHalfFloat(float f) {
  unsigned temp = *(reinterpret_cast<unsigned*>(&f));
  uint16_t signexp = (temp >> 23) & 0x1ff;
  return g_base_table[signexp] +
         ((temp & 0x007fffff) >> g_shift_table[signexp]);
}

/* BEGIN CODE SHARED WITH MOZILLA FIREFOX */

// The following packing and unpacking routines are expressed in terms of
// function templates and inline functions to achieve generality and speedup.
// Explicit template specializations correspond to the cases that would occur.
// Some code are merged back from Mozilla code in
// http://mxr.mozilla.org/mozilla-central/source/content/canvas/src/WebGLTexelConversions.h

//----------------------------------------------------------------------
// Pixel unpacking routines.
template <int format, typename SourceType, typename DstType>
void Unpack(const SourceType*, DstType*, unsigned) {
  NOTREACHED();
}

template <>
void Unpack<WebGLImageConversion::kDataFormatARGB8, uint8_t, uint8_t>(
    const uint8_t* source,
    uint8_t* destination,
    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[1];
    destination[1] = source[2];
    destination[2] = source[3];
    destination[3] = source[0];
    source += 4;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatABGR8, uint8_t, uint8_t>(
    const uint8_t* source,
    uint8_t* destination,
    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[3];
    destination[1] = source[2];
    destination[2] = source[1];
    destination[3] = source[0];
    source += 4;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatBGRA8, uint8_t, uint8_t>(
    const uint8_t* source,
    uint8_t* destination,
    unsigned pixels_per_row) {
  const uint32_t* source32 = reinterpret_cast_ptr<const uint32_t*>(source);
  uint32_t* destination32 = reinterpret_cast_ptr<uint32_t*>(destination);

#if defined(ARCH_CPU_X86_FAMILY)
  simd::UnpackOneRowOfBGRA8LittleToRGBA8(source32, destination32,
                                         pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::unpackOneRowOfBGRA8LittleToRGBA8MSA(source32, destination32,
                                            pixels_per_row);
#endif
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint32_t bgra = source32[i];
#if defined(ARCH_CPU_BIG_ENDIAN)
    uint32_t brMask = 0xff00ff00;
    uint32_t gaMask = 0x00ff00ff;
#else
    uint32_t br_mask = 0x00ff00ff;
    uint32_t ga_mask = 0xff00ff00;
#endif
    uint32_t rgba =
        (((bgra >> 16) | (bgra << 16)) & br_mask) | (bgra & ga_mask);
    destination32[i] = rgba;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatRGBA5551, uint16_t, uint8_t>(
    const uint16_t* source,
    uint8_t* destination,
    unsigned pixels_per_row) {
#if defined(ARCH_CPU_X86_FAMILY)
  simd::UnpackOneRowOfRGBA5551LittleToRGBA8(source, destination,
                                            pixels_per_row);
#endif
#if defined(CPU_ARM_NEON)
  simd::UnpackOneRowOfRGBA5551ToRGBA8(source, destination, pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::unpackOneRowOfRGBA5551ToRGBA8MSA(source, destination, pixels_per_row);
#endif

  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint16_t packed_value = source[0];
    uint8_t r = packed_value >> 11;
    uint8_t g = (packed_value >> 6) & 0x1F;
    uint8_t b = (packed_value >> 1) & 0x1F;
    destination[0] = (r << 3) | (r & 0x7);
    destination[1] = (g << 3) | (g & 0x7);
    destination[2] = (b << 3) | (b & 0x7);
    destination[3] = (packed_value & 0x1) ? 0xFF : 0x0;
    source += 1;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatRGBA4444, uint16_t, uint8_t>(
    const uint16_t* source,
    uint8_t* destination,
    unsigned pixels_per_row) {
#if defined(ARCH_CPU_X86_FAMILY)
  simd::UnpackOneRowOfRGBA4444LittleToRGBA8(source, destination,
                                            pixels_per_row);
#endif
#if defined(CPU_ARM_NEON)
  simd::UnpackOneRowOfRGBA4444ToRGBA8(source, destination, pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::unpackOneRowOfRGBA4444ToRGBA8MSA(source, destination, pixels_per_row);
#endif
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint16_t packed_value = source[0];
    uint8_t r = packed_value >> 12;
    uint8_t g = (packed_value >> 8) & 0x0F;
    uint8_t b = (packed_value >> 4) & 0x0F;
    uint8_t a = packed_value & 0x0F;
    destination[0] = r << 4 | r;
    destination[1] = g << 4 | g;
    destination[2] = b << 4 | b;
    destination[3] = a << 4 | a;
    source += 1;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatRA8, uint8_t, uint8_t>(
    const uint8_t* source,
    uint8_t* destination,
    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    destination[1] = source[0];
    destination[2] = source[0];
    destination[3] = source[1];
    source += 2;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatAR8, uint8_t, uint8_t>(
    const uint8_t* source,
    uint8_t* destination,
    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[1];
    destination[1] = source[1];
    destination[2] = source[1];
    destination[3] = source[0];
    source += 2;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatRGBA8, uint8_t, float>(
    const uint8_t* source,
    float* destination,
    unsigned pixels_per_row) {
  const float kScaleFactor = 1.0f / 255.0f;
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0] * kScaleFactor;
    destination[1] = source[1] * kScaleFactor;
    destination[2] = source[2] * kScaleFactor;
    destination[3] = source[3] * kScaleFactor;
    source += 4;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatBGRA8, uint8_t, float>(
    const uint8_t* source,
    float* destination,
    unsigned pixels_per_row) {
  const float kScaleFactor = 1.0f / 255.0f;
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[2] * kScaleFactor;
    destination[1] = source[1] * kScaleFactor;
    destination[2] = source[0] * kScaleFactor;
    destination[3] = source[3] * kScaleFactor;
    source += 4;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatABGR8, uint8_t, float>(
    const uint8_t* source,
    float* destination,
    unsigned pixels_per_row) {
  const float kScaleFactor = 1.0f / 255.0f;
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[3] * kScaleFactor;
    destination[1] = source[2] * kScaleFactor;
    destination[2] = source[1] * kScaleFactor;
    destination[3] = source[0] * kScaleFactor;
    source += 4;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatARGB8, uint8_t, float>(
    const uint8_t* source,
    float* destination,
    unsigned pixels_per_row) {
  const float kScaleFactor = 1.0f / 255.0f;
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[1] * kScaleFactor;
    destination[1] = source[2] * kScaleFactor;
    destination[2] = source[3] * kScaleFactor;
    destination[3] = source[0] * kScaleFactor;
    source += 4;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatRA32F, float, float>(
    const float* source,
    float* destination,
    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    destination[1] = source[0];
    destination[2] = source[0];
    destination[3] = source[1];
    source += 2;
    destination += 4;
  }
}

template <>
void Unpack<WebGLImageConversion::kDataFormatRGBA2_10_10_10, uint32_t, float>(
    const uint32_t* source,
    float* destination,
    unsigned pixels_per_row) {
  static const float kRgbScaleFactor = 1.0f / 1023.0f;
  static const float kAlphaScaleFactor = 1.0f / 3.0f;
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint32_t packed_value = source[0];
    destination[0] = static_cast<float>(packed_value & 0x3FF) * kRgbScaleFactor;
    destination[1] =
        static_cast<float>((packed_value >> 10) & 0x3FF) * kRgbScaleFactor;
    destination[2] =
        static_cast<float>((packed_value >> 20) & 0x3FF) * kRgbScaleFactor;
    destination[3] = static_cast<float>(packed_value >> 30) * kAlphaScaleFactor;
    source += 1;
    destination += 4;
  }
}

//----------------------------------------------------------------------
// Pixel packing routines.
//

template <int format, int alphaOp, typename SourceType, typename DstType>
void Pack(const SourceType*, DstType*, unsigned) {
  NOTREACHED();
}

template <>
void Pack<WebGLImageConversion::kDataFormatA8,
          WebGLImageConversion::kAlphaDoNothing,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[3];
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR8,
          WebGLImageConversion::kAlphaDoNothing,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR8,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] / 255.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    destination[0] = source_r;
    source += 4;
    destination += 1;
  }
}

// FIXME: this routine is lossy and must be removed.
template <>
void Pack<WebGLImageConversion::kDataFormatR8,
          WebGLImageConversion::kAlphaDoUnmultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
#if defined(ARCH_CPU_X86_FAMILY)
  simd::PackOneRowOfRGBA8LittleToR8(source, destination, pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::packOneRowOfRGBA8LittleToR8MSA(source, destination, pixels_per_row);
#endif
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    destination[0] = source_r;
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA8,
          WebGLImageConversion::kAlphaDoNothing,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    destination[1] = source[3];
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA8,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] / 255.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    destination[0] = source_r;
    destination[1] = source[3];
    source += 4;
    destination += 2;
  }
}

// FIXME: this routine is lossy and must be removed.
template <>
void Pack<WebGLImageConversion::kDataFormatRA8,
          WebGLImageConversion::kAlphaDoUnmultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
#if defined(ARCH_CPU_X86_FAMILY)
  simd::PackOneRowOfRGBA8LittleToRA8(source, destination, pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::packOneRowOfRGBA8LittleToRA8MSA(source, destination, pixels_per_row);
#endif
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    destination[0] = source_r;
    destination[1] = source[3];
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB8,
          WebGLImageConversion::kAlphaDoNothing,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB8,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] / 255.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    destination[0] = source_r;
    destination[1] = source_g;
    destination[2] = source_b;
    source += 4;
    destination += 3;
  }
}

// FIXME: this routine is lossy and must be removed.
template <>
void Pack<WebGLImageConversion::kDataFormatRGB8,
          WebGLImageConversion::kAlphaDoUnmultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    destination[0] = source_r;
    destination[1] = source_g;
    destination[2] = source_b;
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA8,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] / 255.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    destination[0] = source_r;
    destination[1] = source_g;
    destination[2] = source_b;
    destination[3] = source[3];
    source += 4;
    destination += 4;
  }
}

// FIXME: this routine is lossy and must be removed.
template <>
void Pack<WebGLImageConversion::kDataFormatRGBA8,
          WebGLImageConversion::kAlphaDoUnmultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
#if defined(ARCH_CPU_X86_FAMILY)
  simd::PackOneRowOfRGBA8LittleToRGBA8(source, destination, pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::packOneRowOfRGBA8LittleToRGBA8MSA(source, destination, pixels_per_row);
#endif
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    destination[0] = source_r;
    destination[1] = source_g;
    destination[2] = source_b;
    destination[3] = source[3];
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA4444,
          WebGLImageConversion::kAlphaDoNothing,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
#if defined(CPU_ARM_NEON)
  simd::PackOneRowOfRGBA8ToUnsignedShort4444(source, destination,
                                             pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::packOneRowOfRGBA8ToUnsignedShort4444MSA(source, destination,
                                                pixels_per_row);
#endif
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    *destination = (((source[0] & 0xF0) << 8) | ((source[1] & 0xF0) << 4) |
                    (source[2] & 0xF0) | (source[3] >> 4));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA4444,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] / 255.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    *destination = (((source_r & 0xF0) << 8) | ((source_g & 0xF0) << 4) |
                    (source_b & 0xF0) | (source[3] >> 4));
    source += 4;
    destination += 1;
  }
}

// FIXME: this routine is lossy and must be removed.
template <>
void Pack<WebGLImageConversion::kDataFormatRGBA4444,
          WebGLImageConversion::kAlphaDoUnmultiply,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    *destination = (((source_r & 0xF0) << 8) | ((source_g & 0xF0) << 4) |
                    (source_b & 0xF0) | (source[3] >> 4));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA5551,
          WebGLImageConversion::kAlphaDoNothing,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
#if defined(CPU_ARM_NEON)
  simd::PackOneRowOfRGBA8ToUnsignedShort5551(source, destination,
                                             pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::packOneRowOfRGBA8ToUnsignedShort5551MSA(source, destination,
                                                pixels_per_row);
#endif
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    *destination = (((source[0] & 0xF8) << 8) | ((source[1] & 0xF8) << 3) |
                    ((source[2] & 0xF8) >> 2) | (source[3] >> 7));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA5551,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] / 255.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    *destination = (((source_r & 0xF8) << 8) | ((source_g & 0xF8) << 3) |
                    ((source_b & 0xF8) >> 2) | (source[3] >> 7));
    source += 4;
    destination += 1;
  }
}

// FIXME: this routine is lossy and must be removed.
template <>
void Pack<WebGLImageConversion::kDataFormatRGBA5551,
          WebGLImageConversion::kAlphaDoUnmultiply,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    *destination = (((source_r & 0xF8) << 8) | ((source_g & 0xF8) << 3) |
                    ((source_b & 0xF8) >> 2) | (source[3] >> 7));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB565,
          WebGLImageConversion::kAlphaDoNothing,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
#if defined(CPU_ARM_NEON)
  simd::PackOneRowOfRGBA8ToUnsignedShort565(source, destination,
                                            pixels_per_row);
#endif
#if defined(HAVE_MIPS_MSA_INTRINSICS)
  simd::packOneRowOfRGBA8ToUnsignedShort565MSA(source, destination,
                                               pixels_per_row);
#endif
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    *destination = (((source[0] & 0xF8) << 8) | ((source[1] & 0xFC) << 3) |
                    ((source[2] & 0xF8) >> 3));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB565,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] / 255.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    *destination = (((source_r & 0xF8) << 8) | ((source_g & 0xFC) << 3) |
                    ((source_b & 0xF8) >> 3));
    source += 4;
    destination += 1;
  }
}

// FIXME: this routine is lossy and must be removed.
template <>
void Pack<WebGLImageConversion::kDataFormatRGB565,
          WebGLImageConversion::kAlphaDoUnmultiply,
          uint8_t,
          uint16_t>(const uint8_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 255.0f / source[3] : 1.0f;
    uint8_t source_r =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    uint8_t source_g =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    uint8_t source_b =
        static_cast<uint8_t>(static_cast<float>(source[2]) * scale_factor);
    *destination = (((source_r & 0xF8) << 8) | ((source_g & 0xFC) << 3) |
                    ((source_b & 0xF8) >> 3));
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB32F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB32F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = source[0] * scale_factor;
    destination[1] = source[1] * scale_factor;
    destination[2] = source[2] * scale_factor;
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB32F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = source[0] * scale_factor;
    destination[1] = source[1] * scale_factor;
    destination[2] = source[2] * scale_factor;
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA32F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = source[0] * scale_factor;
    destination[1] = source[1] * scale_factor;
    destination[2] = source[2] * scale_factor;
    destination[3] = source[3];
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA32F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = source[0] * scale_factor;
    destination[1] = source[1] * scale_factor;
    destination[2] = source[2] * scale_factor;
    destination[3] = source[3];
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatA32F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[3];
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR32F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR32F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = source[0] * scale_factor;
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR32F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = source[0] * scale_factor;
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA32F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    destination[1] = source[3];
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA32F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = source[0] * scale_factor;
    destination[1] = source[3];
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA32F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = source[0] * scale_factor;
    destination[1] = source[3];
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA16F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ConvertFloatToHalfFloat(source[0]);
    destination[1] = ConvertFloatToHalfFloat(source[1]);
    destination[2] = ConvertFloatToHalfFloat(source[2]);
    destination[3] = ConvertFloatToHalfFloat(source[3]);
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA16F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    destination[1] = ConvertFloatToHalfFloat(source[1] * scale_factor);
    destination[2] = ConvertFloatToHalfFloat(source[2] * scale_factor);
    destination[3] = ConvertFloatToHalfFloat(source[3]);
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA16F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    destination[1] = ConvertFloatToHalfFloat(source[1] * scale_factor);
    destination[2] = ConvertFloatToHalfFloat(source[2] * scale_factor);
    destination[3] = ConvertFloatToHalfFloat(source[3]);
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB16F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ConvertFloatToHalfFloat(source[0]);
    destination[1] = ConvertFloatToHalfFloat(source[1]);
    destination[2] = ConvertFloatToHalfFloat(source[2]);
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB16F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    destination[1] = ConvertFloatToHalfFloat(source[1] * scale_factor);
    destination[2] = ConvertFloatToHalfFloat(source[2] * scale_factor);
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGB16F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    destination[1] = ConvertFloatToHalfFloat(source[1] * scale_factor);
    destination[2] = ConvertFloatToHalfFloat(source[2] * scale_factor);
    source += 4;
    destination += 3;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA16F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ConvertFloatToHalfFloat(source[0]);
    destination[1] = ConvertFloatToHalfFloat(source[3]);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA16F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    destination[1] = ConvertFloatToHalfFloat(source[3]);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRA16F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    destination[1] = ConvertFloatToHalfFloat(source[3]);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR16F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ConvertFloatToHalfFloat(source[0]);
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR16F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatR16F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatA16F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ConvertFloatToHalfFloat(source[3]);
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA8_S,
          WebGLImageConversion::kAlphaDoPremultiply,
          int8_t,
          int8_t>(const int8_t* source,
                  int8_t* destination,
                  unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[3] = ClampMin(source[3]);
    float scale_factor = static_cast<float>(destination[3]) / kMaxInt8Value;
    destination[0] = static_cast<int8_t>(
        static_cast<float>(ClampMin(source[0])) * scale_factor);
    destination[1] = static_cast<int8_t>(
        static_cast<float>(ClampMin(source[1])) * scale_factor);
    destination[2] = static_cast<int8_t>(
        static_cast<float>(ClampMin(source[2])) * scale_factor);
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA16,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint16_t,
          uint16_t>(const uint16_t* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = static_cast<float>(source[3]) / kMaxUInt16Value;
    destination[0] =
        static_cast<uint16_t>(static_cast<float>(source[0]) * scale_factor);
    destination[1] =
        static_cast<uint16_t>(static_cast<float>(source[1]) * scale_factor);
    destination[2] =
        static_cast<uint16_t>(static_cast<float>(source[2]) * scale_factor);
    destination[3] = source[3];
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA16_S,
          WebGLImageConversion::kAlphaDoPremultiply,
          int16_t,
          int16_t>(const int16_t* source,
                   int16_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[3] = ClampMin(source[3]);
    float scale_factor = static_cast<float>(destination[3]) / kMaxInt16Value;
    destination[0] = static_cast<int16_t>(
        static_cast<float>(ClampMin(source[0])) * scale_factor);
    destination[1] = static_cast<int16_t>(
        static_cast<float>(ClampMin(source[1])) * scale_factor);
    destination[2] = static_cast<int16_t>(
        static_cast<float>(ClampMin(source[2])) * scale_factor);
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA32,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint32_t,
          uint32_t>(const uint32_t* source,
                    uint32_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    double scale_factor = static_cast<double>(source[3]) / kMaxUInt32Value;
    destination[0] =
        static_cast<uint32_t>(static_cast<double>(source[0]) * scale_factor);
    destination[1] =
        static_cast<uint32_t>(static_cast<double>(source[1]) * scale_factor);
    destination[2] =
        static_cast<uint32_t>(static_cast<double>(source[2]) * scale_factor);
    destination[3] = source[3];
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA32_S,
          WebGLImageConversion::kAlphaDoPremultiply,
          int32_t,
          int32_t>(const int32_t* source,
                   int32_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[3] = ClampMin(source[3]);
    double scale_factor = static_cast<double>(destination[3]) / kMaxInt32Value;
    destination[0] = static_cast<int32_t>(
        static_cast<double>(ClampMin(source[0])) * scale_factor);
    destination[1] = static_cast<int32_t>(
        static_cast<double>(ClampMin(source[1])) * scale_factor);
    destination[2] = static_cast<int32_t>(
        static_cast<double>(ClampMin(source[2])) * scale_factor);
    source += 4;
    destination += 4;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA2_10_10_10,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint32_t>(const float* source,
                    uint32_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint32_t r = static_cast<uint32_t>(source[0] * 1023.0f);
    uint32_t g = static_cast<uint32_t>(source[1] * 1023.0f);
    uint32_t b = static_cast<uint32_t>(source[2] * 1023.0f);
    uint32_t a = static_cast<uint32_t>(source[3] * 3.0f);
    destination[0] = (a << 30) | (b << 20) | (g << 10) | r;
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA2_10_10_10,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint32_t>(const float* source,
                    uint32_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    uint32_t r = static_cast<uint32_t>(source[0] * source[3] * 1023.0f);
    uint32_t g = static_cast<uint32_t>(source[1] * source[3] * 1023.0f);
    uint32_t b = static_cast<uint32_t>(source[2] * source[3] * 1023.0f);
    uint32_t a = static_cast<uint32_t>(source[3] * 3.0f);
    destination[0] = (a << 30) | (b << 20) | (g << 10) | r;
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRGBA2_10_10_10,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint32_t>(const float* source,
                    uint32_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1023.0f / source[3] : 1023.0f;
    uint32_t r = static_cast<uint32_t>(source[0] * scale_factor);
    uint32_t g = static_cast<uint32_t>(source[1] * scale_factor);
    uint32_t b = static_cast<uint32_t>(source[2] * scale_factor);
    uint32_t a = static_cast<uint32_t>(source[3] * 3.0f);
    destination[0] = (a << 30) | (b << 20) | (g << 10) | r;
    source += 4;
    destination += 1;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG8,
          WebGLImageConversion::kAlphaDoNothing,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    destination[1] = source[1];
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG8,
          WebGLImageConversion::kAlphaDoPremultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = static_cast<float>(source[3]) / kMaxUInt8Value;
    destination[0] =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    destination[1] =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    source += 4;
    destination += 2;
  }
}

// FIXME: this routine is lossy and must be removed.
template <>
void Pack<WebGLImageConversion::kDataFormatRG8,
          WebGLImageConversion::kAlphaDoUnmultiply,
          uint8_t,
          uint8_t>(const uint8_t* source,
                   uint8_t* destination,
                   unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor =
        source[3] ? kMaxUInt8Value / static_cast<float>(source[3]) : 1.0f;
    destination[0] =
        static_cast<uint8_t>(static_cast<float>(source[0]) * scale_factor);
    destination[1] =
        static_cast<uint8_t>(static_cast<float>(source[1]) * scale_factor);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG16F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = ConvertFloatToHalfFloat(source[0]);
    destination[1] = ConvertFloatToHalfFloat(source[1]);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG16F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    destination[1] = ConvertFloatToHalfFloat(source[1] * scale_factor);
    source += 4;
    destination += 2;
  }
}

// FIXME: this routine is lossy and must be removed.
template <>
void Pack<WebGLImageConversion::kDataFormatRG16F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          uint16_t>(const float* source,
                    uint16_t* destination,
                    unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = ConvertFloatToHalfFloat(source[0] * scale_factor);
    destination[1] = ConvertFloatToHalfFloat(source[1] * scale_factor);
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG32F,
          WebGLImageConversion::kAlphaDoNothing,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    destination[0] = source[0];
    destination[1] = source[1];
    source += 4;
    destination += 2;
  }
}

template <>
void Pack<WebGLImageConversion::kDataFormatRG32F,
          WebGLImageConversion::kAlphaDoPremultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3];
    destination[0] = source[0] * scale_factor;
    destination[1] = source[1] * scale_factor;
    source += 4;
    destination += 2;
  }
}

// FIXME: this routine is lossy and must be removed.
template <>
void Pack<WebGLImageConversion::kDataFormatRG32F,
          WebGLImageConversion::kAlphaDoUnmultiply,
          float,
          float>(const float* source,
                 float* destination,
                 unsigned pixels_per_row) {
  for (unsigned i = 0; i < pixels_per_row; ++i) {
    float scale_factor = source[3] ? 1.0f / source[3] : 1.0f;
    destination[0] = source[0] * scale_factor;
    destination[1] = source[1] * scale_factor;
    source += 4;
    destination += 2;
  }
}

bool HasAlpha(int format) {
  return format == WebGLImageConversion::kDataFormatA8 ||
         format == WebGLImageConversion::kDataFormatA16F ||
         format == WebGLImageConversion::kDataFormatA32F ||
         format == WebGLImageConversion::kDataFormatRA8 ||
         format == WebGLImageConversion::kDataFormatAR8 ||
         format == WebGLImageConversion::kDataFormatRA16F ||
         format == WebGLImageConversion::kDataFormatRA32F ||
         format == WebGLImageConversion::kDataFormatRGBA8 ||
         format == WebGLImageConversion::kDataFormatBGRA8 ||
         format == WebGLImageConversion::kDataFormatARGB8 ||
         format == WebGLImageConversion::kDataFormatABGR8 ||
         format == WebGLImageConversion::kDataFormatRGBA16F ||
         format == WebGLImageConversion::kDataFormatRGBA32F ||
         format == WebGLImageConversion::kDataFormatRGBA4444 ||
         format == WebGLImageConversion::kDataFormatRGBA5551 ||
         format == WebGLImageConversion::kDataFormatRGBA8_S ||
         format == WebGLImageConversion::kDataFormatRGBA16 ||
         format == WebGLImageConversion::kDataFormatRGBA16_S ||
         format == WebGLImageConversion::kDataFormatRGBA32 ||
         format == WebGLImageConversion::kDataFormatRGBA32_S ||
         format == WebGLImageConversion::kDataFormatRGBA2_10_10_10;
}

bool HasColor(int format) {
  return format == WebGLImageConversion::kDataFormatRGBA8 ||
         format == WebGLImageConversion::kDataFormatRGBA16F ||
         format == WebGLImageConversion::kDataFormatRGBA32F ||
         format == WebGLImageConversion::kDataFormatRGB8 ||
         format == WebGLImageConversion::kDataFormatRGB16F ||
         format == WebGLImageConversion::kDataFormatRGB32F ||
         format == WebGLImageConversion::kDataFormatBGR8 ||
         format == WebGLImageConversion::kDataFormatBGRA8 ||
         format == WebGLImageConversion::kDataFormatARGB8 ||
         format == WebGLImageConversion::kDataFormatABGR8 ||
         format == WebGLImageConversion::kDataFormatRGBA5551 ||
         format == WebGLImageConversion::kDataFormatRGBA4444 ||
         format == WebGLImageConversion::kDataFormatRGB565 ||
         format == WebGLImageConversion::kDataFormatR8 ||
         format == WebGLImageConversion::kDataFormatR16F ||
         format == WebGLImageConversion::kDataFormatR32F ||
         format == WebGLImageConversion::kDataFormatRA8 ||
         format == WebGLImageConversion::kDataFormatRA16F ||
         format == WebGLImageConversion::kDataFormatRA32F ||
         format == WebGLImageConversion::kDataFormatAR8 ||
         format == WebGLImageConversion::kDataFormatRGBA8_S ||
         format == WebGLImageConversion::kDataFormatRGBA16 ||
         format == WebGLImageConversion::kDataFormatRGBA16_S ||
         format == WebGLImageConversion::kDataFormatRGBA32 ||
         format == WebGLImageConversion::kDataFormatRGBA32_S ||
         format == WebGLImageConversion::kDataFormatRGBA2_10_10_10 ||
         format == WebGLImageConversion::kDataFormatRGB8_S ||
         format == WebGLImageConversion::kDataFormatRGB16 ||
         format == WebGLImageConversion::kDataFormatRGB16_S ||
         format == WebGLImageConversion::kDataFormatRGB32 ||
         format == WebGLImageConversion::kDataFormatRGB32_S ||
         format == WebGLImageConversion::kDataFormatRGB10F11F11F ||
         format == WebGLImageConversion::kDataFormatRGB5999 ||
         format == WebGLImageConversion::kDataFormatRG8 ||
         format == WebGLImageConversion::kDataFormatRG8_S ||
         format == WebGLImageConversion::kDataFormatRG16 ||
         format == WebGLImageConversion::kDataFormatRG16_S ||
         format == WebGLImageConversion::kDataFormatRG32 ||
         format == WebGLImageConversion::kDataFormatRG32_S ||
         format == WebGLImageConversion::kDataFormatRG16F ||
         format == WebGLImageConversion::kDataFormatRG32F ||
         format == WebGLImageConversion::kDataFormatR8_S ||
         format == WebGLImageConversion::kDataFormatR16 ||
         format == WebGLImageConversion::kDataFormatR16_S ||
         format == WebGLImageConversion::kDataFormatR32 ||
         format == WebGLImageConversion::kDataFormatR32_S;
}

template <int Format>
struct IsInt8Format {
  STATIC_ONLY(IsInt8Format);
  static const bool value =
      Format == WebGLImageConversion::kDataFormatRGBA8_S ||
      Format == WebGLImageConversion::kDataFormatRGB8_S ||
      Format == WebGLImageConversion::kDataFormatRG8_S ||
      Format == WebGLImageConversion::kDataFormatR8_S;
};

template <int Format>
struct IsInt16Format {
  STATIC_ONLY(IsInt16Format);
  static const bool value =
      Format == WebGLImageConversion::kDataFormatRGBA16_S ||
      Format == WebGLImageConversion::kDataFormatRGB16_S ||
      Format == WebGLImageConversion::kDataFormatRG16_S ||
      Format == WebGLImageConversion::kDataFormatR16_S;
};

template <int Format>
struct IsInt32Format {
  STATIC_ONLY(IsInt32Format);
  static const bool value =
      Format == WebGLImageConversion::kDataFormatRGBA32_S ||
      Format == WebGLImageConversion::kDataFormatRGB32_S ||
      Format == WebGLImageConversion::kDataFormatRG32_S ||
      Format == WebGLImageConversion::kDataFormatR32_S;
};

template <int Format>
struct IsUInt8Format {
  STATIC_ONLY(IsUInt8Format);
  static const bool value = Format == WebGLImageConversion::kDataFormatRGBA8 ||
                            Format == WebGLImageConversion::kDataFormatRGB8 ||
                            Format == WebGLImageConversion::kDataFormatRG8 ||
                            Format == WebGLImageConversion::kDataFormatR8 ||
                            Format == WebGLImageConversion::kDataFormatBGRA8 ||
                            Format == WebGLImageConversion::kDataFormatBGR8 ||
                            Format == WebGLImageConversion::kDataFormatARGB8 ||
                            Format == WebGLImageConversion::kDataFormatABGR8 ||
                            Format == WebGLImageConversion::kDataFormatRA8 ||
                            Format == WebGLImageConversion::kDataFormatAR8 ||
                            Format == WebGLImageConversion::kDataFormatA8;
};

template <int Format>
struct IsUInt16Format {
  STATIC_ONLY(IsUInt16Format);
  static const bool value = Format == WebGLImageConversion::kDataFormatRGBA16 ||
                            Format == WebGLImageConversion::kDataFormatRGB16 ||
                            Format == WebGLImageConversion::kDataFormatRG16 ||
                            Format == WebGLImageConversion::kDataFormatR16;
};

template <int Format>
struct IsUInt32Format {
  STATIC_ONLY(IsUInt32Format);
  static const bool value = Format == WebGLImageConversion::kDataFormatRGBA32 ||
                            Format == WebGLImageConversion::kDataFormatRGB32 ||
                            Format == WebGLImageConversion::kDataFormatRG32 ||
                            Format == WebGLImageConversion::kDataFormatR32;
};

template <int Format>
struct IsFloatFormat {
  STATIC_ONLY(IsFloatFormat);
  static const bool value =
      Format == WebGLImageConversion::kDataFormatRGBA32F ||
      Format == WebGLImageConversion::kDataFormatRGB32F ||
      Format == WebGLImageConversion::kDataFormatRA32F ||
      Format == WebGLImageConversion::kDataFormatR32F ||
      Format == WebGLImageConversion::kDataFormatA32F ||
      Format == WebGLImageConversion::kDataFormatRG32F;
};

template <int Format>
struct IsHalfFloatFormat {
  STATIC_ONLY(IsHalfFloatFormat);
  static const bool value =
      Format == WebGLImageConversion::kDataFormatRGBA16F ||
      Format == WebGLImageConversion::kDataFormatRGB16F ||
      Format == WebGLImageConversion::kDataFormatRA16F ||
      Format == WebGLImageConversion::kDataFormatR16F ||
      Format == WebGLImageConversion::kDataFormatA16F ||
      Format == WebGLImageConversion::kDataFormatRG16F;
};

template <int Format>
struct Is32bppFormat {
  STATIC_ONLY(Is32bppFormat);
  static const bool value =
      Format == WebGLImageConversion::kDataFormatRGBA2_10_10_10 ||
      Format == WebGLImageConversion::kDataFormatRGB5999 ||
      Format == WebGLImageConversion::kDataFormatRGB10F11F11F;
};

template <int Format>
struct Is16bppFormat {
  STATIC_ONLY(Is16bppFormat);
  static const bool value =
      Format == WebGLImageConversion::kDataFormatRGBA5551 ||
      Format == WebGLImageConversion::kDataFormatRGBA4444 ||
      Format == WebGLImageConversion::kDataFormatRGB565;
};

template <int Format,
          bool IsInt8Format = IsInt8Format<Format>::value,
          bool IsUInt8Format = IsUInt8Format<Format>::value,
          bool IsInt16Format = IsInt16Format<Format>::value,
          bool IsUInt16Format = IsUInt16Format<Format>::value,
          bool IsInt32Format = IsInt32Format<Format>::value,
          bool IsUInt32Format = IsUInt32Format<Format>::value,
          bool IsFloat = IsFloatFormat<Format>::value,
          bool IsHalfFloat = IsHalfFloatFormat<Format>::value,
          bool Is16bpp = Is16bppFormat<Format>::value,
          bool Is32bpp = Is32bppFormat<Format>::value>
struct DataTypeForFormat {
  STATIC_ONLY(DataTypeForFormat);
  typedef double Type;  // Use a type that's not used in unpack/pack.
};

template <int Format>
struct DataTypeForFormat<Format,
                         true,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef int8_t Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         true,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef uint8_t Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         false,
                         true,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef int16_t Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         false,
                         false,
                         true,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef uint16_t Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         false,
                         false,
                         false,
                         true,
                         false,
                         false,
                         false,
                         false,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef int32_t Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         false,
                         false,
                         false,
                         false,
                         true,
                         false,
                         false,
                         false,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef uint32_t Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         true,
                         false,
                         false,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef float Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         true,
                         false,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef uint16_t Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         true,
                         false> {
  STATIC_ONLY(DataTypeForFormat);
  typedef uint16_t Type;
};

template <int Format>
struct DataTypeForFormat<Format,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         false,
                         true> {
  STATIC_ONLY(DataTypeForFormat);
  typedef uint32_t Type;
};

template <int Format>
struct UsesFloatIntermediateFormat {
  STATIC_ONLY(UsesFloatIntermediateFormat);
  static const bool value =
      IsFloatFormat<Format>::value || IsHalfFloatFormat<Format>::value ||
      Format == WebGLImageConversion::kDataFormatRGBA2_10_10_10 ||
      Format == WebGLImageConversion::kDataFormatRGB10F11F11F ||
      Format == WebGLImageConversion::kDataFormatRGB5999;
};

template <int Format>
struct IntermediateFormat {
  STATIC_ONLY(IntermediateFormat);
  static const int value =
      UsesFloatIntermediateFormat<Format>::value
          ? WebGLImageConversion::kDataFormatRGBA32F
          : IsInt32Format<Format>::value
                ? WebGLImageConversion::kDataFormatRGBA32_S
                : IsUInt32Format<Format>::value
                      ? WebGLImageConversion::kDataFormatRGBA32
                      : IsInt16Format<Format>::value
                            ? WebGLImageConversion::kDataFormatRGBA16_S
                            : (IsUInt16Format<Format>::value ||
                               Is32bppFormat<Format>::value)
                                  ? WebGLImageConversion::kDataFormatRGBA16
                                  : IsInt8Format<Format>::value
                                        ? WebGLImageConversion::
                                              kDataFormatRGBA8_S
                                        : WebGLImageConversion::
                                              kDataFormatRGBA8;
};

unsigned TexelBytesForFormat(WebGLImageConversion::DataFormat format) {
  switch (format) {
    case WebGLImageConversion::kDataFormatR8:
    case WebGLImageConversion::kDataFormatR8_S:
    case WebGLImageConversion::kDataFormatA8:
      return 1;
    case WebGLImageConversion::kDataFormatRG8:
    case WebGLImageConversion::kDataFormatRG8_S:
    case WebGLImageConversion::kDataFormatRA8:
    case WebGLImageConversion::kDataFormatAR8:
    case WebGLImageConversion::kDataFormatRGBA5551:
    case WebGLImageConversion::kDataFormatRGBA4444:
    case WebGLImageConversion::kDataFormatRGB565:
    case WebGLImageConversion::kDataFormatA16F:
    case WebGLImageConversion::kDataFormatR16:
    case WebGLImageConversion::kDataFormatR16_S:
    case WebGLImageConversion::kDataFormatR16F:
    case WebGLImageConversion::kDataFormatD16:
      return 2;
    case WebGLImageConversion::kDataFormatRGB8:
    case WebGLImageConversion::kDataFormatRGB8_S:
    case WebGLImageConversion::kDataFormatBGR8:
      return 3;
    case WebGLImageConversion::kDataFormatRGBA8:
    case WebGLImageConversion::kDataFormatRGBA8_S:
    case WebGLImageConversion::kDataFormatARGB8:
    case WebGLImageConversion::kDataFormatABGR8:
    case WebGLImageConversion::kDataFormatBGRA8:
    case WebGLImageConversion::kDataFormatR32:
    case WebGLImageConversion::kDataFormatR32_S:
    case WebGLImageConversion::kDataFormatR32F:
    case WebGLImageConversion::kDataFormatA32F:
    case WebGLImageConversion::kDataFormatRA16F:
    case WebGLImageConversion::kDataFormatRGBA2_10_10_10:
    case WebGLImageConversion::kDataFormatRGB10F11F11F:
    case WebGLImageConversion::kDataFormatRGB5999:
    case WebGLImageConversion::kDataFormatRG16:
    case WebGLImageConversion::kDataFormatRG16_S:
    case WebGLImageConversion::kDataFormatRG16F:
    case WebGLImageConversion::kDataFormatD32:
    case WebGLImageConversion::kDataFormatD32F:
    case WebGLImageConversion::kDataFormatDS24_8:
      return 4;
    case WebGLImageConversion::kDataFormatRGB16:
    case WebGLImageConversion::kDataFormatRGB16_S:
    case WebGLImageConversion::kDataFormatRGB16F:
      return 6;
    case WebGLImageConversion::kDataFormatRGBA16:
    case WebGLImageConversion::kDataFormatRGBA16_S:
    case WebGLImageConversion::kDataFormatRA32F:
    case WebGLImageConversion::kDataFormatRGBA16F:
    case WebGLImageConversion::kDataFormatRG32:
    case WebGLImageConversion::kDataFormatRG32_S:
    case WebGLImageConversion::kDataFormatRG32F:
      return 8;
    case WebGLImageConversion::kDataFormatRGB32:
    case WebGLImageConversion::kDataFormatRGB32_S:
    case WebGLImageConversion::kDataFormatRGB32F:
      return 12;
    case WebGLImageConversion::kDataFormatRGBA32:
    case WebGLImageConversion::kDataFormatRGBA32_S:
    case WebGLImageConversion::kDataFormatRGBA32F:
      return 16;
    default:
      return 0;
  }
}

/* END CODE SHARED WITH MOZILLA FIREFOX */

class FormatConverter {
  STACK_ALLOCATED();

 public:
  FormatConverter(const IntRect& source_data_sub_rectangle,
                  int depth,
                  int unpack_image_height,
                  const void* src_start,
                  void* dst_start,
                  int src_stride,
                  int src_row_offset,
                  int dst_stride)
      : src_sub_rectangle_(source_data_sub_rectangle),
        depth_(depth),
        unpack_image_height_(unpack_image_height),
        src_start_(src_start),
        dst_start_(dst_start),
        src_stride_(src_stride),
        src_row_offset_(src_row_offset),
        dst_stride_(dst_stride),
        success_(false) {
    const unsigned kMaxNumberOfComponents = 4;
    const unsigned kMaxBytesPerComponent = 4;
    unpacked_intermediate_src_data_ = std::make_unique<uint8_t[]>(
        src_sub_rectangle_.Width() * kMaxNumberOfComponents *
        kMaxBytesPerComponent);
    DCHECK(unpacked_intermediate_src_data_.get());
  }

  void Convert(WebGLImageConversion::DataFormat src_format,
               WebGLImageConversion::DataFormat dst_format,
               WebGLImageConversion::AlphaOp);
  bool Success() const { return success_; }

 private:
  template <WebGLImageConversion::DataFormat SrcFormat>
  void Convert(WebGLImageConversion::DataFormat dst_format,
               WebGLImageConversion::AlphaOp);

  template <WebGLImageConversion::DataFormat SrcFormat,
            WebGLImageConversion::DataFormat DstFormat>
  void Convert(WebGLImageConversion::AlphaOp);

  template <WebGLImageConversion::DataFormat SrcFormat,
            WebGLImageConversion::DataFormat DstFormat,
            WebGLImageConversion::AlphaOp alphaOp>
  void Convert();

  const IntRect& src_sub_rectangle_;
  const int depth_;
  const int unpack_image_height_;
  const void* const src_start_;
  void* const dst_start_;
  const int src_stride_, src_row_offset_, dst_stride_;
  bool success_;
  std::unique_ptr<uint8_t[]> unpacked_intermediate_src_data_;
};

void FormatConverter::Convert(WebGLImageConversion::DataFormat src_format,
                              WebGLImageConversion::DataFormat dst_format,
                              WebGLImageConversion::AlphaOp alpha_op) {
#define FORMATCONVERTER_CASE_SRCFORMAT(SrcFormat) \
  case SrcFormat:                                 \
    return Convert<SrcFormat>(dst_format, alpha_op);

  switch (src_format) {
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatRA8)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatRA32F)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatRGBA8)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatARGB8)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatABGR8)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatAR8)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatBGRA8)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatRGBA5551)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatRGBA4444)
    FORMATCONVERTER_CASE_SRCFORMAT(WebGLImageConversion::kDataFormatRGBA32F)
    FORMATCONVERTER_CASE_SRCFORMAT(
        WebGLImageConversion::kDataFormatRGBA2_10_10_10)
    default:
      NOTREACHED();
  }
#undef FORMATCONVERTER_CASE_SRCFORMAT
}

template <WebGLImageConversion::DataFormat SrcFormat>
void FormatConverter::Convert(WebGLImageConversion::DataFormat dst_format,
                              WebGLImageConversion::AlphaOp alpha_op) {
#define FORMATCONVERTER_CASE_DSTFORMAT(DstFormat) \
  case DstFormat:                                 \
    return Convert<SrcFormat, DstFormat>(alpha_op);

  switch (dst_format) {
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatR8)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatR16F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatR32F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatA8)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatA16F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatA32F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRA8)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRA16F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRA32F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGB8)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGB565)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGB16F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGB32F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA8)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA5551)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA4444)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA16F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA32F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA8_S)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA16)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA16_S)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA32)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRGBA32_S)
    FORMATCONVERTER_CASE_DSTFORMAT(
        WebGLImageConversion::kDataFormatRGBA2_10_10_10)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRG8)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRG16F)
    FORMATCONVERTER_CASE_DSTFORMAT(WebGLImageConversion::kDataFormatRG32F)
    default:
      NOTREACHED();
  }

#undef FORMATCONVERTER_CASE_DSTFORMAT
}

template <WebGLImageConversion::DataFormat SrcFormat,
          WebGLImageConversion::DataFormat DstFormat>
void FormatConverter::Convert(WebGLImageConversion::AlphaOp alpha_op) {
#define FORMATCONVERTER_CASE_ALPHAOP(alphaOp) \
  case alphaOp:                               \
    return Convert<SrcFormat, DstFormat, alphaOp>();

  switch (alpha_op) {
    FORMATCONVERTER_CASE_ALPHAOP(WebGLImageConversion::kAlphaDoNothing)
    FORMATCONVERTER_CASE_ALPHAOP(WebGLImageConversion::kAlphaDoPremultiply)
    FORMATCONVERTER_CASE_ALPHAOP(WebGLImageConversion::kAlphaDoUnmultiply)
    default:
      NOTREACHED();
  }
#undef FORMATCONVERTER_CASE_ALPHAOP
}

template <int Format>
struct SupportsConversionFromDomElements {
  STATIC_ONLY(SupportsConversionFromDomElements);
  static const bool value =
      Format == WebGLImageConversion::kDataFormatRGBA8 ||
      Format == WebGLImageConversion::kDataFormatRGB8 ||
      Format == WebGLImageConversion::kDataFormatRG8 ||
      Format == WebGLImageConversion::kDataFormatRA8 ||
      Format == WebGLImageConversion::kDataFormatR8 ||
      Format == WebGLImageConversion::kDataFormatRGBA32F ||
      Format == WebGLImageConversion::kDataFormatRGB32F ||
      Format == WebGLImageConversion::kDataFormatRG32F ||
      Format == WebGLImageConversion::kDataFormatRA32F ||
      Format == WebGLImageConversion::kDataFormatR32F ||
      Format == WebGLImageConversion::kDataFormatRGBA16F ||
      Format == WebGLImageConversion::kDataFormatRGB16F ||
      Format == WebGLImageConversion::kDataFormatRG16F ||
      Format == WebGLImageConversion::kDataFormatRA16F ||
      Format == WebGLImageConversion::kDataFormatR16F ||
      Format == WebGLImageConversion::kDataFormatRGBA5551 ||
      Format == WebGLImageConversion::kDataFormatRGBA4444 ||
      Format == WebGLImageConversion::kDataFormatRGB565 ||
      Format == WebGLImageConversion::kDataFormatRGBA2_10_10_10;
};

template <WebGLImageConversion::DataFormat SrcFormat,
          WebGLImageConversion::DataFormat DstFormat,
          WebGLImageConversion::AlphaOp alphaOp>
void FormatConverter::Convert() {
  // Many instantiations of this template function will never be entered, so we
  // try to return immediately in these cases to avoid generating useless code.
  if (SrcFormat == DstFormat &&
      alphaOp == WebGLImageConversion::kAlphaDoNothing) {
    NOTREACHED();
    return;
  }
  if (!IsFloatFormat<DstFormat>::value && IsFloatFormat<SrcFormat>::value) {
    NOTREACHED();
    return;
  }

  // Only textures uploaded from DOM elements or ImageData can allow DstFormat
  // != SrcFormat.
  const bool src_format_comes_from_dom_element_or_image_data =
      WebGLImageConversion::SrcFormatComeFromDOMElementOrImageData(SrcFormat);
  if (!src_format_comes_from_dom_element_or_image_data &&
      SrcFormat != DstFormat) {
    NOTREACHED();
    return;
  }
  // Likewise, only textures uploaded from DOM elements or ImageData can
  // possibly need to be unpremultiplied.
  if (!src_format_comes_from_dom_element_or_image_data &&
      alphaOp == WebGLImageConversion::kAlphaDoUnmultiply) {
    NOTREACHED();
    return;
  }
  if (src_format_comes_from_dom_element_or_image_data &&
      alphaOp == WebGLImageConversion::kAlphaDoUnmultiply &&
      !SupportsConversionFromDomElements<DstFormat>::value) {
    NOTREACHED();
    return;
  }
  if ((!HasAlpha(SrcFormat) || !HasColor(SrcFormat) || !HasColor(DstFormat)) &&
      alphaOp != WebGLImageConversion::kAlphaDoNothing) {
    NOTREACHED();
    return;
  }
  // If converting DOM element data to UNSIGNED_INT_5_9_9_9_REV or
  // UNSIGNED_INT_10F_11F_11F_REV, we should always switch to FLOAT instead to
  // avoid unpacking/packing these two types.
  if (src_format_comes_from_dom_element_or_image_data &&
      SrcFormat != DstFormat &&
      (DstFormat == WebGLImageConversion::kDataFormatRGB5999 ||
       DstFormat == WebGLImageConversion::kDataFormatRGB10F11F11F)) {
    NOTREACHED();
    return;
  }

  typedef typename DataTypeForFormat<SrcFormat>::Type SrcType;
  typedef typename DataTypeForFormat<DstFormat>::Type DstType;
  const int kIntermFormat = IntermediateFormat<DstFormat>::value;
  typedef typename DataTypeForFormat<kIntermFormat>::Type IntermType;
  // stride here could be negative.
  const ptrdiff_t src_stride_in_elements =
      src_stride_ / static_cast<int>(sizeof(SrcType));
  const ptrdiff_t dst_stride_in_elements =
      dst_stride_ / static_cast<int>(sizeof(DstType));
  const bool kTrivialUnpack = SrcFormat == kIntermFormat;
  const bool kTrivialPack = DstFormat == kIntermFormat &&
                            alphaOp == WebGLImageConversion::kAlphaDoNothing;
  DCHECK(!kTrivialUnpack || !kTrivialPack);

  const SrcType* src_row_start =
      static_cast<const SrcType*>(static_cast<const void*>(
          static_cast<const uint8_t*>(src_start_) +
          ((src_stride_ * src_sub_rectangle_.Y()) + src_row_offset_)));

  // If packing multiple images into a 3D texture, and flipY is true,
  // then the sub-rectangle is pointing at the start of the
  // "bottommost" of those images. Since the source pointer strides in
  // the positive direction, we need to back it up to point at the
  // last, or "topmost", of these images.
  if (dst_stride_ < 0 && depth_ > 1) {
    src_row_start -=
        (depth_ - 1) * src_stride_in_elements * unpack_image_height_;
  }

  DstType* dst_row_start = static_cast<DstType*>(dst_start_);
  if (kTrivialUnpack) {
    for (int d = 0; d < depth_; ++d) {
      for (int i = 0; i < src_sub_rectangle_.Height(); ++i) {
        Pack<DstFormat, alphaOp>(src_row_start, dst_row_start,
                                 src_sub_rectangle_.Width());
        src_row_start += src_stride_in_elements;
        dst_row_start += dst_stride_in_elements;
      }
      src_row_start += src_stride_in_elements *
                       (unpack_image_height_ - src_sub_rectangle_.Height());
    }
  } else if (kTrivialPack) {
    for (int d = 0; d < depth_; ++d) {
      for (int i = 0; i < src_sub_rectangle_.Height(); ++i) {
        Unpack<SrcFormat>(src_row_start, dst_row_start,
                          src_sub_rectangle_.Width());
        src_row_start += src_stride_in_elements;
        dst_row_start += dst_stride_in_elements;
      }
      src_row_start += src_stride_in_elements *
                       (unpack_image_height_ - src_sub_rectangle_.Height());
    }
  } else {
    for (int d = 0; d < depth_; ++d) {
      for (int i = 0; i < src_sub_rectangle_.Height(); ++i) {
        Unpack<SrcFormat>(src_row_start,
                          reinterpret_cast<IntermType*>(
                              unpacked_intermediate_src_data_.get()),
                          src_sub_rectangle_.Width());
        Pack<DstFormat, alphaOp>(reinterpret_cast<IntermType*>(
                                     unpacked_intermediate_src_data_.get()),
                                 dst_row_start, src_sub_rectangle_.Width());
        src_row_start += src_stride_in_elements;
        dst_row_start += dst_stride_in_elements;
      }
      src_row_start += src_stride_in_elements *
                       (unpack_image_height_ - src_sub_rectangle_.Height());
    }
  }
  success_ = true;
  return;
}

bool FrameIsValid(const SkBitmap& frame_bitmap) {
  return !frame_bitmap.isNull() && !frame_bitmap.empty() &&
         frame_bitmap.colorType() == kN32_SkColorType;
}

}  // anonymous namespace

WebGLImageConversion::PixelStoreParams::PixelStoreParams()
    : alignment(4),
      row_length(0),
      image_height(0),
      skip_pixels(0),
      skip_rows(0),
      skip_images(0) {}

bool WebGLImageConversion::ComputeFormatAndTypeParameters(
    GLenum format,
    GLenum type,
    unsigned* components_per_pixel,
    unsigned* bytes_per_component) {
  switch (format) {
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_RED:
    case GL_RED_INTEGER:
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_STENCIL:  // Treat it as one component.
      *components_per_pixel = 1;
      break;
    case GL_LUMINANCE_ALPHA:
    case GL_RG:
    case GL_RG_INTEGER:
      *components_per_pixel = 2;
      break;
    case GL_RGB:
    case GL_RGB_INTEGER:
    case GL_SRGB_EXT:  // GL_EXT_sRGB
      *components_per_pixel = 3;
      break;
    case GL_RGBA:
    case GL_RGBA_INTEGER:
    case GL_BGRA_EXT:        // GL_EXT_texture_format_BGRA8888
    case GL_SRGB_ALPHA_EXT:  // GL_EXT_sRGB
      *components_per_pixel = 4;
      break;
    default:
      return false;
  }
  switch (type) {
    case GL_BYTE:
      *bytes_per_component = sizeof(GLbyte);
      break;
    case GL_UNSIGNED_BYTE:
      *bytes_per_component = sizeof(GLubyte);
      break;
    case GL_SHORT:
      *bytes_per_component = sizeof(GLshort);
      break;
    case GL_UNSIGNED_SHORT:
      *bytes_per_component = sizeof(GLushort);
      break;
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
      *components_per_pixel = 1;
      *bytes_per_component = sizeof(GLushort);
      break;
    case GL_INT:
      *bytes_per_component = sizeof(GLint);
      break;
    case GL_UNSIGNED_INT:
      *bytes_per_component = sizeof(GLuint);
      break;
    case GL_UNSIGNED_INT_24_8_OES:
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
    case GL_UNSIGNED_INT_5_9_9_9_REV:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
      *components_per_pixel = 1;
      *bytes_per_component = sizeof(GLuint);
      break;
    case GL_FLOAT:  // OES_texture_float
      *bytes_per_component = sizeof(GLfloat);
      break;
    case GL_HALF_FLOAT:
    case GL_HALF_FLOAT_OES:  // OES_texture_half_float
      *bytes_per_component = sizeof(GLushort);
      break;
    default:
      return false;
  }
  return true;
}

GLenum WebGLImageConversion::ComputeImageSizeInBytes(
    GLenum format,
    GLenum type,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    const PixelStoreParams& params,
    unsigned* image_size_in_bytes,
    unsigned* padding_in_bytes,
    unsigned* skip_size_in_bytes) {
  DCHECK(image_size_in_bytes);
  DCHECK(params.alignment == 1 || params.alignment == 2 ||
         params.alignment == 4 || params.alignment == 8);
  DCHECK_GE(params.row_length, 0);
  DCHECK_GE(params.image_height, 0);
  DCHECK_GE(params.skip_pixels, 0);
  DCHECK_GE(params.skip_rows, 0);
  DCHECK_GE(params.skip_images, 0);
  if (width < 0 || height < 0 || depth < 0)
    return GL_INVALID_VALUE;
  if (!width || !height || !depth) {
    *image_size_in_bytes = 0;
    if (padding_in_bytes)
      *padding_in_bytes = 0;
    if (skip_size_in_bytes)
      *skip_size_in_bytes = 0;
    return GL_NO_ERROR;
  }

  int row_length = params.row_length > 0 ? params.row_length : width;
  int image_height = params.image_height > 0 ? params.image_height : height;

  unsigned bytes_per_component, components_per_pixel;
  if (!ComputeFormatAndTypeParameters(format, type, &bytes_per_component,
                                      &components_per_pixel))
    return GL_INVALID_ENUM;
  unsigned bytes_per_group = bytes_per_component * components_per_pixel;
  base::CheckedNumeric<uint32_t> checked_value =
      static_cast<uint32_t>(row_length);
  checked_value *= bytes_per_group;
  if (!checked_value.IsValid())
    return GL_INVALID_VALUE;

  unsigned last_row_size;
  if (params.row_length > 0 && params.row_length != width) {
    base::CheckedNumeric<uint32_t> tmp = width;
    tmp *= bytes_per_group;
    if (!tmp.IsValid())
      return GL_INVALID_VALUE;
    last_row_size = tmp.ValueOrDie();
  } else {
    last_row_size = checked_value.ValueOrDie();
  }

  unsigned padding = 0;
  base::CheckedNumeric<uint32_t> checked_residual = checked_value;
  checked_residual %= static_cast<uint32_t>(params.alignment);
  if (!checked_residual.IsValid()) {
    return GL_INVALID_VALUE;
  }
  unsigned residual = checked_residual.ValueOrDie();
  if (residual) {
    padding = params.alignment - residual;
    checked_value += padding;
  }
  if (!checked_value.IsValid())
    return GL_INVALID_VALUE;
  unsigned padded_row_size = checked_value.ValueOrDie();

  base::CheckedNumeric<uint32_t> rows = image_height;
  rows *= (depth - 1);
  // Last image is not affected by IMAGE_HEIGHT parameter.
  rows += height;
  if (!rows.IsValid())
    return GL_INVALID_VALUE;
  checked_value *= (rows - 1);
  // Last row is not affected by ROW_LENGTH parameter.
  checked_value += last_row_size;
  if (!checked_value.IsValid())
    return GL_INVALID_VALUE;
  *image_size_in_bytes = checked_value.ValueOrDie();
  if (padding_in_bytes)
    *padding_in_bytes = padding;

  base::CheckedNumeric<uint32_t> skip_size = 0;
  if (params.skip_images > 0) {
    base::CheckedNumeric<uint32_t> tmp = padded_row_size;
    tmp *= image_height;
    tmp *= params.skip_images;
    if (!tmp.IsValid())
      return GL_INVALID_VALUE;
    skip_size += tmp.ValueOrDie();
  }
  if (params.skip_rows > 0) {
    base::CheckedNumeric<uint32_t> tmp = padded_row_size;
    tmp *= params.skip_rows;
    if (!tmp.IsValid())
      return GL_INVALID_VALUE;
    skip_size += tmp.ValueOrDie();
  }
  if (params.skip_pixels > 0) {
    base::CheckedNumeric<uint32_t> tmp = bytes_per_group;
    tmp *= params.skip_pixels;
    if (!tmp.IsValid())
      return GL_INVALID_VALUE;
    skip_size += tmp.ValueOrDie();
  }
  if (!skip_size.IsValid())
    return GL_INVALID_VALUE;
  if (skip_size_in_bytes)
    *skip_size_in_bytes = skip_size.ValueOrDie();

  checked_value += skip_size.ValueOrDie();
  if (!checked_value.IsValid())
    return GL_INVALID_VALUE;
  return GL_NO_ERROR;
}

WebGLImageConversion::ImageExtractor::ImageExtractor(
    Image* image,
    ImageHtmlDomSource image_html_dom_source,
    bool premultiply_alpha,
    bool ignore_color_space) {
  image_ = image;
  image_html_dom_source_ = image_html_dom_source;
  ExtractImage(premultiply_alpha, ignore_color_space);
}

void WebGLImageConversion::ImageExtractor::ExtractImage(
    bool premultiply_alpha,
    bool ignore_color_space) {
  DCHECK(!image_pixel_locker_);

  if (!image_)
    return;

  sk_sp<SkImage> skia_image = image_->PaintImageForCurrentFrame().GetSkImage();
  SkImageInfo info =
      skia_image ? SkImageInfo::MakeN32Premul(image_->width(), image_->height())
                 : SkImageInfo::MakeUnknown();
  alpha_op_ = kAlphaDoNothing;
  bool has_alpha = skia_image ? !skia_image->isOpaque() : true;

  if ((!skia_image || ignore_color_space ||
       (has_alpha && !premultiply_alpha)) &&
      image_->Data()) {
    // Attempt to get raw unpremultiplied image data.
    const bool data_complete = true;
    std::unique_ptr<ImageDecoder> decoder(ImageDecoder::Create(
        image_->Data(), data_complete, ImageDecoder::kAlphaNotPremultiplied,
        ImageDecoder::kDefaultBitDepth,
        ignore_color_space ? ColorBehavior::Ignore()
                           : ColorBehavior::TransformToSRGB(),
        ImageDecoder::OverrideAllowDecodeToYuv::kDeny));
    if (!decoder || !decoder->FrameCount())
      return;
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
    if (!frame || frame->GetStatus() != ImageFrame::kFrameComplete)
      return;
    has_alpha = frame->HasAlpha();
    SkBitmap bitmap = frame->Bitmap();
    if (!FrameIsValid(bitmap))
      return;

    // TODO(fmalita): Partial frames are not supported currently: only fully
    // decoded frames make it through.  We could potentially relax this and
    // use SkImage::MakeFromBitmap(bitmap) to make a copy.
    skia_image = frame->FinalizePixelsAndGetImage();
    info = bitmap.info();

    if (has_alpha && premultiply_alpha)
      alpha_op_ = kAlphaDoPremultiply;
  } else if (!premultiply_alpha && has_alpha) {
    // 1. For texImage2D with HTMLVideoElment input, assume no PremultiplyAlpha
    //    had been applied and the alpha value for each pixel is 0xFF.  This is
    //    true at present; if it is changed in the future it will need
    //    adjustment accordingly.
    // 2. For texImage2D with HTMLCanvasElement input in which alpha is already
    //    premultiplied in this port, do AlphaDoUnmultiply if
    //    UNPACK_PREMULTIPLY_ALPHA_WEBGL is set to false.
    if (image_html_dom_source_ != kHtmlDomVideo)
      alpha_op_ = kAlphaDoUnmultiply;
  }

  if (!skia_image)
    return;

  image_source_format_ = SK_B32_SHIFT ? kDataFormatRGBA8 : kDataFormatBGRA8;
  image_source_unpack_alignment_ =
      0;  // FIXME: this seems to always be zero - why use at all?

  DCHECK(skia_image->width());
  DCHECK(skia_image->height());
  image_width_ = skia_image->width();
  image_height_ = skia_image->height();

  // Fail if the image was downsampled because of memory limits.
  if (image_width_ != (unsigned)image_->width() ||
      image_height_ != (unsigned)image_->height())
    return;

  image_pixel_locker_.emplace(std::move(skia_image), info.alphaType(),
                              kN32_SkColorType);
}

unsigned WebGLImageConversion::GetChannelBitsByFormat(GLenum format) {
  switch (format) {
    case GL_ALPHA:
      return kChannelAlpha;
    case GL_RED:
    case GL_RED_INTEGER:
    case GL_R8:
    case GL_R8_SNORM:
    case GL_R8UI:
    case GL_R8I:
    case GL_R16UI:
    case GL_R16I:
    case GL_R32UI:
    case GL_R32I:
    case GL_R16F:
    case GL_R32F:
      return kChannelRed;
    case GL_RG:
    case GL_RG_INTEGER:
    case GL_RG8:
    case GL_RG8_SNORM:
    case GL_RG8UI:
    case GL_RG8I:
    case GL_RG16UI:
    case GL_RG16I:
    case GL_RG32UI:
    case GL_RG32I:
    case GL_RG16F:
    case GL_RG32F:
      return kChannelRG;
    case GL_LUMINANCE:
      return kChannelRGB;
    case GL_LUMINANCE_ALPHA:
      return kChannelRGBA;
    case GL_RGB:
    case GL_RGB_INTEGER:
    case GL_RGB8:
    case GL_RGB8_SNORM:
    case GL_RGB8UI:
    case GL_RGB8I:
    case GL_RGB16UI:
    case GL_RGB16I:
    case GL_RGB32UI:
    case GL_RGB32I:
    case GL_RGB16F:
    case GL_RGB32F:
    case GL_RGB565:
    case GL_R11F_G11F_B10F:
    case GL_RGB9_E5:
    case GL_SRGB_EXT:
    case GL_SRGB8:
      return kChannelRGB;
    case GL_RGBA:
    case GL_RGBA_INTEGER:
    case GL_RGBA8:
    case GL_RGBA8_SNORM:
    case GL_RGBA8UI:
    case GL_RGBA8I:
    case GL_RGBA16UI:
    case GL_RGBA16I:
    case GL_RGBA32UI:
    case GL_RGBA32I:
    case GL_RGBA16F:
    case GL_RGBA32F:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGB10_A2:
    case GL_RGB10_A2UI:
    case GL_SRGB_ALPHA_EXT:
    case GL_SRGB8_ALPHA8:
      return kChannelRGBA;
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32F:
      return kChannelDepth;
    case GL_STENCIL:
    case GL_STENCIL_INDEX8:
      return kChannelStencil;
    case GL_DEPTH_STENCIL:
    case GL_DEPTH24_STENCIL8:
    case GL_DEPTH32F_STENCIL8:
      return kChannelDepthStencil;
    default:
      return 0;
  }
}

bool WebGLImageConversion::PackImageData(
    Image* image,
    const void* pixels,
    GLenum format,
    GLenum type,
    bool flip_y,
    AlphaOp alpha_op,
    DataFormat source_format,
    unsigned source_image_width,
    unsigned source_image_height,
    const IntRect& source_image_sub_rectangle,
    int depth,
    unsigned source_unpack_alignment,
    int unpack_image_height,
    Vector<uint8_t>& data) {
  if (!pixels)
    return false;

  unsigned packed_size;
  // Output data is tightly packed (alignment == 1).
  PixelStoreParams params;
  params.alignment = 1;
  if (ComputeImageSizeInBytes(format, type, source_image_sub_rectangle.Width(),
                              source_image_sub_rectangle.Height(), depth,
                              params, &packed_size, nullptr,
                              nullptr) != GL_NO_ERROR)
    return false;
  data.resize(packed_size);

  return PackPixels(reinterpret_cast<const uint8_t*>(pixels), source_format,
                    source_image_width, source_image_height,
                    source_image_sub_rectangle, depth, source_unpack_alignment,
                    unpack_image_height, format, type, alpha_op, data.data(),
                    flip_y);
}

bool WebGLImageConversion::ExtractImageData(
    const uint8_t* image_data,
    DataFormat source_data_format,
    const IntSize& image_data_size,
    const IntRect& source_image_sub_rectangle,
    int depth,
    int unpack_image_height,
    GLenum format,
    GLenum type,
    bool flip_y,
    bool premultiply_alpha,
    Vector<uint8_t>& data) {
  if (!image_data)
    return false;
  int width = image_data_size.Width();
  int height = image_data_size.Height();

  unsigned packed_size;
  // Output data is tightly packed (alignment == 1).
  PixelStoreParams params;
  params.alignment = 1;
  if (ComputeImageSizeInBytes(format, type, source_image_sub_rectangle.Width(),
                              source_image_sub_rectangle.Height(), depth,
                              params, &packed_size, nullptr,
                              nullptr) != GL_NO_ERROR)
    return false;
  data.resize(packed_size);

  if (!PackPixels(image_data, source_data_format, width, height,
                  source_image_sub_rectangle, depth, 0, unpack_image_height,
                  format, type,
                  premultiply_alpha ? kAlphaDoPremultiply : kAlphaDoNothing,
                  data.data(), flip_y))
    return false;

  return true;
}

bool WebGLImageConversion::ExtractTextureData(
    unsigned width,
    unsigned height,
    GLenum format,
    GLenum type,
    const PixelStoreParams& unpack_params,
    bool flip_y,
    bool premultiply_alpha,
    const void* pixels,
    Vector<uint8_t>& data) {
  // Assumes format, type, etc. have already been validated.
  DataFormat source_data_format = GetDataFormat(format, type);
  if (source_data_format == kDataFormatNumFormats)
    return false;

  // Resize the output buffer.
  unsigned int components_per_pixel, bytes_per_component;
  if (!ComputeFormatAndTypeParameters(format, type, &components_per_pixel,
                                      &bytes_per_component))
    return false;
  unsigned bytes_per_pixel = components_per_pixel * bytes_per_component;
  data.resize(width * height * bytes_per_pixel);

  unsigned image_size_in_bytes, skip_size_in_bytes;
  ComputeImageSizeInBytes(format, type, width, height, 1, unpack_params,
                          &image_size_in_bytes, nullptr, &skip_size_in_bytes);
  const uint8_t* src_data = static_cast<const uint8_t*>(pixels);
  if (skip_size_in_bytes) {
    src_data += skip_size_in_bytes;
  }

  if (!PackPixels(src_data, source_data_format,
                  unpack_params.row_length ? unpack_params.row_length : width,
                  height, IntRect(0, 0, width, height), 1,
                  unpack_params.alignment, 0, format, type,
                  (premultiply_alpha ? kAlphaDoPremultiply : kAlphaDoNothing),
                  data.data(), flip_y))
    return false;

  return true;
}

bool WebGLImageConversion::PackPixels(const uint8_t* source_data,
                                      DataFormat source_data_format,
                                      unsigned source_data_width,
                                      unsigned source_data_height,
                                      const IntRect& source_data_sub_rectangle,
                                      int depth,
                                      unsigned source_unpack_alignment,
                                      int unpack_image_height,
                                      unsigned destination_format,
                                      unsigned destination_type,
                                      AlphaOp alpha_op,
                                      void* destination_data,
                                      bool flip_y) {
  DCHECK_GE(depth, 1);
  if (unpack_image_height == 0) {
    unpack_image_height = source_data_sub_rectangle.Height();
  }
  int valid_src = source_data_width * TexelBytesForFormat(source_data_format);
  int remainder =
      source_unpack_alignment ? (valid_src % source_unpack_alignment) : 0;
  int src_stride =
      remainder ? (valid_src + source_unpack_alignment - remainder) : valid_src;
  int src_row_offset =
      source_data_sub_rectangle.X() * TexelBytesForFormat(source_data_format);

  DataFormat dst_data_format =
      GetDataFormat(destination_format, destination_type);
  if (dst_data_format == kDataFormatNumFormats)
    return false;
  int dst_stride =
      source_data_sub_rectangle.Width() * TexelBytesForFormat(dst_data_format);
  if (flip_y) {
    destination_data =
        static_cast<uint8_t*>(destination_data) +
        dst_stride * ((depth * source_data_sub_rectangle.Height()) - 1);
    dst_stride = -dst_stride;
  }
  if (!HasAlpha(source_data_format) || !HasColor(source_data_format) ||
      !HasColor(dst_data_format))
    alpha_op = kAlphaDoNothing;

  if (source_data_format == dst_data_format && alpha_op == kAlphaDoNothing) {
    const uint8_t* base_ptr =
        source_data + src_stride * source_data_sub_rectangle.Y();
    const uint8_t* base_end =
        source_data + src_stride * source_data_sub_rectangle.MaxY();

    // If packing multiple images into a 3D texture, and flipY is true,
    // then the sub-rectangle is pointing at the start of the
    // "bottommost" of those images. Since the source pointer strides in
    // the positive direction, we need to back it up to point at the
    // last, or "topmost", of these images.
    if (flip_y && depth > 1) {
      const ptrdiff_t distance_to_top_image =
          (depth - 1) * src_stride * unpack_image_height;
      base_ptr -= distance_to_top_image;
      base_end -= distance_to_top_image;
    }

    unsigned row_size = (dst_stride > 0) ? dst_stride : -dst_stride;
    uint8_t* dst = static_cast<uint8_t*>(destination_data);

    for (int i = 0; i < depth; ++i) {
      const uint8_t* ptr = base_ptr;
      const uint8_t* ptr_end = base_end;
      while (ptr < ptr_end) {
        memcpy(dst, ptr + src_row_offset, row_size);
        ptr += src_stride;
        dst += dst_stride;
      }
      base_ptr += unpack_image_height * src_stride;
      base_end += unpack_image_height * src_stride;
    }
    return true;
  }

  FormatConverter converter(source_data_sub_rectangle, depth,
                            unpack_image_height, source_data, destination_data,
                            src_stride, src_row_offset, dst_stride);
  converter.Convert(source_data_format, dst_data_format, alpha_op);
  if (!converter.Success())
    return false;
  return true;
}

void WebGLImageConversion::UnpackPixels(const uint16_t* source_data,
                                        DataFormat source_data_format,
                                        unsigned pixels_per_row,
                                        uint8_t* destination_data) {
  switch (source_data_format) {
    case kDataFormatRGBA4444: {
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA4444>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(source_data);
      Unpack<WebGLImageConversion::kDataFormatRGBA4444>(
          src_row_start, destination_data, pixels_per_row);
    } break;
    case kDataFormatRGBA5551: {
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA5551>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(source_data);
      Unpack<WebGLImageConversion::kDataFormatRGBA5551>(
          src_row_start, destination_data, pixels_per_row);
    } break;
    case kDataFormatBGRA8: {
      const uint8_t* psrc = (const uint8_t*)source_data;
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatBGRA8>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(psrc);
      Unpack<WebGLImageConversion::kDataFormatBGRA8>(
          src_row_start, destination_data, pixels_per_row);
    } break;
    default:
      break;
  }
}

void WebGLImageConversion::PackPixels(const uint8_t* source_data,
                                      DataFormat source_data_format,
                                      unsigned pixels_per_row,
                                      uint8_t* destination_data) {
  switch (source_data_format) {
    case kDataFormatRA8: {
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA8>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(source_data);
      Pack<WebGLImageConversion::kDataFormatRA8,
           WebGLImageConversion::kAlphaDoUnmultiply>(
          src_row_start, destination_data, pixels_per_row);
    } break;
    case kDataFormatR8: {
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA8>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(source_data);
      Pack<WebGLImageConversion::kDataFormatR8,
           WebGLImageConversion::kAlphaDoUnmultiply>(
          src_row_start, destination_data, pixels_per_row);
    } break;
    case kDataFormatRGBA8: {
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA8>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(source_data);
      Pack<WebGLImageConversion::kDataFormatRGBA8,
           WebGLImageConversion::kAlphaDoUnmultiply>(
          src_row_start, destination_data, pixels_per_row);
    } break;
    case kDataFormatRGBA4444: {
      uint16_t* pdst = (uint16_t*)destination_data;
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA8>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(source_data);
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA4444>::Type DstType;
      DstType* dst_row_start = static_cast<DstType*>(pdst);
      Pack<WebGLImageConversion::kDataFormatRGBA4444,
           WebGLImageConversion::kAlphaDoNothing>(src_row_start, dst_row_start,
                                                  pixels_per_row);
    } break;
    case kDataFormatRGBA5551: {
      uint16_t* pdst = (uint16_t*)destination_data;
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA8>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(source_data);
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA5551>::Type DstType;
      DstType* dst_row_start = static_cast<DstType*>(pdst);
      Pack<WebGLImageConversion::kDataFormatRGBA5551,
           WebGLImageConversion::kAlphaDoNothing>(src_row_start, dst_row_start,
                                                  pixels_per_row);
    } break;
    case kDataFormatRGB565: {
      uint16_t* pdst = (uint16_t*)destination_data;
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGBA8>::Type SrcType;
      const SrcType* src_row_start = static_cast<const SrcType*>(source_data);
      typedef typename DataTypeForFormat<
          WebGLImageConversion::kDataFormatRGB565>::Type DstType;
      DstType* dst_row_start = static_cast<DstType*>(pdst);
      Pack<WebGLImageConversion::kDataFormatRGB565,
           WebGLImageConversion::kAlphaDoNothing>(src_row_start, dst_row_start,
                                                  pixels_per_row);
    } break;
    default:
      break;
  }
}

}  // namespace blink
