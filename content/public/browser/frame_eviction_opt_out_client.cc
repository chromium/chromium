// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/frame_eviction_opt_out_client.h"

namespace content {

// static
FrameEvictionOptOutClient::PassKey
FrameEvictionOptOutClient::GetPassKeyForTesting() {
  return GetPassKey();
}

// static
FrameEvictionOptOutClient::PassKey FrameEvictionOptOutClient::GetPassKey() {
  return PassKey();
}

}  // namespace content
