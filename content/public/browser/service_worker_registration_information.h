// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_REGISTRATION_INFORMATION_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_REGISTRATION_INFORMATION_H_

#include <vector>

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

struct CONTENT_EXPORT ServiceWorkerRegistrationInformation {
  ServiceWorkerRegistrationInformation();
  ServiceWorkerRegistrationInformation(
      ServiceWorkerRegistrationInformation&&) noexcept;
  ServiceWorkerRegistrationInformation& operator=(
      ServiceWorkerRegistrationInformation&&) noexcept;
  ServiceWorkerRegistrationInformation(
      const ServiceWorkerRegistrationInformation&) = delete;
  ServiceWorkerRegistrationInformation& operator=(
      const ServiceWorkerRegistrationInformation&) = delete;
  ~ServiceWorkerRegistrationInformation();

  // URLs of scripts imported by the service worker via the importScripts
  // function and import keyword.
  std::vector<GURL> resources;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_REGISTRATION_INFORMATION_H_
