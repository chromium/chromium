// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_ENCODER_H_
#define MEDIA_BASE_VIDEO_ENCODER_H_

#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "media/base/bitrate.h"
#include "media/base/encoder_status.h"
#include "media/base/media_export.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_codecs.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace media {

struct VideoEncoderInfo;
class VideoFrame;

MEDIA_EXPORT uint32_t GetDefaultVideoEncodeBitrate(gfx::Size frame_size,
                                                   uint32_t framerate);

MEDIA_EXPORT int GetNumberOfThreadsForSoftwareEncoding(gfx::Size frame_size);

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
  int temporal_id = 0;
  gfx::ColorSpace color_space;

  // Some platforms may adjust the encoding size to meet hardware requirements.
  // If not set, the encoded size is the same as configured.
  absl::optional<gfx::Size> encoded_size;
};

class MEDIA_EXPORT VideoEncoder {
 public:
  // TODO: Move this to a new file if there are more codec specific options.
  struct MEDIA_EXPORT AvcOptions {
    bool produce_annexb = false;
  };

  struct MEDIA_EXPORT HevcOptions {
    bool produce_annexb = false;
  };

  enum class LatencyMode { Realtime, Quality };

  struct MEDIA_EXPORT Options {
    Options();
    Options(const Options&);
    ~Options();
    absl::optional<Bitrate> bitrate;
    absl::optional<double> framerate;

    gfx::Size frame_size;

    absl::optional<int> keyframe_interval = 10000;

    LatencyMode latency_mode = LatencyMode::Realtime;

    absl::optional<SVCScalabilityMode> scalability_mode;

    // Only used for H264 encoding.
    AvcOptions avc;

    // Only used for HEVC encoding.
    HevcOptions hevc;
  };

  struct MEDIA_EXPORT EncodeOptions {
    explicit EncodeOptions(bool key_frame);
    EncodeOptions();
    EncodeOptions(const EncodeOptions&);
    ~EncodeOptions();
    bool key_frame = false;
    // Per-frame codec-specific quantizer value.
    // Should only be used when encoder configured with kExternal bitrate mode.
    absl::optional<double> quantizer;
  };

  // A sequence of codec specific bytes, commonly known as extradata.
  // If available, it should be given to the decoder as part of the
  // decoder config.
  using CodecDescription = std::vector<uint8_t>;

  // Provides the VideoEncoder client with information about the specific
  // encoder implementation.
  using EncoderInfoCB =
      base::RepeatingCallback<void(const VideoEncoderInfo& encoder_info)>;

  // Callback for VideoEncoder to report an encoded video frame whenever it
  // becomes available.
  using OutputCB =
      base::RepeatingCallback<void(VideoEncoderOutput output,
                                   absl::optional<CodecDescription>)>;

  // Callback to report success and errors in encoder calls.
  using EncoderStatusCB = base::OnceCallback<void(EncoderStatus error)>;

  struct MEDIA_EXPORT PendingEncode {
    PendingEncode();
    PendingEncode(PendingEncode&&);
    ~PendingEncode();
    EncoderStatusCB done_callback;
    scoped_refptr<VideoFrame> frame;
    EncodeOptions options;
  };

  VideoEncoder();
  VideoEncoder(const VideoEncoder&) = delete;
  VideoEncoder& operator=(const VideoEncoder&) = delete;
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
                          EncoderInfoCB info_cb,
                          OutputCB output_cb,
                          EncoderStatusCB done_cb) = 0;

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
                      const EncodeOptions& options,
                      EncoderStatusCB done_cb) = 0;

  // Adjust encoder options and the output callback for future frames, executing
  // the |done_cb| upon completion.
  //
  // Note:
  // 1. Not all options can be changed on the fly.
  // 2. ChangeOptions() should be called after calling Flush() and waiting
  // for it to finish.
  virtual void ChangeOptions(const Options& options,
                             OutputCB output_cb,
                             EncoderStatusCB done_cb) = 0;

  // Requests all outputs for already encoded frames to be
  // produced via |output_cb| and calls |dene_cb| after that.
  virtual void Flush(EncoderStatusCB done_cb) = 0;

  // Normally VideoEncoder implementations aren't supposed to call
  // EncoderInfoCB, OutputCB, and EncoderStatusCB directly from inside any of
  // VideoEncoder's methods.  This method tells VideoEncoder that all callbacks
  // can be called directly from within its methods. It saves extra thread hops
  // if it's known that all callbacks already point to a task runner different
  // from the current one.
  virtual void DisablePostedCallbacks();

 protected:
  template <typename Callback>
  Callback BindCallbackToCurrentLoopIfNeeded(Callback callback) {
    return post_callbacks_
               ? base::BindPostTaskToCurrentDefault(std::move(callback))
               : std::move(callback);
  }

 private:
  bool post_callbacks_ = true;
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_ENCODER_H_
