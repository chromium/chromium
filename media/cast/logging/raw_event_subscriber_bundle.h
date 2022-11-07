// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_RAW_EVENT_SUBSCRIBER_BUNDLE_H_
#define MEDIA_CAST_LOGGING_RAW_EVENT_SUBSCRIBER_BUNDLE_H_

#include "base/memory/scoped_refptr.h"
#include "media/cast/logging/encoding_event_subscriber.h"
#include "media/cast/logging/stats_event_subscriber.h"

namespace media {
namespace cast {

class CastEnvironment;
class ReceiverTimeOffsetEstimator;

// Allow 9MB for serialized video / audio event logs.
const int kMaxSerializedBytes = 9000000;

// Assume serialized log data for each frame will take up to 150 bytes.
const int kMaxVideoEventEntries = kMaxSerializedBytes / 150;

// Assume serialized log data for each frame will take up to 75 bytes.
const int kMaxAudioEventEntries = kMaxSerializedBytes / 75;

// A bundle for raw event subscribers for a single stream.
// It contains an EncodingEventSubscriber and a StatsSubscriber.
class RawEventSubscriberBundleForStream {
 public:
  RawEventSubscriberBundleForStream(
      const scoped_refptr<CastEnvironment>& cast_environment,
      bool is_audio,
      ReceiverTimeOffsetEstimator* offset_estimator);

  RawEventSubscriberBundleForStream(const RawEventSubscriberBundleForStream&) =
      delete;
  RawEventSubscriberBundleForStream& operator=(
      const RawEventSubscriberBundleForStream&) = delete;

  ~RawEventSubscriberBundleForStream();

  EncodingEventSubscriber* GetEncodingEventSubscriber();
  StatsEventSubscriber* GetStatsEventSubscriber();

 private:
  const scoped_refptr<CastEnvironment> cast_environment_;
  EncodingEventSubscriber event_subscriber_;
  StatsEventSubscriber stats_subscriber_;
};

// A bundle of subscribers for all streams. An instance of this object
// is associated with a CastEnvironment.
// This class can be used for managing event subscribers
// in a session where they could be multiple streams (i.e. CastSessionDelegate).
// It also contains a ReceiverTimeOffsetEstimator that is shared by subscribers
// of different streams.
class RawEventSubscriberBundle {
 public:
  explicit RawEventSubscriberBundle(
      const scoped_refptr<CastEnvironment>& cast_environment);

  RawEventSubscriberBundle(const RawEventSubscriberBundle&) = delete;
  RawEventSubscriberBundle& operator=(const RawEventSubscriberBundle&) = delete;

  ~RawEventSubscriberBundle();

  void AddEventSubscribers(bool is_audio);
  void RemoveEventSubscribers(bool is_audio);
  EncodingEventSubscriber* GetEncodingEventSubscriber(
      bool is_audio);
  StatsEventSubscriber* GetStatsEventSubscriber(bool is_audio);

 private:
  const scoped_refptr<CastEnvironment> cast_environment_;
  // Map from (is_audio) -> RawEventSubscriberBundleForStream.
  // TODO(imcheng): This works because we only have 1 audio and 1 video stream.
  // This needs to scale better.
  std::map<bool, std::unique_ptr<RawEventSubscriberBundleForStream>>
      subscribers_;
  std::unique_ptr<ReceiverTimeOffsetEstimator> receiver_offset_estimator_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_RAW_EVENT_SUBSCRIBER_BUNDLE_H_
