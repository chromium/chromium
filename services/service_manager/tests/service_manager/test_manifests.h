// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_TESTS_SERVICE_MANAGER_TEST_MANIFESTS_H_
#define SERVICES_SERVICE_MANAGER_TESTS_SERVICE_MANAGER_TEST_MANIFESTS_H_

#include <vector>

#include "services/service_manager/public/cpp/manifest.h"

namespace service_manager {

extern const char kTestServiceName[];
extern const char kTestTargetName[];
extern const char kTestEmbedderName[];
extern const char kTestRegularServiceName[];
extern const char kTestSharedServiceName[];
extern const char kTestSingletonServiceName[];

const std::vector<Manifest>& GetTestManifests();

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_TESTS_SERVICE_MANAGER_TEST_MANIFESTS_H_
