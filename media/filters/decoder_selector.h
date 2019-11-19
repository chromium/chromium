// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DECODER_SELECTOR_H_
#define MEDIA_FILTERS_DECODER_SELECTOR_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline_status.h"
#include "media/base/waiting.h"
#include "media/filters/decoder_stream_traits.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class CdmContext;
class DecryptingDemuxerStream;
class MediaLog;

// DecoderSelector handles construction and initialization of Decoders for a
// DemuxerStream, and maintains the state required for decoder fallback.
// The template parameter |StreamType| is the type of stream we will be
// selecting a decoder for.
template<DemuxerStream::Type StreamType>
class MEDIA_EXPORT DecoderSelector {
 public:
  typedef DecoderStreamTraits<StreamType> StreamTraits;
  typedef typename StreamTraits::DecoderType Decoder;
  typedef typename StreamTraits::DecoderConfigType DecoderConfig;

  // Callback to create a list of decoders to select from.
  // TODO(xhwang): Use a DecoderFactory to create decoders one by one as needed,
  // instead of creating a list of decoders all at once.
  using CreateDecodersCB =
      base::RepeatingCallback<std::vector<std::unique_ptr<Decoder>>()>;

  // Emits the result of a single call to SelectDecoder(). Parameters are
  //   1: The initialized Decoder. nullptr if selection failed.
  //   2: The initialized DecryptingDemuxerStream, if one was created. This
  //      happens at most once for a DecoderSelector instance.
  // The caller owns the Decoder and DecryptingDemuxerStream.
  //
  // The caller should call DecryptingDemuxerStream::Reset() before
  // calling Decoder::Reset() to release any pending decryption or read.
  using SelectDecoderCB =
      base::OnceCallback<void(std::unique_ptr<Decoder>,
                              std::unique_ptr<DecryptingDemuxerStream>)>;

  DecoderSelector(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                  CreateDecodersCB create_decoders_cb,
                  MediaLog* media_log);

  // Aborts any pending decoder selection.
  ~DecoderSelector();

  // Initialize with stream parameters. Should be called exactly once.
  void Initialize(StreamTraits* traits,
                  DemuxerStream* stream,
                  CdmContext* cdm_context,
                  WaitingCB waiting_cb);

  // Selects and initializes a decoder, which will be returned via
  // |select_decoder_cb| posted to |task_runner|. Subsequent calls to
  // SelectDecoder() will return different decoder instances, until all
  // potential decoders have been exhausted.
  //
  // When the caller determines that decoder selection has succeeded (eg.
  // because the decoder decoded a frame successfully), it should call
  // FinalizeDecoderSelection().
  //
  // Must not be called while another selection is pending.
  void SelectDecoder(SelectDecoderCB select_decoder_cb,
                     typename Decoder::OutputCB output_cb);

  // Signals that decoder selection has been completed (successfully). Future
  // calls to SelectDecoder() will select from the full list of decoders.
  void FinalizeDecoderSelection();

  // Signals that a config change has started being processed.
  // Currently only for metric collection.
  void NotifyConfigChanged();

 private:
  void InitializeDecoder();
  void OnDecoderInitializeDone(bool success);
  void ReturnNullDecoder();
  void InitializeDecryptingDemuxerStream();
  void OnDecryptingDemuxerStreamInitializeDone(PipelineStatus status);
  void RunSelectDecoderCB();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  CreateDecodersCB create_decoders_cb_;
  MediaLog* media_log_;

  StreamTraits* traits_ = nullptr;
  DemuxerStream* stream_ = nullptr;
  CdmContext* cdm_context_ = nullptr;
  WaitingCB waiting_cb_;

  // Overall decoder selection state.
  DecoderConfig config_;
  bool is_selecting_decoders_ = false;
  std::vector<std::unique_ptr<Decoder>> decoders_;

  // State for a single SelectDecoder() invocation.
  SelectDecoderCB select_decoder_cb_;
  typename Decoder::OutputCB output_cb_;
  std::unique_ptr<Decoder> decoder_;
  std::unique_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream_;

  // Metrics.
  bool is_platform_decoder_ = false;
  bool is_codec_changing_ = false;
  base::TimeTicks codec_change_start_;

  base::WeakPtrFactory<DecoderSelector> weak_this_factory_{this};

  DISALLOW_IMPLICIT_CONSTRUCTORS(DecoderSelector);
};

typedef DecoderSelector<DemuxerStream::VIDEO> VideoDecoderSelector;
typedef DecoderSelector<DemuxerStream::AUDIO> AudioDecoderSelector;

}  // namespace media

#endif  // MEDIA_FILTERS_DECODER_SELECTOR_H_
