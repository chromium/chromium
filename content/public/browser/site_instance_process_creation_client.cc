// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/site_instance_process_creation_client.h"

namespace content {

// static
SiteInstanceProcessCreationClient::PassKey
SiteInstanceProcessCreationClient::GetPassKey() {
  return PassKey();
}

}  // namespace content
