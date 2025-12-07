// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/preconnect_test_util.h"

#include "content/public/browser/preconnect_request.h"

namespace content {

bool operator==(const PreconnectRequest& lhs, const PreconnectRequest& rhs) {
  return lhs.origin == rhs.origin && lhs.num_sockets == rhs.num_sockets &&
         lhs.allow_credentials == rhs.allow_credentials &&
         lhs.network_anonymization_key == rhs.network_anonymization_key;
}

}  // namespace content
