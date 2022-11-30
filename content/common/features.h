// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FEATURES_H_
#define CONTENT_COMMON_FEATURES_H_

#include "base/compiler_specific.h"
#include "base/feature_list.h"

namespace content {

// Please keep features in alphabetical order.

// When enabled, queues navigations instead of cancelling the previous
// navigation if the previous navigation is already waiting for commit.
// See https://crbug.com/838348 and https://crbug.com/1220337.
BASE_DECLARE_FEATURE(kQueueNavigationsWhileWaitingForCommit);

// Please keep features in alphabetical order.

}  // namespace content

#endif  // CONTENT_COMMON_FEATURES_H_
