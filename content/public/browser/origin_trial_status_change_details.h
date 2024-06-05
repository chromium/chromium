// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ORIGIN_TRIAL_STATUS_CHANGE_DETAILS_H_
#define CONTENT_PUBLIC_BROWSER_ORIGIN_TRIAL_STATUS_CHANGE_DETAILS_H_

#include <string>

#include "content/common/content_export.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

namespace content {

struct CONTENT_EXPORT OriginTrialStatusChangeDetails {
  url::Origin origin;
  std::string partition_site;
  bool match_subdomains;
  bool enabled;
  std::optional<ukm::SourceId> source_id;

  OriginTrialStatusChangeDetails();
  OriginTrialStatusChangeDetails(const url::Origin& origin,
                                 const std::string& partition_site,
                                 bool match_subdomains,
                                 bool enabled,
                                 std::optional<ukm::SourceId> source_id);
  ~OriginTrialStatusChangeDetails();

  OriginTrialStatusChangeDetails(const OriginTrialStatusChangeDetails&);
  OriginTrialStatusChangeDetails& operator=(
      const OriginTrialStatusChangeDetails&);
  bool operator==(const OriginTrialStatusChangeDetails& other) const;

  std::ostream& operator<<(std::ostream& out) {
    out << "{";
    out << "origin: " << origin << ", ";
    out << "partition_site: " << partition_site << ", ";
    out << "match_subdomains:" << (match_subdomains ? "true" : "false") << ", ";
    out << "enabled: " << (enabled ? "true" : "false") << ", ";
    out << "source_id: "
        << (source_id.has_value() ? base::NumberToString(source_id.value())
                                  : "<empty>");
    out << "}";
    return out;
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ORIGIN_TRIAL_STATUS_CHANGE_DETAILS_H_
