// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_STATS_COLLECTING_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_STATS_COLLECTING_DECODER_H_

#include <algorithm>
#include <vector>

#include "base/callback.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/base/video_codecs.h"
#include "third_party/blink/renderer/platform/peerconnection/linear_histogram.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_decoder.h"

namespace blink {

// This class acts as a wrapper around the WebRTC video decoder that is used in
// Chrome.
//
// Its purpose is to collect decode performance statistics for the current video
// stream. The performance statistics is pushed to a local database through a
// callback and is later used to determine if a specific video configuration is
// considered to be smooth or not, see
// https://w3c.github.io/media-capabilities/. Smooth will be an optimistic
// prediction and data collection therefore only takes place if there's a single
// decoder active.
//
// It's assumed that Configure(), Decode(), and RegisterDecodeCompleteCallback()
// are called on the decode sequence. Decoded() may be called on either the
// decode sequecene or the media sequence depending on if the underlying decoder
// is a HW or SW decoder. However, the calls to Decoded() on these sequences are
// mutual exclusive. Release() may be called on any sequence as long as the
// decoding sequence has stopped.
class PLATFORM_EXPORT StatsCollectingDecoder
    : public webrtc::VideoDecoder,
      private webrtc::DecodedImageCallback {
 public:
  struct StatsKey {
    bool is_decode;
    media::VideoCodecProfile codec_profile;
    int pixel_size;
    bool hw_accelerated;
  };

  struct VideoStats {
    int frame_count;
    int key_frame_count;
    float p99_processing_time_ms;
  };

  // This callback is used to store processing stats.
  using StoreProcessingStatsCB =
      base::RepeatingCallback<void(const StatsKey&, const VideoStats&)>;

  // Creates a StatsCollectingDecoder object for the specified `format`.
  // `decoder` specifies the underlying decoder that is wrapped and all calls to
  // the methods of the webrtc::VideoDecoder interface are forwarded to
  // `decoder`. The provided `stats_callback` will be called periodically to
  // push the performance data that has been collected. The lifetime of
  // `stats_callback` must outlive the lifetime of the StatsCollectingDecoder.
  explicit StatsCollectingDecoder(const webrtc::SdpVideoFormat& format,
                                  std::unique_ptr<webrtc::VideoDecoder> decoder,
                                  StoreProcessingStatsCB stats_callback);

  ~StatsCollectingDecoder() override;

  // Implementation of webrtc::VideoDecoder.
  bool Configure(const Settings& settings) override;
  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override;
  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;
  int32_t Release() override;
  DecoderInfo GetDecoderInfo() const override;

 private:
  // Implementation of webrtc::DecodedImageCallback.
  int32_t Decoded(webrtc::VideoFrame& decodedImage) override;
  void Decoded(webrtc::VideoFrame& decodedImage,
               absl::optional<int32_t> decode_time_ms,
               absl::optional<uint8_t> qp) override;

  void StartStatsCollection();
  void ClearStatsCollection();
  void ReportStats() const;

  const media::VideoCodecProfile codec_profile_;
  const std::unique_ptr<webrtc::VideoDecoder> decoder_;
  const StoreProcessingStatsCB stats_callback_;
  webrtc::DecodedImageCallback* decoded_callback_{nullptr};

  // Lock for variables that are accessed in both Decode() and Decoded(). This
  // is needed because Decode() and Decoded() may be called simultaneously on
  // the decode sequence and the media sequence.
  base::Lock lock_;

  bool first_frame_decoded_{false};
  bool stats_collection_finished_{false};
  // Tracks the processing time in ms as well as the number of processed frames.
  std::unique_ptr<LinearHistogram> decode_time_ms_histogram_;
  // `number_of_new_keyframes_` is used to count the number of processed key
  // frames, which is only known in Decode(). The value of this counter is
  // continuously transferred to `number_of_keyframes_` and subsequently cleared
  // in the Decoded() callback. This way `number_of_keyframes_` can be accessed
  // without a lock where needed.
  size_t number_of_new_keyframes_ GUARDED_BY(lock_){0};
  // Tracks the total number of processed key frames.
  size_t number_of_keyframes_;  // Initialized in ClearStatsCollection().
  StatsKey current_stats_key_;  // Initialized in ClearStatsCollection().
  base::TimeTicks last_report_;
  base::TimeTicks last_check_for_simultaneous_decoders_;

  SEQUENCE_CHECKER(decoding_sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_STATS_COLLECTING_DECODER_H_
