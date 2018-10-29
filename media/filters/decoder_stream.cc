// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decoder_stream.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/filters/decrypting_demuxer_stream.h"

namespace media {

#define FUNCTION_DVLOG(level) \
  DVLOG(level) << __func__ << "<" << GetStreamTypeString() << ">"

template <DemuxerStream::Type StreamType>
static const char* GetDecodeTraceString();
template <DemuxerStream::Type StreamType>
static const char* GetReadTraceString();
template <DemuxerStream::Type StreamType>
static const char* GetDemuxerReadTraceString();
template <DemuxerStream::Type StreamType>
static const char* GetPrepareTraceString();

template <>
const char* GetDecodeTraceString<DemuxerStream::VIDEO>() {
  return "VideoDecoderStream::Decode";
}

template <>
const char* GetDecodeTraceString<DemuxerStream::AUDIO>() {
  return "AudioDecoderStream::Decode";
}

template <>
const char* GetReadTraceString<DemuxerStream::VIDEO>() {
  return "VideoDecoderStream::Read";
}

template <>
const char* GetReadTraceString<DemuxerStream::AUDIO>() {
  return "AudioDecoderStream::Read";
}

template <>
const char* GetDemuxerReadTraceString<DemuxerStream::VIDEO>() {
  return "VideoDecoderStream::ReadFromDemuxerStream";
}

template <>
const char* GetDemuxerReadTraceString<DemuxerStream::AUDIO>() {
  return "AudioDecoderStream::ReadFromDemuxerStream";
}

template <>
const char* GetPrepareTraceString<DemuxerStream::VIDEO>() {
  return "VideoDecoderStream::PrepareOutput";
}

template <>
const char* GetPrepareTraceString<DemuxerStream::AUDIO>() {
  return "AudioDecoderStream::PrepareOutput";
}

template <DemuxerStream::Type StreamType>
const char* GetStatusString(typename DecoderStream<StreamType>::Status status) {
  switch (status) {
    case DecoderStream<StreamType>::OK:
      return "okay";
    case DecoderStream<StreamType>::ABORTED:
      return "aborted";
    case DecoderStream<StreamType>::DEMUXER_READ_ABORTED:
      return "demuxer_read_aborted";
    case DecoderStream<StreamType>::DECODE_ERROR:
      return "decode_error";
  }
}

template <DemuxerStream::Type StreamType>
DecoderStream<StreamType>::DecoderStream(
    std::unique_ptr<DecoderStreamTraits<StreamType>> traits,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    CreateDecodersCB create_decoders_cb,
    MediaLog* media_log)
    : traits_(std::move(traits)),
      task_runner_(task_runner),
      media_log_(media_log),
      state_(STATE_UNINITIALIZED),
      stream_(nullptr),
      cdm_context_(nullptr),
      decoder_produced_a_frame_(false),
      decoder_selector_(task_runner, std::move(create_decoders_cb), media_log),
      decoding_eos_(false),
      preparing_output_(false),
      pending_decode_requests_(0),
      duration_tracker_(8),
      received_config_change_during_reinit_(false),
      pending_demuxer_read_(false),
      weak_factory_(this),
      fallback_weak_factory_(this),
      prepare_weak_factory_(this) {
  FUNCTION_DVLOG(1);
}

template <DemuxerStream::Type StreamType>
DecoderStream<StreamType>::~DecoderStream() {
  FUNCTION_DVLOG(1);
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (init_cb_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(init_cb_), false));
  }
  if (read_cb_) {
    read_cb_ = BindToCurrentLoop(std::move(read_cb_));
    SatisfyRead(ABORTED, nullptr);
  }
  if (reset_cb_)
    task_runner_->PostTask(FROM_HERE, std::move(reset_cb_));

  if (preparing_output_)
    CompletePrepare(nullptr);

  // Don't manually reset anything here; rely on the order of member variables
  // within the header, which enforces WeakPtrFactory invalidation first.
}

template <DemuxerStream::Type StreamType>
std::string DecoderStream<StreamType>::GetStreamTypeString() {
  return DecoderStreamTraits<StreamType>::ToString();
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::Initialize(
    DemuxerStream* stream,
    InitCB init_cb,
    CdmContext* cdm_context,
    StatisticsCB statistics_cb,
    base::RepeatingClosure waiting_for_decryption_key_cb) {
  FUNCTION_DVLOG(1);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_UNINITIALIZED);
  DCHECK(!init_cb_);
  DCHECK(init_cb);

  stream_ = stream;
  init_cb_ = std::move(init_cb);
  cdm_context_ = cdm_context;
  statistics_cb_ = std::move(statistics_cb);
  waiting_for_decryption_key_cb_ = waiting_for_decryption_key_cb;

  traits_->OnStreamReset(stream_);
  decoder_selector_.Initialize(traits_.get(), stream, cdm_context,
                               std::move(waiting_for_decryption_key_cb));

  state_ = STATE_INITIALIZING;
  SelectDecoder();
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::Read(ReadCB read_cb) {
  FUNCTION_DVLOG(3);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ != STATE_UNINITIALIZED && state_ != STATE_INITIALIZING)
      << state_;
  // No two reads in the flight at any time.
  DCHECK(!read_cb_);
  // No read during resetting or stopping process.
  DCHECK(!reset_cb_);

  TRACE_EVENT_ASYNC_BEGIN0("media", GetReadTraceString<StreamType>(), this);
  if (state_ == STATE_ERROR) {
    read_cb_ = BindToCurrentLoop(std::move(read_cb));
    SatisfyRead(DECODE_ERROR, nullptr);
    return;
  }

  if (state_ == STATE_END_OF_STREAM && ready_outputs_.empty() &&
      unprepared_outputs_.empty()) {
    read_cb_ = BindToCurrentLoop(std::move(read_cb));
    SatisfyRead(OK, StreamTraits::CreateEOSOutput());
    return;
  }

  if (!ready_outputs_.empty()) {
    read_cb_ = BindToCurrentLoop(std::move(read_cb));
    SatisfyRead(OK, ready_outputs_.front());
    ready_outputs_.pop_front();
    MaybePrepareAnotherOutput();
  } else {
    read_cb_ = std::move(read_cb);
  }

  if (state_ == STATE_NORMAL && CanDecodeMore())
    ReadFromDemuxerStream();
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::Reset(base::OnceClosure closure) {
  FUNCTION_DVLOG(2);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(state_, STATE_UNINITIALIZED);
  DCHECK(!reset_cb_);

  reset_cb_ = std::move(closure);

  if (read_cb_) {
    read_cb_ = BindToCurrentLoop(std::move(read_cb_));
    SatisfyRead(ABORTED, nullptr);
  }

  ClearOutputs();
  traits_->OnStreamReset(stream_);

  // It's possible to have received a DECODE_ERROR and entered STATE_ERROR right
  // before a Reset() is executed. If we are still waiting for a demuxer read,
  // OnBufferReady() will handle the reset callback.
  // See crbug.com/597605 and crbug.com/607454.
  if (state_ == STATE_ERROR && !pending_demuxer_read_) {
    task_runner_->PostTask(FROM_HERE, std::move(reset_cb_));
    return;
  }

  // During decoder reinitialization, the Decoder does not need to be and
  // cannot be Reset(). |decrypting_demuxer_stream_| was reset before decoder
  // reinitialization.
  if (state_ == STATE_REINITIALIZING_DECODER)
    return;

  // |decrypting_demuxer_stream_| will fire all of its read requests when
  // it resets. |reset_cb_| will be fired in OnDecoderReset(), after the
  // decrypting demuxer stream finishes its reset.
  if (decrypting_demuxer_stream_) {
    decrypting_demuxer_stream_->Reset(base::BindRepeating(
        &DecoderStream<StreamType>::ResetDecoder, weak_factory_.GetWeakPtr()));
    return;
  }

  // During pending demuxer read and when not using DecryptingDemuxerStream,
  // the Decoder will be reset after demuxer read is returned
  // (in OnBufferReady()).
  if (pending_demuxer_read_)
    return;

  ResetDecoder();
}

template <DemuxerStream::Type StreamType>
bool DecoderStream<StreamType>::CanReadWithoutStalling() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return !ready_outputs_.empty() || !unprepared_outputs_.empty() ||
         (decoder_ && decoder_->CanReadWithoutStalling());
}

template <>
bool DecoderStream<DemuxerStream::AUDIO>::CanReadWithoutStalling() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return true;
}

template <DemuxerStream::Type StreamType>
int DecoderStream<StreamType>::GetMaxDecodeRequests() const {
  return decoder_->GetMaxDecodeRequests();
}

template <>
int DecoderStream<DemuxerStream::AUDIO>::GetMaxDecodeRequests() const {
  return 1;
}

template <DemuxerStream::Type StreamType>
bool DecoderStream<StreamType>::CanDecodeMore() const {
  DCHECK(task_runner_->BelongsToCurrentThread());

  bool buffers_left = !(fallback_buffers_.empty() && decoding_eos_);

  // Limit total number of outputs stored and being decoded. It only makes sense
  // to saturate decoder completely when our output queues are empty.
  int num_decodes = ready_outputs_.size() + unprepared_outputs_.size() +
                    pending_decode_requests_;
  return buffers_left && num_decodes < GetMaxDecodeRequests();
}

template <DemuxerStream::Type StreamType>
base::TimeDelta DecoderStream<StreamType>::AverageDuration() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return duration_tracker_.count() ? duration_tracker_.Average()
                                   : base::TimeDelta();
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::SetPrepareCB(PrepareCB prepare_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  prepare_cb_ = std::move(prepare_cb);
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::SkipPrepareUntil(
    base::TimeDelta start_timestamp) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  skip_prepare_until_timestamp_ = start_timestamp;
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::SelectDecoder() {
  decoder_selector_.SelectDecoder(
      base::BindRepeating(&DecoderStream<StreamType>::OnDecoderSelected,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&DecoderStream<StreamType>::OnDecodeOutputReady,
                          fallback_weak_factory_.GetWeakPtr()));
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::OnDecoderSelected(
    std::unique_ptr<Decoder> selected_decoder,
    std::unique_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream) {
  FUNCTION_DVLOG(1) << ": "
                    << (selected_decoder ? selected_decoder->GetDisplayName()
                                         : "No decoder selected.");
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == STATE_INITIALIZING || state_ == STATE_REINITIALIZING_DECODER)
      << state_;

  if (state_ == STATE_INITIALIZING) {
    DCHECK(init_cb_);
    DCHECK(!read_cb_);
    DCHECK(!reset_cb_);
  } else if (state_ == STATE_REINITIALIZING_DECODER) {
    DCHECK(decoder_);
  }

  decoder_ = std::move(selected_decoder);
  if (decrypting_demuxer_stream) {
    decrypting_demuxer_stream_ = std::move(decrypting_demuxer_stream);
    stream_ = decrypting_demuxer_stream_.get();
    // Also clear |cdm_context_|, it shouldn't be passed during reinitialize for
    // a sream that isn't encrypted.
    cdm_context_ = nullptr;
  }
  if (decoder_change_observer_cb_)
    decoder_change_observer_cb_.Run(decoder_.get());

  // TODO(tguilbert): crbug.com/603713 support config changes on decoder reinit.
  if (received_config_change_during_reinit_) {
    CompleteDecoderReinitialization(false);
    return;
  }

  // Attempt to decode buffers from previous decoders (when those decoders have
  // never successfully outputed a frame).
  fallback_buffers_ = pending_buffers_;

  if (!decoder_) {
    if (state_ == STATE_INITIALIZING) {
      state_ = STATE_UNINITIALIZED;
      MEDIA_LOG(ERROR, media_log_)
          << GetStreamTypeString() << " decoder initialization failed";
      std::move(init_cb_).Run(false);
    } else {
      CompleteDecoderReinitialization(false);
    }
    return;
  }

  // Send logs and statistics updates including the decoder name.
  traits_->ReportStatistics(statistics_cb_, 0);
  media_log_->SetBooleanProperty(GetStreamTypeString() + "_dds",
                                 !!decrypting_demuxer_stream_);
  media_log_->SetStringProperty(GetStreamTypeString() + "_decoder",
                                decoder_->GetDisplayName());
  media_log_->SetBooleanProperty(
      "is_platform_" + GetStreamTypeString() + "_decoder",
      decoder_->IsPlatformDecoder());

  MEDIA_LOG(INFO, media_log_)
      << "Selected " << decoder_->GetDisplayName() << " for "
      << GetStreamTypeString() << " decoding, config: "
      << traits_->GetDecoderConfig(stream_).AsHumanReadableString();

  if (state_ == STATE_REINITIALIZING_DECODER) {
    CompleteDecoderReinitialization(true);
    return;
  }

  // Initialization succeeded.
  state_ = STATE_NORMAL;
  if (StreamTraits::NeedsBitstreamConversion(decoder_.get()))
    stream_->EnableBitstreamConverter();
  std::move(init_cb_).Run(true);
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::SatisfyRead(
    Status status,
    const scoped_refptr<Output>& output) {
  DCHECK(read_cb_);
  TRACE_EVENT_ASYNC_END1("media", GetReadTraceString<StreamType>(), this,
                         "status", GetStatusString<StreamType>(status));
  std::move(read_cb_).Run(status, output);
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::Decode(scoped_refptr<DecoderBuffer> buffer) {
  FUNCTION_DVLOG(3);

  // We don't know if the decoder will error out on first decode yet. Save the
  // buffer to feed it to the fallback decoder later if needed.
  if (!decoder_produced_a_frame_)
    pending_buffers_.push_back(buffer);

  // It's possible for a buffer to arrive from the demuxer right after the
  // fallback decoder successfully completed its initialization. At this point
  // |pending_buffers_| has already been copied to |fallback_buffers_| and we
  // need to append it ourselves.
  if (!fallback_buffers_.empty()) {
    fallback_buffers_.push_back(buffer);

    scoped_refptr<DecoderBuffer> temp = std::move(fallback_buffers_.front());
    fallback_buffers_.pop_front();
    DecodeInternal(std::move(temp));
  } else {
    DecodeInternal(std::move(buffer));
  }
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::DecodeInternal(
    scoped_refptr<DecoderBuffer> buffer) {
  FUNCTION_DVLOG(3);
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER) << state_;
  DCHECK_LT(pending_decode_requests_, GetMaxDecodeRequests());
  DCHECK(!reset_cb_);
  DCHECK(buffer);

  std::unique_ptr<ScopedDecodeTrace> trace_event;
  if (ScopedDecodeTrace::IsEnabled()) {
    // Because multiple Decode() calls may be in flight, each call needs a
    // unique trace event class to identify it. This scoped event is bound
    // into the OnDecodeDone callback to ensure the trace is always closed.
    trace_event = std::make_unique<ScopedDecodeTrace>(
        GetDecodeTraceString<StreamType>(), *buffer);
  }

  traits_->OnDecode(*buffer);

  const bool is_eos = buffer->end_of_stream();
  if (is_eos)
    decoding_eos_ = true;
  else if (buffer->duration() != kNoTimestamp)
    duration_tracker_.AddSample(buffer->duration());

  ++pending_decode_requests_;

  const int buffer_size = is_eos ? 0 : buffer->data_size();
  decoder_->Decode(
      std::move(buffer),
      base::BindRepeating(&DecoderStream<StreamType>::OnDecodeDone,
                          fallback_weak_factory_.GetWeakPtr(), buffer_size,
                          decoding_eos_, base::Passed(&trace_event)));
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::FlushDecoder() {
  // Send the EOS directly to the decoder, bypassing a potential add to
  // |pending_buffers_|.
  DecodeInternal(DecoderBuffer::CreateEOSBuffer());
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::OnDecodeDone(
    int buffer_size,
    bool end_of_stream,
    std::unique_ptr<ScopedDecodeTrace> trace_event,
    DecodeStatus status) {
  FUNCTION_DVLOG(3) << ": " << status;
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER ||
         state_ == STATE_ERROR)
      << state_;
  DCHECK_GT(pending_decode_requests_, 0);

  --pending_decode_requests_;
  if (trace_event)
    trace_event->EndTrace(status);

  if (end_of_stream) {
    DCHECK(!pending_decode_requests_);
    decoding_eos_ = false;
    if (status == DecodeStatus::OK) {
      // Even if no frames were decoded, completing a flush counts as
      // successfully selecting a decoder. This allows back-to-back config
      // changes to select from all decoders.
      decoder_selector_.FinalizeDecoderSelection();
    }
  }

  if (state_ == STATE_ERROR) {
    DCHECK(!read_cb_);
    return;
  }

  // Drop decoding result if Reset() was called during decoding.
  // The resetting process will be handled when the decoder is reset.
  if (reset_cb_)
    return;

  switch (status) {
    case DecodeStatus::DECODE_ERROR:
      if (!decoder_produced_a_frame_ &&
          base::FeatureList::IsEnabled(kFallbackAfterDecodeError)) {
        pending_decode_requests_ = 0;

        // Prevent all pending decode requests and outputs from those requests
        // from being called back.
        fallback_weak_factory_.InvalidateWeakPtrs();

        FUNCTION_DVLOG(1)
            << ": Falling back to new decoder after initial decode error.";
        state_ = STATE_REINITIALIZING_DECODER;
        SelectDecoder();
        return;
      }

      FUNCTION_DVLOG(1) << ": Decode error!";
      state_ = STATE_ERROR;
      MEDIA_LOG(ERROR, media_log_) << GetStreamTypeString() << " decode error";
      ClearOutputs();
      if (read_cb_)
        SatisfyRead(DECODE_ERROR, nullptr);
      return;

    case DecodeStatus::ABORTED:
      // Decoder can return DecodeStatus::ABORTED during Reset() or during
      // destruction.
      return;

    case DecodeStatus::OK:
      // Any successful decode counts!
      if (buffer_size > 0)
        traits_->ReportStatistics(statistics_cb_, buffer_size);

      if (state_ == STATE_NORMAL) {
        if (end_of_stream) {
          state_ = STATE_END_OF_STREAM;
          if (ready_outputs_.empty() && unprepared_outputs_.empty() && read_cb_)
            SatisfyRead(OK, StreamTraits::CreateEOSOutput());
          return;
        }

        if (CanDecodeMore())
          ReadFromDemuxerStream();
        return;
      }

      if (state_ == STATE_FLUSHING_DECODER && !pending_decode_requests_)
        ReinitializeDecoder();
      return;
  }
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::OnDecodeOutputReady(
    const scoped_refptr<Output>& output) {
  FUNCTION_DVLOG(3) << ": " << output->timestamp().InMilliseconds() << " ms";
  DCHECK(output);
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER ||
         state_ == STATE_ERROR)
      << state_;

  if (state_ == STATE_ERROR) {
    DCHECK(!read_cb_);
    return;
  }

  // Drop decoding result if Reset() was called during decoding.
  // The resetting process will be handled when the decoder is reset.
  if (reset_cb_)
    return;

  // |decoder_| successfully decoded a frame. No need to keep buffers for a
  // fallback decoder.
  // Note: |fallback_buffers_| might still have buffers, and we will keep
  // reading from there before requesting new buffers from |stream_|.
  if (!decoder_produced_a_frame_) {
    decoder_produced_a_frame_ = true;
    decoder_selector_.FinalizeDecoderSelection();
    pending_buffers_.clear();
  }

  // If the frame should be dropped, exit early and decode another frame.
  if (traits_->OnDecodeDone(output) == PostDecodeAction::DROP)
    return;

  if (prepare_cb_ && output->timestamp() + AverageDuration() >=
                         skip_prepare_until_timestamp_) {
    unprepared_outputs_.push_back(output);
    MaybePrepareAnotherOutput();
    return;
  }

  if (read_cb_) {
    // If |ready_outputs_| was non-empty, the read would have already been
    // satisifed by Read().
    DCHECK(ready_outputs_.empty());
    SatisfyRead(OK, output);
    return;
  }

  // Store decoded output.
  ready_outputs_.push_back(output);
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::ReadFromDemuxerStream() {
  FUNCTION_DVLOG(3);
  DCHECK_EQ(state_, STATE_NORMAL);
  DCHECK(CanDecodeMore());
  DCHECK(!reset_cb_);

  if (!fallback_buffers_.empty()) {
    scoped_refptr<DecoderBuffer> buffer = std::move(fallback_buffers_.front());
    fallback_buffers_.pop_front();

    // Decode the buffer without re-appending it to |pending_buffers_|.
    DecodeInternal(std::move(buffer));
    return;
  }

  // We may get here when a read is already pending, ignore this.
  if (pending_demuxer_read_)
    return;

  TRACE_EVENT_ASYNC_BEGIN0("media", GetDemuxerReadTraceString<StreamType>(),
                           this);
  pending_demuxer_read_ = true;
  stream_->Read(base::BindRepeating(&DecoderStream<StreamType>::OnBufferReady,
                                    weak_factory_.GetWeakPtr()));
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::OnBufferReady(
    DemuxerStream::Status status,
    scoped_refptr<DecoderBuffer> buffer) {
  TRACE_EVENT_ASYNC_END1("media", GetDemuxerReadTraceString<StreamType>(), this,
                         "status", DemuxerStream::GetStatusName(status));
  FUNCTION_DVLOG(3) << ": " << status << ", "
                    << (buffer ? buffer->AsHumanReadableString() : "nullptr");

  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(pending_demuxer_read_);
  if (!decoder_produced_a_frame_) {
    DCHECK(state_ == STATE_ERROR || state_ == STATE_REINITIALIZING_DECODER ||
           state_ == STATE_NORMAL)
        << state_;
  }
  DCHECK_EQ(buffer != nullptr, status == DemuxerStream::kOk) << status;
  pending_demuxer_read_ = false;

  // If parallel decode requests are supported, multiple read requests might
  // have been sent to the demuxer. The buffers might arrive while the decoder
  // is reinitializing after falling back on first decode error.
  if (state_ == STATE_REINITIALIZING_DECODER && !decoder_produced_a_frame_) {
    switch (status) {
      case DemuxerStream::kOk:
        // Save valid buffers to be consumed by the new decoder.
        // |pending_buffers_| is copied to |fallback_buffers| in
        // OnDecoderSelected().
        pending_buffers_.push_back(std::move(buffer));
        break;
      case DemuxerStream::kConfigChanged:
        // TODO(tguilbert): crbug.com/603713
        // |decoder_| might have a stale config by the time it is reinitialized.
        // Ideally, we would save the config from |stream_| and reinitialize the
        // decoder by playing back the sequence of buffers and config changes.
        received_config_change_during_reinit_ = true;
        pending_buffers_.clear();
        break;
      case DemuxerStream::kAborted:
      case DemuxerStream::kError:
        // Will read from the demuxer stream again in OnDecoderSelected().
        pending_buffers_.clear();
        break;
    }
    return;
  }

  if (status == DemuxerStream::kError) {
    FUNCTION_DVLOG(1) << ": Demuxer stream read error!";
    state_ = STATE_ERROR;
    MEDIA_LOG(ERROR, media_log_)
        << GetStreamTypeString() << " demuxer stream read error!";
    pending_buffers_.clear();
    ClearOutputs();
    if (read_cb_)
      SatisfyRead(DECODE_ERROR, nullptr);
  }

  // Decoding has been stopped.
  if (state_ == STATE_ERROR) {
    DCHECK(!read_cb_);

    if (reset_cb_) {
      // If we are using DecryptingDemuxerStream, we already called DDS::Reset()
      // which will continue the resetting process in its callback.
      if (!decrypting_demuxer_stream_)
        Reset(std::move(reset_cb_));
    }
    return;
  }

  state_ = STATE_NORMAL;

  if (status == DemuxerStream::kConfigChanged) {
    FUNCTION_DVLOG(2) << ": ConfigChanged";
    DCHECK(stream_->SupportsConfigChanges());

    // Pending buffers might not match the reinitialized decoder's new config.
    //
    // Note: as part of crbug.com/603713, we should record the config in order
    // to play it back to the fallback decoder.
    //
    // Clearing the buffers is an acceptable workaround for the time being. It
    // assures us that we maintain a consistent state, at the cost of
    // potentially dropping some frames. Flushing the decoder will cause one of
    // the following outcomes:
    //   - The decoder outputs a valid frame during flushing (we no longer
    //   care about |pending_buffers_| and fallback scenarios).
    //   - The decoder returns a DECODE_ERROR via OnDecodeDone() without having
    //   outputted a frame (we fallback to a new decoder which will read
    //   straight from the demuxer, dropping some frames).
    //   - The decoder is flushed without returning a frame or without a
    //   DECODE_ERROR (we reinitialize the decoder as if a normal flush
    //   happened, and read straight from the demuxer, which could lead to some
    //   lost frames if we were to fallback then).
    pending_buffers_.clear();

    const DecoderConfig& config = traits_->GetDecoderConfig(stream_);

    MEDIA_LOG(INFO, media_log_)
        << GetStreamTypeString()
        << " decoder config changed midstream, new config: "
        << config.AsHumanReadableString();

    decoder_selector_.NotifyConfigChanged();
    if (config_change_observer_cb_)
      config_change_observer_cb_.Run(config);

    state_ = STATE_FLUSHING_DECODER;
    if (reset_cb_) {
      // If we are using DecryptingDemuxerStream, we already called DDS::Reset()
      // which will continue the resetting process in its callback.
      if (!decrypting_demuxer_stream_)
        Reset(std::move(reset_cb_));
      // Reinitialization will continue after Reset() is done.
    } else {
      FlushDecoder();
    }
    return;
  }

  if (reset_cb_) {
    // If we are using DecryptingDemuxerStream, we already called DDS::Reset()
    // which will continue the resetting process in its callback.
    if (!decrypting_demuxer_stream_)
      Reset(std::move(reset_cb_));
    return;
  }

  if (status == DemuxerStream::kAborted) {
    if (read_cb_)
      SatisfyRead(DEMUXER_READ_ABORTED, nullptr);
    return;
  }

  DCHECK(status == DemuxerStream::kOk) << status;
  Decode(std::move(buffer));

  // Read more data if the decoder supports multiple parallel decoding requests.
  if (CanDecodeMore())
    ReadFromDemuxerStream();
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::ReinitializeDecoder() {
  FUNCTION_DVLOG(2);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_FLUSHING_DECODER);
  DCHECK_EQ(pending_decode_requests_, 0);

  state_ = STATE_REINITIALIZING_DECODER;

  // TODO(sandersd): Detect whether a new decoder is required before
  // attempting reinitialization.
  traits_->InitializeDecoder(
      decoder_.get(), traits_->GetDecoderConfig(stream_),
      stream_->liveness() == DemuxerStream::LIVENESS_LIVE, cdm_context_,
      base::BindRepeating(&DecoderStream<StreamType>::OnDecoderReinitialized,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&DecoderStream<StreamType>::OnDecodeOutputReady,
                          fallback_weak_factory_.GetWeakPtr()),
      waiting_for_decryption_key_cb_);
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::OnDecoderReinitialized(bool success) {
  FUNCTION_DVLOG(2) << ": success = " << success;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_REINITIALIZING_DECODER);

  // ReinitializeDecoder() can be called in two cases:
  // 1, Flushing decoder finished (see OnDecodeOutputReady()).
  // 2, Reset() was called during flushing decoder (see OnDecoderReset()).
  // Also, Reset() can be called during pending ReinitializeDecoder().
  // This function needs to handle them all!

  if (!success) {
    // Reinitialization failed. Try to fall back to one of the remaining
    // decoders. This will consume at least one decoder so doing it more than
    // once is safe.
    SelectDecoder();
  } else {
    CompleteDecoderReinitialization(true);
  }
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::CompleteDecoderReinitialization(bool success) {
  FUNCTION_DVLOG(2);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_REINITIALIZING_DECODER);

  state_ = success ? STATE_NORMAL : STATE_ERROR;

  if (reset_cb_) {
    std::move(reset_cb_).Run();
    return;
  }

  if (!read_cb_)
    return;

  if (state_ == STATE_ERROR) {
    MEDIA_LOG(ERROR, media_log_)
        << GetStreamTypeString() << " decoder reinitialization failed";
    SatisfyRead(DECODE_ERROR, nullptr);
    return;
  }

  // Re-enable fallback to software after reinitialization. This is the last
  // place we can clear that state, and as such is the least likely to interfere
  // with the rest of the fallback algorithm.
  // TODO(tguilbert): investigate setting this flag at an earlier time. This
  // could fix the hypothetical edge case of receiving a decode error when
  // flushing the decoder during a seek operation.
  decoder_produced_a_frame_ = false;

  // We may still have too many |ready_outputs_| or |unprepared_outputs_| to
  // initiate another read to the demuxer stream. If so, the read will be
  // initiated later once we vended enough outputs to read again.
  if (CanDecodeMore())
    ReadFromDemuxerStream();
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::ResetDecoder() {
  FUNCTION_DVLOG(2);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER ||
         state_ == STATE_ERROR || state_ == STATE_END_OF_STREAM)
      << state_;
  DCHECK(reset_cb_);

  decoder_->Reset(base::BindRepeating(
      &DecoderStream<StreamType>::OnDecoderReset, weak_factory_.GetWeakPtr()));
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::OnDecoderReset() {
  FUNCTION_DVLOG(2);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER ||
         state_ == STATE_ERROR || state_ == STATE_END_OF_STREAM)
      << state_;
  // If Reset() was called during pending read, read callback should be fired
  // before the reset callback is fired.
  DCHECK(!read_cb_);
  DCHECK(reset_cb_);

  // Make sure we read directly from the demuxer after a reset.
  fallback_buffers_.clear();
  pending_buffers_.clear();

  if (state_ != STATE_FLUSHING_DECODER) {
    state_ = STATE_NORMAL;
    // Pending read, on failure, could have fired the reset callback already.
    if (reset_cb_)
      std::move(reset_cb_).Run();
    return;
  }

  // The resetting process will be continued in OnDecoderReinitialized().
  ReinitializeDecoder();
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::ClearOutputs() {
  if (preparing_output_)
    CompletePrepare(nullptr);
  ready_outputs_.clear();
  unprepared_outputs_.clear();
  prepare_weak_factory_.InvalidateWeakPtrs();
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::MaybePrepareAnotherOutput() {
  FUNCTION_DVLOG(3);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER ||
         state_ == STATE_END_OF_STREAM ||
         state_ == STATE_REINITIALIZING_DECODER)
      << state_;

  // If there's nothing to prepare or a prepare is underway, we're done.
  if (!prepare_cb_ || unprepared_outputs_.empty() || preparing_output_)
    return;

  // If there's too many ready outputs, we're done.
  if (ready_outputs_.size() >= static_cast<size_t>(GetMaxDecodeRequests()))
    return;

  TRACE_EVENT_ASYNC_BEGIN1(
      "media", GetPrepareTraceString<StreamType>(), this, "timestamp_us",
      unprepared_outputs_.front()->timestamp().InMicroseconds());
  preparing_output_ = true;
  prepare_cb_.Run(
      unprepared_outputs_.front(),
      base::BindOnce(&DecoderStream<StreamType>::OnPreparedOutputReady,
                     prepare_weak_factory_.GetWeakPtr()));
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::OnPreparedOutputReady(
    const scoped_refptr<Output>& output) {
  FUNCTION_DVLOG(2);
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Errors and reset invalidate the WeakPtr factory for this function, so it
  // should only be called in normal and flush states.
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER ||
         state_ == STATE_END_OF_STREAM ||
         state_ == STATE_REINITIALIZING_DECODER)
      << state_;
  DCHECK(!reset_cb_);
  DCHECK(!unprepared_outputs_.empty());
  DCHECK(preparing_output_);

  CompletePrepare(output.get());
  unprepared_outputs_.pop_front();
  if (!read_cb_)
    ready_outputs_.emplace_back(output);
  else
    SatisfyRead(OK, output);

  MaybePrepareAnotherOutput();

  // Since decoding may have been stalled by unprepared outputs, we should try
  // to queue another decode here if one has been returned.
  if (state_ == STATE_NORMAL && CanDecodeMore())
    ReadFromDemuxerStream();
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::CompletePrepare(const Output* output) {
  DCHECK(preparing_output_);
  TRACE_EVENT_ASYNC_END1(
      "media", GetPrepareTraceString<StreamType>(), this, "timestamp_us",
      (output ? output->timestamp() : kNoTimestamp).InMicroseconds());
  preparing_output_ = false;
}

template class DecoderStream<DemuxerStream::VIDEO>;
template class DecoderStream<DemuxerStream::AUDIO>;

}  // namespace media
