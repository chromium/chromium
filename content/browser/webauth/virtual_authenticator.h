// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_H_
#define CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "content/common/content_export.h"
#include "device/fido/virtual_fido_device.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace content {

// Implements a stateful virtual authenticator.
//
// This class has very little logic itself, it merely stores a unique ID and the
// state of the authenticator, whereas performing all cryptographic operations
// is delegated to the VirtualFidoDevice class.
class CONTENT_EXPORT VirtualAuthenticator
    : public device::VirtualFidoDevice::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnCredentialCreated(
        VirtualAuthenticator* authenticator,
        const device::VirtualFidoDevice::Credential& credential) = 0;
    virtual void OnCredentialDeleted(
        VirtualAuthenticator* authenticator,
        base::span<const uint8_t> credential_id) = 0;
    virtual void OnCredentialUpdated(
        VirtualAuthenticator* authenticator,
        const device::VirtualFidoDevice::Credential& credential) = 0;
    virtual void OnAssertion(
        VirtualAuthenticator* authenticator,
        const device::VirtualFidoDevice::Credential& credential) = 0;
    virtual void OnAuthenticatorWillBeDestroyed(
        VirtualAuthenticator* authenticator) = 0;
  };

  // Attributes that the virtual authenticator should simulate.
  struct CONTENT_EXPORT Options {
    Options();
    ~Options();

    // protocol is the client-to-authenticator protocol that the virtual
    // authenticator simulates.
    device::ProtocolVersion protocol = device::ProtocolVersion::kCtap2;
    // ctap2_version indicates which minor version of CTAP2 the authenticator
    // should simulate if |protocol| is CTAP2. It is ignored otherwise.
    device::Ctap2Version ctap2_version = device::Ctap2Version::kCtap2_0;
    device::FidoTransportProtocol transport =
        device::FidoTransportProtocol::kUsbHumanInterfaceDevice;
    device::AuthenticatorAttachment attachment =
        device::AuthenticatorAttachment::kCrossPlatform;
    bool has_resident_key = false;
    bool has_user_verification = false;
    bool is_user_present = true;
    bool has_large_blob = false;
    bool has_cred_blob = false;
    bool has_min_pin_length = false;
    bool has_prf = false;
    bool default_backup_eligibility = false;
    bool default_backup_state = false;
  };

  using GetLargeBlobCallback =
      base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>;
  using SetLargeBlobCallback = base::OnceCallback<void(bool)>;

  explicit VirtualAuthenticator(const Options& options);

  VirtualAuthenticator(const VirtualAuthenticator&) = delete;
  VirtualAuthenticator& operator=(const VirtualAuthenticator&) = delete;

  ~VirtualAuthenticator() override;

  device::VirtualFidoDevice::State::RegistrationsMap& registrations() const {
    return state_->registrations;
  }

  // Register a new credential. Returns true if the registration was successful,
  // false otherwise.
  bool AddRegistration(std::vector<uint8_t> key_handle,
                       const std::string& rp_id,
                       base::span<const uint8_t> private_key,
                       int32_t counter);

  // Register a new resident credential. Returns true if the registration was
  // successful, false otherwise.
  bool AddResidentRegistration(std::vector<uint8_t> key_handle,
                               std::string rp_id,
                               base::span<const uint8_t> private_key,
                               int32_t counter,
                               std::vector<uint8_t> user_handle,
                               std::optional<std::string> user_name,
                               std::optional<std::string> user_display_name);

  // Removes all the credentials.
  void ClearRegistrations();

  // Remove a credential identified by |key_handle|. Returns true if the
  // credential was found and removed, false otherwise.
  bool RemoveRegistration(const std::vector<uint8_t>& key_handle);

  // Updates the name and display name of registrations matching
  // |relying_party_id| and |user_id|.
  void UpdateUserDetails(std::string_view relying_party_id,
                         base::span<const uint8_t> user_id,
                         std::string_view name,
                         std::string_view display_name);

  // Sets whether tests of user presence succeed or not for new requests sent to
  // this authenticator. The default is true.
  void SetUserPresence(bool is_user_present);

  // Sets whether user verification should succeed or not for new requests sent
  // to this authenticator. Defaults to true.
  void set_user_verified(bool is_user_verified) {
    is_user_verified_ = is_user_verified;
  }

  // If set, overrides the signature in the authenticator response to be zero.
  // Defaults to false.
  void set_bogus_signature(bool is_bogus) {
    state_->ctap2_invalid_signature = is_bogus;
  }

  // If set, overrides the UV bit in the flags in the authenticator response to
  // be zero. Defaults to false.
  void set_bad_uv_bit(bool is_bad_bit) { state_->unset_uv_bit = is_bad_bit; }

  // If set, overrides the UP bit in the flags in the authenticator response to
  // be zero. Defaults to false.
  void set_bad_up_bit(bool is_bad_bit) { state_->unset_up_bit = is_bad_bit; }

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
  std::unique_ptr<device::VirtualFidoDevice> ConstructDevice();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserversForTest();

  // Set the BE flag for a given |key_handle|. |key_handle| must match a
  // credential.
  void SetBackupEligibility(const std::vector<uint8_t>& key_handle,
                            bool backup_eligibility);
  // Set the BS flag for a given |key_handle|. |key_handle| must match a
  // credential.
  void SetBackupState(const std::vector<uint8_t>& key_handle,
                      bool backup_state);

  void GetLargeBlob(const std::vector<uint8_t>& key_handle,
                    GetLargeBlobCallback callback);
  void SetLargeBlob(const std::vector<uint8_t>& key_handle,
                    const std::vector<uint8_t>& blob,
                    SetLargeBlobCallback callback);

 private:
  void OnLargeBlobUncompressed(
      GetLargeBlobCallback callback,
      base::expected<mojo_base::BigBuffer, std::string> result);
  void OnLargeBlobCompressed(
      base::span<const uint8_t> key_handle,
      uint64_t original_size,
      SetLargeBlobCallback callback,
      base::expected<mojo_base::BigBuffer, std::string> result);

  // device::VirtualFidoDevice::Observer:
  void OnCredentialCreated(
      const device::VirtualFidoDevice::Credential& credential) override;
  void OnCredentialDeleted(base::span<const uint8_t> credential_id) override;
  void OnCredentialUpdated(
      const device::VirtualFidoDevice::Credential& credential) override;
  void OnAssertion(
      const device::VirtualFidoDevice::Credential& credential) override;

  const device::ProtocolVersion protocol_;
  const device::Ctap2Version ctap2_version_;
  const device::AuthenticatorAttachment attachment_;
  const bool has_resident_key_;
  const bool has_user_verification_;
  const bool has_large_blob_;
  const bool has_cred_blob_;
  const bool has_min_pin_length_;
  const bool has_prf_;
  bool is_user_verified_ = true;
  const std::string unique_id_;
  bool is_user_present_;
  base::ObserverList<Observer> observers_;
  data_decoder::DataDecoder data_decoder_;
  scoped_refptr<device::VirtualFidoDevice::State> state_;
  base::ScopedObservation<device::VirtualFidoDevice::State,
                          device::VirtualFidoDevice::Observer>
      observation_{this};

  base::WeakPtrFactory<VirtualAuthenticator> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_H_
