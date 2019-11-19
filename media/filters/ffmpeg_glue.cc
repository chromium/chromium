// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_glue.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "media/base/container_names.h"
#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

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
      NOTREACHED();
  }
  return new_offset;
}

static void LogContainer(bool is_local_file,
                         container_names::MediaContainerName container) {
  base::UmaHistogramSparse("Media.DetectedContainer", container);
  if (is_local_file)
    base::UmaHistogramSparse("Media.DetectedContainer.Local", container);
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

  // Ensures we can read out various metadata bits like vp8 alpha.
  format_context_->flags |= AVFMT_FLAG_KEEP_SIDE_DATA;

  // Ensures format parsing errors will bail out. From an audit on 11/2017, all
  // instances were real failures. Solves bugs like http://crbug.com/710791.
  format_context_->error_recognition |= AV_EF_EXPLODE;

  format_context_->pb = avio_context_.get();
}

bool FFmpegGlue::OpenContext(bool is_local_file) {
  DCHECK(!open_called_) << "OpenContext() shouldn't be called twice.";

  // If avformat_open_input() is called we have to take a slightly different
  // destruction path to avoid double frees.
  open_called_ = true;

  // By passing nullptr for the filename (second parameter) we are telling
  // FFmpeg to use the AVIO context we setup from the AVFormatContext structure.
  const int ret =
      avformat_open_input(&format_context_, nullptr, nullptr, nullptr);

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
        container_ == container_names::MediaContainerName::CONTAINER_HLS;
    return false;
  } else if (ret < 0) {
    return false;
  }

  // Rely on ffmpeg's parsing if we're able to succesfully open the file.
  if (strcmp(format_context_->iformat->name, "mov,mp4,m4a,3gp,3g2,mj2") == 0)
    container_ = container_names::CONTAINER_MOV;
  else if (strcmp(format_context_->iformat->name, "flac") == 0)
    container_ = container_names::CONTAINER_FLAC;
  else if (strcmp(format_context_->iformat->name, "matroska,webm") == 0)
    container_ = container_names::CONTAINER_WEBM;
  else if (strcmp(format_context_->iformat->name, "ogg") == 0)
    container_ = container_names::CONTAINER_OGG;
  else if (strcmp(format_context_->iformat->name, "wav") == 0)
    container_ = container_names::CONTAINER_WAV;
  else if (strcmp(format_context_->iformat->name, "aac") == 0)
    container_ = container_names::CONTAINER_AAC;
  else if (strcmp(format_context_->iformat->name, "mp3") == 0)
    container_ = container_names::CONTAINER_MP3;
  else if (strcmp(format_context_->iformat->name, "amr") == 0)
    container_ = container_names::CONTAINER_AMR;
  else if (strcmp(format_context_->iformat->name, "avi") == 0)
    container_ = container_names::CONTAINER_AVI;

  DCHECK_NE(container_, container_names::CONTAINER_UNKNOWN);
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
