// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/storable_source.h"

#include <utility>

namespace content {

StorableSource::StorableSource(CommonSourceInfo common_info)
    : common_info_(std::move(common_info)) {}

StorableSource::~StorableSource() = default;

StorableSource::StorableSource(const StorableSource&) = default;

StorableSource::StorableSource(StorableSource&&) = default;

StorableSource& StorableSource::operator=(const StorableSource&) = default;

StorableSource& StorableSource::operator=(StorableSource&&) = default;

}  // namespace content
