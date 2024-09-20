// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decoder_selector.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/audio_decoder.h"
#include "media/base/cdm_context.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_decoder.h"
#include "media/filters/decoder_stream_traits.h"
#include "media/filters/decrypting_demuxer_stream.h"

namespace media {

namespace {

constexpr char kSelectDecoderTrace[] = "DecoderSelector::SelectDecoder";

enum class DecoderPriority {
  // `kNormal` indicates that the current decoder should continue through with
  // selection in it's current order.
  kNormal,

  // `kDeprioritized` indicates that the current decoder should only be selected
  // if other decoders have failed.
  kDeprioritized,

  // `kSkipped` indicates that the current decoder should not be used at all.
  kSkipped,
};

DecoderPriority SelectDecoderPriority(const VideoDecoderConfig& config,
                                      const VideoDecoder& decoder) {
  constexpr auto kSoftwareDecoderHeightCutoff = 360;

  // We only do a height check to err on the side of prioritizing platform
  // decoders.
  const auto at_or_above_software_cutoff =
      config.visible_rect().height() >= kSoftwareDecoderHeightCutoff;

  // Platform decoders are deprioritized below the cutoff, and non-platform
  // decoders are deprioritized above it.
  return at_or_above_software_cutoff == decoder.IsPlatformDecoder()
             ? DecoderPriority::kNormal
             : DecoderPriority::kDeprioritized;
}

DecoderPriority SelectDecoderPriority(const AudioDecoderConfig& config,
                                      const AudioDecoder& decoder) {
  // Platform audio decoders are not currently prioritized or deprioritized
  return DecoderPriority::kNormal;
}

}  // namespace

template <DemuxerStream::Type StreamType>
DecoderSelector<StreamType>::DecoderSelector(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    CreateDecodersCB create_decoders_cb,
    MediaLog* media_log,
    bool enable_priority_based_selection)
    : task_runner_(std::move(task_runner)),
      create_decoders_cb_(std::move(create_decoders_cb)),
      media_log_(media_log),
      enable_priority_based_selection_(enable_priority_based_selection) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

template <DemuxerStream::Type StreamType>
DecoderSelector<StreamType>::~DecoderSelector() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (select_decoder_cb_)
    ReturnSelectionError(DecoderStatus::Codes::kFailed);
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::Initialize(StreamTraits* traits,
                                             DemuxerStream* stream,
                                             CdmContext* cdm_context,
                                             WaitingCB waiting_cb) {
  DVLOG(2) << __func__;
  DCHECK(traits);
  DCHECK(stream);

  traits_ = traits;
  stream_ = stream;
  cdm_context_ = cdm_context;
  waiting_cb_ = std::move(waiting_cb);
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::SelectDecoderInternal(
    SelectDecoderCB select_decoder_cb,
    typename Decoder::OutputCB output_cb,
    bool needs_new_decoders) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(select_decoder_cb);
  DCHECK(!select_decoder_cb_);
  select_decoder_cb_ = std::move(select_decoder_cb);
  output_cb_ = std::move(output_cb);
  config_ = traits_->GetDecoderConfig(stream_);

  TRACE_EVENT_ASYNC_BEGIN2("media", kSelectDecoderTrace, this, "type",
                           DemuxerStream::GetTypeName(StreamType), "config",
                           config_.AsHumanReadableString());

  if (!config_.IsValidConfig()) {
    DLOG(ERROR) << "Invalid stream config";
    ReturnSelectionError(DecoderStatus::Codes::kUnsupportedConfig);
    return;
  }

  if (needs_new_decoders) {
    decode_failure_reinit_cause_ = std::nullopt;
    CreateDecoders();
  }

  GetAndInitializeNextDecoder();
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::BeginDecoderSelection(
    SelectDecoderCB select_decoder_cb,
    typename Decoder::OutputCB output_cb) {
  SelectDecoderInternal(std::move(select_decoder_cb), std::move(output_cb),
                        /*needs_new_decoders = */ true);
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::ResumeDecoderSelection(
    SelectDecoderCB select_decoder_cb,
    typename Decoder::OutputCB output_cb,
    DecoderStatus&& reinit_cause) {
  DVLOG(2) << __func__;
  if (!decode_failure_reinit_cause_.has_value())
    decode_failure_reinit_cause_ = std::move(reinit_cause);
  SelectDecoderInternal(std::move(select_decoder_cb), std::move(output_cb),
                        /*needs_new_decoders = */ false);
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::FinalizeDecoderSelection() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!select_decoder_cb_);

  // Discard any remaining decoder instances, they won't be used.
  decoders_.clear();
  prefer_prepended_platform_decoder_ = false;
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::PrependDecoder(
    std::unique_ptr<Decoder> decoder) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(decoders_.empty());

  // Prefer the existing decoder if it's a platform decoder, regardless of the
  // current resolution. This avoids the potential for graphical glitches when
  // temporaily adapting below the hardware decoder threshold.
  prefer_prepended_platform_decoder_ = decoder->IsPlatformDecoder();
  decoders_.insert(decoders_.begin(), std::move(decoder));
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::CreateDecoders() {
  // Post-insert decoders returned by `create_decoders_cb_`, so that
  // any decoders added via `PrependDecoder()` are not overwritten and retain
  // priority (even if they are ultimately de-ranked by
  // `FilterAndSortAvailableDecoders()`)
  auto new_decoders = create_decoders_cb_.Run();
  std::move(new_decoders.begin(), new_decoders.end(),
            std::inserter(decoders_, decoders_.end()));
  FilterAndSortAvailableDecoders();
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::GetAndInitializeNextDecoder() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!decoder_);

  if (decoders_.empty()) {
    // Decoder selection failed. If the stream is encrypted, try again using
    // DecryptingDemuxerStream.
    if (config_.is_encrypted() && cdm_context_) {
      InitializeDecryptingDemuxerStream();
      return;
    }

    if (decode_failure_reinit_cause_.has_value()) {
      ReturnSelectionError(std::move(*decode_failure_reinit_cause_));
    } else {
      ReturnSelectionError(DecoderStatus::Codes::kUnsupportedConfig);
    }
    return;
  }

  // Initialize the first decoder on the list.
  decoder_ = std::move(decoders_.front());
  decoders_.erase(decoders_.begin());
  TRACE_EVENT_ASYNC_STEP_INTO0("media", kSelectDecoderTrace, this,
                               GetDecoderName(decoder_->GetDecoderType()));

  DVLOG(2) << __func__ << ": initializing " << decoder_->GetDecoderType();
  const bool is_live = stream_->liveness() == StreamLiveness::kLive;
  traits_->InitializeDecoder(
      decoder_.get(), config_, is_live, cdm_context_,
      base::BindOnce(&DecoderSelector<StreamType>::OnDecoderInitializeDone,
                     weak_this_factory_.GetWeakPtr()),
      output_cb_, waiting_cb_);
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::OnDecoderInitializeDone(
    DecoderStatus status) {
  DCHECK(decoder_);
  DVLOG(2) << __func__ << ": " << decoder_->GetDecoderType()
           << " success=" << static_cast<int>(status.code());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.is_ok()) {
    // Note: Don't track this decode status, as it is the result of decoder
    // selection (initialization) failure.
    MEDIA_LOG(INFO, media_log_)
        << "Cannot select " << decoder_->GetDecoderType() << " for "
        << DemuxerStream::GetTypeName(StreamType) << " decoding";

    // Try the next decoder on the list.
    decoder_ = nullptr;
    GetAndInitializeNextDecoder();
    return;
  }

  RunSelectDecoderCB(std::move(decoder_));
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::ReturnSelectionError(DecoderStatus error) {
  DVLOG(1) << __func__ << ": No decoder selected";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!error.is_ok());

  decrypting_demuxer_stream_.reset();
  decoders_.clear();
  prefer_prepended_platform_decoder_ = false;
  RunSelectDecoderCB(std::move(error));
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::InitializeDecryptingDemuxerStream() {
  DCHECK(decoders_.empty());
  DCHECK(config_.is_encrypted());
  DCHECK(cdm_context_);
  TRACE_EVENT_ASYNC_STEP_INTO0("media", kSelectDecoderTrace, this,
                               "DecryptingDemuxerStream");

  decrypting_demuxer_stream_ = std::make_unique<DecryptingDemuxerStream>(
      task_runner_, media_log_, waiting_cb_);

  decrypting_demuxer_stream_->Initialize(
      stream_, cdm_context_,
      base::BindOnce(
          &DecoderSelector<StreamType>::OnDecryptingDemuxerStreamInitializeDone,
          weak_this_factory_.GetWeakPtr()));
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::OnDecryptingDemuxerStreamInitializeDone(
    PipelineStatus status) {
  DVLOG(2) << __func__ << ": status=" << status;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != PIPELINE_OK) {
    // Since we already tried every potential decoder without DDS, give up.
    ReturnSelectionError(
        {DecoderStatus::Codes::kUnsupportedEncryptionMode, std::move(status)});
    return;
  }

  // Once DDS is enabled, there is no going back.
  // TODO(sandersd): Support transitions from encrypted to unencrypted.
  stream_ = decrypting_demuxer_stream_.get();
  cdm_context_ = nullptr;

  // We'll use the decrypted config from now on.
  config_ = traits_->GetDecoderConfig(stream_);
  DCHECK(!config_.is_encrypted());

  // Try decoder selection again now that DDS is being used.
  CreateDecoders();
  GetAndInitializeNextDecoder();
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::RunSelectDecoderCB(
    DecoderOrError decoder_or_error) {
  DCHECK(select_decoder_cb_);
  TRACE_EVENT_ASYNC_END2(
      "media", kSelectDecoderTrace, this, "type",
      DemuxerStream::GetTypeName(StreamType), "decoder",
      base::StringPrintf(
          "%s (%s)",
          decoder_or_error.has_value()
              ? GetDecoderName(decoder_or_error->GetDecoderType()).c_str()
              : "null",
          decrypting_demuxer_stream_ ? "encrypted" : "unencrypted"));

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(select_decoder_cb_), std::move(decoder_or_error),
                     std::move(decrypting_demuxer_stream_)));
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::FilterAndSortAvailableDecoders() {
  std::vector<std::unique_ptr<Decoder>> decoders = std::move(decoders_);
  std::vector<std::unique_ptr<Decoder>> deprioritized_decoders;

  size_t decoder_index = 0;
  for (auto& decoder : decoders) {
    ++decoder_index;

    // If the config is encrypted, skip decoders which don't support encryption.
    if (config_.is_encrypted() && !decoder->SupportsDecryption()) {
      continue;
    }

    if (!enable_priority_based_selection_) {
      decoders_.push_back(std::move(decoder));
      continue;
    }

    if (prefer_prepended_platform_decoder_ && decoder_index == 1) {
      decoders_.push_back(std::move(decoder));
      continue;
    }

    // Run the predicate on this decoder.
    switch (SelectDecoderPriority(config_, *decoder)) {
      case DecoderPriority::kSkipped:
        continue;
      case DecoderPriority::kNormal:
        decoders_.push_back(std::move(decoder));
        continue;
      case DecoderPriority::kDeprioritized:
        deprioritized_decoders.push_back(std::move(decoder));
        continue;
    }
  }

  // Post-insert deprioritized decoders
  std::move(deprioritized_decoders.begin(), deprioritized_decoders.end(),
            std::inserter(decoders_, decoders_.end()));
}

// These forward declarations tell the compiler that we will use
// DecoderSelector with these arguments, allowing us to keep these definitions
// in our .cc without causing linker errors. This also means if anyone tries to
// instantiate a DecoderSelector with anything but these two specializations
// they'll most likely get linker errors.
template class DecoderSelector<DemuxerStream::AUDIO>;
template class DecoderSelector<DemuxerStream::VIDEO>;

}  // namespace media
