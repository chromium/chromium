// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/audio_focus_manager_metrics_helper.h"

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/strings/string_util.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace media_session {

namespace {

static const char kHistogramPrefix[] = "Media.Session.AudioFocus.";
static const char kHistogramSeparator[] = ".";

static const char kRequestAudioFocusName[] = "Request";
static const char kAudioFocusTypeName[] = "Type";
static const char kAbandonAudioFocusName[] = "Abandon";

static constexpr base::HistogramBase::Sample kHistogramMinimum = 1;

}  // namespace

AudioFocusManagerMetricsHelper::AudioFocusManagerMetricsHelper(
    const std::string& source_name)
    : source_name_(source_name),
      request_source_histogram_(GetHistogram(
          kRequestAudioFocusName,
          static_cast<Sample>(AudioFocusRequestSource::kMaxValue))),
      focus_type_histogram_(
          GetHistogram(kAudioFocusTypeName,
                       static_cast<Sample>(AudioFocusType::kMaxValue))),
      abandon_source_histogram_(GetHistogram(
          kAbandonAudioFocusName,
          static_cast<Sample>(AudioFocusAbandonSource::kMaxValue))) {}

AudioFocusManagerMetricsHelper::~AudioFocusManagerMetricsHelper() = default;

void AudioFocusManagerMetricsHelper::OnRequestAudioFocus(
    AudioFocusManagerMetricsHelper::AudioFocusRequestSource source,
    mojom::AudioFocusType type) {
  if (!ShouldRecordMetrics())
    return;

  request_source_histogram_->Add(static_cast<Sample>(source));
  focus_type_histogram_->Add(static_cast<Sample>(FromMojoFocusType(type)));
}

void AudioFocusManagerMetricsHelper::OnAbandonAudioFocus(
    AudioFocusManagerMetricsHelper::AudioFocusAbandonSource source) {
  if (!ShouldRecordMetrics())
    return;

  abandon_source_histogram_->Add(static_cast<Sample>(source));
}

base::HistogramBase* AudioFocusManagerMetricsHelper::GetHistogram(
    const char* name,
    Sample max) const {
  std::string histogram_name;
  histogram_name.append(kHistogramPrefix);
  histogram_name.append(name);
  histogram_name.append(kHistogramSeparator);

  // This will ensure that |source_name| starts with an upper case letter.
  for (auto it = source_name_.begin(); it < source_name_.end(); ++it) {
    if (it == source_name_.begin())
      histogram_name.push_back(base::ToUpperASCII(*it));
    else
      histogram_name.push_back(*it);
  }

  return base::LinearHistogram::FactoryGet(histogram_name, kHistogramMinimum,
                                           max, max + 1,
                                           base::HistogramBase::kNoFlags);
}

// static
AudioFocusManagerMetricsHelper::AudioFocusType
AudioFocusManagerMetricsHelper::FromMojoFocusType(mojom::AudioFocusType type) {
  switch (type) {
    case mojom::AudioFocusType::kGain:
      return AudioFocusType::kGain;
    case mojom::AudioFocusType::kGainTransientMayDuck:
      return AudioFocusType::kGainTransientMayDuck;
    case mojom::AudioFocusType::kGainTransient:
      return AudioFocusType::kGainTransient;
  }

  NOTREACHED();
  return AudioFocusType::kUnknown;
}

bool AudioFocusManagerMetricsHelper::ShouldRecordMetrics() const {
  return !source_name_.empty();
}

}  // namespace media_session
