// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_glue.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/types/cxx23_to_underlying.h"
#include "media/base/container_names.h"
#include "media/base/media_switches.h"
#include "media/base/supported_types.h"
#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

// Kill switches in case things explode. Remove after M132.
// TODO(crbug.com/355485812): Re-enable this flag.
BASE_FEATURE(kAllowOnlyAudioCodecsDuringDemuxing,
             "AllowOnlyAudioCodecsDuringDemuxing",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kForbidH264ParsingDuringDemuxing,
             "ForbidH264ParsingDuringDemuxing",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Internal buffer size used by AVIO for reading.
// TODO(dalecurtis): Experiment with this buffer size and measure impact on
// performance.  Currently we want to use 32kb to preserve existing behavior
// with the previous URLProtocol based approach.
enum { kBufferSize = 32 * 1024 };

static int AVIOReadOperation(void* opaque, uint8_t* buf, int buf_size) {
  return reinterpret_cast<FFmpegURLProtocol*>(opaque)->Read(buf_size, buf);
}

static int64_t AVIOSeekOperation(void* opaque, int64_t offset, int whence) {
  FFmpegURLProtocol* protocol = reinterpret_cast<FFmpegURLProtocol*>(opaque);
  int64_t new_offset = AVERROR(EIO);
  switch (whence) {
    case SEEK_SET:
      if (protocol->SetPosition(offset))
        protocol->GetPosition(&new_offset);
      break;

    case SEEK_CUR:
      int64_t pos;
      if (!protocol->GetPosition(&pos))
        break;
      if (protocol->SetPosition(pos + offset))
        protocol->GetPosition(&new_offset);
      break;

    case SEEK_END:
      int64_t size;
      if (!protocol->GetSize(&size))
        break;
      if (protocol->SetPosition(size + offset))
        protocol->GetPosition(&new_offset);
      break;

    case AVSEEK_SIZE:
      protocol->GetSize(&new_offset);
      break;

    default:
      NOTREACHED_IN_MIGRATION();
  }
  return new_offset;
}

static void LogContainer(bool is_local_file,
                         container_names::MediaContainerName container) {
  base::UmaHistogramSparse("Media.DetectedContainer",
                           base::to_underlying(container));
  if (is_local_file) {
    base::UmaHistogramSparse("Media.DetectedContainer.Local",
                             base::to_underlying(container));
  }
}

static const char* GetAllowedDemuxers() {
  static const base::NoDestructor<std::string> kAllowedDemuxers([]() {
    // This should match the configured lists in //third_party/ffmpeg.
    std::vector<std::string> allowed_demuxers = {"ogg",  "matroska", "wav",
                                                 "flac", "mp3",      "mov"};
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    allowed_demuxers.push_back("aac");
#endif
    return base::JoinString(allowed_demuxers, ",");
  }());
  return kAllowedDemuxers->c_str();
}

FFmpegGlue::FFmpegGlue(FFmpegURLProtocol* protocol) {
  // Initialize an AVIOContext using our custom read and seek operations.  Don't
  // keep pointers to the buffer since FFmpeg may reallocate it on the fly.  It
  // will be cleaned up
  format_context_ = avformat_alloc_context();
  avio_context_.reset(avio_alloc_context(
      static_cast<unsigned char*>(av_malloc(kBufferSize)), kBufferSize, 0,
      protocol, &AVIOReadOperation, nullptr, &AVIOSeekOperation));

  // Ensure FFmpeg only tries to seek on resources we know to be seekable.
  avio_context_->seekable =
      protocol->IsStreaming() ? 0 : AVIO_SEEKABLE_NORMAL;

  // Ensure writing is disabled.
  avio_context_->write_flag = 0;

  // Tell the format context about our custom IO context.  avformat_open_input()
  // will set the AVFMT_FLAG_CUSTOM_IO flag for us, but do so here to ensure an
  // early error state doesn't cause FFmpeg to free our resources in error.
  format_context_->flags |= AVFMT_FLAG_CUSTOM_IO;

  // Enable fast, but inaccurate seeks for MP3.
  format_context_->flags |= AVFMT_FLAG_FAST_SEEK;

  // We don't allow H.264 parsing during demuxing since we have our own parser
  // and the ffmpeg one increases memory usage unnecessarily.
  if (base::FeatureList::IsEnabled(kForbidH264ParsingDuringDemuxing)) {
    format_context_->flags |= AVFMT_FLAG_NOH264PARSE;
  }

  // Ensures format parsing errors will bail out. From an audit on 11/2017, all
  // instances were real failures. Solves bugs like http://crbug.com/710791.
  format_context_->error_recognition |= AV_EF_EXPLODE;

  format_context_->pb = avio_context_.get();

  if (base::FeatureList::IsEnabled(kFFmpegAllowLists)) {
    // Enhance security by forbidding ffmpeg from decoding / demuxing codecs and
    // containers which should be unsupported.
    //
    // Normally these aren't even compiled in, but during codec/container
    // deprecations and when an external ffmpeg is used this adds extra
    // security.
    static const base::NoDestructor<std::string> kCombinedCodecList([]() {
      if (base::FeatureList::IsEnabled(kAllowOnlyAudioCodecsDuringDemuxing)) {
        // We also don't allow ffmpeg to use any video decoders during demuxing
        // since it's unnecessary for the codecs we use and just increases
        // memory usage.
        return std::string(GetAllowedAudioDecoders());
      }

      return base::JoinString(
          {GetAllowedAudioDecoders(), GetAllowedVideoDecoders()}, ",");
    }());

    // Note: FFmpeg will try to free these strings, so we must duplicate them.
    format_context_->codec_whitelist = av_strdup(kCombinedCodecList->c_str());
    format_context_->format_whitelist = av_strdup(GetAllowedDemuxers());
  }
}

// static
const char* FFmpegGlue::GetAllowedAudioDecoders() {
  static const base::NoDestructor<std::string> kAllowedAudioCodecs([]() {
    // This should match the configured lists in //third_party/ffmpeg.
    std::string allowed_decoders(
        "vorbis,libopus,flac,pcm_u8,pcm_s16le,pcm_s24le,pcm_s32le,pcm_f32le,"
        "mp3,pcm_s16be,pcm_s24be,pcm_mulaw,pcm_alaw");
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    allowed_decoders += ",aac";
#endif
    return allowed_decoders;
  }());
  return kAllowedAudioCodecs->c_str();
}

// static
const char* FFmpegGlue::GetAllowedVideoDecoders() {
  // This should match the configured lists in //third_party/ffmpeg.
#if BUILDFLAG(USE_PROPRIETARY_CODECS) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  return IsBuiltInVideoCodec(VideoCodec::kH264) ? "h264" : "";
#else
  return "";
#endif
}

bool FFmpegGlue::OpenContext(bool is_local_file) {
  DCHECK(!open_called_) << "OpenContext() shouldn't be called twice.";

  // If avformat_open_input() is called we have to take a slightly different
  // destruction path to avoid double frees.
  open_called_ = true;

  // We need to set the WAV decoder max size to what it had previously been set
  // to. The auto-selectable max size ends up at 64k, which is larger than the
  // read size from a MultiBufferDataSource, causing demuxer init to never
  // complete.
  AVDictionary* options = nullptr;
  av_dict_set(&options, "max_size", "4096", 0);

  // By passing nullptr for the filename (second parameter) we are telling
  // FFmpeg to use the AVIO context we setup from the AVFormatContext structure.
  const int ret =
      avformat_open_input(&format_context_, nullptr, nullptr, &options);

  if (options) {
    av_dict_free(&options);
  }

  // If FFmpeg can't identify the file, read the first 8k and attempt to guess
  // at the container type ourselves. This way we can track emergent formats.
  // Only try on AVERROR_INVALIDDATA to avoid running after I/O errors.
  if (ret == AVERROR_INVALIDDATA) {
    std::vector<uint8_t> buffer(8192);

    const int64_t pos = AVIOSeekOperation(avio_context_->opaque, 0, SEEK_SET);
    if (pos < 0)
      return false;

    const int num_read =
        AVIOReadOperation(avio_context_->opaque, buffer.data(), buffer.size());
    if (num_read < container_names::kMinimumContainerSize)
      return false;

    container_ = container_names::DetermineContainer(buffer.data(), num_read);
    LogContainer(is_local_file, container_);

    detected_hls_ =
        container_ == container_names::MediaContainerName::kContainerHLS;
    return false;
  } else if (ret < 0) {
    return false;
  }

  // Rely on ffmpeg's parsing if we're able to successfully open the file.
  if (strcmp(format_context_->iformat->name, "mov,mp4,m4a,3gp,3g2,mj2") == 0)
    container_ = container_names::MediaContainerName::kContainerMOV;
  else if (strcmp(format_context_->iformat->name, "flac") == 0)
    container_ = container_names::MediaContainerName::kContainerFLAC;
  else if (strcmp(format_context_->iformat->name, "matroska,webm") == 0)
    container_ = container_names::MediaContainerName::kContainerWEBM;
  else if (strcmp(format_context_->iformat->name, "ogg") == 0)
    container_ = container_names::MediaContainerName::kContainerOgg;
  else if (strcmp(format_context_->iformat->name, "wav") == 0)
    container_ = container_names::MediaContainerName::kContainerWAV;
  else if (strcmp(format_context_->iformat->name, "aac") == 0)
    container_ = container_names::MediaContainerName::kContainerAAC;
  else if (strcmp(format_context_->iformat->name, "mp3") == 0)
    container_ = container_names::MediaContainerName::kContainerMP3;
  else if (strcmp(format_context_->iformat->name, "amr") == 0)
    container_ = container_names::MediaContainerName::kContainerAMR;
  else if (strcmp(format_context_->iformat->name, "avi") == 0)
    container_ = container_names::MediaContainerName::kContainerAVI;

  // For a successfully opened file, we will get a container we've compiled in.
  CHECK_NE(container_, container_names::MediaContainerName::kContainerUnknown);
  LogContainer(is_local_file, container_);

  return true;
}

FFmpegGlue::~FFmpegGlue() {
  // In the event of avformat_open_input() failure, FFmpeg may sometimes free
  // our AVFormatContext behind the scenes, but leave the buffer alive.  It will
  // helpfully set |format_context_| to nullptr in this case.
  if (!format_context_) {
    av_free(avio_context_->buffer);
    return;
  }

  // If avformat_open_input() hasn't been called, we should simply free the
  // AVFormatContext and buffer instead of using avformat_close_input().
  if (!open_called_) {
    avformat_free_context(format_context_);
    av_free(avio_context_->buffer);
    return;
  }

  avformat_close_input(&format_context_);
  av_free(avio_context_->buffer);
}

}  // namespace media
