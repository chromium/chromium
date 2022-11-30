// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/constants.h"

#include "services/service_manager/public/mojom/constants.mojom.h"

namespace service_manager {

const base::Token kSystemInstanceGroup{mojom::kSystemInstanceGroupHigh,
                                       mojom::kSystemInstanceGroupLow};

}  // namespace service_manager
