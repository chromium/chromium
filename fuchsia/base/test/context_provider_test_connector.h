// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_TEST_CONTEXT_PROVIDER_TEST_CONNECTOR_H_
#define FUCHSIA_BASE_TEST_CONTEXT_PROVIDER_TEST_CONNECTOR_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/sys/cpp/service_directory.h>

#include "base/command_line.h"

namespace cr_fuchsia {

// Starts a WebEngine and connects a ContextProvider instance for tests.
// WebEngine logs will be included in the test output but not in the Fuchsia
// system log.
fuchsia::web::ContextProviderPtr ConnectContextProvider(
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request,
    const base::CommandLine& command_line =
        base::CommandLine(base::CommandLine::NO_PROGRAM));

// Does the same thing as ConnectContextProvider() except WebEngine logs are not
// redirected and thus are included in the Fuchsia system log. WebEngine logs
// are not included in the test output. Only use with tests that are verifying
// WebEngine's logging behavior.
fuchsia::web::ContextProviderPtr ConnectContextProviderForLoggingTest(
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request,
    const base::CommandLine& command_line =
        base::CommandLine(base::CommandLine::NO_PROGRAM));
}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_TEST_CONTEXT_PROVIDER_TEST_CONNECTOR_H_
