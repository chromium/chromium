// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_HEADLESS_CONTENT_UTILITY_CLIENT_H_
#define HEADLESS_LIB_HEADLESS_CONTENT_UTILITY_CLIENT_H_

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
  ~HeadlessContentUtilityClient() override;

  // content::ContentUtilityClient:
  mojo::ServiceFactory* GetMainThreadServiceFactory() override;
  void RegisterNetworkBinders(
      service_manager::BinderRegistry* registry) override;

 private:
  const std::string user_agent_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessContentUtilityClient);
};

}  // namespace headless

#endif  // HEADLESS_LIB_HEADLESS_CONTENT_UTILITY_CLIENT_H_
