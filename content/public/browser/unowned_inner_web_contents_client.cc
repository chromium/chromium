// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/unowned_inner_web_contents_client.h"

namespace content {

// static
UnownedInnerWebContentsClient::PassKey
UnownedInnerWebContentsClient::GetPassKeyForTesting() {
  return GetPassKey();
}

// static
UnownedInnerWebContentsClient::PassKey
UnownedInnerWebContentsClient::GetPassKey() {
  return PassKey();
}

}  // namespace content
