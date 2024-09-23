// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CODEC_UTIL_H_
#define MEDIA_BASE_ANDROID_MEDIA_CODEC_UTIL_H_

#include <jni.h>

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "media/base/android/media_codec_direction.h"
#include "media/base/audio_codecs.h"
#include "media/base/media_export.h"
#include "media/base/sample_format.h"
#include "media/base/video_codecs.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// WARNING: Not all methods on this class can be used in the renderer process,
// only those which do not attempt to use MediaCodec or MediaCodecList.
//
// TODO(dalecurtis): We should move out all methods which can't be used in the
// renderer into a media/gpu helper class.
class MEDIA_EXPORT MediaCodecUtil {
 public:
  static std::string CodecToAndroidMimeType(AudioCodec codec);
  static std::string CodecToAndroidMimeType(AudioCodec codec,
                                            SampleFormat sample_format);
  static std::string CodecToAndroidMimeType(VideoCodec codec);

  // Indicates if the vp8 decoder or encoder is available on this device.
  static bool IsVp8DecoderAvailable();
  static bool IsVp8EncoderAvailable();

  // Indicates if the vp9 decoder is available on this device.
  static bool IsVp9DecoderAvailable();
  static bool IsVp9Profile2DecoderAvailable();
  static bool IsVp9Profile3DecoderAvailable();

  // Indicates if the Opus decoder is available on this device.
  static bool IsOpusDecoderAvailable();

  // Indicates if the av1 decoder is available on this device.
  static bool IsAv1DecoderAvailable();

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  // Indicates if the h265 decoder is available on this device.
  static bool IsHEVCDecoderAvailable();
#endif

  // Indicates if the AAC encoder is available on this device.
  static bool IsAACEncoderAvailable();

  // Indicates if SurfaceView and MediaCodec work well together on this device.
  static bool IsSurfaceViewOutputSupported();

  // Indicates if MediaCodec.setOutputSurface() works on this device.
  static bool IsSetOutputSurfaceSupported();

  // Returns a known alignment which can be used to translate visible size into
  // coded size. E.g., a size of (1, 1) means no alignment while a size of
  // (64, 1) would mean visible width should be rounded up to the nearest
  // multiple of 64 and height should be left untouched.
  //
  // Returns std::nullopt if the decoder isn't recognized. `host_sdk_int` may
  // be set for testing purposes.
  static std::optional<gfx::Size> LookupCodedSizeAlignment(
      std::string_view name,
      std::optional<int> host_sdk_int = std::nullopt);

  //
  // ***************************************************************
  // *** THE FOLLOWING METHODS CAN'T BE CALLED FROM THE RENDERER ***
  // ***************************************************************
  //

  // Returns whether it's possible to create a MediaCodec for the given codec
  // and secureness.
  //
  // WARNING: This can't be used from the renderer process since it attempts to
  // create a MediaCodec (which requires permissions) to get the codec name.
  static bool CanDecode(VideoCodec codec, bool is_secure);
  static bool CanDecode(AudioCodec codec);

  // Indicates if the h264 encoder is available on this device.
  //
  // This can't be used from the renderer process since it attempts to
  // access MediaCodecList (which requires permissions).
  static bool IsH264EncoderAvailable();

  // Returns a vector of supported codecs profiles and levels.
  //
  // WARNING: This can't be used from the renderer process since it attempts to
  // access MediaCodecList (which requires permissions).
  static void AddSupportedCodecProfileLevels(
      std::vector<CodecProfileLevel>* out);

  // Get a list of encoder supported color formats for |mime_type|.
  // The mapping of color format name and its value refers to
  // MediaCodecInfo.CodecCapabilities.
  //
  // WARNING: This can't be used from the renderer process since it attempts to
  // access MediaCodecList (which requires permissions).
  static std::set<int> GetEncoderColorFormats(const std::string& mime_type);

  // Returns true if |mime_type| is known to be unaccelerated (i.e. backed by a
  // software codec instead of a hardware one).
  //
  // WARNING: This can't be used from the renderer process since it attempts to
  // create a MediaCodec (which requires permissions) to get the codec name.
  static bool IsKnownUnaccelerated(VideoCodec codec,
                                   MediaCodecDirection direction);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CODEC_UTIL_H_
