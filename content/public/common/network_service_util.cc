// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/network_service_util.h"

#include "base/command_line.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/cpp/features.h"

namespace content {

bool IsOutOfProcessNetworkService() {
  return base::FeatureList::IsEnabled(network::features::kNetworkService) &&
         !base::FeatureList::IsEnabled(features::kNetworkServiceInProcess) &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kSingleProcess);
}

}  // namespace content
