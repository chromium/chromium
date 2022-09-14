// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CDM_SERVICE_MOCK_PROVISION_FETCHER_H_
#define MEDIA_FUCHSIA_CDM_SERVICE_MOCK_PROVISION_FETCHER_H_

#include <memory>
#include <string>

#include "media/base/provision_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace testing {

// This is a mock for the Chromium media::ProvisionFetcher (and not Fuchsia's
// similarly named ProvisioningFetcher protocol).
class MockProvisionFetcher : public ProvisionFetcher {
 public:
  MockProvisionFetcher();
  ~MockProvisionFetcher() override;

  // Disallow copy and assign
  MockProvisionFetcher(const MockProvisionFetcher&) = delete;
  MockProvisionFetcher(MockProvisionFetcher&&) = delete;
  MockProvisionFetcher& operator=(const MockProvisionFetcher&) = delete;
  MockProvisionFetcher& operator=(MockProvisionFetcher&&) = delete;

  MOCK_METHOD(void,
              Retrieve,
              (const GURL& default_url,
               const std::string& request_data,
               ResponseCB response_cb),
              (override));
};

}  // namespace testing
}  // namespace media

#endif  // MEDIA_FUCHSIA_CDM_SERVICE_MOCK_PROVISION_FETCHER_H_
