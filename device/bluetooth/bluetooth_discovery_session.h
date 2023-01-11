// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_SESSION_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_SESSION_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

// BluetoothDiscoverySession represents a current active or inactive device
// discovery session. Instances of this class are obtained by calling
// BluetoothAdapter::StartDiscoverySession. The Bluetooth adapter will be
// constantly searching for nearby devices, as long as at least one instance
// of an active BluetoothDiscoverySession exists. A BluetoothDiscoverySession is
// considered active, as long as the adapter is discovering AND the owner of the
// instance has not called BluetoothDiscoverySession::Stop. A
// BluetoothDiscoverySession might unexpectedly become inactive, if the adapter
// unexpectedly stops discovery. Users can implement the
// AdapterDiscoveringChanged method of the BluetoothAdapter::Observer interface
// to be notified of such a change and promptly request a new
// BluetoothDiscoverySession if their existing sessions have become inactive.
class DEVICE_BLUETOOTH_EXPORT BluetoothDiscoverySession {
 public:
  // The ErrorCallback is used by methods to asynchronously report errors.
  using ErrorCallback = base::OnceClosure;

  enum SessionStatus {
    // Just added to the adapter.
    PENDING_START,
    // Request sent to OS.
    STARTING,
    // Actively scanning.
    SCANNING,
    // Finished scanning, should be deleted soon.
    INACTIVE
  };

  BluetoothDiscoverySession(const BluetoothDiscoverySession&) = delete;
  BluetoothDiscoverySession& operator=(const BluetoothDiscoverySession&) =
      delete;

  // Terminates the discovery session. If this is the last active discovery
  // session, a call to the underlying system to stop device discovery is made.
  // Users may call BluetoothDiscoverySession::Stop() if they need to observe
  // the result of that operation, but this is usually unnecessary.
  virtual ~BluetoothDiscoverySession();

  // Returns true if the session is active, false otherwise. If false, the
  // adapter might still be discovering as there might still be other active
  // sessions; this just means that this instance no longer has a say in
  // whether or not discovery should continue. In this case, the application
  // should request a new BluetoothDiscoverySession to make sure that device
  // discovery continues.
  virtual bool IsActive() const;

  // Requests this discovery session instance to stop. If this is the last
  // active discovery session, a call to the underlying system to stop device
  // discovery is made, and |error_callback| will be invoked if such a call
  // fails. Typically, users can ignore this and simply destroy the instance
  // instead of calling Stop().
  virtual void Stop(base::OnceClosure callback = base::DoNothing(),
                    ErrorCallback error_callback = base::DoNothing());

  virtual const BluetoothDiscoveryFilter* GetDiscoveryFilter() const;

  SessionStatus status() const { return status_; }

  // Updates the status from PENDING_START to STARTING.
  void PendingSessionsStarting();

  // Updates the status from STARTING to SCANNING.
  void StartingSessionsScanning();

  base::WeakPtr<BluetoothDiscoverySession> GetWeakPtr();

 protected:
  explicit BluetoothDiscoverySession(
      scoped_refptr<BluetoothAdapter> adapter,
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter);

 private:
  friend class BluetoothAdapter;

  // Internal callback invoked when a call to
  // BluetoothAdapter::RemoveDiscoverySession has succeeded. Invokes
  // |deactivate_discovery_session| if the session object still
  // exists when this callback executes. Always invokes |success_callback|.
  static void OnDiscoverySessionRemoved(
      base::WeakPtr<BluetoothDiscoverySession> session,
      base::OnceClosure deactivate_discovery_session,
      base::OnceClosure success_callback);

  static void OnDiscoverySessionRemovalFailed(
      base::WeakPtr<BluetoothDiscoverySession> session,
      base::OnceClosure error_callback,
      UMABluetoothDiscoverySessionOutcome outcome);

  // Deactivate discovery session object after
  // BluetoothAdapter::RemoveDiscoverySession completes.
  void DeactivateDiscoverySession();

  // Marks this instance as inactive. Called by BluetoothAdapter to mark a
  // session as inactive in the case of an unexpected change to the adapter
  // discovery state.
  void MarkAsInactive();

  // Indicates the state of this session.
  SessionStatus status_;

  // Whether a Stop() operation is in progress for this session.
  bool is_stop_in_progress_;

  // The adapter that created this instance.
  scoped_refptr<BluetoothAdapter> adapter_;

  // Filter assigned to this session, if any
  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothDiscoverySession> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_SESSION_H_
