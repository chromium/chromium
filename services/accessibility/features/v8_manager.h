// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_V8_MANAGER_H_
#define SERVICES_ACCESSIBILITY_FEATURES_V8_MANAGER_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/threading/sequence_bound.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/accessibility/features/bindings_isolate_holder.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom-forward.h"
#include "services/accessibility/public/mojom/automation.mojom-forward.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-local-handle.h"

namespace v8 {
class Isolate;
}  // namespace v8

namespace gin {
class ContextHolder;
class IsolateHolder;
}  // namespace gin

namespace ax {

class AutomationInternalBindings;
class InterfaceBinder;
class V8Manager;
class OSDevToolsAgent;

// A V8Environment owns a V8 context within the Accessibility Service, the
// bindings that belong to that context, as well as loading the Javascript that
// will run in that context.
//
// It lives on an implementation-defined task runner (typically a background
// task runner dedicated to this isolate+context) and should primarily be used
// through its owning class, V8Manager.
//
// TODO(dcheng): Move this into v8_environment.h.
class V8Environment : public BindingsIsolateHolder {
 public:
  // The default Context ID to use. We currently will have one context per
  // isolate. In the future we may need to switch this to an incrementing
  // system.
  static const int kDefaultContextId = 1;

  // Creates a new V8Environment with its own isolate and context.
  static base::SequenceBound<V8Environment> Create(
      base::WeakPtr<V8Manager> manager);

  // Gets a pointer to the V8 manager that belongs to this `context`.
  static V8Environment* GetFromContext(v8::Local<v8::Context> context);

  V8Environment(const V8Environment&) = delete;
  V8Environment& operator=(const V8Environment&) = delete;

  // Creates a devtools agent to debug javascript running in this environment.
  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent);

  // All of the APIs needed for this V8Manager (based on the AT type) should be
  // installed before adding V8 bindings.
  void InstallAutomation(
      mojo::PendingAssociatedReceiver<mojom::Automation> automation,
      mojo::PendingRemote<mojom::AutomationClient> automation_client);
  void AddV8Bindings();

  // Executes the given string as a Javascript script, and calls the
  // callback when execution is complete.
  void ExecuteScript(const std::string& script,
                     base::OnceCallback<void()> on_complete);

  // BindingsIsolateHolder overrides:
  v8::Isolate* GetIsolate() const override;
  v8::Local<v8::Context> GetContext() const override;

  explicit V8Environment(scoped_refptr<base::SequencedTaskRunner> main_runner,
                         base::WeakPtr<V8Manager> manager);
  virtual ~V8Environment();

  // Called by the Mojo JS API when ready to bind an interface.
  void BindInterface(const std::string& interface_name,
                     mojo::GenericPendingReceiver pending_receiver);

 private:
  void CreateIsolate();

  // Thread runner for communicating with object which constructed this
  // class using V8Environment::Create. This may be the main service thread,
  // but that is not required.
  const scoped_refptr<base::SequencedTaskRunner> main_runner_;
  const base::WeakPtr<V8Manager> manager_;

  // Bindings wrappers for V8 APIs.
  // TODO(crbug.com/1355633): Add more APIs including TTS, SST, etc.
  std::unique_ptr<AutomationInternalBindings> automation_bindings_;

  // Holders for isolate and context.
  std::unique_ptr<gin::IsolateHolder> isolate_holder_;
  std::unique_ptr<gin::ContextHolder> context_holder_;

  std::unique_ptr<OSDevToolsAgent> devtools_agent_;
};

// Owns the V8Environment and any Mojo interfaces exposed to that V8Environment.
// Lives on the main service thread; any use of the internally-owned
// V8Environment will be proxied to the v8 task runner.
//
// There may be one V8Manager per Assistive Technology feature or features
// may share V8Managers.
class V8Manager {
 public:
  V8Manager();
  ~V8Manager();

  // Various optional features that can be configured. All configuration must be
  // done before calling `FinishContextSetUp()`.
  void ConfigureAutomation(
      mojo::PendingAssociatedReceiver<mojom::Automation> automation,
      mojo::PendingRemote<mojom::AutomationClient> automation_client);
  void ConfigureTts(mojom::AccessibilityServiceClient* ax_service_client);
  void ConfigureUserInterface(
      mojom::AccessibilityServiceClient* ax_service_client);

  void FinishContextSetUp();

  // Instructs V8Environment to create a devtools agent.
  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent);

  // Called by V8Environment when JS wants to bind a Mojo interface.
  void BindInterface(const std::string& interface_name,
                     mojo::GenericPendingReceiver pending_receiver);

  // Allow tests to expose additional Mojo interfaces to JS.
  void AddInterfaceForTest(std::unique_ptr<InterfaceBinder> interface_binder);
  void RunScriptForTest(const std::string& script,
                        base::OnceClosure on_complete);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  base::SequenceBound<V8Environment> v8_env_;
  // The Mojo interfaces that are exposed to JS. When JS wants to bind a Mojo
  // interface, the first matching InterfaceBinder will be used.
  std::vector<std::unique_ptr<InterfaceBinder>> interface_binders_;

  base::WeakPtrFactory<V8Manager> weak_factory_{this};
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_V8_MANAGER_H_
