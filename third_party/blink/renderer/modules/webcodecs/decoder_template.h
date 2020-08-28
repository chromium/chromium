// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_DECODER_TEMPLATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_DECODER_TEMPLATE_H_

#include <stdint.h>
#include <memory>

#include "media/base/decode_status.h"
#include "media/base/media_log.h"
#include "media/base/status.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_codec_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_codecs_error_callback.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_config_eval.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

template <typename Traits>
class MODULES_EXPORT DecoderTemplate : public ScriptWrappable {
 public:
  typedef typename Traits::ConfigType ConfigType;
  typedef typename Traits::MediaConfigType MediaConfigType;
  typedef typename Traits::InputType InputType;
  typedef typename Traits::InitType InitType;
  typedef typename Traits::MediaDecoderType MediaDecoderType;
  typedef typename Traits::MediaOutputType MediaOutputType;
  typedef typename Traits::OutputType OutputType;
  typedef typename Traits::OutputCallbackType OutputCallbackType;

  DecoderTemplate(ScriptState*, const InitType*, ExceptionState&);
  ~DecoderTemplate() override;

  int32_t decodeQueueSize();
  void configure(const ConfigType*, ExceptionState&);
  void decode(const InputType*, ExceptionState&);
  ScriptPromise flush(ExceptionState&);
  void reset(ExceptionState&);
  void close(ExceptionState&);
  String state() const { return state_; }

  // GarbageCollected override.
  void Trace(Visitor*) const override;

 protected:
  // TODO(sandersd): Consider moving these to the Traits class, and creating an
  // instance of the traits.

  // Convert a configuration to a DecoderConfig.
  virtual CodecConfigEval MakeMediaConfig(const ConfigType& config,
                                          MediaConfigType* out_media_config,
                                          String* out_console_message) = 0;

  // Convert a chunk to a DecoderBuffer. You can assume that the last
  // configuration sent to MakeMediaConfig() is the active configuration for
  // |chunk|.
  virtual scoped_refptr<media::DecoderBuffer> MakeDecoderBuffer(
      const InputType& chunk) = 0;

 private:
  struct Request final : public GarbageCollected<Request> {
    enum class Type {
      kConfigure,
      kDecode,
      kFlush,
      kReset,
    };

    void Trace(Visitor*) const;

    Type type;

    // For kConfigure Requests.
    std::unique_ptr<MediaConfigType> media_config;

    // For kDecode Requests.
    scoped_refptr<media::DecoderBuffer> decoder_buffer;

    // For kFlush Requests.
    Member<ScriptPromiseResolver> resolver;
  };

  void ProcessRequests();
  bool ProcessConfigureRequest(Request* request);
  bool ProcessDecodeRequest(Request* request);
  bool ProcessFlushRequest(Request* request);
  bool ProcessResetRequest(Request* request);
  void HandleError();
  void Shutdown(bool is_error);

  // Called by |decoder_|.
  void OnInitializeDone(media::Status status);
  void OnDecodeDone(uint32_t id, media::DecodeStatus);
  void OnFlushDone(media::DecodeStatus);
  void OnConfigureFlushDone(media::DecodeStatus);
  void OnResetDone();
  void OnOutput(scoped_refptr<MediaOutputType>);

  // Helper function making it easier to check |state_|.
  bool IsClosed();

  Member<ScriptState> script_state_;
  Member<OutputCallbackType> output_cb_;
  Member<V8WebCodecsErrorCallback> error_cb_;

  HeapDeque<Member<Request>> requests_;
  int32_t requested_decodes_ = 0;
  int32_t requested_resets_ = 0;

  // Which state the codec is in, determining which calls we can receive.
  V8CodecState state_;

  // An in-flight, mutually-exclusive request.
  // Could be a configure, flush, or reset. Decodes go in |pending_decodes_|.
  Member<Request> pending_request_;

  std::unique_ptr<media::MediaLog> media_log_;

  // TODO(sandersd): Store the last config, flush, and reset so that
  // duplicates can be elided.
  std::unique_ptr<MediaDecoderType> decoder_;
  bool initializing_sync_ = false;

  // TODO(sandersd): Can this just be a HashSet by ptr comparison?
  uint32_t pending_decode_id_ = 0;
  HeapHashMap<uint32_t, Member<Request>> pending_decodes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_DECODER_TEMPLATE_H_
