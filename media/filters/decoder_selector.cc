// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decoder_selector.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
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

template <typename ConfigT, typename DecoderT>
DecoderPriority NormalDecoderPriority(const ConfigT& /*config*/,
                                      const DecoderT& /*decoder*/) {
  return DecoderPriority::kNormal;
}

DecoderPriority ResolutionBasedDecoderPriority(const VideoDecoderConfig& config,
                                               const VideoDecoder& decoder) {
#if defined(OS_ANDROID)
  constexpr auto kSoftwareDecoderHeightCutoff = 360;
#elif defined(OS_CHROMEOS)
  constexpr auto kSoftwareDecoderHeightCutoff = 360;
#else
  constexpr auto kSoftwareDecoderHeightCutoff = 720;
#endif

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

template <typename ConfigT, typename DecoderT>
DecoderPriority SkipNonPlatformDecoders(const ConfigT& /*config*/,
                                        const DecoderT& decoder) {
  return decoder.IsPlatformDecoder() ? DecoderPriority::kNormal
                                     : DecoderPriority::kSkipped;
}

void SetDefaultDecoderPriorityCB(VideoDecoderSelector::DecoderPriorityCB* out) {
  if (base::FeatureList::IsEnabled(kForceHardwareVideoDecoders)) {
    *out = base::BindRepeating(
        SkipNonPlatformDecoders<VideoDecoderConfig, VideoDecoder>);
  } else if (base::FeatureList::IsEnabled(kResolutionBasedDecoderPriority)) {
    *out = base::BindRepeating(ResolutionBasedDecoderPriority);
  } else {
    *out = base::BindRepeating(
        NormalDecoderPriority<VideoDecoderConfig, VideoDecoder>);
  }
}

void SetDefaultDecoderPriorityCB(AudioDecoderSelector::DecoderPriorityCB* out) {
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
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    CreateDecodersCB create_decoders_cb,
    MediaLog* media_log)
    : task_runner_(std::move(task_runner)),
      create_decoders_cb_(std::move(create_decoders_cb)),
      media_log_(media_log) {
  SetDefaultDecoderPriorityCB(&decoder_priority_cb_);
}

template <DemuxerStream::Type StreamType>
DecoderSelector<StreamType>::~DecoderSelector() {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (select_decoder_cb_)
    ReturnNullDecoder();
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
void DecoderSelector<StreamType>::SelectDecoder(
    SelectDecoderCB select_decoder_cb,
    typename Decoder::OutputCB output_cb) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
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
    ReturnNullDecoder();
    return;
  }

  // If this is the first selection (ever or since FinalizeDecoderSelection()),
  // start selection with the full list of potential decoders.
  if (!is_selecting_decoders_) {
    is_selecting_decoders_ = true;
    decoder_selection_start_ = base::TimeTicks::Now();
    CreateDecoders();
  }

  InitializeDecoder();
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::FinalizeDecoderSelection() {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!select_decoder_cb_);
  is_selecting_decoders_ = false;

  const std::string decoder_type = is_platform_decoder_ ? "HW" : "SW";
  const std::string stream_type =
      StreamType == DemuxerStream::AUDIO ? "Audio" : "Video";

  if (is_selecting_for_config_change_) {
    is_selecting_for_config_change_ = false;
    base::UmaHistogramTimes("Media.ConfigChangeDecoderSelectionTime." +
                                stream_type + "." + decoder_type,
                            base::TimeTicks::Now() - decoder_selection_start_);
  } else {
    // Initial selection
    base::UmaHistogramTimes(
        "Media.InitialDecoderSelectionTime." + stream_type + "." + decoder_type,
        base::TimeTicks::Now() - decoder_selection_start_);
  }

  if (is_codec_changing_) {
    is_codec_changing_ = false;
    base::UmaHistogramTimes(
        "Media.MSE.CodecChangeTime." + stream_type + "." + decoder_type,
        base::TimeTicks::Now() - codec_change_start_);
  }

  // Discard any remaining decoder instances, they won't be used.
  decoders_.clear();
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::NotifyConfigChanged() {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  is_selecting_for_config_change_ = true;

  DecoderConfig config = traits_->GetDecoderConfig(stream_);
  if (config.codec() != config_.codec()) {
    is_codec_changing_ = true;
    codec_change_start_ = base::TimeTicks::Now();
  }
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::PrependDecoder(
    std::unique_ptr<Decoder> decoder) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Decoders inserted directly should be given priority over those returned by
  // |create_decoders_cb_|.
  decoders_.insert(decoders_.begin(), std::move(decoder));

  if (is_selecting_decoders_)
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
void DecoderSelector<StreamType>::InitializeDecoder() {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!decoder_);

  if (decoders_.empty()) {
    // Decoder selection failed. If the stream is encrypted, try again using
    // DecryptingDemuxerStream.
    if (config_.is_encrypted() && cdm_context_) {
      InitializeDecryptingDemuxerStream();
      return;
    }

    ReturnNullDecoder();
    return;
  }

  // Initialize the first decoder on the list.
  decoder_ = std::move(decoders_.front());
  decoders_.erase(decoders_.begin());
  is_platform_decoder_ = decoder_->IsPlatformDecoder();
  TRACE_EVENT_ASYNC_STEP_INTO0("media", kSelectDecoderTrace, this,
                               decoder_->GetDisplayName());

  DVLOG(2) << __func__ << ": initializing " << decoder_->GetDisplayName();
  const bool is_live = stream_->liveness() == DemuxerStream::LIVENESS_LIVE;
  traits_->InitializeDecoder(
      decoder_.get(), config_, is_live, cdm_context_,
      base::BindOnce(&DecoderSelector<StreamType>::OnDecoderInitializeDone,
                     weak_this_factory_.GetWeakPtr()),
      output_cb_, waiting_cb_);
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::OnDecoderInitializeDone(Status status) {
  DVLOG(2) << __func__ << ": " << decoder_->GetDisplayName()
           << " success=" << std::hex << status.code();
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!status.is_ok()) {
    // TODO(tmathmeyer) this was too noisy in media log. Batch all the logs
    // together and then send them as an informational notice instead of
    // using NotifyError.
    MEDIA_LOG(INFO, media_log_)
        << "Failed to initialize " << decoder_->GetDisplayName();

    // Try the next decoder on the list.
    decoder_.reset();
    InitializeDecoder();
    return;
  }

  RunSelectDecoderCB();
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::ReturnNullDecoder() {
  DVLOG(1) << __func__ << ": No decoder selected";
  DCHECK(task_runner_->BelongsToCurrentThread());

  decrypting_demuxer_stream_.reset();
  decoder_.reset();
  decoders_.clear();
  RunSelectDecoderCB();
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
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (status != PIPELINE_OK) {
    // Since we already tried every potential decoder without DDS, give up.
    ReturnNullDecoder();
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
  InitializeDecoder();
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::RunSelectDecoderCB() {
  DCHECK(select_decoder_cb_);
  TRACE_EVENT_ASYNC_END2(
      "media", kSelectDecoderTrace, this, "type",
      DemuxerStream::GetTypeName(StreamType), "decoder",
      base::StringPrintf(
          "%s (%s)", decoder_ ? decoder_->GetDisplayName().c_str() : "null",
          decrypting_demuxer_stream_ ? "encrypted" : "unencrypted"));

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(select_decoder_cb_), std::move(decoder_),
                     std::move(decrypting_demuxer_stream_)));
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::FilterAndSortAvailableDecoders() {
  std::vector<std::unique_ptr<Decoder>> decoders = std::move(decoders_);
  std::vector<std::unique_ptr<Decoder>> deprioritized_decoders;

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
