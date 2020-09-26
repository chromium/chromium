// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_ENCODER_H_
#define MEDIA_BASE_VIDEO_ENCODER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "media/base/media_export.h"
#include "media/base/status.h"
#include "media/base/video_codecs.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class VideoFrame;

// Encoded video frame, its data and metadata.
struct MEDIA_EXPORT VideoEncoderOutput {
  VideoEncoderOutput();
  VideoEncoderOutput(VideoEncoderOutput&&);
  ~VideoEncoderOutput();

  // Feel free take this buffer out and use underlying memory as is without
  // copying.
  std::unique_ptr<uint8_t[]> data;
  size_t size = 0;

  base::TimeDelta timestamp;
  bool key_frame = false;
};

class MEDIA_EXPORT VideoEncoder {
 public:
  struct MEDIA_EXPORT Options {
    Options();
    Options(const Options&);
    ~Options();
    base::Optional<uint64_t> bitrate;
    double framerate = 30.0;

    int width = 0;
    int height = 0;

    base::Optional<int> keyframe_interval = 10000;
  };

  // A sequence of codec specific bytes, commonly known as extradata.
  // If available, it should be given to the decoder as part of the
  // decoder config.
  using CodecDescription = std::vector<uint8_t>;

  // Callback for VideoEncoder to report an encoded video frame whenever it
  // becomes available.
  using OutputCB =
      base::RepeatingCallback<void(VideoEncoderOutput output,
                                   base::Optional<CodecDescription>)>;

  // Callback to report success and errors in encoder calls.
  using StatusCB = base::OnceCallback<void(Status error)>;

  VideoEncoder();
  virtual ~VideoEncoder();

  // Initializes a VideoEncoder with the given |options|, executing the
  // |done_cb| upon completion. |output_cb| is called for each encoded frame
  // produced by the coder.
  //
  // Note:
  // 1) Can't be called more than once for the same instance of the encoder.
  // 2) No VideoEncoder calls should be made before |done_cb| is executed.
  virtual void Initialize(VideoCodecProfile profile,
                          const Options& options,
                          OutputCB output_cb,
                          StatusCB done_cb) = 0;

  // Requests a |frame| to be encoded. The status of the encoder and the frame
  // are returned via the provided callback |done_cb|.
  //
  // |done_cb| will not be called from within this method, and that it will be
  // called even if Encode() is never called again.

  // After the frame, or several frames, are encoded the encoder calls
  // |output_cb| specified in Initialize() for available VideoEncoderOutput.
  // |output_cb| may be called before or after |done_cb|,
  // including before Encode() returns.
  // Encode() does not expect EOS frames, use Flush() to finalize the stream
  // and harvest the outputs.
  virtual void Encode(scoped_refptr<VideoFrame> frame,
                      bool key_frame,
                      StatusCB done_cb) = 0;

  // Adjust encoder options for future frames, executing the
  // |done_cb| upon completion.
  //
  // Note:
  // 1. Not all options can be changed on the fly.
  // 2. ChangeOptions() should be called after calling Flush() and waiting
  // for it to finish.
  virtual void ChangeOptions(const Options& options, StatusCB done_cb) = 0;

  // Requests all outputs for already encoded frames to be
  // produced via |output_cb| and calls |dene_cb| after that.
  virtual void Flush(StatusCB done_cb) = 0;

 protected:
  DISALLOW_COPY_AND_ASSIGN(VideoEncoder);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_ENCODER_H_
