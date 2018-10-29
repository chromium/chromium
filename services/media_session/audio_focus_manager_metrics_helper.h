// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_METRICS_HELPER_H_
#define SERVICES_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_METRICS_HELPER_H_

#include <string>

#include "base/macros.h"
#include "base/metrics/histogram_base.h"

namespace media_session {

namespace mojom {
enum class AudioFocusType;
}  // namespace mojom

class AudioFocusManagerMetricsHelper {
 public:
  using Sample = base::HistogramBase::Sample;

  AudioFocusManagerMetricsHelper(const std::string& source_name);
  ~AudioFocusManagerMetricsHelper();

  // This is used for UMA histogram
  // (Media.Session.AudioFocus.*.RequestAudioFocus). New values should be
  // appended only and update |kMaxValue|.
  enum class AudioFocusRequestSource {
    kUnknown = 0,
    kInitial = 1,
    kUpdate = 2,
    kMaxValue = kUpdate  // Leave at the end.
  };

  // This is used for UMA histogram
  // (Media.Session.AudioFocus.*.AbandonAudioFocus). New values should be
  // appended only and update |kMaxValue|.
  enum class AudioFocusAbandonSource {
    kUnknown = 0,
    kAPI = 1,
    kConnectionError = 2,
    kMaxValue = kConnectionError  // Leave at the end.
  };

  // This is used for UMA histogram
  // (Media.Session.AudioFocus.*.AudioFocusType). New values should be
  // appended only and update |kMaxValue|. It should mirror the
  // media_session::mojom::AudioFocusType enum.
  enum class AudioFocusType {
    kUnknown = 0,
    kGain = 1,
    kGainTransientMayDuck = 2,
    kGainTransient = 3,
    kMaxValue = kGainTransient  // Leave at the end.
  };

  void OnRequestAudioFocus(AudioFocusRequestSource, mojom::AudioFocusType);
  void OnAbandonAudioFocus(AudioFocusAbandonSource);

 private:
  static AudioFocusType FromMojoFocusType(mojom::AudioFocusType);

  base::HistogramBase* GetHistogram(const char* name, Sample max) const;

  bool ShouldRecordMetrics() const;

  std::string source_name_;

  base::HistogramBase* const request_source_histogram_;
  base::HistogramBase* const focus_type_histogram_;
  base::HistogramBase* const abandon_source_histogram_;

  DISALLOW_COPY_AND_ASSIGN(AudioFocusManagerMetricsHelper);
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_METRICS_HELPER_H_
