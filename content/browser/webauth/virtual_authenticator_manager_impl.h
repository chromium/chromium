// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_MANAGER_IMPL_H_
#define CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/common/content_export.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {

class VirtualFidoDiscoveryFactory;

// Allows setting up and configurating virtual authenticator devices for
// testing, the devtools WebAuthn pane, and WebDriver.
class CONTENT_EXPORT VirtualAuthenticatorManagerImpl {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void AuthenticatorAdded(VirtualAuthenticator*) = 0;
    virtual void AuthenticatorRemoved(const std::string& authenticator_id) = 0;
  };

  VirtualAuthenticatorManagerImpl();
  VirtualAuthenticatorManagerImpl(const VirtualAuthenticatorManagerImpl&) =
      delete;
  VirtualAuthenticatorManagerImpl& operator=(
      const VirtualAuthenticatorManagerImpl&) = delete;
  ~VirtualAuthenticatorManagerImpl();

  void AddObserver(Observer*);
  void RemoveObserver(Observer*);

  // Creates an authenticator based on |options| and adds it to the list of
  // authenticators owned by this object. It returns a non-owning pointer to
  // the authenticator, or |nullptr| on error.
  VirtualAuthenticator* AddAuthenticatorAndReturnNonOwningPointer(
      const VirtualAuthenticator::Options& options);

  // Sets whether the UI is enabled or not. Defaults to false.
  void enable_ui(bool enable_ui) { enable_ui_ = enable_ui; }
  bool is_ui_enabled() const { return enable_ui_; }

  // Returns the authenticator with the given |id|. Returns nullptr if no
  // authenticator matches the ID.
  VirtualAuthenticator* GetAuthenticator(const std::string& id);

  // Returns all the authenticators attached to the factory.
  std::vector<VirtualAuthenticator*> GetAuthenticators();

  // Removes the authenticator with the given |id|. Returns true if an
  // authenticator matched the |id|, false otherwise.
  bool RemoveAuthenticator(const std::string& id);

  std::unique_ptr<VirtualFidoDiscoveryFactory> MakeDiscoveryFactory();

 private:
  VirtualAuthenticator* AddAuthenticator(
      std::unique_ptr<VirtualAuthenticator> authenticator);

  base::ObserverList<Observer> observers_;

  bool enable_ui_ = false;

  // The key is the unique_id of the corresponding value (the authenticator).
  std::map<std::string, std::unique_ptr<VirtualAuthenticator>> authenticators_;

  base::WeakPtrFactory<VirtualAuthenticatorManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_MANAGER_IMPL_H_
