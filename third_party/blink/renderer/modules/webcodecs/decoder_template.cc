// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/decoder_template.h"

#include <limits>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/media_util.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_decoder.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_config_eval.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_state_helper.h"
#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

template <typename Traits>
DecoderTemplate<Traits>::DecoderTemplate(ScriptState* script_state,
                                         const InitType* init,
                                         ExceptionState& exception_state)
    : script_state_(script_state), state_(V8CodecState::Enum::kUnconfigured) {
  DVLOG(1) << __func__;
  DCHECK(init->hasOutput());
  DCHECK(init->hasError());
  output_cb_ = init->output();
  error_cb_ = init->error();
}

template <typename Traits>
DecoderTemplate<Traits>::~DecoderTemplate() {
  DVLOG(1) << __func__;
}

template <typename Traits>
int32_t DecoderTemplate<Traits>::decodeQueueSize() {
  return requested_decodes_;
}

template <typename Traits>
bool DecoderTemplate<Traits>::IsClosed() {
  return state_ == V8CodecState::Enum::kClosed;
}

template <typename Traits>
void DecoderTemplate<Traits>::configure(const ConfigType* config,
                                        ExceptionState& exception_state) {
  DVLOG(1) << __func__;
  if (ThrowIfCodecStateClosed(state_, "decode", exception_state))
    return;

  auto media_config = std::make_unique<MediaConfigType>();
  String console_message;

  CodecConfigEval eval =
      MakeMediaConfig(*config, media_config.get(), &console_message);
  switch (eval) {
    case CodecConfigEval::kInvalid:
      exception_state.ThrowTypeError(console_message);
      return;
    case CodecConfigEval::kUnsupported:
      exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                        console_message);
      return;
    case CodecConfigEval::kSupported:
      // Good, lets proceed.
      break;
  }

  state_ = V8CodecState(V8CodecState::Enum::kConfigured);

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kConfigure;
  request->media_config = std::move(media_config);
  requests_.push_back(request);
  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::decode(const InputType* chunk,
                                     ExceptionState& exception_state) {
  DVLOG(3) << __func__;
  if (ThrowIfCodecStateClosed(state_, "decode", exception_state))
    return;

  if (ThrowIfCodecStateUnconfigured(state_, "decode", exception_state))
    return;

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kDecode;
  request->decoder_buffer = MakeDecoderBuffer(*chunk);
  requests_.push_back(request);
  ++requested_decodes_;
  ProcessRequests();
}

template <typename Traits>
ScriptPromise DecoderTemplate<Traits>::flush(ExceptionState& exception_state) {
  DVLOG(3) << __func__;
  if (ThrowIfCodecStateClosed(state_, "flush", exception_state))
    return ScriptPromise();

  if (ThrowIfCodecStateUnconfigured(state_, "flush", exception_state))
    return ScriptPromise();

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kFlush;
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  request->resolver = resolver;
  requests_.push_back(request);
  ProcessRequests();
  return resolver->Promise();
}

template <typename Traits>
void DecoderTemplate<Traits>::reset(ExceptionState& exception_state) {
  DVLOG(3) << __func__;
  if (ThrowIfCodecStateClosed(state_, "reset", exception_state))
    return;

  if (state_ == V8CodecState::Enum::kUnconfigured)
    return;

  state_ = V8CodecState(V8CodecState::Enum::kUnconfigured);

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kReset;
  requests_.push_back(request);
  ++requested_resets_;
  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::close(ExceptionState& exception_state) {
  DVLOG(3) << __func__;
  if (ThrowIfCodecStateClosed(state_, "close", exception_state))
    return;

  Shutdown(false);
}

template <typename Traits>
void DecoderTemplate<Traits>::ProcessRequests() {
  DVLOG(3) << __func__;
  DCHECK(!IsClosed());
  while (!pending_request_ && !requests_.IsEmpty()) {
    Request* request = requests_.front();
    switch (request->type) {
      case Request::Type::kConfigure:
        if (!ProcessConfigureRequest(request))
          return;
        break;
      case Request::Type::kDecode:
        if (!ProcessDecodeRequest(request))
          return;
        break;
      case Request::Type::kFlush:
        if (!ProcessFlushRequest(request))
          return;
        break;
      case Request::Type::kReset:
        if (!ProcessResetRequest(request))
          return;
        break;
    }
    requests_.pop_front();
  }
}

template <typename Traits>
bool DecoderTemplate<Traits>::ProcessConfigureRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK(!IsClosed());
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK(request->media_config);

  // TODO(sandersd): Record this configuration as pending but don't apply it
  // until there is a decode request.

  if (!decoder_) {
    media_log_ = std::make_unique<media::NullMediaLog>();
    decoder_ = Traits::CreateDecoder(*ExecutionContext::From(script_state_),
                                     media_log_.get());
    if (!decoder_) {
      HandleError();
      return false;
    }

    // Processing continues in OnInitializeDone().
    // Note: OnInitializeDone() must not call ProcessRequests() reentrantly,
    // which can happen if InitializeDecoder() calls it synchronously.
    pending_request_ = request;
    initializing_sync_ = true;
    Traits::InitializeDecoder(
        *decoder_, *pending_request_->media_config,
        WTF::Bind(&DecoderTemplate::OnInitializeDone, WrapWeakPersistent(this)),
        WTF::BindRepeating(&DecoderTemplate::OnOutput,
                           WrapWeakPersistent(this)));
    initializing_sync_ = false;
    return true;
  }

  // Note: This flush must not be elided when there is a pending reset. An
  // alternative would be to process Reset() requests immediately, then process
  // already queued requests in a special mode. It seems easier to drop all of
  // this and require configure() after reset() instead.
  if (pending_decodes_.size() + 1 >
      size_t{Traits::GetMaxDecodeRequests(*decoder_)}) {
    // Try again after OnDecodeDone().
    return false;
  }

  // Processing continues in OnConfigureFlushDone().
  pending_request_ = request;
  decoder_->Decode(media::DecoderBuffer::CreateEOSBuffer(),
                   WTF::Bind(&DecoderTemplate::OnConfigureFlushDone,
                             WrapWeakPersistent(this)));
  return true;
}

template <typename Traits>
bool DecoderTemplate<Traits>::ProcessDecodeRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kDecode);
  DCHECK_GT(requested_decodes_, 0);

  // TODO(sandersd): If a reset has been requested, complete immediately.

  if (!decoder_) {
    HandleError();
    return false;
  }

  if (pending_decodes_.size() + 1 >
      size_t{Traits::GetMaxDecodeRequests(*decoder_)}) {
    // Try again after OnDecodeDone().
    return false;
  }

  // The request may be invalid, if so report that now.
  if (!request->decoder_buffer || request->decoder_buffer->data_size() == 0) {
    HandleError();
    return false;
  }

  // Submit for decoding.
  //
  // |pending_decode_id_| must not be 0 nor max because it HashMap reserves
  // these values for "emtpy" and "deleted".
  while (++pending_decode_id_ == 0 ||
         pending_decode_id_ == std::numeric_limits<uint32_t>::max() ||
         pending_decodes_.Contains(pending_decode_id_))
    ;
  pending_decodes_.Set(pending_decode_id_, request);
  --requested_decodes_;
  decoder_->Decode(std::move(request->decoder_buffer),
                   WTF::Bind(&DecoderTemplate::OnDecodeDone,
                             WrapWeakPersistent(this), pending_decode_id_));
  return true;
}

template <typename Traits>
bool DecoderTemplate<Traits>::ProcessFlushRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK(!IsClosed());
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kFlush);

  // TODO(sandersd): If a reset has been requested, resolve immediately.

  if (!decoder_) {
    // TODO(sandersd): Maybe it is valid to flush no decoder? If not, it may be
    // necessary to enter a full error state here.
    request->resolver.Release()->Reject();
    return true;
  }

  if (pending_decodes_.size() + 1 >
      size_t{Traits::GetMaxDecodeRequests(*decoder_)}) {
    // Try again after OnDecodeDone().
    return false;
  }

  // Processing continues in OnFlushDone().
  pending_request_ = request;
  decoder_->Decode(
      media::DecoderBuffer::CreateEOSBuffer(),
      WTF::Bind(&DecoderTemplate::OnFlushDone, WrapWeakPersistent(this)));
  return true;
}

template <typename Traits>
bool DecoderTemplate<Traits>::ProcessResetRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK(!IsClosed());
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kReset);
  DCHECK_GT(requested_resets_, 0);

  // Processing continues in OnResetDone().
  pending_request_ = request;
  --requested_resets_;
  decoder_->Reset(
      WTF::Bind(&DecoderTemplate::OnResetDone, WrapWeakPersistent(this)));
  return true;
}

template <typename Traits>
void DecoderTemplate<Traits>::HandleError() {
  DVLOG(1) << __func__;
  if (IsClosed())
    return;

  Shutdown(true);
}

template <typename Traits>
void DecoderTemplate<Traits>::Shutdown(bool is_error) {
  DVLOG(3) << __func__;
  DCHECK(!IsClosed());

  // Store the error callback so that we can use it after clearing state.
  V8WebCodecsErrorCallback* error_cb = error_cb_.Get();

  // Prevent any new public API calls during teardown.
  // This should make it safe to call into JS synchronously.
  state_ = V8CodecState(V8CodecState::Enum::kClosed);

  // Prevent any late callbacks running.
  output_cb_.Release();
  error_cb_.Release();

  // Clear decoding and JS-visible queue state.
  decoder_.reset();
  pending_decodes_.clear();
  requested_decodes_ = 0;
  requested_resets_ = 0;

  // Fire the error callback if necessary.
  // TODO(sandersd): Create a DOMException to report.
  if (is_error)
    error_cb->InvokeAndReportException(nullptr, nullptr);

  // Clear any pending requests, rejecting all promises.
  if (pending_request_ && pending_request_->resolver)
    pending_request_.Release()->resolver.Release()->Reject();

  while (!requests_.IsEmpty()) {
    Request* request = requests_.front();
    if (request->resolver)
      request->resolver.Release()->Reject();
    requests_.pop_front();
  }
}

template <typename Traits>
void DecoderTemplate<Traits>::OnConfigureFlushDone(media::DecodeStatus status) {
  DVLOG(3) << __func__;
  if (IsClosed())
    return;

  DCHECK(pending_request_);
  DCHECK_EQ(pending_request_->type, Request::Type::kConfigure);

  if (status != media::DecodeStatus::OK) {
    HandleError();
    return;
  }

  // Processing continues in OnInitializeDone().
  Traits::InitializeDecoder(
      *decoder_, *pending_request_->media_config,
      WTF::Bind(&DecoderTemplate::OnInitializeDone, WrapWeakPersistent(this)),
      WTF::BindRepeating(&DecoderTemplate::OnOutput, WrapWeakPersistent(this)));
}

template <typename Traits>
void DecoderTemplate<Traits>::OnInitializeDone(media::Status status) {
  DVLOG(3) << __func__;
  if (IsClosed())
    return;

  DCHECK(pending_request_);
  DCHECK_EQ(pending_request_->type, Request::Type::kConfigure);

  if (!status.is_ok()) {
    // TODO(tmathmeyer): this drops the media error - should we consider logging
    // it or converting it to the DOMException type somehow?
    HandleError();
    return;
  }

  pending_request_.Release();

  if (!initializing_sync_)
    ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::OnDecodeDone(uint32_t id,
                                           media::DecodeStatus status) {
  DVLOG(3) << __func__;
  if (IsClosed())
    return;

  if (status != media::DecodeStatus::OK &&
      status != media::DecodeStatus::ABORTED) {
    HandleError();
    return;
  }

  DCHECK(pending_decodes_.Contains(id));
  auto it = pending_decodes_.find(id);
  pending_decodes_.erase(it);
  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::OnFlushDone(media::DecodeStatus status) {
  DVLOG(3) << __func__;
  if (IsClosed())
    return;

  DCHECK(pending_request_);
  DCHECK_EQ(pending_request_->type, Request::Type::kFlush);

  if (status != media::DecodeStatus::OK) {
    HandleError();
    return;
  }

  pending_request_.Release()->resolver.Release()->Resolve();
  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::OnResetDone() {
  DVLOG(3) << __func__;
  if (IsClosed())
    return;

  DCHECK(pending_request_);
  DCHECK_EQ(pending_request_->type, Request::Type::kReset);

  pending_request_.Release();
  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::OnOutput(scoped_refptr<MediaOutputType> output) {
  DVLOG(3) << __func__;
  if (state_.AsEnum() != V8CodecState::Enum::kConfigured)
    return;

  output_cb_->InvokeAndReportException(
      nullptr, MakeGarbageCollected<OutputType>(output));
}

template <typename Traits>
void DecoderTemplate<Traits>::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(output_cb_);
  visitor->Trace(error_cb_);
  visitor->Trace(requests_);
  visitor->Trace(pending_request_);
  visitor->Trace(pending_decodes_);
  ScriptWrappable::Trace(visitor);
}

template <typename Traits>
void DecoderTemplate<Traits>::Request::Trace(Visitor* visitor) const {
  visitor->Trace(resolver);
}

template class DecoderTemplate<AudioDecoderTraits>;
template class DecoderTemplate<VideoDecoderTraits>;

}  // namespace blink
