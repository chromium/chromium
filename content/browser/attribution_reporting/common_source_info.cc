// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/common_source_info.h"

#include <utility>

#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "net/base/schemeful_site.h"

namespace content {

namespace {

using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::SourceType;

}  // namespace

CommonSourceInfo::CommonSourceInfo(SuitableOrigin source_origin,
                                   SuitableOrigin reporting_origin,
                                   SourceType source_type,
                                   bool debug_cookie_set)
    : source_site_(net::SchemefulSite(source_origin)),
      source_origin_(std::move(source_origin)),
      reporting_origin_(std::move(reporting_origin)),
      source_type_(source_type),
      debug_cookie_set_(debug_cookie_set) {}

CommonSourceInfo::~CommonSourceInfo() = default;

CommonSourceInfo::CommonSourceInfo(const CommonSourceInfo&) = default;

CommonSourceInfo::CommonSourceInfo(CommonSourceInfo&&) = default;

CommonSourceInfo& CommonSourceInfo::operator=(const CommonSourceInfo&) =
    default;

CommonSourceInfo& CommonSourceInfo::operator=(CommonSourceInfo&&) = default;

}  // namespace content
