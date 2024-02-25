// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FFmpegGlue is an interface between FFmpeg and Chrome used to proxy FFmpeg's
// read and seek requests to Chrome's internal data structures.  The glue works
// through the AVIO interface provided by FFmpeg.
//
// AVIO works through a special AVIOContext created through avio_alloc_context()
// which is attached to the AVFormatContext used for demuxing.  The AVIO context
// is initialized with read and seek methods which FFmpeg calls when necessary.
//
// During OpenContext() FFmpegGlue will tell FFmpeg to use Chrome's AVIO context
// by passing NULL in for the filename parameter to avformat_open_input().  All
// FFmpeg operations using the configured AVFormatContext will then redirect
// reads and seeks through the glue.
//
// The glue in turn processes those read and seek requests using the
// FFmpegURLProtocol provided during construction.

#ifndef MEDIA_FILTERS_FFMPEG_GLUE_H_
#define MEDIA_FILTERS_FFMPEG_GLUE_H_

#include <stdint.h>

#include <memory>

#include "base/check.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "media/base/container_names.h"
#include "media/base/media_export.h"
#include "media/ffmpeg/ffmpeg_deleters.h"

struct AVFormatContext;
struct AVIOContext;

namespace media {

class MEDIA_EXPORT FFmpegURLProtocol {
 public:
  // Read the given amount of bytes into data, returns the number of bytes read
  // if successful, kReadError otherwise.
  virtual int Read(int size, uint8_t* data) = 0;

  // Returns true and the current file position for this file, false if the
  // file position could not be retrieved.
  virtual bool GetPosition(int64_t* position_out) = 0;

  // Returns true if the file position could be set, false otherwise.
  virtual bool SetPosition(int64_t position) = 0;

  // Returns true and the file size, false if the file size could not be
  // retrieved.
  virtual bool GetSize(int64_t* size_out) = 0;

  // Returns false if this protocol supports random seeking.
  virtual bool IsStreaming() = 0;
};

class MEDIA_EXPORT FFmpegGlue {
 public:
  // See file documentation for usage.  |protocol| must outlive FFmpegGlue.
  explicit FFmpegGlue(FFmpegURLProtocol* protocol);

  FFmpegGlue(const FFmpegGlue&) = delete;
  FFmpegGlue& operator=(const FFmpegGlue&) = delete;

  ~FFmpegGlue();

  // Returns the list of allowed decoders for audio/video respectively.
  static const char* GetAllowedAudioDecoders();
  static const char* GetAllowedVideoDecoders();

  // Opens an AVFormatContext specially prepared to process reads and seeks
  // through the FFmpegURLProtocol provided during construction.
  // |is_local_file| is an optional parameter used for metrics reporting.
  bool OpenContext(bool is_local_file = false);
  AVFormatContext* format_context() { return format_context_; }
  // Returns the container name.
  // Note that it is only available after calling OpenContext.
  container_names::MediaContainerName container() const {
    DCHECK(open_called_);
    return container_;
  }

  // Used on Android to switch to using the native MediaPlayer to play HLS.
  bool detected_hls() { return detected_hls_; }

 private:
  bool open_called_ = false;
  bool detected_hls_ = false;

  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION AVFormatContext* format_context_ = nullptr;
  std::unique_ptr<AVIOContext, ScopedPtrAVFree> avio_context_;
  container_names::MediaContainerName container_ =
      container_names::MediaContainerName::kContainerUnknown;
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_GLUE_H_
