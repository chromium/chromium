// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_UKM_SOURCE_H_
#define SERVICES_METRICS_PUBLIC_CPP_UKM_SOURCE_H_

#include <map>
#include <vector>

#include "base/macros.h"
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
  enum CustomTabState {
    kCustomTabUnset,
    kCustomTabTrue,
    kCustomTabFalse,
  };

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
    //   TODO(crbug.com/869123): This may end up containing all the URLs in the
    //   redirect chain for navigation sources.
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

    // The navigation start time relative to session start. The navigation
    // time within session should be monotonically increasing.
    base::Optional<base::TimeTicks> navigation_time;
  };

  UkmSource(SourceId id, const GURL& url);
  UkmSource(SourceId id, const NavigationData& data);
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

  // Sets the current "custom tab" state. This can be called from any thread.
  static void SetCustomTabVisible(bool visible);

 private:
  const ukm::SourceId id_;

  NavigationData navigation_data_;

  // A flag indicating if metric was collected in a custom tab. This is set
  // automatically when the object is created and so represents the state when
  // the metric was created.
  const CustomTabState custom_tab_state_;

  // When this object was created.
  const base::TimeTicks creation_time_;

  DISALLOW_COPY_AND_ASSIGN(UkmSource);
};

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_UKM_SOURCE_H_
