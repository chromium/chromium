// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_NETWORK_OBSERVER_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_NETWORK_OBSERVER_H_

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/background_sync/background_sync.pb.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace content {

class CONTENT_EXPORT BackgroundSyncNetworkObserver
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // Creates a BackgroundSyncNetworkObserver. |network_changed_callback| is
  // called when the network connection changes asynchronously via PostMessage.
  BackgroundSyncNetworkObserver(
      base::RepeatingClosure network_changed_callback);

  ~BackgroundSyncNetworkObserver() override;

  // Does nothing in this class, but can be overridden to do some work
  // separately from the constructor.
  virtual void Init() {}

  // Enable or disable notifications coming from the NetworkConnectionTracker.
  // (For preventing flakes in tests)
  static void SetIgnoreNetworkChangesForTests(bool ignore);

  // Returns true if the network is online.
  bool NetworkSufficient();

  // NetworkConnectionObserver overrides
  void OnConnectionChanged(
      network::mojom::ConnectionType connection_type) override;

  // Allow tests to call NotifyManagerIfConnectionChanged.
  void NotifyManagerIfConnectionChangedForTesting(
      network::mojom::ConnectionType connection_type);

 private:
  // Finishes setup once we get the NetworkConnectionTracker from the UI thread.
  virtual void RegisterWithNetworkConnectionTracker(
      network::NetworkConnectionTracker* network_connection_tracker);

  // Update the current connection type from the NetworkConnectionTracker.
  void UpdateConnectionType();

  // Calls NotifyConnectionChanged if the connection type has changed.
  void NotifyManagerIfConnectionChanged(
      network::mojom::ConnectionType connection_type);

  void NotifyConnectionChanged();

  // NetworkConnectionTracker is a global singleton which will outlive this
  // object.
  network::NetworkConnectionTracker* network_connection_tracker_;

  network::mojom::ConnectionType connection_type_;

  // The callback to run when the connection changes.
  base::RepeatingClosure connection_changed_callback_;

  // Set true to ignore notifications coming from the NetworkConnectionTracker
  // (to prevent flakes in tests).
  static bool ignore_network_changes_;

  base::WeakPtrFactory<BackgroundSyncNetworkObserver> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncNetworkObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_NETWORK_OBSERVER_H_
