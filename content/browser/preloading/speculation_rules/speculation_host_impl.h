// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_HOST_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_HOST_IMPL_H_

#include <vector>

#include "content/public/browser/document_service.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {

class RenderFrameHost;

// Receiver for speculation rules from the web platform. See
// third_party/blink/renderer/core/speculation_rules/README.md
class CONTENT_EXPORT SpeculationHostImpl final
    : public DocumentService<blink::mojom::SpeculationHost> {
 public:
  // Creates and binds an instance of this per-frame.
  static void Bind(
      RenderFrameHost* frame_host,
      mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver);

  SpeculationHostImpl(const SpeculationHostImpl&) = delete;
  SpeculationHostImpl& operator=(const SpeculationHostImpl&) = delete;
  SpeculationHostImpl(SpeculationHostImpl&&) = delete;
  SpeculationHostImpl& operator=(SpeculationHostImpl&&) = delete;

 private:
  SpeculationHostImpl(
      RenderFrameHost& frame_host,
      mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver);
  ~SpeculationHostImpl() override;

  void UpdateSpeculationCandidates(
      std::vector<blink::mojom::SpeculationCandidatePtr> candidates) override;
  void OnLCPPredicted() override;
  void InitiatePreview(const GURL& url) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_HOST_IMPL_H_
