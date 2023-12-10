// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_ENVIRONMENT_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_ENVIRONMENT_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom-forward.h"

namespace device {
class FidoDiscoveryFactory;
}  // namespace device

namespace content {

class VirtualAuthenticatorManagerImpl;

// Allows enabling and disabling per-frame virtual environments for the Web
// Authentication API. Disabling the environment resets its state.
//
// This class is a singleton.
class CONTENT_EXPORT AuthenticatorEnvironment : public FrameTreeNode::Observer {
 public:
  static AuthenticatorEnvironment* GetInstance();

  AuthenticatorEnvironment(const AuthenticatorEnvironment&) = delete;
  AuthenticatorEnvironment& operator=(const AuthenticatorEnvironment&) = delete;

  // Disables all test environments.
  void Reset();

  // Enables the scoped virtual authenticator environment for the |node| and its
  // descendants.
  // Does not have any effect if the |node| already has the virtual environment
  // enabled.
  void EnableVirtualAuthenticatorFor(FrameTreeNode* node, bool enable_ui);

  // Disables the scoped virtual authenticator environment for this |node|,
  // resetting the state. If the environment is set on one of the |node|'s
  // parents instead, this won't have any effect.
  void DisableVirtualAuthenticatorFor(FrameTreeNode* node);

  // Returns whether the virtual authenticator environment is enabled for
  // |node|.
  bool IsVirtualAuthenticatorEnabledFor(FrameTreeNode* node);

  // Returns the virtual fido discovery factory for the |node| if the virtual
  // environment is enabled for it, otherwise returns nullptr.
  VirtualAuthenticatorManagerImpl* MaybeGetVirtualAuthenticatorManager(
      FrameTreeNode* node);

  // Adds the receiver to the virtual authenticator enabled for the |node|. The
  // virtual authenticator must be enabled beforehand.
  void AddVirtualAuthenticatorReceiver(
      FrameTreeNode* node,
      mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
          receiver);

  // Returns whether |node| has the virtual authenticator environment enabled
  // with a user-verifying platform installed in that environment.
  bool HasVirtualUserVerifyingPlatformAuthenticator(FrameTreeNode* node);

  // Returns the override installed by
  // ReplaceDefaultDiscoveryFactoryForTesting().
  device::FidoDiscoveryFactory* MaybeGetDiscoveryFactoryTestOverride();

  void ReplaceDefaultDiscoveryFactoryForTesting(
      std::unique_ptr<device::FidoDiscoveryFactory> factory);

  // FrameTreeNode::Observer:
  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override;

 protected:
  AuthenticatorEnvironment();
  ~AuthenticatorEnvironment() override;

 private:
  friend class base::NoDestructor<AuthenticatorEnvironment>;

  std::unique_ptr<device::FidoDiscoveryFactory> replaced_discovery_factory_;

  std::map<FrameTreeNode*, std::unique_ptr<VirtualAuthenticatorManagerImpl>>
      virtual_authenticator_managers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_ENVIRONMENT_H_
