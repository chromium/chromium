// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODER_H_
#define MEDIA_BASE_DECODER_H_

#include <ostream>
#include <string>

#include "media/base/media_export.h"
#include "media/base/status.h"
#include "media/gpu/buildflags.h"

namespace media {

// List of known AudioDecoder implementations; recorded to UKM, always add new
// values to the end and do not reorder or delete values from this list.
enum class AudioDecoderType : int {
  kUnknown = 0,          // Decoder name string is not recognized or n/a.
  kFFmpeg = 1,           // FFmpegAudioDecoder
  kMojo = 2,             // MojoAudioDecoder
  kDecrypting = 3,       // DecryptingAudioDecoder
  kMediaCodec = 4,       // MediaCodecAudioDecoder (Android)
  kBroker = 5,           // AudioDecoderBroker
  kTesting = 6,          // Never send this to UKM, for tests only.
  kAudioToolbox = 7,     // AudioToolbox (macOS)
  kMediaFoundation = 8,  // MediaFoundationAudioDecoder
  kPassthroughDTS = 9,   // Passthrough DTS audio
  kSymphonia = 10,       // Symphonia Rust-backed SymphoniaAudioDecoder.
  kOpus = 11,            // OpusAudioDecoder

  // Keep this at the end and equal to the last entry.
  kMaxValue = kOpus,
};

// List of known VideoDecoder implementations; recorded to UKM, always add new
// values to the end and do not reorder or delete values from this list.
enum class VideoDecoderType : int {
  kUnknown = 0,  // Decoder name string is not recognized or n/a.
  // kGpu = 1,      // GpuVideoDecoder (DEPRECATED)
  kFFmpeg = 2,      // FFmpegVideoDecoder
  kVpx = 3,         // VpxVideoDecoder
  kAom = 4,         // AomVideoDecoder
  kMojo = 5,        // MojoVideoDecoder
  kDecrypting = 6,  // DecryptingVideoDecoder
  kDav1d = 7,       // Dav1dVideoDecoder
  kFuchsia = 8,     // FuchsiaVideoDecoder
  kMediaCodec = 9,  // MediaCodecVideoDecoder (Android)
  // kGav1 = 10,    // Gav1VideoDecoder. (DEPRECATED)
  kD3D11 = 11,   // D3D11VideoDecoder
  kVaapi = 12,   // VaapiVideoDecoder
  kBroker = 13,  // VideoDecoderBroker (Webcodecs)
  kVda = 14,     // VDAVideoDecoder
  // kChromeOs = 15,  // DEPRECATED, should be kVaapi, kV4L2, or kOutOfProcess
  // instead.
  kV4L2 = 16,          // V4L2VideoDecoder
  kTesting = 17,       // Never send this to UKM, for tests only.
  kOutOfProcess = 18,  // OOPVideoDecoder (Linux and ChromeOS)
  kVideoToolbox = 19,  // VideoToolboxVideoDecoder (Mac)

  // Keep this at the end and equal to the last entry.
  kMaxValue = kVideoToolbox
};

MEDIA_EXPORT const char* GetDecoderName(AudioDecoderType type);
MEDIA_EXPORT const char* GetDecoderName(VideoDecoderType type);
MEDIA_EXPORT std::ostream& operator<<(std::ostream& out, AudioDecoderType type);
MEDIA_EXPORT std::ostream& operator<<(std::ostream& out, VideoDecoderType type);

#if BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
// Returns the active hardware video acceleration backend for Linux. Used by the
// decoder, encoder, image processor and mojo media clients so they share a
// consistent backend. When both USE_VAAPI and USE_V4L2_CODEC are compiled in,
// this consults the kPreferV4L2VideoAcceleration feature (default: VA-API).
// Otherwise, it returns whichever backend is compiled in. Never returns
// kOutOfProcess; OOP selection is layered on top by the caller.
MEDIA_EXPORT VideoDecoderType ActiveLinuxVideoDecoderType();
#endif  // BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)

class MEDIA_EXPORT Decoder {
 public:
  virtual ~Decoder();

  // Returns true if the implementation is expected to be implemented by the
  // platform. The value should be available immediately after construction and
  // should not change within the lifetime of a decoder instance.
  virtual bool IsPlatformDecoder() const;

  // Returns true if the implementation supports decoding configs with
  // encryption.
  // TODO(crbug.com/40137516): Sometimes it's not possible to give a definitive
  // yes or no answer unless more context is given. While this doesn't pose any
  // problems, it does allow incompatible decoders to pass the filtering step in
  // |DecoderSelector| potentially slowing down the selection process.
  virtual bool SupportsDecryption() const;

 protected:
  Decoder();
};

}  // namespace media

#endif  // MEDIA_BASE_DECODER_H_
