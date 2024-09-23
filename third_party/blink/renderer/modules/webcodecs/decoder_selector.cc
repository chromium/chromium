// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/decoder_selector.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/channel_layout.h"
#include "media/base/demuxer_stream.h"
#include "media/base/sample_format.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// Demuxing isn't part of WebCodecs. This shim allows us to reuse decoder
// selection logic from <video>.
// TODO(chcunningham): Maybe refactor DecoderSelector to separate dependency on
// media::DemuxerStream. DecoderSelection doesn't conceptually require a
// Demuxer. The tough part is re-working Decryptingmedia::DemuxerStream.
template <media::DemuxerStream::Type StreamType>
class NullDemuxerStream : public media::DemuxerStream {
 public:
  using DecoderConfigType =
      typename media::DecoderStreamTraits<StreamType>::DecoderConfigType;

  ~NullDemuxerStream() override = default;

  void Read(uint32_t count, ReadCB read_cb) override {
    NOTREACHED_IN_MIGRATION();
  }

  void Configure(DecoderConfigType config);

  media::AudioDecoderConfig audio_decoder_config() override {
    DCHECK_EQ(type(), media::DemuxerStream::AUDIO);
    return audio_decoder_config_;
  }

  media::VideoDecoderConfig video_decoder_config() override {
    DCHECK_EQ(type(), media::DemuxerStream::VIDEO);
    return video_decoder_config_;
  }

  Type type() const override { return stream_type; }

  bool SupportsConfigChanges() override {
    NOTREACHED_IN_MIGRATION();
    return true;
  }

  void set_low_delay(bool low_delay) { low_delay_ = low_delay; }
  media::StreamLiveness liveness() const override {
    return low_delay_ ? media::StreamLiveness::kLive
                      : media::StreamLiveness::kUnknown;
  }

 private:
  static const media::DemuxerStream::Type stream_type = StreamType;

  media::AudioDecoderConfig audio_decoder_config_;
  media::VideoDecoderConfig video_decoder_config_;
  bool low_delay_ = false;
};

template <>
void NullDemuxerStream<media::DemuxerStream::AUDIO>::Configure(
    DecoderConfigType config) {
  audio_decoder_config_ = config;
}

template <>
void NullDemuxerStream<media::DemuxerStream::VIDEO>::Configure(
    DecoderConfigType config) {
  video_decoder_config_ = config;
}

// TODO(crbug.com/368085608): Flip `enable_priority_based_selection` to true.
template <media::DemuxerStream::Type StreamType>
DecoderSelector<StreamType>::DecoderSelector(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    CreateDecodersCB create_decoders_cb,
    typename Decoder::OutputCB output_cb)
    : impl_(std::move(task_runner),
            std::move(create_decoders_cb),
            &null_media_log_,
            /*enable_priority_based_selection=*/false),
      demuxer_stream_(new NullDemuxerStream<StreamType>()),
      stream_traits_(CreateStreamTraits()),
      output_cb_(output_cb) {
  impl_.Initialize(stream_traits_.get(), demuxer_stream_.get(),
                   nullptr /*CdmContext*/, media::WaitingCB());
}

template <media::DemuxerStream::Type StreamType>
DecoderSelector<StreamType>::~DecoderSelector() = default;

template <media::DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::SelectDecoder(
    const DecoderConfig& config,
    bool low_delay,
    SelectDecoderCB select_decoder_cb) {
  // |impl_| will internally use this the |config| from our NullDemuxerStream.
  demuxer_stream_->Configure(config);
  demuxer_stream_->set_low_delay(low_delay);

  // media::DecoderSelector will call back with a DecoderStatus if selection is
  // in progress when it is destructed.
  impl_.BeginDecoderSelection(
      WTF::BindOnce(&DecoderSelector<StreamType>::OnDecoderSelected,
                    weak_factory_.GetWeakPtr(), std::move(select_decoder_cb)),
      output_cb_);
}

template <>
std::unique_ptr<WebCodecsAudioDecoderSelector::StreamTraits>
DecoderSelector<media::DemuxerStream::AUDIO>::CreateStreamTraits() {
  // TODO(chcunningham): Consider plumbing real hw channel layout.
  return std::make_unique<DecoderSelector::StreamTraits>(
      &null_media_log_, media::CHANNEL_LAYOUT_NONE,
      media::kUnknownSampleFormat);
}

template <>
std::unique_ptr<WebCodecsVideoDecoderSelector::StreamTraits>
DecoderSelector<media::DemuxerStream::VIDEO>::CreateStreamTraits() {
  return std::make_unique<DecoderSelector::StreamTraits>(&null_media_log_);
}

template <media::DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::OnDecoderSelected(
    SelectDecoderCB select_decoder_cb,
    DecoderOrError decoder_or_error,
    std::unique_ptr<media::DecryptingDemuxerStream> decrypting_demuxer_stream) {
  DCHECK(!decrypting_demuxer_stream);

  // We immediately finalize decoder selection.
  // TODO(chcunningham): Rework this to do finalize after first frame
  // successfully decoded. This updates to match latest plans for spec
  // (configure() no longer takes a promise).
  impl_.FinalizeDecoderSelection();

  if (!decoder_or_error.has_value()) {
    std::move(select_decoder_cb).Run(nullptr);
  } else {
    std::move(select_decoder_cb).Run(std::move(decoder_or_error).value());
  }
}

template class MODULES_EXPORT DecoderSelector<media::DemuxerStream::VIDEO>;
template class MODULES_EXPORT DecoderSelector<media::DemuxerStream::AUDIO>;

}  // namespace blink
