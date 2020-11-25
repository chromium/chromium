// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_ENCODER_H_

#include <memory>

#include "base/optional.h"
#include "media/base/media_log.h"
#include "media/base/status.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_encoder.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_codec_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_output_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_codecs_error_callback.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"

namespace media {
class VideoEncoder;
struct VideoEncoderOutput;
}  // namespace media

namespace blink {

class ExceptionState;
enum class DOMExceptionCode;
class VideoEncoderConfig;
class VideoEncoderInit;
class VideoEncoderEncodeOptions;
class Visitor;

class MODULES_EXPORT VideoEncoder final
    : public ScriptWrappable,
      public ActiveScriptWrappable<VideoEncoder>,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VideoEncoder* Create(ScriptState*,
                              const VideoEncoderInit*,
                              ExceptionState&);
  VideoEncoder(ScriptState*, const VideoEncoderInit*, ExceptionState&);
  ~VideoEncoder() override;

  // video_encoder.idl implementation.
  int32_t encodeQueueSize();

  void encode(VideoFrame* frame,
              const VideoEncoderEncodeOptions*,
              ExceptionState&);

  void configure(const VideoEncoderConfig*, ExceptionState&);

  ScriptPromise flush(ExceptionState&);

  void reset(ExceptionState&);

  void close(ExceptionState&);

  String state() { return state_; }

  // ExecutionContextLifecycleObserver override.
  void ContextDestroyed() override;

  // ScriptWrappable override.
  bool HasPendingActivity() const override;

  // GarbageCollected override.
  void Trace(Visitor*) const override;

 private:
  enum class AccelerationPreference { kAllow, kDeny, kRequire };

  // TODO(ezemtsov): Replace this with a {Audio|Video}EncoderConfig.
  struct ParsedConfig final {
    media::VideoCodec codec;
    media::VideoCodecProfile profile;
    uint8_t level;
    media::VideoColorSpace color_space;

    AccelerationPreference acc_pref;

    media::VideoEncoder::Options options;
    String codec_string;
  };

  struct Request final : public GarbageCollected<Request> {
    enum class Type {
      kConfigure,
      kEncode,
      kFlush,
    };

    void Trace(Visitor*) const;

    Type type;
    // Current value of VideoEncoder.reset_count_ when request was created.
    uint32_t reset_count = 0;
    Member<VideoFrame> frame;                            // used by kEncode
    Member<const VideoEncoderEncodeOptions> encodeOpts;  // used by kEncode
    Member<ScriptPromiseResolver> resolver;              // used by kFlush
  };

  void CallOutputCallback(
      uint32_t reset_count,
      media::VideoEncoderOutput output,
      base::Optional<media::VideoEncoder::CodecDescription> codec_desc);
  void HandleError(DOMException* ex);
  void HandleError(std::string context, media::Status);
  void EnqueueRequest(Request* request);
  void ProcessRequests();
  void ProcessEncode(Request* request);
  void ProcessConfigure(Request* request);
  void ProcessFlush(Request* request);

  void UpdateEncoderLog(std::string encoder_name, bool is_hw_accelerated);

  void ResetInternal();
  ScriptPromiseResolver* MakePromise();
  void ResolvePromise(Request* req);
  void RejectPromise(Request* req, DOMException* ex = nullptr);

  std::unique_ptr<ParsedConfig> ParseConfig(const VideoEncoderConfig*,
                                            ExceptionState&);
  bool VerifyCodecSupport(ParsedConfig*, ExceptionState&);
  std::unique_ptr<media::VideoEncoder> CreateMediaVideoEncoder(
      const ParsedConfig& config);

  std::unique_ptr<ParsedConfig> active_config_;
  std::unique_ptr<media::VideoEncoder> media_encoder_;
  bool is_hw_accelerated_ = false;

  // |parent_media_log_| must be destroyed if ever the ExecutionContext is
  // destroyed, since the blink::MediaInspectorContext* pointer given to
  // InspectorMediaEventHandler might no longer be valid.
  // |parent_media_log_| should not be used directly. Use |media_log_| instead.
  std::unique_ptr<media::MediaLog> parent_media_log_;

  // We might destroy |parent_media_log_| at any point, so keep a clone which
  // can be safely accessed, and whose raw pointer can be given callbacks.
  std::unique_ptr<media::MediaLog> media_log_;

  V8CodecState state_;

  Member<ScriptState> script_state_;
  Member<V8VideoEncoderOutputCallback> output_callback_;
  Member<V8WebCodecsErrorCallback> error_callback_;
  HeapDeque<Member<Request>> requests_;
  int32_t requested_encodes_ = 0;
  // How many times reset() was called on the encoder. It's used to decide
  // when a callback needs to be dismissed because reset() was called between
  // an operation and its callback.
  uint32_t reset_count_ = 0;

  // Number of not resolved/rejected promises created by this VideoEncoder.
  uint32_t outstanding_promises_ = 0;

  // Some kConfigure and kFlush requests can't be executed in parallel with
  // kEncode. This flag stops processing of new requests in the requests_ queue
  // till the current requests is finished.
  bool stall_request_processing_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_ENCODER_H_
