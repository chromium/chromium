// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_V2_REGISTRATION_H_
#define DEVICE_FIDO_CABLE_V2_REGISTRATION_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "device/fido/cable/v2_constants.h"

namespace instance_id {
class InstanceIDDriver;
}

namespace device {
namespace cablev2 {
namespace authenticator {

// Registration represents a subscription to events from the tunnel service.
class Registration {
 public:
  // An Event contains the information sent by the tunnel service when a peer is
  // trying to connect.
  struct Event {
    Event();
    ~Event();
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    std::array<uint8_t, kTunnelIdSize> tunnel_id;
    std::array<uint8_t, kRoutingIdSize> routing_id;
    std::vector<uint8_t> pairing_id;
    std::array<uint8_t, kClientNonceSize> client_nonce;
  };

  virtual ~Registration();

  // PrepareContactID indicates that |contact_id| will soon be called. In order
  // to save resources for the case when |contact_id| is never used,
  // registration will be deferred until this is called.
  virtual void PrepareContactID() = 0;

  // RotateContactID invalidates the current contact ID and prepares a fresh
  // one.
  virtual void RotateContactID() = 0;

  // contact_id returns an opaque token that may be placed in pairing data for
  // desktops to later connect to. |nullopt| will be returned if the value is
  // not yet ready.
  virtual base::Optional<std::vector<uint8_t>> contact_id() const = 0;
};

// Register subscribes to the tunnel service and returns a |Registration|. This
// should only be called once in an address space. Subsequent calls may CHECK.
// The |event_callback| is called, on the same thread, whenever a paired device
// requests a tunnel.
std::unique_ptr<Registration> Register(
    instance_id::InstanceIDDriver* instance_id_driver,
    base::RepeatingCallback<void(std::unique_ptr<Registration::Event>)>
        event_callback);

}  // namespace authenticator
}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_V2_REGISTRATION_H_
