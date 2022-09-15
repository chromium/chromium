// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/bind_source_info.h"

namespace service_manager {

BindSourceInfo::BindSourceInfo() = default;
BindSourceInfo::BindSourceInfo(const Identity& identity,
                               const CapabilitySet& required_capabilities)
    : identity(identity), required_capabilities(required_capabilities) {}
BindSourceInfo::BindSourceInfo(const BindSourceInfo& other) = default;
BindSourceInfo::~BindSourceInfo() {}

}  // namespace service_manager
