// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_ASSISTIVE_TECHNOLOGY_CONTROLLER_IMPL_H_
#define SERVICES_ACCESSIBILITY_ASSISTIVE_TECHNOLOGY_CONTROLLER_IMPL_H_

#include <memory>
#include <set>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace ax {
class V8Manager;

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

  // Called by the AccessibilityService.
  void Bind(mojo::PendingReceiver<mojom::AssistiveTechnologyController>
                at_controller_receiver);

  // Called by the Automation implementation within a V8 isolate to request
  // binding to the OS automation and automation client.
  void BindAutomation(
      mojo::PendingRemote<mojom::Automation> automation,
      mojo::PendingReceiver<mojom::AutomationClient> automation_client);

  // TODO(crbug.com/1355633): Override this method from
  // mojom::AssistiveTechnologyController:
  void EnableAssistiveTechnology(mojom::AssistiveTechnologyType type,
                                 bool enabled);

  bool IsFeatureEnabled(mojom::AssistiveTechnologyType type) const;

  // Methods for testing.
  void SetAutomationBoundClosureForTest(base::OnceClosure closure);
  void RunScriptForTest(mojom::AssistiveTechnologyType type,
                        const std::string& script,
                        base::OnceClosure on_complete);

 private:
  scoped_refptr<V8Manager> GetOrMakeV8Manager(
      mojom::AssistiveTechnologyType type);

  std::map<mojom::AssistiveTechnologyType, scoped_refptr<V8Manager>>
      enabled_ATs_;

  // Whether V8 has been initialized once. Allows us to only
  // initialize V8 for the service one time. Assumes this class has the same
  // lifetime as the service (as it's constructed and owned by the
  // AccessibilityServiceCros).
  bool v8_initialized_ = false;

  // For testing.
  base::OnceClosure automation_bound_closure_for_test_;

  // This class is a receiver for mojom::AssistiveTechnologyController.
  mojo::Receiver<mojom::AssistiveTechnologyController> at_controller_receiver_{
      this};

  base::WeakPtrFactory<AssistiveTechnologyControllerImpl> weak_ptr_factory_{
      this};
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_ASSISTIVE_TECHNOLOGY_CONTROLLER_IMPL_H_
