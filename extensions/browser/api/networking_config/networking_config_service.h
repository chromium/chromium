// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_CONFIG_NETWORKING_CONFIG_SERVICE_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_CONFIG_NETWORKING_CONFIG_SERVICE_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace extensions {

// This class provides the session-scoped storage backing the networking config
// extension API. Currently only the parts relevant for captive portal handling
// are implemented.
class NetworkingConfigService : public ExtensionRegistryObserver,
                                public KeyedService {
 public:
  class EventDelegate {
   public:
    EventDelegate() {}
    virtual ~EventDelegate() {}
    virtual bool HasExtensionRegisteredForEvent(
        const std::string& extension_id) const = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(EventDelegate);
  };

  // Indicates the authentication state of the portal.
  enum AuthenticationState { NOTRY, TRYING, SUCCESS, REJECTED, FAILED };

  // Provides information about the current authentication state of the portal.
  struct AuthenticationResult {
    AuthenticationResult();
    AuthenticationResult(ExtensionId extension_id,
                         std::string guid,
                         AuthenticationState authentication_state);
    ExtensionId extension_id;
    std::string guid;
    AuthenticationState authentication_state;
  };

  // Note: |extension_registry| must outlive this class.
  NetworkingConfigService(content::BrowserContext* browser_context,
                          std::unique_ptr<EventDelegate> event_delegate,
                          ExtensionRegistry* extension_registry);
  ~NetworkingConfigService() override;

  // ExtensionRegistryObserver
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Returns the extension id registered for |hex_ssid|. If no extension is
  // registered for this |hex_ssid|, the function returns an empty string.
  // |hex_ssid|: SSID in hex encoding.
  std::string LookupExtensionIdForHexSsid(std::string hex_ssid) const;

  // Returns true if the extension with id |extension_id| registered for
  // |onCaptivePortalDetected| events, otherwise false.
  bool IsRegisteredForCaptivePortalEvent(const std::string& extension_id) const;

  // Registers |hex_ssid| as being handled by the extension with extension ID
  // |extension_id|. Returns true on success and false if another extension
  // already registered for |hex_ssid|.
  // |hex_ssid|:     SSID in hex encoding of the network to be registered.
  // |extension_id|: Extension ID of the extension handling the network
  //                 configuration for this network.
  bool RegisterHexSsid(std::string hex_ssid, const std::string& extension_id);

  // Unregisters extension with the ID |extension_id| removing all associated
  // HexSSIDs from the map.
  // |extension_id|: ID identifying the extension to be removed
  void UnregisterExtension(const std::string& extensionId);

  // Returns the current AuthenticationResult.
  const AuthenticationResult& GetAuthenticationResult() const;

  // Sets the authentication_state to NOTRY and clears all other fields.
  void ResetAuthenticationResult();

  // Sets the current AuthenticationResult.
  void SetAuthenticationResult(
      const AuthenticationResult& authentication_result);

  // Sends a PortalDetected event for the network with |guid| to the extension
  // with |extension_id|.
  // |authentication_callback| is stored and called if the extension finishes
  // authentication succesfully. This Service handles at most one portal
  // detection at a time, i.e. another call to this function or a not successful
  // authentication will drop a previously provided |authentication_callback|.
  void DispatchPortalDetectedEvent(
      const std::string& extension_id,
      const std::string& guid,
      const base::Closure& authentication_callback);

 private:
  void OnGotProperties(const std::string& extension_id,
                       const std::string& guid,
                       const base::Closure& authentication_callback,
                       const std::string& service_path,
                       const base::DictionaryValue& onc_network_config);

  void OnGetPropertiesFailed(const std::string& extension_id,
                             const std::string& guid,
                             const std::string& error_name,
                             std::unique_ptr<base::DictionaryValue> error_data);

  // Creates the captive portal event about the network with guid |guid| that is
  // to be dispatched to the extension identified by |extension_id|. |bssid|
  // contains a human readable, hex-encoded version of the BSSID with bytes
  // separated by colons, e.g. 45:67:89:ab:cd:ef.
  std::unique_ptr<Event> CreatePortalDetectedEventAndDispatch(
      const std::string& extension_id,
      const std::string& guid,
      const std::string* bssid);

  content::BrowserContext* const browser_context_;

  AuthenticationResult authentication_result_;
  base::Closure authentication_callback_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observer_{this};

  std::unique_ptr<EventDelegate> event_delegate_;

  // This map associates a given hex encoded SSID to an extension entry.
  std::map<std::string, std::string> hex_ssid_to_extension_id_;

  base::WeakPtrFactory<NetworkingConfigService> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_CONFIG_NETWORKING_CONFIG_SERVICE_H_
