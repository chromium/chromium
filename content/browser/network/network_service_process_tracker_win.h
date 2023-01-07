// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_NETWORK_SERVICE_PROCESS_TRACKER_WIN_H_
#define CONTENT_BROWSER_NETWORK_NETWORK_SERVICE_PROCESS_TRACKER_WIN_H_

#include "base/process/process.h"
#include "content/common/content_export.h"

namespace content {

// Obtains a base::Process representing the current running network service.
// This might return an invalid base::Process if the network service does not
// exist. Must be called on the UI thread.
CONTENT_EXPORT base::Process GetNetworkServiceProcess();

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_NETWORK_SERVICE_PROCESS_TRACKER_WIN_H_
