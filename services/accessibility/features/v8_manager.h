// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_V8_MANAGER_H_
#define SERVICES_ACCESSIBILITY_FEATURES_V8_MANAGER_H_

#include <memory>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "services/accessibility/features/bindings_isolate_holder.h"
#include "services/accessibility/features/interface_binder.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-value.h"

namespace v8 {
class Isolate;
}  // namespace v8

namespace gin {
class ContextHolder;
class IsolateHolder;
}  // namespace gin

namespace ax {
class AutomationInternalBindings;
class AssistiveTechnologyControllerImpl;

// A V8Manager owns a V8 isolate within the Accessibility Service, and manages
// the bindings that belong to that isolate, as well as loading the Javascript
// that will run in that isolate.
// V8Manager may be created on any service thread but must be destroyed
// on the V8 thread created in V8Manager::Create so that the V8 context and
// isolate are only accessed from that thread.
// There may be one V8Manager per Assistive Technology feature or features
// may share V8Managers.
class V8Manager : public BindingsIsolateHolder,
                  public base::RefCountedDeleteOnSequence<V8Manager> {
 public:
  // Creates a new V8Manager with its own isolate and context.
  static scoped_refptr<V8Manager> Create();

  // Gets a pointer to the V8 manager that belongs to this `context`.
  static V8Manager* GetFromContext(v8::Local<v8::Context> context);

  V8Manager(const V8Manager&) = delete;
  V8Manager& operator=(const V8Manager&) = delete;

  // Called from main service thread.
  // All of the APIs should be installed before adding V8 bindings.
  void InstallAutomation(
      base::WeakPtr<AssistiveTechnologyControllerImpl> at_controller);
  void AddV8Bindings();

  // Executes the given string as a Javascript script, and calls the
  // callback when execution is complete.
  void ExecuteScript(const std::string& script,
                     base::OnceCallback<void()> on_complete);

  // Called from V8 thread.
  // BindingsIsolateHolder overrides:
  v8::Isolate* GetIsolate() const override;
  v8::Local<v8::Context> GetContext() const override;

  // Called from the V8 thread by Mojo when ready to bind an interface.
  void BindInterface(const std::string& interface_name,
                     mojo::GenericPendingReceiver pending_receiver);

  // Sets the InterfaceBinder used for when trying to bind
  // axtest.mojom.TestBindingInterface. Used for testing.
  void SetTestMojoInterface(std::unique_ptr<InterfaceBinder> test_interface);

 private:
  // Allows RefCountedDeleteOnSequence access to the destructor.
  friend class base::RefCountedDeleteOnSequence<V8Manager>;
  friend class base::DeleteHelper<V8Manager>;

  explicit V8Manager(scoped_refptr<base::SingleThreadTaskRunner> v8_runner,
                     scoped_refptr<base::SequencedTaskRunner> main_runner);
  virtual ~V8Manager();

  // Methods called from V8 thread.
  void ConstructIsolateOnThread();
  void AddV8BindingsOnThread();
  void BindAutomationOnThread(
      base::WeakPtr<AssistiveTechnologyControllerImpl> at_controller);
  void ExecuteScriptOnThread(const std::string& script,
                             base::OnceCallback<void()> on_complete);

  // Thread runner for all things V8.
  scoped_refptr<base::SingleThreadTaskRunner> v8_runner_;

  // Thread runner for communicating with object which constructed this
  // class using V8Manager::Create. This may be the main service thread
  // but that is not required.
  scoped_refptr<base::SequencedTaskRunner> main_runner_;

  // Bindings wrappers for V8 APIs.
  // TODO(crbug.com/1355633): Add more APIs including TTS, SST, etc.
  std::unique_ptr<AutomationInternalBindings> automation_bindings_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Bindings wrappers for test.
  std::unique_ptr<InterfaceBinder> test_mojo_interface_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Holders for isolate and context.
  // These may only be accessed from the v8_runner_ thread.
  std::unique_ptr<gin::IsolateHolder> isolate_holder_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<gin::ContextHolder> context_holder_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to check that the correct thread is used for V8 work and main
  // service thread communication.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<V8Manager> weak_ptr_factory_{this};
};
}  // namespace ax
#endif  // SERVICES_ACCESSIBILITY_FEATURES_V8_MANAGER_H_
