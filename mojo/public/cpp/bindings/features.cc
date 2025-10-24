// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/features.h"

namespace mojo {
namespace features {

// Enables measurement of MessageChannel unread message counts. When enabled, a
// small random selection of Connectors enable the unread message count quota
// on their associated message pipe, and record the highest unread message count
// seen during the Connector's lifetime.
BASE_FEATURE(kMojoRecordUnreadMessageCount, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace mojo
