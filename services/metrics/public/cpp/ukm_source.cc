// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/ukm_source.h"

#include <utility>

#include "base/atomicops.h"
#include "base/check_op.h"
#include "base/hash/hash.h"
#include "base/notreached.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/metrics_proto/ukm/source.pb.h"

namespace ukm {

namespace {

// The maximum length of a URL we will record.
constexpr int kMaxURLLength = 2 * 1024;

// The string sent in place of a URL if the real URL was too long.
constexpr char kMaxUrlLengthMessage[] = "URLTooLong";

// Using a simple global assumes that all access to it will be done on the same
// thread, namely the UI thread. If this becomes not the case then it can be
// changed to an Atomic32 (make AndroidActivityTypeState derive from int32_t)
// and accessed with no-barrier loads and stores.
int32_t g_android_activity_type_state = -1;

// Returns a URL that is under the length limit, by returning a constant
// string when the URL is too long.
std::string GetShortenedURL(const GURL& url) {
  if (url.spec().length() > kMaxURLLength)
    return kMaxUrlLengthMessage;
  return url.spec();
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
    default:
      NOTREACHED_IN_MIGRATION();
      return AndroidActivityType::TABBED;
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
  DCHECK_LE(sanitized_urls.size(), 2u);
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
  return sanitized_navigation_data;
}

UkmSource::UkmSource(ukm::SourceId id, const GURL& url)
    : id_(id),
      type_(GetSourceIdType(id_)),
      android_activity_type_state_(g_android_activity_type_state),
      creation_time_(base::TimeTicks::Now()) {
  navigation_data_.urls = {url};
  DCHECK(!url.is_empty());
}

UkmSource::UkmSource(ukm::SourceId id, const NavigationData& navigation_data)
    : id_(id),
      type_(GetSourceIdType(id_)),
      navigation_data_(navigation_data),
      android_activity_type_state_(g_android_activity_type_state),
      creation_time_(base::TimeTicks::Now()) {
  DCHECK(type_ == SourceIdType::NAVIGATION_ID);
  DCHECK(!navigation_data.urls.empty());
  DCHECK(!navigation_data.urls.back().is_empty());
}

UkmSource::~UkmSource() = default;

void UkmSource::UpdateUrl(const GURL& new_url) {
  DCHECK(!new_url.is_empty());
  DCHECK_EQ(1u, navigation_data_.urls.size());
  if (url() == new_url)
    return;
  navigation_data_.urls = {new_url};
}

void UkmSource::PopulateProto(Source* proto_source) const {
  DCHECK(!proto_source->has_id());
  DCHECK(!proto_source->has_type());

  proto_source->set_id(id_);
  proto_source->set_type(ToProtobufSourceType(type_));
  for (const auto& url : urls()) {
    proto_source->add_urls()->set_url(GetShortenedURL(url));
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
}

}  // namespace ukm
