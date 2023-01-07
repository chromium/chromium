// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_HOST_OBSERVER_H_
#define EXTENSIONS_BROWSER_EXTENSION_HOST_OBSERVER_H_

#include <stdint.h>

#include <string>

namespace extensions {
class ExtensionHost;

class ExtensionHostObserver {
 public:
  virtual ~ExtensionHostObserver() {}

  // TODO(kalman): Why do these all return const ExtensionHosts? It seems
  // perfectly reasonable for an Observer implementation to mutate any
  // ExtensionHost it's given.

  // Called when an ExtensionHost is destroyed.
  virtual void OnExtensionHostDestroyed(ExtensionHost* host) {}

  // Called when the ExtensionHost has finished the first load.
  virtual void OnExtensionHostDidStopFirstLoad(const ExtensionHost* host) {}

  // Called when a message has been disptached to the event page corresponding
  // to |host|.
  virtual void OnBackgroundEventDispatched(const ExtensionHost* host,
                                           const std::string& event_name,
                                           int event_id) {}

  // Called when a previously dispatched message has been acked by the
  // event page for |host|.
  virtual void OnBackgroundEventAcked(const ExtensionHost* host, int event_id) {
  }

  // Called when the extension associated with |host| starts a new network
  // request.
  virtual void OnNetworkRequestStarted(const ExtensionHost* host,
                                       uint64_t request_id) {}

  // Called when the network request with |request_id| is done.
  virtual void OnNetworkRequestDone(const ExtensionHost* host,
                                    uint64_t request_id) {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_HOST_OBSERVER_H_
