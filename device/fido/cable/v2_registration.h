// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_V2_REGISTRATION_H_
#define DEVICE_FIDO_CABLE_V2_REGISTRATION_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/fido_constants.h"

namespace instance_id {
class InstanceIDDriver;
}

namespace device {
namespace cablev2 {
namespace authenticator {

// Registration represents a subscription to events from the tunnel service.
class Registration {
 public:
  // Type enumerates the types of registrations that are maintained.
  enum class Type : uint8_t {
    // LINKING is for link information shared with desktops after scanning a QR
    // code. If the user chooses to unlink devices then this registration can be
    // rotated by calling |RotateContactID|. That will cause the server to
    // inform clients that the registration is no longer valid and that they
    // should forget about it.
    LINKING = 0,
    // SYNC is for information shared via the Sync service. This is separate so
    // that unlinking devices doesn't break sync peers.
    SYNC = 1,
  };

  // An Event contains the information sent by the tunnel service when a peer is
  // trying to connect.
  struct Event {
    Event();
    ~Event();
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    // Serialize returns a serialized form of the |Event|. This format is
    // not stable and is suitable only for transient storage.
    std::optional<std::vector<uint8_t>> Serialize();

    // FromSerialized parses the bytes produced by |Serialize|. It assumes that
    // the input is well formed. It returns |nullptr| on error.
    static std::unique_ptr<Event> FromSerialized(base::span<const uint8_t> in);

    Type source;
    FidoRequestType request_type;
    std::array<uint8_t, kTunnelIdSize> tunnel_id;
    std::array<uint8_t, kRoutingIdSize> routing_id;
    std::array<uint8_t, kPairingIDSize> pairing_id;
    std::array<uint8_t, kClientNonceSize> client_nonce;
    std::optional<std::vector<uint8_t>> contact_id;
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
  virtual std::optional<std::vector<uint8_t>> contact_id() const = 0;
};

// Register subscribes to the tunnel service and returns a |Registration|. This
// should only be called once in an address space. Subsequent calls may CHECK.
// The |event_callback| is called, on the same thread, whenever a paired device
// requests a tunnel.
std::unique_ptr<Registration> Register(
    instance_id::InstanceIDDriver* instance_id_driver,
    Registration::Type type,
    base::OnceCallback<void()> on_ready,
    base::RepeatingCallback<void(std::unique_ptr<Registration::Event>)>
        event_callback);

}  // namespace authenticator
}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_V2_REGISTRATION_H_
