// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_API_HELPER_H_
#define EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_API_HELPER_H_

#include <string>

#include "content/public/renderer/render_frame_observer.h"
#include "extensions/common/mojom/automation_query.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace extensions {

// Renderer-side implementation for chrome.automation API (for the few pieces
// which aren't built in to the existing accessibility system).
class AutomationApiHelper : public content::RenderFrameObserver,
                            public mojom::AutomationQuery {
 public:
  explicit AutomationApiHelper(content::RenderFrame* render_frame);

  AutomationApiHelper(const AutomationApiHelper&) = delete;
  AutomationApiHelper& operator=(const AutomationApiHelper&) = delete;

  ~AutomationApiHelper() override;

 private:
  void BindAutomationQueryReceiver(
      mojo::PendingAssociatedReceiver<mojom::AutomationQuery> receiver);

  // mojom::AutomationQuery
  void QuerySelector(int32_t acc_obj_id,
                     const std::string& selector,
                     QuerySelectorCallback callback) override;

  // content::RenderFrameObserver:
  void OnDestruct() override;

  mojo::AssociatedReceiverSet<mojom::AutomationQuery> receivers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_API_HELPER_H_
