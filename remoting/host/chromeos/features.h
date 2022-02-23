// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_FEATURES_H_
#define REMOTING_HOST_CHROMEOS_FEATURES_H_

#include "base/feature_list.h"

namespace remoting {

// Enable to allow CRD to stream other monitors than the primary display.
extern const base::Feature kEnableMultiMonitorsInCrd;

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_FEATURES_H_
