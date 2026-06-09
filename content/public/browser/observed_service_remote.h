// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_OBSERVED_SERVICE_REMOTE_H_
#define CONTENT_PUBLIC_BROWSER_OBSERVED_SERVICE_REMOTE_H_

#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_observer_hub.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// Couples a mojo::Remote with a ServiceProcessObserverHub, tying the
// observer lifecycle to the service remote. When the remote disconnects
// (process crash or normal termination), registered observers are
// notified via the hub.
//
// Usage:
//   ObservedServiceRemote<audio::mojom::AudioService> service_;
//   service_.AddObserver(listener);
//   ServiceProcessHost::Launch(service_,
//       ServiceProcessHost::Options()
//           .WithDisplayName("Audio Service")
//           .Pass());
template <typename ServiceInterface>
class ObservedServiceRemote {
 public:
  using InterfaceType = ServiceInterface;
  using Observer =
      typename ServiceProcessObserverHub<ServiceInterface>::Observer;

  ObservedServiceRemote() = default;
  ~ObservedServiceRemote() = default;

  ObservedServiceRemote(const ObservedServiceRemote&) = delete;
  ObservedServiceRemote& operator=(const ObservedServiceRemote&) = delete;

  mojo::Remote<ServiceInterface>& remote() { return remote_; }
  const mojo::Remote<ServiceInterface>& remote() const { return remote_; }

  // For cases where the receiver is managed externally (e.g. the EDID
  // async path in audio service).
  base::WeakPtr<ServiceProcessHost::Observer> AsWeakObserver() {
    return hub_.AsWeakPtr();
  }

  void AddObserver(Observer* observer) { hub_.AddObserver(observer); }
  void RemoveObserver(Observer* observer) { hub_.RemoveObserver(observer); }

 private:
  mojo::Remote<ServiceInterface> remote_;
  ServiceProcessObserverHub<ServiceInterface> hub_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_OBSERVED_SERVICE_REMOTE_H_
