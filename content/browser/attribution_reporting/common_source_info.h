// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMMON_SOURCE_INFO_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMMON_SOURCE_INFO_H_

#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/common/content_export.h"
#include "net/base/schemeful_site.h"

namespace content {

// Contains common attributes of `StorableSource` and `StoredSource`.
class CONTENT_EXPORT CommonSourceInfo {
 public:
  CommonSourceInfo(attribution_reporting::SuitableOrigin source_origin,
                   attribution_reporting::SuitableOrigin reporting_origin,
                   attribution_reporting::mojom::SourceType,
                   bool debug_cookie_set = false);

  ~CommonSourceInfo();

  CommonSourceInfo(const CommonSourceInfo&);
  CommonSourceInfo(CommonSourceInfo&&);

  CommonSourceInfo& operator=(const CommonSourceInfo&);
  CommonSourceInfo& operator=(CommonSourceInfo&&);

  const attribution_reporting::SuitableOrigin& source_origin() const {
    return source_origin_;
  }

  const attribution_reporting::SuitableOrigin& reporting_origin() const {
    return reporting_origin_;
  }

  attribution_reporting::mojom::SourceType source_type() const {
    return source_type_;
  }

  const net::SchemefulSite& source_site() const { return source_site_; }

  bool debug_cookie_set() const { return debug_cookie_set_; }

  void set_debug_cookie_set(bool value) { debug_cookie_set_ = value; }

  friend bool operator==(const CommonSourceInfo&,
                         const CommonSourceInfo&) = default;

 private:
  net::SchemefulSite source_site_;
  attribution_reporting::SuitableOrigin source_origin_;
  attribution_reporting::SuitableOrigin reporting_origin_;
  attribution_reporting::mojom::SourceType source_type_;
  bool debug_cookie_set_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMMON_SOURCE_INFO_H_
