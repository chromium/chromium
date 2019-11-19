// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_DECODER_H_
#define MEDIA_BASE_VIDEO_DECODER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/decode_status.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/base/waiting.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class CdmContext;
class DecoderBuffer;
class VideoDecoderConfig;
class VideoFrame;

class MEDIA_EXPORT VideoDecoder {
 public:
  // Callback for VideoDecoder initialization.
  using InitCB = base::OnceCallback<void(bool success)>;

  // Callback for VideoDecoder to return a decoded frame whenever it becomes
  // available. Only non-EOS frames should be returned via this callback.
  using OutputCB = base::RepeatingCallback<void(scoped_refptr<VideoFrame>)>;

  // Callback type for Decode(). Called after the decoder has completed decoding
  // corresponding DecoderBuffer, indicating that it's ready to accept another
  // buffer to decode.
  using DecodeCB = base::OnceCallback<void(DecodeStatus)>;

  VideoDecoder();

  // Returns the name of the decoder for logging and decoder selection purposes.
  // This name should be available immediately after construction (e.g. before
  // Initialize() is called). It should also be stable in the sense that the
  // name does not change across multiple constructions.
  virtual std::string GetDisplayName() const = 0;

  // Returns true if the implementation is expected to be implemented by the
  // platform. The value should be available immediately after construction and
  // should not change within the lifetime of a decoder instance. The value is
  // used for logging and metrics recording.
  //
  // TODO(sandersd): Use this to decide when to switch to software decode for
  // low-resolution videos. https://crbug.com/684792
  virtual bool IsPlatformDecoder() const;

  // Initializes a VideoDecoder with the given |config|, executing the
  // |init_cb| upon completion. |output_cb| is called for each output frame
  // decoded by Decode().
  //
  // If |low_delay| is true then the decoder is not allowed to queue frames,
  // except for out-of-order frames, i.e. if the next frame can be returned it
  // must be returned without waiting for Decode() to be called again.
  // Initialization should fail if |low_delay| is true and the decoder cannot
  // satisfy the requirements above.
  //
  // |cdm_context| can be used to handle encrypted buffers. May be null if the
  // stream is not encrypted.
  //
  // |waiting_cb| is called whenever the decoder is stalled waiting for
  // something, e.g. decryption key. May be called at any time after
  // Initialize().
  //
  // Note:
  // 1) The VideoDecoder will be reinitialized if it was initialized before.
  //    Upon reinitialization, all internal buffered frames will be dropped.
  // 2) This method should not be called during pending decode or reset.
  // 3) No VideoDecoder calls should be made before |init_cb| is executed.
  // 4) VideoDecoders should take care to run |output_cb| as soon as the frame
  // is ready (i.e. w/o thread trampolining) since it can strongly affect frame
  // delivery times with high-frame-rate material.  See Decode() for additional
  // notes.
  // 5) |init_cb| may be called before this returns.
  virtual void Initialize(const VideoDecoderConfig& config,
                          bool low_delay,
                          CdmContext* cdm_context,
                          InitCB init_cb,
                          const OutputCB& output_cb,
                          const WaitingCB& waiting_cb) = 0;

  // Requests a |buffer| to be decoded. The status of the decoder and decoded
  // frame are returned via the provided callback. Some decoders may allow
  // decoding multiple buffers in parallel. Callers should call
  // GetMaxDecodeRequests() to get number of buffers that may be decoded in
  // parallel.
  //
  // Implementations guarantee that the |decode_cb| will not be called from
  // within this method, and that it will be called even if Decode() is never
  // called again.
  //
  // After decoding is finished the decoder calls |output_cb| specified in
  // Initialize() for each decoded frame. |output_cb| may be called before or
  // after |decode_cb|, including before Decode() returns.
  //
  // If |buffer| is an EOS buffer then the decoder must be flushed, i.e.
  // |output_cb| must be called for each frame pending in the queue and
  // |decode_cb| must be called after that. Callers will not call Decode()
  // again until after the flush completes.
  virtual void Decode(scoped_refptr<DecoderBuffer> buffer,
                      DecodeCB decode_cb) = 0;

  // Resets decoder state. All pending Decode() requests will be finished or
  // aborted before |closure| is called.
  // Note: No VideoDecoder calls should be made before |closure| is executed.
  virtual void Reset(base::OnceClosure closure) = 0;

  // Returns true if the decoder needs bitstream conversion before decoding.
  virtual bool NeedsBitstreamConversion() const;

  // Returns true if the decoder currently has the ability to decode and return
  // a VideoFrame. Most implementations can allocate a new VideoFrame and hence
  // this will always return true. Override and return false for decoders that
  // use a fixed set of VideoFrames for decoding.
  virtual bool CanReadWithoutStalling() const;

  // Returns maximum number of parallel decode requests.
  virtual int GetMaxDecodeRequests() const;

  // Returns the recommended number of threads for software video decoding. If
  // the --video-threads command line option is specified and is valid, that
  // value is returned. Otherwise |desired_threads| is clamped to the number of
  // logical processors and then further clamped to
  // [|limits::kMinVideoDecodeThreads|, |limits::kMaxVideoDecodeThreads|].
  static int GetRecommendedThreadCount(int desired_threads);

 protected:
  // Deletion is only allowed via Destroy().
  virtual ~VideoDecoder();

 private:
  friend struct std::default_delete<VideoDecoder>;

  // Fires any pending callbacks, stops and destroys the decoder. After this
  // call, external resources (e.g. raw pointers) |this| holds might be
  // invalidated immediately. So if the decoder is destroyed asynchronously
  // (e.g. DeleteSoon), external resources must be released in this call.
  virtual void Destroy();

  DISALLOW_COPY_AND_ASSIGN(VideoDecoder);
};

}  // namespace media

namespace std {

// Specialize std::default_delete to call Destroy().
template <>
struct MEDIA_EXPORT default_delete<media::VideoDecoder> {
  constexpr default_delete() = default;

  template <typename U,
            typename = typename std::enable_if<
                std::is_convertible<U*, media::VideoDecoder*>::value>::type>
  default_delete(const default_delete<U>& d) {}

  void operator()(media::VideoDecoder* ptr) const;
};

}  // namespace std

#endif  // MEDIA_BASE_VIDEO_DECODER_H_
