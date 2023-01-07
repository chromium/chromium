// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/raw_event_subscriber_bundle.h"

#include <memory>

#include "media/cast/cast_environment.h"
#include "media/cast/logging/receiver_time_offset_estimator_impl.h"

namespace media {
namespace cast {

RawEventSubscriberBundleForStream::RawEventSubscriberBundleForStream(
    const scoped_refptr<CastEnvironment>& cast_environment,
    bool is_audio,
    ReceiverTimeOffsetEstimator* offset_estimator)
    : cast_environment_(cast_environment),
      event_subscriber_(
          is_audio ? AUDIO_EVENT : VIDEO_EVENT,
          is_audio ? kMaxAudioEventEntries : kMaxVideoEventEntries),
      stats_subscriber_(
          is_audio ? AUDIO_EVENT : VIDEO_EVENT,
          cast_environment->Clock(), offset_estimator) {
  cast_environment_->logger()->Subscribe(&event_subscriber_);
  cast_environment_->logger()->Subscribe(&stats_subscriber_);
}

RawEventSubscriberBundleForStream::~RawEventSubscriberBundleForStream() {
  cast_environment_->logger()->Unsubscribe(&event_subscriber_);
  cast_environment_->logger()->Unsubscribe(&stats_subscriber_);
}

EncodingEventSubscriber*
RawEventSubscriberBundleForStream::GetEncodingEventSubscriber() {
  return &event_subscriber_;
}

StatsEventSubscriber*
RawEventSubscriberBundleForStream::GetStatsEventSubscriber() {
  return &stats_subscriber_;
}

RawEventSubscriberBundle::RawEventSubscriberBundle(
    const scoped_refptr<CastEnvironment>& cast_environment)
    : cast_environment_(cast_environment) {}

RawEventSubscriberBundle::~RawEventSubscriberBundle() {
  if (receiver_offset_estimator_.get())
    cast_environment_->logger()->Unsubscribe(receiver_offset_estimator_.get());
}

void RawEventSubscriberBundle::AddEventSubscribers(bool is_audio) {
  if (!receiver_offset_estimator_.get()) {
    receiver_offset_estimator_ =
        std::make_unique<ReceiverTimeOffsetEstimatorImpl>();
    cast_environment_->logger()->Subscribe(receiver_offset_estimator_.get());
  }
  auto it = subscribers_.find(is_audio);
  if (it != subscribers_.end())
    return;

  subscribers_.insert(std::make_pair(
      is_audio,
      std::make_unique<RawEventSubscriberBundleForStream>(
          cast_environment_, is_audio, receiver_offset_estimator_.get())));
}

void RawEventSubscriberBundle::RemoveEventSubscribers(bool is_audio) {
  auto it = subscribers_.find(is_audio);
  if (it == subscribers_.end())
    return;

  subscribers_.erase(it);
  if (subscribers_.empty()) {
    cast_environment_->logger()->Unsubscribe(receiver_offset_estimator_.get());
    receiver_offset_estimator_.reset();
  }
}

EncodingEventSubscriber*
RawEventSubscriberBundle::GetEncodingEventSubscriber(bool is_audio) {
  auto it = subscribers_.find(is_audio);
  return it == subscribers_.end() ? nullptr
                                  : it->second->GetEncodingEventSubscriber();
}

StatsEventSubscriber*
RawEventSubscriberBundle::GetStatsEventSubscriber(bool is_audio) {
  auto it = subscribers_.find(is_audio);
  return it == subscribers_.end() ? nullptr
                                  : it->second->GetStatsEventSubscriber();
}

}  // namespace cast
}  // namespace media
