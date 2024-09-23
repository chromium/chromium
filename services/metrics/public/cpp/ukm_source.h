// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_UKM_SOURCE_H_
#define SERVICES_METRICS_PUBLIC_CPP_UKM_SOURCE_H_

#include <map>
#include <optional>
#include <vector>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "services/metrics/public/cpp/metrics_export.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace ukm {

class Source;

// Contains UKM URL data for a single source id.
class METRICS_EXPORT UkmSource {
 public:
  // Extra navigation data associated with a particular Source. Currently, all
  // of these members except |url| are only set for navigation id sources.
  //
  // Note: If adding more members to this class, make sure you update
  // CopyWithSanitizedUrls.
  struct METRICS_EXPORT NavigationData {
    NavigationData();
    ~NavigationData();

    NavigationData(const NavigationData& other);

    // Creates a copy of this struct, replacing the URL members with sanitized
    // versions. Currently, |sanitized_urls| expects a one or two element
    // vector. The last element in the vector will always be the final URL in
    // the redirect chain. For two-element vectors, the first URL is assumed to
    // be the first URL in the redirect chain. The URLs in |sanitized_urls| are
    // expected to be non-empty.
    NavigationData CopyWithSanitizedUrls(
        std::vector<GURL> sanitized_urls) const;

    // The URLs associated with this sources navigation. Some notes:
    // - This will always contain at least one element.
    // - For non navigation sources, this will contain exactly one element.
    // - For navigation sources, this will only contain at most two elements,
    //   one for the first URL in the redirect chain and one for the final URL
    //   that committed.
    // TODO(crbug.com/40587196): This may end up containing all the URLs in the
    // redirect chain for navigation sources.
    std::vector<GURL> urls;

    // The previous source id for this tab.
    SourceId previous_source_id = kInvalidSourceId;

    // The source id for the previous same document navigation, if the
    // previously committed source was a same document navigation. If
    // the previously committed source was not a same document
    // navigation, this field will be set to kInvalidSourceId.
    SourceId previous_same_document_source_id = kInvalidSourceId;

    // The source id for the source which opened this tab. This should be set to
    // kInvalidSourceId for all but the first navigation in the tab.
    SourceId opener_source_id = kInvalidSourceId;

    // A unique identifier for the tab the source navigated in. Tab ids should
    // be increasing over time within a session.
    int64_t tab_id = 0;

    // Whether this source is for a same document navigation. Examples of same
    // document navigations are fragment navigations, pushState/replaceState,
    // and same page history navigation.
    bool is_same_document_navigation = false;

    // Represents the same origin status of the navigation compared to the
    // previous document.
    enum SourceSameOriginStatus {
      SOURCE_SAME_ORIGIN_STATUS_UNSET = 0,
      SOURCE_SAME_ORIGIN,
      SOURCE_CROSS_ORIGIN,
    };

    // Whether this is the same origin as the previous document.
    //
    // This is set to the NavigationHandle's same origin state when the
    // navigation is committed, is not a same document navigation and is not
    // committed as an error page. Otherwise, this remains unset.
    SourceSameOriginStatus same_origin_status =
        SourceSameOriginStatus::SOURCE_SAME_ORIGIN_STATUS_UNSET;

    // Whether this navigation is initiated by the renderer.
    bool is_renderer_initiated = false;

    // Whether the navigation committed an error page.
    bool is_error_page = false;

    // The navigation start time relative to session start. The navigation
    // time within session should be monotonically increasing.
    std::optional<base::TimeTicks> navigation_time;
  };

  UkmSource(SourceId id, const GURL& url);
  UkmSource(SourceId id, const NavigationData& data);

  UkmSource(const UkmSource&) = delete;
  UkmSource& operator=(const UkmSource&) = delete;

  ~UkmSource();

  ukm::SourceId id() const { return id_; }

  const GURL& url() const { return navigation_data_.urls.back(); }

  const std::vector<GURL>& urls() const { return navigation_data_.urls; }

  const NavigationData& navigation_data() const { return navigation_data_; }

  // The object creation time. This is for internal purposes only and is not
  // intended to be anything useful for UKM clients.
  const base::TimeTicks creation_time() const { return creation_time_; }

  // Records a new URL for this source.
  void UpdateUrl(const GURL& url);

  // Serializes the members of the class into the supplied proto.
  void PopulateProto(Source* proto_source) const;

  // Sets the current "android_activity_type" state.
  static void SetAndroidActivityTypeState(int32_t android_activity_type);

 private:
  const ukm::SourceId id_;
  const ukm::SourceIdType type_;

  NavigationData navigation_data_;

  // The type of the visible activity when the metric was collected. This is
  // set automatically when the object is created and so represents the state
  // when the metric was created.
  const int32_t android_activity_type_state_ = -1;

  // When this object was created.
  const base::TimeTicks creation_time_;
};

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_UKM_SOURCE_H_
