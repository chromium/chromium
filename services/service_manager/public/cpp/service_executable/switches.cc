// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_executable/switches.h"

namespace service_manager {
namespace switches {

// Indicates the name of the service to run. Useful for debugging, or if a
// service executable is built to support being run as a number of potential
// different services.
const char kServiceName[] = "service-name";

// The name of the |mojo::PendingReceiver<service_manager::mojom::Service>|
// message pipe handle that is attached to the incoming Mojo invitation received
// by the service.
const char kServiceRequestAttachmentName[] = "service-request-attachment-name";

}  // namespace switches
}  // namespace service_manager
