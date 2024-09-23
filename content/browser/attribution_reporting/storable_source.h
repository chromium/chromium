// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORABLE_SOURCE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORABLE_SOURCE_H_

#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/store_source_result.mojom.h"
#include "content/common/content_export.h"

namespace attribution_reporting {
class SuitableOrigin;
}  // namespace attribution_reporting

namespace content {

// Contains attributes specific to a source that hasn't been stored yet.
class CONTENT_EXPORT StorableSource {
 public:
  using Result = ::attribution_reporting::mojom::StoreSourceResult;

  StorableSource(attribution_reporting::SuitableOrigin reporting_origin,
                 attribution_reporting::SourceRegistration,
                 attribution_reporting::SuitableOrigin source_origin,
                 attribution_reporting::mojom::SourceType,
                 bool is_within_fenced_frame);

  ~StorableSource();

  StorableSource(const StorableSource&);
  StorableSource(StorableSource&&);

  StorableSource& operator=(const StorableSource&);
  StorableSource& operator=(StorableSource&&);

  const attribution_reporting::SourceRegistration& registration() const {
    return registration_;
  }

  attribution_reporting::SourceRegistration& registration() {
    return registration_;
  }

  const CommonSourceInfo& common_info() const { return common_info_; }

  bool is_within_fenced_frame() const { return is_within_fenced_frame_; }

  void set_debug_cookie_set(bool value) {
    common_info_.set_debug_cookie_set(value);
  }

  friend bool operator==(const StorableSource&,
                         const StorableSource&) = default;

 private:
  attribution_reporting::SourceRegistration registration_;

  CommonSourceInfo common_info_;

  // Whether the source is registered within a fenced frame tree.
  bool is_within_fenced_frame_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORABLE_SOURCE_H_
