// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DECODER_STREAM_H_
#define MEDIA_FILTERS_DECODER_STREAM_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/moving_window.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/decoder_status.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/pipeline_status.h"
#include "media/base/timestamp_constants.h"
#include "media/base/waiting.h"
#include "media/filters/decoder_selector.h"
#include "media/filters/decoder_stream_traits.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {

class CdmContext;
class DecryptingDemuxerStream;

// Wraps a DemuxerStream and a list of Decoders and provides decoded
// output to its client (e.g. Audio/VideoRendererImpl).
template <DemuxerStream::Type StreamType>
class MEDIA_EXPORT DecoderStream {
 public:
  using StreamTraits = DecoderStreamTraits<StreamType>;
  using Decoder = typename StreamTraits::DecoderType;
  using Output = typename StreamTraits::OutputType;
  using DecoderConfig = typename StreamTraits::DecoderConfigType;

  // Callback to create a list of decoders.
  using CreateDecodersCB =
      base::RepeatingCallback<std::vector<std::unique_ptr<Decoder>>()>;

  // Indicates completion of a DecoderStream initialization.
  using InitCB = base::OnceCallback<void(bool success)>;

  // Indicates completion of a DecoderStream read.
  using ReadResult = DecoderStatus::Or<scoped_refptr<Output>>;
  using ReadCB = base::OnceCallback<void(ReadResult)>;

  DecoderStream(std::unique_ptr<DecoderStreamTraits<StreamType>> traits,
                scoped_refptr<base::SequencedTaskRunner> task_runner,
                CreateDecodersCB create_decoders_cb,
                MediaLog* media_log);
  virtual ~DecoderStream();

  // Initializes the DecoderStream and returns the initialization result
  // through |init_cb|. Note that |init_cb| is always called asynchronously.
  // |cdm_context| can be used to handle encrypted stream. Can be null if the
  // stream is not encrypted.
  void Initialize(DemuxerStream* stream,
                  InitCB init_cb,
                  CdmContext* cdm_context,
                  StatisticsCB statistics_cb,
                  WaitingCB waiting_cb);

  // Reads a decoded Output and returns it via the |read_cb|. Note that
  // |read_cb| is always called asynchronously. This method should only be
  // called after initialization has succeeded and must not be called during
  // pending Reset().
  void Read(ReadCB read_cb);

  // Resets the decoder, flushes all decoded outputs and/or internal buffers,
  // fires any existing pending read callback and calls |closure| on completion.
  // Note that |closure| is always called asynchronously. This method should
  // only be called after initialization has succeeded and must not be called
  // during pending Reset().
  // N.B: If the decoder stream has run into an error, calling this method does
  // not 'reset' it to a normal state.
  void Reset(base::OnceClosure closure);

  // Returns true if the decoder currently has the ability to decode and return
  // an Output.
  bool CanReadWithoutStalling() const;

  base::TimeDelta AverageDuration() const;

  // Indicates that outputs need preparation (e.g., copying into GPU buffers)
  // before being marked as ready. When an output is given by the decoder it
  // will be added to |unprepared_outputs_| if a PrepareCB has been specified.
  // If the size of |ready_outputs_| is less than
  // Decoder::GetMaxDecodeRequests(), the provided PrepareCB will be called for
  // the output. Once an output has been prepared by the PrepareCB it must call
  // the given OutputReadyCB with the prepared output.
  //
  // This process is structured such that only a fixed number of outputs are
  // prepared at any one time; this alleviates resource usage issues incurred by
  // the preparation process when a decoder has a burst of outputs after on
  // Decode(). For more context on why, see https://crbug.com/820167.
  using OutputReadyCB = base::OnceCallback<void(scoped_refptr<Output>)>;
  using PrepareCB =
      base::RepeatingCallback<void(scoped_refptr<Output>, OutputReadyCB)>;
  void SetPrepareCB(PrepareCB prepare_cb);

  // Indicates that we won't need to prepare outputs before |start_timestamp|,
  // so that the preparation step (which is generally expensive) can be skipped.
  void SkipPrepareUntil(base::TimeDelta start_timestamp);

  // Allows callers to register for notification of config changes; this is
  // called immediately after receiving the 'kConfigChanged' status from the
  // DemuxerStream, before any action is taken to handle the config change.
  using ConfigChangeObserverCB =
      base::RepeatingCallback<void(const DecoderConfig&)>;
  void set_config_change_observer(
      ConfigChangeObserverCB config_change_observer) {
    config_change_observer_cb_ = config_change_observer;
  }

  // Allow interested folks to keep track the currently selected decoder.  The
  // provided decoder is valid only during the scope of the callback.
  using DecoderChangeObserverCB = base::RepeatingCallback<void(Decoder*)>;
  void set_decoder_change_observer(
      DecoderChangeObserverCB decoder_change_observer_cb) {
    decoder_change_observer_cb_ = std::move(decoder_change_observer_cb);
  }

  void set_fallback_observer(PipelineStatusCB fallback_cb) {
    fallback_cb_ = std::move(fallback_cb);
  }

  int get_pending_buffers_size_for_testing() const {
    return pending_buffers_.size();
  }

  int get_fallback_buffers_size_for_testing() const {
    return fallback_buffers_.size();
  }

  bool is_demuxer_read_pending() const { return pending_demuxer_read_; }

  DecoderSelector<StreamType>& GetDecoderSelectorForTesting(
      base::PassKey<class VideoDecoderStreamTest>) {
    return decoder_selector_;
  }

 private:
  enum class State {
    kStateUninitialized,
    kStateInitializing,
    kStateNormal,  // Includes idle, pending decoder decode/reset.
    kStateFlushingDecoder,
    kStateReinitializingDecoder,
    kStateEndOfStream,  // End of stream reached; returns EOS on all reads.
    kStateError,
  };

  // Returns the string representation of the StreamType for logging purpose.
  std::string GetStreamTypeString();

  // Returns maximum concurrent decode requests for the current |decoder_|.
  int GetMaxDecodeRequests() const;

  // Returns if current |decoder_| is a platform decoder.
  bool IsPlatformDecoder() const;

  // Returns the maximum number of outputs we should keep ready at any one time.
  int GetMaxReadyOutputs() const;

  // Returns true if one more decode request can be submitted to the decoder.
  bool CanDecodeMore() const;

  void BeginDecoderSelection();
  void ResumeDecoderSelection(DecoderStatus&& reinit_cause);

  // Called when |decoder_selector| selected the |selected_decoder|.
  // |decrypting_demuxer_stream| was also populated if a DecryptingDemuxerStream
  // is created to help decrypt the encrypted stream.
  void OnDecoderSelected(
      DecoderStatus::Or<std::unique_ptr<Decoder>> decoder_or_error,
      std::unique_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream);

  // Satisfy pending |read_cb_| with |result|.
  void SatisfyRead(ReadResult result);

  // Decodes |buffer| and returns the result via OnDecodeOutputReady().
  // Saves |buffer| into |pending_buffers_| if appropriate.
  void Decode(scoped_refptr<DecoderBuffer> buffer);

  // Performs the heavy lifting of the decode call.
  void DecodeInternal(scoped_refptr<DecoderBuffer> buffer);

  // Callback for Decoder::Decode().
  void OnDecodeDone(int buffer_size,
                    bool end_of_stream,
                    std::unique_ptr<ScopedDecodeTrace> trace_event,
                    DecoderStatus status);

  // Output callback passed to Decoder::Initialize().
  void OnDecodeOutputReady(scoped_refptr<Output> output);

  // Reads a buffer from |stream_| and returns the result via OnBufferReady().
  void ReadFromDemuxerStream();

  void OnBuffersReady(DemuxerStream::Status status,
                      DemuxerStream::DecoderBufferVector buffers);

  void ReinitializeDecoder();

  void CompleteDecoderReinitialization(DecoderStatus status);

  void ResetDecoder();
  void OnDecoderReset();

  void ClearOutputs();
  void MaybePrepareAnotherOutput();
  void OnPreparedOutputReady(scoped_refptr<Output> frame);
  void CompletePrepare(const Output* output);

  void ReportEncryptionType(const scoped_refptr<DecoderBuffer>& buffer);

  std::unique_ptr<DecoderStreamTraits<StreamType>> traits_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  raw_ptr<MediaLog> media_log_;

  State state_;

  StatisticsCB statistics_cb_;
  InitCB init_cb_;
  WaitingCB waiting_cb_;
  PipelineStatusCB fallback_cb_;

  ReadCB read_cb_;
  base::OnceClosure reset_cb_;

  raw_ptr<DemuxerStream, DanglingUntriaged> stream_;

  raw_ptr<CdmContext> cdm_context_;

  std::unique_ptr<Decoder> decoder_;

  // Whether |decoder_| has produced a frame yet. Reset on fallback.
  bool decoder_produced_a_frame_;

  std::unique_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream_;

  // Note: Holds pointers to |traits_|, |stream_|, |decrypting_demuxer_stream_|,
  // and |cdm_context_|.
  DecoderSelector<StreamType> decoder_selector_;

  ConfigChangeObserverCB config_change_observer_cb_;
  DecoderChangeObserverCB decoder_change_observer_cb_;

  // An end-of-stream buffer has been sent for decoding, no more buffers should
  // be sent for decoding until it completes.
  // TODO(sandersd): Turn this into a State. http://crbug.com/408316
  bool decoding_eos_;

  PrepareCB prepare_cb_;
  bool preparing_output_;

  // Decoded buffers that haven't been read yet. If |prepare_cb_| has been set
  // |unprepared_outputs_| will contain buffers which haven't been prepared yet.
  // Once prepared or if preparation is not required, outputs will be put into
  // |ready_outputs_|.
  base::circular_deque<scoped_refptr<Output>> unprepared_outputs_;
  base::circular_deque<scoped_refptr<Output>> ready_outputs_;

  // Number of outstanding decode requests sent to the |decoder_|.
  int pending_decode_requests_;

  // Tracks the duration of incoming packets over time.
  base::MovingAverage<base::TimeDelta, base::TimeDelta> duration_tracker_;

  // Stores buffers that might be reused if the decoder fails right after
  // Initialize().
  base::circular_deque<scoped_refptr<DecoderBuffer>> pending_buffers_;

  // Stores buffers that are guaranteed to be fed to the decoder before fetching
  // more from the demuxer stream. All buffers in this queue first were in
  // |pending_buffers_|.
  base::circular_deque<scoped_refptr<DecoderBuffer>> fallback_buffers_;

  // TODO(tguilbert): support config changes during decoder fallback, see
  // crbug.com/603713
  bool received_config_change_during_reinit_;

  // Used to track read requests; not rolled into |state_| since that is
  // overwritten in many cases.
  bool pending_demuxer_read_;

  // Timestamp after which all outputs need to be prepared.
  base::TimeDelta skip_prepare_until_timestamp_;

  bool encryption_type_reported_ = false;

  int fallback_buffers_being_decoded_ = 0;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<DecoderStream<StreamType>> weak_factory_{this};

  // Used to invalidate pending decode requests and output callbacks.
  base::WeakPtrFactory<DecoderStream<StreamType>> fallback_weak_factory_{this};

  // Used to invalidate outputs awaiting preparation. This can't use either of
  // the above factories since they are used to bind one time callbacks given
  // to decoders that may not be reinitialized after Reset().
  base::WeakPtrFactory<DecoderStream<StreamType>> prepare_weak_factory_{this};
};

template <>
bool DecoderStream<DemuxerStream::AUDIO>::CanReadWithoutStalling() const;

template <>
int DecoderStream<DemuxerStream::AUDIO>::GetMaxDecodeRequests() const;

using VideoDecoderStream = DecoderStream<DemuxerStream::VIDEO>;
using AudioDecoderStream = DecoderStream<DemuxerStream::AUDIO>;

}  // namespace media

#endif  // MEDIA_FILTERS_DECODER_STREAM_H_
