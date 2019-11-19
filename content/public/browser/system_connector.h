// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SYSTEM_CONNECTOR_H_
#define CONTENT_PUBLIC_BROWSER_SYSTEM_CONNECTOR_H_

#include "content/common/content_export.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

// Returns a Connector which can be used to connect to service interfaces from
// the browser process. May return null in unit testing environments.
// The system Connector can be overridden for tests using the
// |SetSystemConnectorForTesting()| below.
//
// This function is safe to call from any thread, but the returned pointer is
// different on each thread and is NEVER safe to retain or pass across threads.
CONTENT_EXPORT service_manager::Connector* GetSystemConnector();

// Overrides the system Connector for test environments where Content's regular
// Service Manager environment is not set up. Must be called from the main
// thread.
CONTENT_EXPORT void SetSystemConnectorForTesting(
    std::unique_ptr<service_manager::Connector> connector);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SYSTEM_CONNECTOR_H_
