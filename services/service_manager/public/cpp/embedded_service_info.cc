// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/embedded_service_info.h"

#include "base/callback.h"
#include "services/service_manager/public/cpp/service.h"

namespace service_manager {

EmbeddedServiceInfo::EmbeddedServiceInfo() {}
EmbeddedServiceInfo::EmbeddedServiceInfo(const EmbeddedServiceInfo& other) =
    default;
EmbeddedServiceInfo::~EmbeddedServiceInfo() {}

}  // namespace service_manager
