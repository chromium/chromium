// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_UTILITY_HEADLESS_CONTENT_UTILITY_CLIENT_H_
#define HEADLESS_LIB_UTILITY_HEADLESS_CONTENT_UTILITY_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "content/public/utility/content_utility_client.h"
#include "headless/public/headless_export.h"

namespace headless {

class HEADLESS_EXPORT HeadlessContentUtilityClient
    : public content::ContentUtilityClient {
 public:
  using NetworkBinderCreationCallback =
      base::RepeatingCallback<void(service_manager::BinderRegistry*)>;

  static void SetNetworkBinderCreationCallbackForTests(
      NetworkBinderCreationCallback callback);

  explicit HeadlessContentUtilityClient(const std::string& user_agent);

  HeadlessContentUtilityClient(const HeadlessContentUtilityClient&) = delete;
  HeadlessContentUtilityClient& operator=(const HeadlessContentUtilityClient&) =
      delete;

  ~HeadlessContentUtilityClient() override;

  // content::ContentUtilityClient:
  void RegisterMainThreadServices(mojo::ServiceFactory& services) override;
  void RegisterNetworkBinders(
      service_manager::BinderRegistry* registry) override;

 private:
  const std::string user_agent_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_UTILITY_HEADLESS_CONTENT_UTILITY_CLIENT_H_
