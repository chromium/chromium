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

const char kSelectDecoderTrace[] = "DecoderSelector::SelectDecoder";

bool SkipDecoderForRTC(const AudioDecoderConfig& /*config*/,
                       const AudioDecoder& /*decoder*/) {
  return false;
}

bool SkipDecoderForRTC(const VideoDecoderConfig& config,
                       const VideoDecoder& decoder) {
  // Skip non-platform decoders for rtc based on the feature flag.
  return config.is_rtc() && !decoder.IsPlatformDecoder() &&
         !base::FeatureList::IsEnabled(kExposeSwDecodersToWebRTC);
}

template <typename ConfigT, typename DecoderT>
DecoderPriority NormalDecoderPriority(const ConfigT& config,
                                      const DecoderT& decoder) {
  if (SkipDecoderForRTC(config, decoder))
    return DecoderPriority::kSkipped;

  return DecoderPriority::kNormal;
}

DecoderPriority ResolutionBasedDecoderPriority(const VideoDecoderConfig& config,
                                               const VideoDecoder& decoder) {
#if BUILDFLAG(IS_ANDROID)
  constexpr auto kSoftwareDecoderHeightCutoff = 360;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  constexpr auto kSoftwareDecoderHeightCutoff = 360;
#else
  constexpr auto kSoftwareDecoderHeightCutoff = 720;
#endif

  if (SkipDecoderForRTC(config, decoder))
    return DecoderPriority::kSkipped;

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

DecoderPriority PreferNonPlatformDecoders(const VideoDecoderConfig& config,
                                          const VideoDecoder& decoder) {
  // Prefer software decoders over hardware decoders.  This is useful to force
  // software fallback for WebRTC, but still use hardware if there's no software
  // implementation to choose.
  return decoder.IsPlatformDecoder() ? DecoderPriority::kDeprioritized
                                     : DecoderPriority::kNormal;
}

DecoderPriority UnifiedDecoderPriority(const VideoDecoderConfig& config,
                                       const VideoDecoder& decoder) {
  if (config.is_rtc() ||
      base::FeatureList::IsEnabled(kResolutionBasedDecoderPriority)) {
    return ResolutionBasedDecoderPriority(config, decoder);
  } else {
    return NormalDecoderPriority(config, decoder);
  }
}

template <typename ConfigT, typename DecoderT>
DecoderPriority SkipNonPlatformDecoders(const ConfigT& config,
                                        const DecoderT& decoder) {
  if (SkipDecoderForRTC(config, decoder))
    return DecoderPriority::kSkipped;

  return decoder.IsPlatformDecoder() ? DecoderPriority::kNormal
                                     : DecoderPriority::kSkipped;
}

void SetDefaultDecoderPriorityCB(
    VideoDecoderSelector::DecoderPriorityCB* out,
    const DecoderStreamTraits<DemuxerStream::VIDEO>* traits) {
  if (base::FeatureList::IsEnabled(kForceHardwareVideoDecoders)) {
    *out = base::BindRepeating(
        SkipNonPlatformDecoders<VideoDecoderConfig, VideoDecoder>);
  } else if (traits->GetPreferNonPlatformDecoders()) {
    *out = base::BindRepeating(PreferNonPlatformDecoders);
  } else {
    *out = base::BindRepeating(UnifiedDecoderPriority);
  }
}

void SetDefaultDecoderPriorityCB(
    AudioDecoderSelector::DecoderPriorityCB* out,
    const DecoderStreamTraits<DemuxerStream::AUDIO>*) {
  if (base::FeatureList::IsEnabled(kForceHardwareAudioDecoders)) {
    *out = base::BindRepeating(
        SkipNonPlatformDecoders<AudioDecoderConfig, AudioDecoder>);
  } else {
    // Platform audio decoders are not currently prioritized or deprioritized
    *out = base::BindRepeating(
        NormalDecoderPriority<AudioDecoderConfig, AudioDecoder>);
  }
}

}  // namespace

template <DemuxerStream::Type StreamType>
DecoderSelector<StreamType>::DecoderSelector(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    CreateDecodersCB create_decoders_cb,
    MediaLog* media_log)
    : task_runner_(std::move(task_runner)),
      create_decoders_cb_(std::move(create_decoders_cb)),
      media_log_(media_log) {
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
  // Only set this here if nobody has overridden it for tests.
  if (!decoder_priority_cb_)
    SetDefaultDecoderPriorityCB(&decoder_priority_cb_, traits_);
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
    decode_failure_reinit_cause_ = absl::nullopt;
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
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::PrependDecoder(
    std::unique_ptr<Decoder> decoder) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Decoders inserted directly should be given priority over those returned by
  // |create_decoders_cb_|.
  decoders_.insert(decoders_.begin(), std::move(decoder));
  FilterAndSortAvailableDecoders();
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::OverrideDecoderPriorityCBForTesting(
    DecoderPriorityCB decoder_priority_cb) {
  decoder_priority_cb_ = std::move(decoder_priority_cb);
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
  DCHECK(decoder_priority_cb_);

  for (auto& decoder : decoders) {
    // Skip the decoder if this decoder doesn't support encryption for a
    // decrypting config
    if (config_.is_encrypted() && !decoder->SupportsDecryption())
      continue;

    // Run the predicate on this decoder.
    switch (decoder_priority_cb_.Run(config_, *decoder)) {
      case DecoderPriority::kSkipped:
        continue;
      case DecoderPriority::kNormal:
        decoders_.push_back(std::move(decoder));
        break;
      case DecoderPriority::kDeprioritized:
        deprioritized_decoders.push_back(std::move(decoder));
        break;
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
