// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/instrumented_simulcast_adapter.h"

#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "third_party/blink/renderer/platform/peerconnection/instrumented_video_encoder_wrapper.h"
#include "third_party/blink/renderer/platform/peerconnection/video_encoder_state_observer.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"

namespace blink {
class InstrumentedSimulcastAdapter::EncoderFactoryAdapter
    : public webrtc::VideoEncoderFactory {
 public:
  EncoderFactoryAdapter(webrtc::VideoEncoderFactory* encoder_factory,
                        VideoEncoderStateObserver* state_observer,
                        bool is_primary)
      : encoder_factory_(encoder_factory),
        state_observer_(state_observer),
        is_primary_(is_primary) {
    // The constructor is performed in the webrtc worker thread, not webrtc
    // encoder sequence.
    DETACH_FROM_SEQUENCE(encoder_sequence_);
  }

  ~EncoderFactoryAdapter() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
  }

  // webrtc::VideoEncoderFactory implementations.
  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
    return encoder_factory_->GetSupportedFormats();
  }

  std::vector<webrtc::SdpVideoFormat> GetImplementations() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
    return encoder_factory_->GetImplementations();
  }
  CodecSupport QueryCodecSupport(
      const webrtc::SdpVideoFormat& format,
      std::optional<std::string> scalability_mode) const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
    return encoder_factory_->QueryCodecSupport(format, scalability_mode);
  }
  std::unique_ptr<webrtc::VideoEncoder> Create(
      const webrtc::Environment& env,
      const webrtc::SdpVideoFormat& format) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
    std::unique_ptr<webrtc::VideoEncoder> encoder =
        encoder_factory_->Create(env, format);
    next_encoder_id_ += is_primary_ ? 1 : -1;
    return std::make_unique<InstrumentedVideoEncoderWrapper>(
        next_encoder_id_, std::move(encoder), state_observer_);
  }
  std::unique_ptr<webrtc::VideoEncoderFactory::EncoderSelectorInterface>
  GetEncoderSelector() const override {
    return encoder_factory_->GetEncoderSelector();
  }

 private:
  const raw_ptr<webrtc::VideoEncoderFactory> encoder_factory_;
  const raw_ptr<VideoEncoderStateObserver> state_observer_;
  const bool is_primary_;

  int next_encoder_id_ GUARDED_BY_CONTEXT(encoder_sequence_);

  // WebRTC encoder sequence.
  SEQUENCE_CHECKER(encoder_sequence_);
};

std::unique_ptr<InstrumentedSimulcastAdapter>
InstrumentedSimulcastAdapter::Create(
    const webrtc::Environment& env,
    webrtc::VideoEncoderFactory* primary_encoder_factory,
    webrtc::VideoEncoderFactory* secondary_encoder_factory,
    std::unique_ptr<VideoEncoderStateObserver> encoder_state_observer,
    const webrtc::SdpVideoFormat& format) {
  // InstrumentedSimulcastAdapter is created on the webrtc worker sequence.
  // The operations (e.g. InitEncode() and Encode()) are performed in the
  // encoder sequence.
  std::unique_ptr<EncoderFactoryAdapter> primary_factory_adapter;
  std::unique_ptr<EncoderFactoryAdapter> secondary_factory_adapter;
  if (primary_encoder_factory) {
    primary_factory_adapter = std::make_unique<EncoderFactoryAdapter>(
        primary_encoder_factory, encoder_state_observer.get(),
        /*is_primary=*/true);
  }
  if (secondary_encoder_factory) {
    secondary_factory_adapter = std::make_unique<EncoderFactoryAdapter>(
        secondary_encoder_factory, encoder_state_observer.get(),
        /*is_primary=*/false);
  }
  return std::unique_ptr<InstrumentedSimulcastAdapter>(
      new InstrumentedSimulcastAdapter(env, std::move(primary_factory_adapter),
                                       std::move(secondary_factory_adapter),
                                       std::move(encoder_state_observer),
                                       format));
}

InstrumentedSimulcastAdapter::~InstrumentedSimulcastAdapter() {
  // The destructor is executed in the encoder sequence. This is checked by
  // the sequence checker in EncoderFactoryAdapter.

  // VideoEncoderStateObserver must outlive encoders.
  DestroyStoredEncoders();
}

InstrumentedSimulcastAdapter::InstrumentedSimulcastAdapter(
    const webrtc::Environment& env,
    std::unique_ptr<EncoderFactoryAdapter> primary_factory_adapter,
    std::unique_ptr<EncoderFactoryAdapter> secondary_factory_adapter,
    std::unique_ptr<VideoEncoderStateObserver> encoder_state_observer,
    const webrtc::SdpVideoFormat& format)
    : webrtc::SimulcastEncoderAdapter(env,
                                      primary_factory_adapter.get(),
                                      secondary_factory_adapter.get(),
                                      format),
      encoder_state_observer_(std::move(encoder_state_observer)),
      primary_factory_adapter_(std::move(primary_factory_adapter)),
      secondary_factory_adapter_(std::move(secondary_factory_adapter)) {}
}  // namespace blink
