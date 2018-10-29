// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_UTILITY_CONTENT_UTILITY_CLIENT_H_
#define CONTENT_PUBLIC_UTILITY_CONTENT_UTILITY_CLIENT_H_

#include <map>
#include <memory>

#include "base/callback_forward.h"
#include "content/public/common/content_client.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/embedded_service_info.h"

namespace content {

// Embedder API for participating in renderer logic.
class CONTENT_EXPORT ContentUtilityClient {
 public:
  using StaticServiceMap =
      std::map<std::string, service_manager::EmbeddedServiceInfo>;

  virtual ~ContentUtilityClient() {}

  // Notifies us that the UtilityThread has been created.
  virtual void UtilityThreadStarted() {}

  // Allows the embedder to filter messages.
  virtual bool OnMessageReceived(const IPC::Message& message);

  virtual void RegisterServices(StaticServiceMap* services) {}

  virtual void RegisterNetworkBinders(
      service_manager::BinderRegistry* registry) {}

  virtual void RegisterAudioBinders(service_manager::BinderRegistry* registry) {
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_UTILITY_CONTENT_UTILITY_CLIENT_H_
