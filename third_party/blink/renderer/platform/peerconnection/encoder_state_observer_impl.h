// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_ENCODER_STATE_OBSERVER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_ENCODER_STATE_OBSERVER_IMPL_H_

#include <memory>
#include <optional>

#include "base/atomic_ref_count.h"
#include "base/containers/flat_map.h"
#include "base/location.h"
#include "third_party/blink/renderer/platform/peerconnection/encoder_state_observer.h"
#include "third_party/blink/renderer/platform/peerconnection/stats_collector.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/video_codecs/video_codec.h"

namespace blink {
// EncoderStateObserverImpl collects the encode stats for the top spatial layer
// in SVC encoding, top stream in simulcast or the vanilla stream otherwise. It
// doesn't collect stats if multiple encoders are running.
class PLATFORM_EXPORT EncoderStateObserverImpl : public EncoderStateObserver,
                                                 public StatsCollector {
 public:
  struct TopLayerInfo {
    int encoder_id;
    int spatial_id;
    int pixel_rate;
  };

  EncoderStateObserverImpl(
      media::VideoCodecProfile profile,
      const StatsCollector::StoreProcessingStatsCB& store_processing_stats_cb);
  ~EncoderStateObserverImpl() override;

  // EncoderStateObserver implementation.
  void OnEncoderCreated(int encoder_id,
                        const webrtc::VideoCodec& config) override;
  void OnEncoderDestroyed(int encoder_id) override;
  void OnRatesUpdated(int encoder_id,
                      const WTF::Vector<bool>& active_spatial_layers) override;
  void OnEncode(int encoder_id, uint32_t rtp_timestamp) override;
  void OnEncodedFrame(int encoder_id,
                      const webrtc::EncodedImage& frame,
                      bool is_hardware_accelerated) override;

  std::optional<TopLayerInfo> FindHighestActiveEncoding() const;

 private:
  class EncoderState;

  EncoderState* GetEncoderState(
      int encoder_id,
      base::Location location = base::Location::Current());
  void UpdateStatsCollection(base::TimeTicks now);

  base::flat_map<int, std::unique_ptr<EncoderState>> encoder_state_by_id_;
  std::optional<TopLayerInfo> top_encoder_info_;

  base::TimeTicks last_update_stats_collection_time_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_ENCODER_STATE_OBSERVER_IMPL_H_
