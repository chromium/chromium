// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SYSTEM_CONNECTOR_IMPL_H_
#define CONTENT_BROWSER_SYSTEM_CONNECTOR_IMPL_H_

#include "content/public/browser/system_connector.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

// Sets the system Connector on the main thread. Called very early in browser
// process startup (i.e. before BrowserMainLoop is instantiated). For unit test
// environments, see |SetSystemConnectorForTesting()| in the public header.
void SetSystemConnector(std::unique_ptr<service_manager::Connector> connector);

}  // namespace content

#endif  // CONTENT_BROWSER_SYSTEM_CONNECTOR_IMPL_H_
