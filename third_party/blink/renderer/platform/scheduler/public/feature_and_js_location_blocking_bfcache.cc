// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/feature_and_js_location_blocking_bfcache.h"

namespace blink {

FeatureAndJSLocationBlockingBFCache::FeatureAndJSLocationBlockingBFCache(
    SchedulingPolicy::Feature feature,
    const String& url,
    const String& function,
    unsigned line_number,
    unsigned column_number)
    : feature_(feature),
      url_(url),
      function_(function),
      line_number_(line_number),
      column_number_(column_number) {}

FeatureAndJSLocationBlockingBFCache::FeatureAndJSLocationBlockingBFCache(
    SchedulingPolicy::Feature feature,
    const SourceLocation* source_location)
    : feature_(feature) {
  if (source_location) {
    url_ = source_location->Url();
    function_ = source_location->Function();
    line_number_ = source_location->LineNumber();
    column_number_ = source_location->ColumnNumber();
  } else {
    url_ = g_empty_string;
    function_ = g_empty_string;
    line_number_ = 0;
    column_number_ = 0;
  }
}

FeatureAndJSLocationBlockingBFCache::~FeatureAndJSLocationBlockingBFCache() =
    default;

bool FeatureAndJSLocationBlockingBFCache::operator==(
    const FeatureAndJSLocationBlockingBFCache& other) const {
  return (feature_ == other.feature_ && url_ == other.url_ &&
          function_ == other.function_ && line_number_ == other.line_number_ &&
          column_number_ == other.column_number_);
}

}  // namespace blink
