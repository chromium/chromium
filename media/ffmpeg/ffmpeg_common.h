// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FFMPEG_FFMPEG_COMMON_H_
#define MEDIA_FFMPEG_FFMPEG_COMMON_H_

#include <stdint.h>
#include <string>

// Used for FFmpeg error codes.
#include <cerrno>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/audio_codecs.h"
#include "media/base/channel_layout.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_export.h"
#include "media/base/sample_format.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_deleters.h"
#include "third_party/ffmpeg/ffmpeg_features.h"

// Include FFmpeg header files.
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
#include <libavutil/dovi_meta.h>
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
#include <libavutil/imgutils.h>
#include <libavutil/log.h>
#include <libavutil/mastering_display_metadata.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
}  // extern "C"

namespace media {

constexpr int64_t kNoFFmpegTimestamp = static_cast<int64_t>(AV_NOPTS_VALUE);

// Alignment requirement by FFmpeg for input and output buffers. This need to
// be updated to match FFmpeg when it changes.
#if defined(ARCH_CPU_ARM_FAMILY)
constexpr inline int kFFmpegBufferAddressAlignment = 16;
#else
constexpr inline int kFFmpegBufferAddressAlignment = 32;
#endif

class AudioDecoderConfig;
class VideoDecoderConfig;

// The following implement the deleters declared in ffmpeg_deleters.h (which
// contains the declarations needed for use with |scoped_ptr| without #include
// "pollution").

inline void ScopedPtrAVFree::operator()(void* x) const {
  av_free(x);
}

inline void ScopedPtrAVFreePacket::operator()(void* x) const {
  AVPacket* packet = static_cast<AVPacket*>(x);
  av_packet_free(&packet);
}

inline void ScopedPtrAVFreeContext::operator()(void* x) const {
  AVCodecContext* codec_context = static_cast<AVCodecContext*>(x);
  avcodec_free_context(&codec_context);
}

inline void ScopedPtrAVFreeFrame::operator()(void* x) const {
  AVFrame* frame = static_cast<AVFrame*>(x);
  av_frame_free(&frame);
}

// Returns the data from `packet` as a `base::span`. `packet` must be a valid
// `AVPacket` returned from ffmpeg.
inline base::span<const uint8_t> AVPacketData(const AVPacket& packet) {
  // SAFETY: Once initialized by ffmpeg, an `AVPacket` will describe a valid
  // buffer. We assume that callers do not create uninitialized `AVPacket`s on
  // the stack, as ffmpeg's documentation says to only create `AVPacket`s with
  // `av_packet_alloc`, or `ScopedAVPacket` in Chromium. This is not enforced
  // due to limitations from ffmpeg being a C API.
  return UNSAFE_BUFFERS(
      base::span(packet.data, base::checked_cast<size_t>(packet.size)));
}

// Converts an int64_t timestamp in |time_base| units to a base::TimeDelta.
// For example if |timestamp| equals 11025 and |time_base| equals {1, 44100}
// then the return value will be a base::TimeDelta for 0.25 seconds since that
// is how much time 11025/44100ths of a second represents.
MEDIA_EXPORT base::TimeDelta ConvertFromTimeBase(const AVRational& time_base,
                                                 int64_t timestamp);

// Converts a base::TimeDelta into an int64_t timestamp in |time_base| units.
// For example if |timestamp| is 0.5 seconds and |time_base| is {1, 44100}, then
// the return value will be 22050 since that is how many 1/44100ths of a second
// represent 0.5 seconds.
MEDIA_EXPORT int64_t ConvertToTimeBase(const AVRational& time_base,
                                       const base::TimeDelta& timestamp);

// Converts an FFmpeg audio codec ID into its corresponding supported codec id.
MEDIA_EXPORT AudioCodec CodecIDToAudioCodec(AVCodecID codec_id);

// Allocates, populates and returns a wrapped AVCodecContext from the
// AVCodecParameters in |stream|. On failure, returns a wrapped nullptr.
// Wrapping helps ensure eventual destruction of the AVCodecContext.
MEDIA_EXPORT std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext>
AVStreamToAVCodecContext(const AVStream* stream);

// Returns true if AVStream is successfully converted to a AudioDecoderConfig.
// Returns false if conversion fails, in which case |config| is not modified.
MEDIA_EXPORT bool AVStreamToAudioDecoderConfig(const AVStream* stream,
                                               AudioDecoderConfig* config);
void AudioDecoderConfigToAVCodecContext(
    const AudioDecoderConfig& config,
    AVCodecContext* codec_context);

// Returns true if AVStream is successfully converted to a VideoDecoderConfig.
// Returns false if conversion fails, in which case |config| is not modified.
MEDIA_EXPORT bool AVStreamToVideoDecoderConfig(const AVStream* stream,
                                               VideoDecoderConfig* config);
void VideoDecoderConfigToAVCodecContext(
    const VideoDecoderConfig& config,
    AVCodecContext* codec_context);

// Returns true if AVCodecContext is successfully converted to an
// AudioDecoderConfig. Returns false if conversion fails, in which case |config|
// is not modified.
MEDIA_EXPORT bool AVCodecContextToAudioDecoderConfig(
    const AVCodecContext* codec_context,
    EncryptionScheme encryption_scheme,
    AudioDecoderConfig* config);

// Converts FFmpeg's channel layout to chrome's ChannelLayout.  |channels| can
// be used when FFmpeg's channel layout is not informative in order to make a
// good guess about the plausible channel layout based on number of channels.
MEDIA_EXPORT ChannelLayout ChannelLayoutToChromeChannelLayout(int64_t layout,
                                                              int channels);

MEDIA_EXPORT AVCodecID AudioCodecToCodecID(AudioCodec audio_codec,
                                           SampleFormat sample_format);
MEDIA_EXPORT AVCodecID VideoCodecToCodecID(VideoCodec video_codec);

// Converts FFmpeg's audio sample format to Chrome's SampleFormat.
MEDIA_EXPORT SampleFormat
AVSampleFormatToSampleFormat(AVSampleFormat sample_format, AVCodecID codec_id);

// Converts FFmpeg's pixel formats to its corresponding supported video format.
MEDIA_EXPORT VideoPixelFormat
AVPixelFormatToVideoPixelFormat(AVPixelFormat pixel_format);

// Converts an AVERROR error number to a description.
std::string AVErrorToString(int errnum);

// Returns a 32-bit hash for the given codec name.  See the VerifyUmaCodecHashes
// unit test for more information and code for generating the histogram XML.
MEDIA_EXPORT int32_t HashCodecName(const char* codec_name);

}  // namespace media

#endif  // MEDIA_FFMPEG_FFMPEG_COMMON_H_
