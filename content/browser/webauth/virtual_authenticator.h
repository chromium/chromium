// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_H_
#define CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "device/fido/fido_constants.h"
#include "device/fido/virtual_fido_device.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom.h"

namespace content {

// Implements the Mojo interface representing a stateful virtual authenticator.
//
// This class has very little logic itself, it merely stores a unique ID and the
// state of the authenticator, whereas performing all cryptographic operations
// is delegated to the VirtualFidoDevice class.
class CONTENT_EXPORT VirtualAuthenticator
    : public blink::test::mojom::VirtualAuthenticator {
 public:
  VirtualAuthenticator(device::ProtocolVersion protocol,
                       device::Ctap2Version ctap2_version,
                       device::FidoTransportProtocol transport,
                       device::AuthenticatorAttachment attachment,
                       bool has_resident_key,
                       bool has_user_verification);
  ~VirtualAuthenticator() override;

  void AddReceiver(
      mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticator> receiver);

  const device::VirtualFidoDevice::State::RegistrationsMap& registrations()
      const {
    return state_->registrations;
  }

  // Register a new credential. Returns true if the registration was successful,
  // false otherwise.
  bool AddRegistration(std::vector<uint8_t> key_handle,
                       const std::string& rp_id,
                       const std::vector<uint8_t>& private_key,
                       int32_t counter);

  // Register a new resident credential. Returns true if the registration was
  // successful, false otherwise.
  bool AddResidentRegistration(std::vector<uint8_t> key_handle,
                               std::string rp_id,
                               const std::vector<uint8_t>& private_key,
                               int32_t counter,
                               std::vector<uint8_t> user_handle);

  // Removes all the credentials.
  void ClearRegistrations();

  // Remove a credential identified by |key_handle|. Returns true if the
  // credential was found and removed, false otherwise.
  bool RemoveRegistration(const std::vector<uint8_t>& key_handle);

  // Sets whether tests of user presence succeed or not for new requests sent to
  // this authenticator. The default is true.
  void SetUserPresence(bool is_user_present);

  // Sets whether user verification should succeed or not for new requests sent
  // to this authenticator. Defaults to true.
  void set_user_verified(bool is_user_verified) {
    is_user_verified_ = is_user_verified;
  }

  bool has_resident_key() const { return has_resident_key_; }

  device::FidoTransportProtocol transport() const { return state_->transport; }
  const std::string& unique_id() const { return unique_id_; }

  bool is_user_verifying_platform_authenticator() const {
    return attachment_ == device::AuthenticatorAttachment::kPlatform &&
           has_user_verification_;
  }

  // Constructs a VirtualFidoDevice instance that will perform cryptographic
  // operations on behalf of, and using the state stored in this virtual
  // authenticator.
  //
  // There is an N:1 relationship between VirtualFidoDevices and this class, so
  // this method can be called any number of times.
  std::unique_ptr<device::FidoDevice> ConstructDevice();

 protected:
  // blink::test::mojom::VirtualAuthenticator:
  void GetUniqueId(GetUniqueIdCallback callback) override;

  void GetRegistrations(GetRegistrationsCallback callback) override;
  void AddRegistration(blink::test::mojom::RegisteredKeyPtr registration,
                       AddRegistrationCallback callback) override;
  void ClearRegistrations(ClearRegistrationsCallback callback) override;
  void RemoveRegistration(const std::vector<uint8_t>& key_handle,
                          RemoveRegistrationCallback callback) override;

  void SetUserVerified(bool verified,
                       SetUserVerifiedCallback callback) override;

 private:
  const device::ProtocolVersion protocol_;
  const device::Ctap2Version ctap2_version_;
  const device::AuthenticatorAttachment attachment_;
  const bool has_resident_key_;
  const bool has_user_verification_;
  bool is_user_verified_ = true;
  const std::string unique_id_;
  bool is_user_present_;
  scoped_refptr<device::VirtualFidoDevice::State> state_;
  mojo::ReceiverSet<blink::test::mojom::VirtualAuthenticator> receiver_set_;

  DISALLOW_COPY_AND_ASSIGN(VirtualAuthenticator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_H_
