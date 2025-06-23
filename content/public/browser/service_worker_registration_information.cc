// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_worker_registration_information.h"

namespace content {

ServiceWorkerRegistrationInformation::ServiceWorkerRegistrationInformation() =
    default;

ServiceWorkerRegistrationInformation::ServiceWorkerRegistrationInformation(
    ServiceWorkerRegistrationInformation&&) noexcept = default;

ServiceWorkerRegistrationInformation&
ServiceWorkerRegistrationInformation::operator=(
    ServiceWorkerRegistrationInformation&&) noexcept = default;

ServiceWorkerRegistrationInformation::~ServiceWorkerRegistrationInformation() =
    default;

}  // namespace content
