// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_security_utils.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(ServiceWorkerSecurityUtilsTest,
     AllOriginsMatchAndCanAccessServiceWorkers) {
  std::vector<GURL> https_same_origin = {GURL("https://example.com/1"),
                                         GURL("https://example.com/2"),
                                         GURL("https://example.com/3")};
  EXPECT_TRUE(
      service_worker_security_utils::AllOriginsMatchAndCanAccessServiceWorkers(
          https_same_origin));

  std::vector<GURL> http_same_origin = {GURL("http://example.com/1"),
                                        GURL("http://example.com/2")};
  EXPECT_FALSE(
      service_worker_security_utils::AllOriginsMatchAndCanAccessServiceWorkers(
          http_same_origin));

  std::vector<GURL> localhost_same_origin = {GURL("http://localhost/1"),
                                             GURL("http://localhost/2")};
  EXPECT_TRUE(
      service_worker_security_utils::AllOriginsMatchAndCanAccessServiceWorkers(
          localhost_same_origin));

  std::vector<GURL> filesystem_same_origin = {
      GURL("https://example.com/1"), GURL("https://example.com/2"),
      GURL("filesystem:https://example.com/3")};
  EXPECT_FALSE(
      service_worker_security_utils::AllOriginsMatchAndCanAccessServiceWorkers(
          filesystem_same_origin));

  std::vector<GURL> https_cross_origin = {GURL("https://example.com/1"),
                                          GURL("https://example.org/2"),
                                          GURL("https://example.com/3")};
  EXPECT_FALSE(
      service_worker_security_utils::AllOriginsMatchAndCanAccessServiceWorkers(
          https_cross_origin));

  // Cross-origin access is permitted with --disable-web-security.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kDisableWebSecurity);
  EXPECT_TRUE(
      service_worker_security_utils::AllOriginsMatchAndCanAccessServiceWorkers(
          https_cross_origin));

  // Disallowed schemes are not permitted even with --disable-web-security.
  EXPECT_FALSE(
      service_worker_security_utils::AllOriginsMatchAndCanAccessServiceWorkers(
          filesystem_same_origin));
}

}  // namespace content
