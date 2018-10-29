// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_REQUEST_HANDLER_BASE_H_
#define DEVICE_FIDO_FIDO_REQUEST_HANDLER_BASE_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/fido_device_discovery.h"
#include "device/fido/fido_transport_protocol.h"

namespace service_manager {
class Connector;
};  // namespace service_manager

namespace device {

class BleAdapterManager;
class FidoAuthenticator;

struct COMPONENT_EXPORT(DEVICE_FIDO) PlatformAuthenticatorInfo {
  PlatformAuthenticatorInfo(std::unique_ptr<FidoAuthenticator> authenticator,
                            bool has_recognized_mac_touch_id_credential);
  PlatformAuthenticatorInfo(PlatformAuthenticatorInfo&&);
  PlatformAuthenticatorInfo& operator=(PlatformAuthenticatorInfo&& other);
  ~PlatformAuthenticatorInfo();

  std::unique_ptr<FidoAuthenticator> authenticator;
  bool has_recognized_mac_touch_id_credential;
};

// Base class that handles authenticator discovery/removal. Its lifetime is
// equivalent to that of a single WebAuthn request. For each authenticator, the
// per-device work is carried out by one FidoAuthenticator instance, which is
// constructed in a FidoDiscoveryBase and passed to the request handler via its
// Observer interface.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoRequestHandlerBase
    : public FidoDiscoveryBase::Observer {
 public:
  using AuthenticatorMap =
      std::map<std::string, FidoAuthenticator*, std::less<>>;
  using RequestCallback = base::RepeatingCallback<void(const std::string&)>;
  using BlePairingCallback =
      base::RepeatingCallback<void(std::string authenticator_id,
                                   std::string pin_code,
                                   base::OnceClosure success_callback,
                                   base::OnceClosure error_callback)>;

  enum class RequestType { kMakeCredential, kGetAssertion };

  // Encapsulates data required to initiate WebAuthN UX dialog. Once all
  // components of TransportAvailabilityInfo is set,
  // AuthenticatorRequestClientDelegate should be notified.
  // TODO(hongjunchoi): Add async calls to notify embedder when Bluetooth is
  // powered on/off.
  struct COMPONENT_EXPORT(DEVICE_FIDO) TransportAvailabilityInfo {
    TransportAvailabilityInfo();
    TransportAvailabilityInfo(const TransportAvailabilityInfo& other);
    TransportAvailabilityInfo& operator=(
        const TransportAvailabilityInfo& other);
    ~TransportAvailabilityInfo();

    // TODO(hongjunchoi): Factor |rp_id| and |request_type| from
    // TransportAvailabilityInfo.
    // See: https://crbug.com/875011
    std::string rp_id;
    RequestType request_type = RequestType::kMakeCredential;

    // The intersection of transports supported by the client and allowed by the
    // relying party.
    base::flat_set<FidoTransportProtocol> available_transports;

    bool has_recognized_mac_touch_id_credential = false;
    bool is_ble_powered = false;
    bool can_power_on_ble_adapter = false;
  };

  class COMPONENT_EXPORT(DEVICE_FIDO) TransportAvailabilityObserver {
   public:
    virtual ~TransportAvailabilityObserver();

    // This method will not be invoked until the observer is set.
    virtual void OnTransportAvailabilityEnumerated(
        TransportAvailabilityInfo data) = 0;

    // If true, the request handler will defer dispatch of its request onto the
    // given authenticator to the embedder. The embedder needs to call
    // |StartAuthenticatorRequest| when it wants to initiate request dispatch.
    //
    // This method is invoked before |FidoAuthenticatorAdded|, and may be
    // invoked multiple times for the same authenticator. Depending on the
    // result, the request handler might decide not to make the authenticator
    // available, in which case it never gets passed to
    // |FidoAuthenticatorAdded|.
    virtual bool EmbedderControlsAuthenticatorDispatch(
        const FidoAuthenticator& authenticator) = 0;

    virtual void BluetoothAdapterPowerChanged(bool is_powered_on) = 0;
    virtual void FidoAuthenticatorAdded(
        const FidoAuthenticator& authenticator) = 0;
    virtual void FidoAuthenticatorRemoved(base::StringPiece device_id) = 0;
    virtual void FidoAuthenticatorIdChanged(
        base::StringPiece old_authenticator_id,
        std::string new_authenticator_id) = 0;
    virtual void FidoAuthenticatorPairingModeChanged(
        base::StringPiece authenticator_id,
        bool is_in_pairing_mode) = 0;
  };

  // TODO(https://crbug.com/769631): Remove the dependency on Connector once
  // device/fido is servicified. The |available_transports| should be the
  // intersection of transports supported by the client and allowed by the
  // relying party.
  FidoRequestHandlerBase(
      service_manager::Connector* connector,
      const base::flat_set<FidoTransportProtocol>& available_transports);
  ~FidoRequestHandlerBase() override;

  // Triggers DispatchRequest() if |active_authenticators_| hold
  // FidoAuthenticator with given |authenticator_id|.
  void StartAuthenticatorRequest(const std::string& authenticator_id);

  // Invokes |FidoAuthenticator::Cancel| on all authenticators, except if
  // matching |exclude_id|, if one is provided. Cancelled authenticators are
  // immediately removed from |active_authenticators_|.
  //
  // This function is invoked either when: (a) the entire WebAuthn API request
  // is canceled or, (b) a successful response or "invalid state error" is
  // received from the any one of the connected authenticators, in which case
  // all other authenticators are cancelled.
  // https://w3c.github.io/webauthn/#iface-pkcredential
  void CancelActiveAuthenticators(base::StringPiece exclude_id = nullptr);
  void OnBluetoothAdapterEnumerated(bool is_present,
                                    bool is_powered_on,
                                    bool can_power_on);
  void OnBluetoothAdapterPowerChanged(bool is_powered_on);
  void PowerOnBluetoothAdapter();
  void InitiatePairingWithDevice(std::string authenticator_id,
                                 std::string pin_code,
                                 base::OnceClosure success_callback,
                                 base::OnceClosure error_callback);

  base::WeakPtr<FidoRequestHandlerBase> GetWeakPtr();

  void set_observer(TransportAvailabilityObserver* observer) {
    DCHECK(!observer_) << "Only one observer is supported.";
    observer_ = observer;

    DCHECK(notify_observer_callback_);
    notify_observer_callback_.Run();
  }

  // Set the platform authenticator for this request, if one is available.
  // |AuthenticatorImpl| must call this method after invoking |set_oberver| even
  // if no platform authenticator is available, in which case it passes nullptr.
  virtual void SetPlatformAuthenticatorOrMarkUnavailable(
      base::Optional<PlatformAuthenticatorInfo> platform_authenticator_info);

  // Returns whether FidoAuthenticator with id equal to |authenticator_id|
  // exists. Fake FidoRequestHandler objects used in testing overrides this
  // function to simulate scenarios where authenticator with |authenticator_id|
  // is known to the system.
  virtual bool HasAuthenticator(const std::string& authentiator_id) const;

  TransportAvailabilityInfo& transport_availability_info() {
    return transport_availability_info_;
  }

 protected:
  // Subclasses implement this method to dispatch their request onto the given
  // FidoAuthenticator. The FidoAuthenticator is owned by this
  // FidoRequestHandler and stored in active_authenticators().
  virtual void DispatchRequest(FidoAuthenticator*) = 0;

  void Start();

  AuthenticatorMap& active_authenticators() { return active_authenticators_; }
  std::vector<std::unique_ptr<FidoDiscoveryBase>>& discoveries() {
    return discoveries_;
  }
  TransportAvailabilityObserver* observer() const { return observer_; }

 private:
  friend class FidoRequestHandlerTest;

  // FidoDiscoveryBase::Observer
  void AuthenticatorAdded(FidoDiscoveryBase* discovery,
                          FidoAuthenticator* authenticator) final;
  void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                            FidoAuthenticator* authenticator) final;
  void AuthenticatorIdChanged(FidoDiscoveryBase* discovery,
                              const std::string& previous_id,
                              std::string new_id) final;
  void AuthenticatorPairingModeChanged(FidoDiscoveryBase* discovery,
                                       const std::string& device_id,
                                       bool is_in_pairing_mode) final;

  void AddAuthenticator(FidoAuthenticator* authenticator);
  void NotifyObserverTransportAvailability();

  // Invokes FidoAuthenticator::InitializeAuthenticator(), followed by
  // DispatchRequest(). InitializeAuthenticator() sends a GetInfo command
  // to FidoDeviceAuthenticator instances in order to determine their protocol
  // versions before a request can be dispatched.
  void InitializeAuthenticatorAndDispatchRequest(FidoAuthenticator*);
  void ConstructBleAdapterPowerManager();

  AuthenticatorMap active_authenticators_;
  std::vector<std::unique_ptr<FidoDiscoveryBase>> discoveries_;
  TransportAvailabilityObserver* observer_ = nullptr;
  TransportAvailabilityInfo transport_availability_info_;
  base::RepeatingClosure notify_observer_callback_;
  std::unique_ptr<BleAdapterManager> bluetooth_adapter_manager_;
  // TODO(martinkr): Inject platform authenticators through a new
  // FidoDiscoveryBase specialization and hold ownership there.
  std::unique_ptr<FidoAuthenticator> platform_authenticator_;

  base::WeakPtrFactory<FidoRequestHandlerBase> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(FidoRequestHandlerBase);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_REQUEST_HANDLER_BASE_H_
