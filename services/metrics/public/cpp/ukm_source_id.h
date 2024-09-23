// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_UKM_SOURCE_ID_H_
#define SERVICES_METRICS_PUBLIC_CPP_UKM_SOURCE_ID_H_

#include <stdint.h>
#include <string>

#include "services/metrics/public/cpp/metrics_export.h"

namespace ukm {

typedef int64_t SourceId;

const SourceId kInvalidSourceId = 0;

// An ID used to identify a Source to UKM, and contains the type information.
// These objects are copyable, assignable, and occupy 64-bits per instance.
// Prefer passing them by value. When a new type is added, please also update
// the enum type in third_party/metrics_proto/ukm/source.proto and the
// conversion function ToProtobufSourceType.
// NOTES ON USAGE: if only the underlying int value is required to identify a
// Source and is used in Mojo interface, and no type conversion needs to be
// performed, use ukm::SourceId instead.
// TODO(crbug.com/40671096): migrate callers to use the public methods below
// then remove METRICS_EXPORT on this class.
class METRICS_EXPORT SourceIdObj {
 public:
  enum class Type : SourceId {
    // Source ids of this type are created via ukm::AssignNewSourceId, to denote
    // 'custom' source other than the types below. Source of this type has
    // additional restrictions with logging, as determined by
    // IsWhitelistedSourceId.
    DEFAULT = 0,
    // Sources created by navigation. They will be kept in memory as long as
    // the associated tab is still alive and the number of sources are within
    // the max threshold.
    NAVIGATION_ID = 1,
    // Source ID used by AppLaunchEventLogger::Log and
    // AppPlatformMetrics::GetSourceId and DesktopWebAppUkmRecorder. They will
    // be kept in memory as long as the associated app is still running and the
    // number of sources are within the max threshold.
    APP_ID = 2,
    // Source ID for background events that don't have an open tab but the
    // associated URL is still present in the browsing history. A new source of
    // this type and associated events are expected to be recorded within the
    // same report interval; it will not be kept in memory between different
    // reports.
    HISTORY_ID = 3,
    // Source ID used by WebApkUkmRecorder. A new source of this type and
    // associated events are expected to be recorded within the same report
    // interval; it will not be kept in memory between different reports.
    WEBAPK_ID = 4,
    // Source ID for service worker based payment handlers. A new source of this
    // type and associated events are expected to be recorded within the same
    // report interval; it will not be kept in memory between different reports.
    PAYMENT_APP_ID = 5,
    // DEPRECATED. Use APP_ID instead.
    DEPRECATED_DESKTOP_WEB_APP_ID = 6,
    // Source ID for web workers, namely SharedWorkers and ServiceWorkers. Web
    // workers may inherit a source ID from the spawner context (in the case of
    // dedicated workers), or may have their own source IDs (in the case of
    // shared workers and service workers). Shared workers and service workers
    // can be connected to multiple clients (e.g. documents or other workers).
    WORKER_ID = 7,
    // Source ID type for metrics that doesn't need to be associated with a
    // specific URL. Metrics with this type will be whitelisted and always
    // recorded. A source ID of this type can be obtained with NoURLSourceId().
    NO_URL_ID = 8,
    // Source ID for server (HTTP) redirects. A new source of this type and
    // associated events are expected to be recorded within the same report
    // interval; it will not be kept in memory between different reports.
    REDIRECT_ID = 9,
    // Source ID type for Identity Providers used by the FedCM API. A new source
    // of this type and associated events are expected to be recorded within the
    // same report interval; it will not be kept in memory between different
    // reports. The URLs are provided by the developer when they call the FedCM
    // API, and hence do not follow a specific pattern. See
    // https://fedidcg.github.io/FedCM/#examples for examples.
    WEB_IDENTITY_ID = 10,
    // Source ID for ChromeOS website stats. A new source of this type and
    // associated events are expected to be recorded within the same report
    // interval; it will not be kept in memory between different reports.
    CHROMEOS_WEBSITE_ID = 11,
    // Source ID type for extensions. A new source of this
    // type and associated events are expected to be recorded within the same
    // report interval; it will not be kept in memory between different reports.
    // Some criteria (e.g. checking if it's a synced extension) will be applied
    // when recording metrics with this type.
    EXTENSION_ID = 12,
    // Source ID type for service-worker triggered persisted notification
    // events.
    // Notification events may occur in the background and an associated URL is
    // not necessarily present in the browsing history. A new source of this
    // type and associated events are expected to be recorded within the same
    // report
    // interval; it will not be kept in memory between different reports.
    NOTIFICATION_ID = 13,

    kMaxValue = NOTIFICATION_ID,
  };

  // Default constructor has the invalid value.
  constexpr SourceIdObj() : value_(kInvalidSourceId) {}

  constexpr SourceIdObj(const SourceIdObj& other) = default;
  constexpr SourceIdObj& operator=(const SourceIdObj& other) = default;

  // Allow identity comparisons.
  constexpr bool operator==(SourceIdObj other) const {
    return value_ == other.value_;
  }
  constexpr bool operator!=(SourceIdObj other) const {
    return value_ != other.value_;
  }

  // Extract the Type of the SourceId.
  Type GetType() const;

  // Return the ID as an int64.
  constexpr int64_t ToInt64() const { return value_; }

  // Convert an int64 ID value to an ID object.
  static constexpr SourceIdObj FromInt64(int64_t internal_value) {
    return SourceIdObj(internal_value);
  }

  // Get a new Default-type SourceId, which is unique within the scope of a
  // browser session.
  static SourceIdObj New();

  // Utility for converting other unique ids to source ids.
  static SourceIdObj FromOtherId(int64_t value, Type type);

 private:
  constexpr explicit SourceIdObj(int64_t value) : value_(value) {}
  int64_t value_;
};

constexpr SourceIdObj kInvalidSourceIdObj = SourceIdObj();

using SourceIdType = ukm::SourceIdObj::Type;

// Get a new source ID, which is unique for the duration of a browser session.
METRICS_EXPORT SourceId AssignNewSourceId();

// Utility for converting other unique ids to source ids.
METRICS_EXPORT SourceId ConvertToSourceId(int64_t other_id,
                                          SourceIdType id_type);

// Utility for getting source ID with NO_URL_ID type.
METRICS_EXPORT SourceId NoURLSourceId();

// Get the SourceIdType of the SourceId object.
METRICS_EXPORT SourceIdType GetSourceIdType(SourceId source_id);

// Get a string representation of the SourceIdType of the SourceId object.
METRICS_EXPORT std::string GetSourceIdTypeDebugString(SourceId source_id);
}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_UKM_SOURCE_ID_H_
