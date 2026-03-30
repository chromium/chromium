// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_CREDENTIAL_ENVIRONMENT_H_
#define CONTENT_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_CREDENTIAL_ENVIRONMENT_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/common/content_export.h"

namespace content {

class VirtualWallet;

// Allows enabling and disabling per-frame virtual wallet environments for the
// Digital Credentials API. Disabling the environment resets its state.
//
// This class is a singleton. Pointers returned by this class are valid only
// for the lifetime of the associated FrameTreeNode.
class CONTENT_EXPORT DigitalCredentialEnvironment
    : public FrameTreeNode::Observer {
 public:
  static DigitalCredentialEnvironment* GetInstance();

  DigitalCredentialEnvironment(const DigitalCredentialEnvironment&) = delete;
  DigitalCredentialEnvironment& operator=(const DigitalCredentialEnvironment&) =
      delete;

  // Disables all virtual wallet environments.
  void Reset();

  // Returns the VirtualWallet for |node| or the nearest ancestor that has one,
  // or nullptr if none is found.
  VirtualWallet* MaybeGetVirtualWallet(FrameTreeNode* node);

  // Returns the VirtualWallet for |node|, creating one if it does not exist.
  // Unlike MaybeGetVirtualWallet(), this does not look up the ancestor chain.
  VirtualWallet* GetOrCreateVirtualWallet(FrameTreeNode* node);

 private:
  friend class base::NoDestructor<DigitalCredentialEnvironment>;

  struct ObservedWallet {
    ObservedWallet(FrameTreeNode* node, FrameTreeNode::Observer* observer);
    ~ObservedWallet();

    std::unique_ptr<VirtualWallet> wallet;
    base::ScopedObservation<FrameTreeNode, FrameTreeNode::Observer> observation;
  };

  DigitalCredentialEnvironment();
  ~DigitalCredentialEnvironment() override;

  // FrameTreeNode::Observer:
  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override;

  // Creates a new VirtualWallet for |node|, begins observing it for
  // destruction, and returns a pointer to it.
  VirtualWallet* EnableVirtualWalletFor(FrameTreeNode* node);

  // Removes the VirtualWallet registered for |node| and stops observing it.
  void DisableVirtualWalletFor(FrameTreeNode* node);

  base::flat_map<FrameTreeNode*, std::unique_ptr<ObservedWallet>>
      virtual_wallets_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_CREDENTIAL_ENVIRONMENT_H_
