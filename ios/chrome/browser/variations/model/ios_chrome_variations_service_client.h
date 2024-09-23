// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SERVICE_CLIENT_H_
#define IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SERVICE_CLIENT_H_

#import "base/memory/scoped_refptr.h"
#import "components/variations/service/variations_service_client.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class PrefService;

namespace variations {
struct SeedResponse;
}  // namespace variations

// IOSChromeVariationsServiceClient provides an implementation of
// VariationsServiceClient that depends on ios/chrome/.
class IOSChromeVariationsServiceClient
    : public variations::VariationsServiceClient {
 public:
  IOSChromeVariationsServiceClient();

  IOSChromeVariationsServiceClient(const IOSChromeVariationsServiceClient&) =
      delete;
  IOSChromeVariationsServiceClient& operator=(
      const IOSChromeVariationsServiceClient&) = delete;

  ~IOSChromeVariationsServiceClient() override;

 private:
  // variations::VariationsServiceClient:
  base::Version GetVersionForSimulation() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  version_info::Channel GetChannel() override;
  bool OverridesRestrictParameter(std::string* parameter) override;
  bool IsEnterprise() override;
  void RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      PrefService* local_state) override;
  std::unique_ptr<variations::SeedResponse>
  TakeSeedFromNativeVariationsSeedStore() override;
};

#endif  // IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SERVICE_CLIENT_H_
