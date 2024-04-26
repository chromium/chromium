// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_BACKGROUND_SERVICE_MANAGER_H_
#define SERVICES_SERVICE_MANAGER_BACKGROUND_SERVICE_MANAGER_H_

#include <memory>
#include <vector>

#include "base/threading/thread.h"
#include "build/build_config.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace base {
class WaitableEvent;
}

namespace service_manager {

class Identity;
class ServiceManager;

// BackgroundServiceManager is a helper class that be can used to instantiate a
// ServiceManager instance on a dedicated background thread. This is only
// slightly more convenient than simply running your own background thread and
// instantiating ServiceManager there yourself.
//
// TODO(crbug.com/40601935): Consider deleting this class since it has
// such limited use and is trivial to replicate.
class BackgroundServiceManager {
 public:
  explicit BackgroundServiceManager(const std::vector<Manifest>& manifests);

  BackgroundServiceManager(const BackgroundServiceManager&) = delete;
  BackgroundServiceManager& operator=(const BackgroundServiceManager&) = delete;

  ~BackgroundServiceManager();

  // Creates a service instance for |identity|. This is intended for use by the
  // Service Manager's embedder to register instances directly, without
  // requiring a Connector.
  //
  // |metadata_receiver| may be null, in which case the Service Manager assumes
  // the new service is running in the calling process.
  void RegisterService(
      const Identity& identity,
      mojo::PendingRemote<mojom::Service> service,
      mojo::PendingReceiver<mojom::ProcessMetadata> metadata_receiver);

 private:
  void InitializeOnBackgroundThread(const std::vector<Manifest>& manifests);
  void ShutDownOnBackgroundThread(base::WaitableEvent* done_event);
  void RegisterServiceOnBackgroundThread(
      const Identity& identity,
      mojo::PendingRemote<mojom::Service> service,
      mojo::PendingReceiver<mojom::ProcessMetadata> metadata_receiver);

  base::Thread background_thread_;

  // Must only be used on the background thread.
  std::unique_ptr<ServiceManager> service_manager_;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_BACKGROUND_SERVICE_MANAGER_H_
