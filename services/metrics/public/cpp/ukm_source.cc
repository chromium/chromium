// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/ukm_source.h"

#include <string>
#include <utility>
#include <vector>

#include "base/atomicops.h"
#include "base/check_op.h"
#include "base/hash/hash.h"
#include "base/notreached.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/metrics_proto/ukm/source.pb.h"

namespace ukm {

namespace {

// The maximum length of a URL we will record. URL exceeding this limit will be
// truncated.
constexpr int kMaxURLLength = 2 * 1024;

// Using a simple global assumes that all access to it will be done on the same
// thread, namely the UI thread. If this becomes not the case then it can be
// changed to an Atomic32 (make AndroidActivityTypeState derive from int32_t)
// and accessed with no-barrier loads and stores.
int32_t g_android_activity_type_state = -1;

// Returns where the URL got truncated.
TruncationStatus GetTruncationStatus(const GURL& url) {
  // Determine whether the URL was truncated within the query (preserving the
  // full path), within the path (preserving the URL without query), or within
  // the origin.
  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();
  int query_start = parsed.CountCharactersBefore(url::Parsed::QUERY, true);
  int path_start = parsed.CountCharactersBefore(url::Parsed::PATH, true);

  if (url.has_query() && kMaxURLLength >= query_start) {
    return TRUNCATED_AT_FULL_PATH;
  } else if (kMaxURLLength > path_start) {
    return TRUNCATED_AT_URL_WITHOUT_QUERY;
  } else {
    return TRUNCATED_AT_ORIGIN;
  }
}

// Sets the URL on the UrlInfo protobuf, and truncates the string if it's too
// long. Also sets the truncation_status on the UrlInfo if it's truncated.
void SetTruncatedURL(const GURL& url, UrlInfo* url_info) {
  if (url.spec().length() <= kMaxURLLength) {
    url_info->set_url(url.spec());
    url_info->set_truncation_status(NOT_TRUNCATED);
    return;
  }

  url_info->set_url(url.spec().substr(0, kMaxURLLength));
  url_info->set_truncation_status(GetTruncationStatus(url));
}

// Translates ukm::SourceIdType to the equivalent Source proto enum value.
SourceType ToProtobufSourceType(SourceIdType source_id_type) {
  switch (source_id_type) {
    case SourceIdType::DEFAULT:
      return SourceType::DEFAULT;
    case SourceIdType::NAVIGATION_ID:
      return SourceType::NAVIGATION_ID;
    case SourceIdType::APP_ID:
      return SourceType::APP_ID;
    case SourceIdType::HISTORY_ID:
      return SourceType::HISTORY_ID;
    case SourceIdType::WEBAPK_ID:
      return SourceType::WEBAPK_ID;
    case SourceIdType::PAYMENT_APP_ID:
      return SourceType::PAYMENT_APP_ID;
    case SourceIdType::DEPRECATED_DESKTOP_WEB_APP_ID:
      return SourceType::DESKTOP_WEB_APP_ID;
    case SourceIdType::WORKER_ID:
      return SourceType::WORKER_ID;
    case SourceIdType::NO_URL_ID:
      return SourceType::NO_URL_ID;
    case SourceIdType::REDIRECT_ID:
      return SourceType::REDIRECT_ID;
    case SourceIdType::WEB_IDENTITY_ID:
      return SourceType::WEB_IDENTITY_ID;
    case SourceIdType::CHROMEOS_WEBSITE_ID:
      return SourceType::CHROMEOS_WEBSITE_ID;
    case SourceIdType::EXTENSION_ID:
      return SourceType::EXTENSION_ID;
    case SourceIdType::NOTIFICATION_ID:
      return SourceType::NOTIFICATION_ID;
    case SourceIdType::CDM_ID:
      return SourceType::CDM_ID;
  }
}

AndroidActivityType ToProtobufActivityType(int32_t type) {
  switch (type) {
    case 0:
      return AndroidActivityType::TABBED;
    case 1:
      return AndroidActivityType::CUSTOM_TAB;
    case 2:
      return AndroidActivityType::TRUSTED_WEB_ACTIVITY;
    case 3:
      return AndroidActivityType::WEB_APP;
    case 4:
      return AndroidActivityType::WEB_APK;
    case 5:
      return AndroidActivityType::PRE_FIRST_TAB;
    case 6:
      return AndroidActivityType::AUTH_TAB;
    case 7:
      return AndroidActivityType::DEV_TOOLS;
    default:
      NOTREACHED();
  }
}

}  // namespace

// static
void UkmSource::SetAndroidActivityTypeState(int32_t activity_type) {
  g_android_activity_type_state = activity_type;
}

UkmSource::NavigationData::NavigationData() = default;
UkmSource::NavigationData::~NavigationData() = default;

UkmSource::NavigationData::NavigationData(const NavigationData& other) =
    default;

UkmSource::NavigationData UkmSource::NavigationData::CopyWithSanitizedUrls(
    std::vector<GURL> sanitized_urls) const {
  DCHECK(!sanitized_urls.empty());
  DCHECK(!sanitized_urls.back().is_empty());
  DCHECK(!sanitized_urls.front().is_empty());

  NavigationData sanitized_navigation_data;
  sanitized_navigation_data.urls = std::move(sanitized_urls);
  sanitized_navigation_data.previous_source_id = previous_source_id;
  sanitized_navigation_data.previous_same_document_source_id =
      previous_same_document_source_id;
  sanitized_navigation_data.opener_source_id = opener_source_id;
  sanitized_navigation_data.tab_id = tab_id;
  sanitized_navigation_data.is_same_document_navigation =
      is_same_document_navigation;
  sanitized_navigation_data.same_origin_status = same_origin_status;
  sanitized_navigation_data.is_renderer_initiated = is_renderer_initiated;
  sanitized_navigation_data.is_error_page = is_error_page;
  sanitized_navigation_data.navigation_time = navigation_time;
  sanitized_navigation_data.resolved_urls = resolved_urls;
  return sanitized_navigation_data;
}

UkmSource::UkmSource(ukm::SourceId id, const GURL& url)
    : id_(id),
      type_(GetSourceIdType(id_)),
      android_activity_type_state_(g_android_activity_type_state),
      creation_time_(base::TimeTicks::Now()) {
  if (!url.is_empty()) {
    navigation_data_.urls = {url};
  }
}

UkmSource::UkmSource(ukm::SourceId id, const NavigationData& navigation_data)
    : id_(id),
      type_(GetSourceIdType(id_)),
      navigation_data_(navigation_data),
      android_activity_type_state_(g_android_activity_type_state),
      creation_time_(base::TimeTicks::Now()) {
  DCHECK(type_ == SourceIdType::NAVIGATION_ID);
}

UkmSource::~UkmSource() = default;

void UkmSource::UpdateUrl(const GURL& new_url) {
  DCHECK(!new_url.is_empty());
  DCHECK_EQ(1u, navigation_data_.urls.size());
  if (url() == new_url)
    return;
  navigation_data_.urls = {new_url};
}

void UkmSource::set_resolved_urls(const std::vector<GURL>& resolved_urls) {
  navigation_data_.resolved_urls = resolved_urls;
}

void UkmSource::PopulateProto(Source* proto_source) const {
  DCHECK(!proto_source->has_id());
  DCHECK(!proto_source->has_type());

  proto_source->set_id(id_);
  proto_source->set_type(ToProtobufSourceType(type_));
  for (const auto& url : urls()) {
    if (!url.is_empty()) {
      UrlInfo* url_info = proto_source->add_urls();
      SetTruncatedURL(url, url_info);
    }
  }

  // -1 corresponds to the unset state. Android activity type values start at 0.
  // See chrome/browser/flags/ActivityType.java
  if (android_activity_type_state_ != -1)
    proto_source->set_android_activity_type(
        ToProtobufActivityType(android_activity_type_state_));

  if (navigation_data_.previous_source_id != kInvalidSourceId)
    proto_source->set_previous_source_id(navigation_data_.previous_source_id);

  if (navigation_data_.previous_same_document_source_id != kInvalidSourceId) {
    proto_source->set_previous_same_document_source_id(
        navigation_data_.previous_same_document_source_id);
  }

  if (navigation_data_.opener_source_id != kInvalidSourceId)
    proto_source->set_opener_source_id(navigation_data_.opener_source_id);

  // Tab ids will always be greater than 0. See CreateUniqueTabId in
  // source_url_recorder.cc
  if (navigation_data_.tab_id != 0)
    proto_source->set_tab_id(navigation_data_.tab_id);

  if (navigation_data_.is_same_document_navigation)
    proto_source->set_is_same_document_navigation(true);

  ukm::SameOriginStatus status = ukm::SAME_ORIGIN_STATUS_UNSET;
  if (navigation_data_.same_origin_status ==
      UkmSource::NavigationData::SourceSameOriginStatus::SOURCE_SAME_ORIGIN) {
    status = ukm::SAME_ORIGIN;
  } else if (navigation_data_.same_origin_status ==
             UkmSource::NavigationData::SourceSameOriginStatus::
                 SOURCE_CROSS_ORIGIN) {
    status = ukm::CROSS_ORIGIN;
  }

  proto_source->mutable_navigation_metadata()->set_same_origin_status(status);
  proto_source->mutable_navigation_metadata()->set_is_renderer_initiated(
      navigation_data_.is_renderer_initiated);
  proto_source->mutable_navigation_metadata()->set_is_error_page(
      navigation_data_.is_error_page);

  if (navigation_data_.navigation_time) {
    proto_source->set_navigation_time_msec(
        navigation_data_.navigation_time->since_origin().InMilliseconds());
  }

  for (const auto& url : navigation_data_.resolved_urls) {
    if (!url.is_empty()) {
      UrlInfo* url_info = proto_source->add_resolved_urls();
      SetTruncatedURL(url, url_info);
    }
  }
}

}  // namespace ukm
