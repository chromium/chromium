// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_ASSISTIVE_TECHNOLOGY_CONTROLLER_IMPL_H_
#define SERVICES_ACCESSIBILITY_ASSISTIVE_TECHNOLOGY_CONTROLLER_IMPL_H_

#include <set>
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace ax {

// Implementation of the assistive technology controller interface
// for Chrome OS. This tracks which features are enabled and will
// load/unload feature implementations from V8 as needed.
class AssistiveTechnologyControllerImpl
    : public mojom::AssistiveTechnologyController {
 public:
  AssistiveTechnologyControllerImpl();
  ~AssistiveTechnologyControllerImpl() override;
  AssistiveTechnologyControllerImpl(const AssistiveTechnologyControllerImpl&) =
      delete;
  AssistiveTechnologyControllerImpl& operator=(
      const AssistiveTechnologyControllerImpl&) = delete;

  void Bind(mojo::PendingReceiver<mojom::AssistiveTechnologyController>
                at_controller_receiver);

  // TODO(crbug.com/1355633): Override this method from
  // mojom::AssistiveTechnologyController:
  void EnableAssistiveTechnology(mojom::AssistiveTechnologyType type,
                                 bool enabled);

  bool IsFeatureEnabled(mojom::AssistiveTechnologyType type) const;

 private:
  std::set<mojom::AssistiveTechnologyType> enabled_ATs_;

  mojo::ReceiverSet<mojom::AssistiveTechnologyController>
      at_controller_receivers_;
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_ASSISTIVE_TECHNOLOGY_CONTROLLER_IMPL_H_
