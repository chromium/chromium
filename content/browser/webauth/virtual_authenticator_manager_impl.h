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
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom.h"

namespace content {

class VirtualAuthenticator;
class VirtualFidoDiscoveryFactory;

// Implements the Mojo interface representing a virtual authenticator manager
// for the Web Authentication API. Allows setting up and configurating virtual
// authenticator devices for testing.
class CONTENT_EXPORT VirtualAuthenticatorManagerImpl
    : public blink::test::mojom::VirtualAuthenticatorManager {
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
  ~VirtualAuthenticatorManagerImpl() override;

  void AddObserver(Observer*);
  void RemoveObserver(Observer*);

  void AddReceiver(
      mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
          receiver);

  // Creates an authenticator based on |options| and adds it to the list of
  // authenticators owned by this object. It returns a non-owning pointer to
  // the authenticator, or |nullptr| on error.
  VirtualAuthenticator* AddAuthenticatorAndReturnNonOwningPointer(
      const blink::test::mojom::VirtualAuthenticatorOptions& options);

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

 protected:
  // blink::test::mojom::VirtualAuthenticatorManager:
  void CreateAuthenticator(
      blink::test::mojom::VirtualAuthenticatorOptionsPtr options,
      CreateAuthenticatorCallback callback) override;
  void GetAuthenticators(GetAuthenticatorsCallback callback) override;
  void RemoveAuthenticator(const std::string& id,
                           RemoveAuthenticatorCallback callback) override;
  void ClearAuthenticators(ClearAuthenticatorsCallback callback) override;

 private:
  VirtualAuthenticator* AddAuthenticator(
      std::unique_ptr<VirtualAuthenticator> authenticator);

  base::ObserverList<Observer> observers_;

  bool enable_ui_ = false;

  // The key is the unique_id of the corresponding value (the authenticator).
  std::map<std::string, std::unique_ptr<VirtualAuthenticator>> authenticators_;

  mojo::ReceiverSet<blink::test::mojom::VirtualAuthenticatorManager> receivers_;

  base::WeakPtrFactory<VirtualAuthenticatorManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_MANAGER_IMPL_H_
