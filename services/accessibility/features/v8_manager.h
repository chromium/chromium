// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_V8_MANAGER_H_
#define SERVICES_ACCESSIBILITY_FEATURES_V8_MANAGER_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/threading/sequence_bound.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/accessibility/features/bindings_isolate_holder.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom-forward.h"
#include "services/accessibility/public/mojom/automation.mojom-forward.h"
#include "services/accessibility/public/mojom/file_loader.mojom-forward.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "v8-persistent-handle.h"
#include "v8-script.h"
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

  using OnFileLoadedCallback = base::OnceCallback<void(base::File)>;

  // Creates a new V8Environment with its own isolate and context.
  static base::SequenceBound<V8Environment> Create(
      base::WeakPtr<V8Manager> manager);

  // Gets a pointer to the V8 manager that belongs to this `context`.
  static V8Environment* GetFromContext(v8::Local<v8::Context> context);

  // Resolves `relative_path` to `base_dir` and performs some normalizations:
  // * Removes references to current directory;
  // * Replaces '..'with the directory name.
  //
  // Notes:
  // * This function will fail if `relative_path` is an absolute path;
  // * `relative_path` must be a relative path of `base_dir` and does not
  // reference any parents of `base_dir`;
  // * `base_dir`is not empty.
  static std::string NormalizeRelativePath(const std::string& relative_path,
                                           const std::string& base_dir);

  V8Environment(const V8Environment&) = delete;
  V8Environment& operator=(const V8Environment&) = delete;

  // Creates a devtools agent to debug javascript running in this environment.
  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent);

  // All of the APIs needed for this V8Manager (based on the AT type) should be
  // installed before adding V8 bindings.
  void InstallAutomation(
      mojo::PendingAssociatedReceiver<mojom::Automation> automation);
  void InstallOSState();
  void AddV8Bindings();

  // Executes the given string as a Javascript script, and calls the
  // callback when execution is complete.
  void ExecuteScript(const std::string& script,
                     base::OnceCallback<void()> on_complete);

  void ExecuteModule(base::FilePath file_path, base::OnceClosure on_complete);

  // BindingsIsolateHolder overrides:
  v8::Isolate* GetIsolate() const override;
  v8::Local<v8::Context> GetContext() const override;

  explicit V8Environment(scoped_refptr<base::SequencedTaskRunner> main_runner,
                         base::WeakPtr<V8Manager> manager);
  virtual ~V8Environment();

  // Called by the Mojo JS API when ready to bind an interface.
  void BindInterface(const std::string& interface_name,
                     mojo::GenericPendingReceiver pending_receiver);

  // `identifier` that represents a module. The identifier is composed of the
  // normalized resolved path of the directory name of the root module relative
  // path concatenated with the module specifier (which may be a file name or a
  // relative path of the root module).
  v8::MaybeLocal<v8::Module> GetModuleFromIdentifier(
      const std::string& identifier);

  std::optional<std::string> GetIdentifierFromModule(
      v8::Global<v8::Module> module);

 private:
  // Provides a hash function for a Global Module object.
  class ModuleGlobalHash {
   public:
    explicit ModuleGlobalHash(v8::Isolate* isolate) : isolate_(isolate) {}
    size_t operator()(const v8::Global<v8::Module>& module) const {
      return module.Get(isolate_)->GetIdentityHash();
    }

   private:
    raw_ptr<v8::Isolate> isolate_;
  };

  void CreateIsolate();

  // Loads the file contents of the module referenced by `file_pat`, invoking
  // `OnFileLoaded()` when the operation is done.
  // `file_path` must be a relative path and for now accepts only files in the
  // current directory.
  void RequestModuleContents(base::FilePath file_path);

  // Callback function invoked when the file contents of the module identified
  // by `module_identifier` is finished loading.
  void OnFileLoaded(std::string module_identifier, base::File file);

  // Evaluates the module identified by `root_module_identifier_` if all module
  // dependencies have been loaded and compiled into modules.
  void EvaluateModule();

  // Resets module evaluation to not in progress and handles the error thrown by
  // the v8 isolate or by the environment.
  void HandleModuleError(const std::string& message);

  // Thread runner for communicating with object which constructed this
  // class using V8Environment::Create. This may be the main service thread,
  // but that is not required.
  const scoped_refptr<base::SequencedTaskRunner> main_runner_;
  const base::WeakPtr<V8Manager> manager_;

  // Sync API bindings need to be installed during AddV8Bindings(), because the
  // IsolateScope and HandleScope are limited to that function.
  // Track which APIs need to be installed.
  bool os_state_needed_ = false;

  // Bindings wrappers for V8 APIs.
  // TODO(crbug.com/1355633): Add more APIs including TTS, SST, etc.
  std::unique_ptr<AutomationInternalBindings> automation_bindings_;

  // Holders for isolate and context.
  std::unique_ptr<gin::IsolateHolder> isolate_holder_;
  std::unique_ptr<gin::ContextHolder> context_holder_;

  // Whether a module is being evaluated by this object.
  bool module_evaluation_in_progress_ = false;

  // If a module is being evaluated, contains the identifier of the root module.
  std::optional<std::string> root_module_identifier_;

  // Callback to be invoked when the module is finished evaluating.
  base::OnceClosure on_complete_;

  // Number of modules that have not been compiled into a v8::Module object.
  // Once there are not remaining modules to be loaded, the root module can be
  // evaluated since all its dependencies are compiled and ready to be consumed.
  unsigned int num_unloaded_modules_ = 0;

  // Module identifier to Module object.
  std::map<std::string, v8::Global<v8::Module>> identifier_to_module_map_;

  using ModuleToIdentifierMap =
      std::unordered_map<v8::Global<v8::Module>, std::string, ModuleGlobalHash>;
  std::unique_ptr<ModuleToIdentifierMap> module_to_identifier_map_;

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
  void ConfigureAutoclick(mojom::AccessibilityServiceClient* ax_service_client);
  void ConfigureAutomation(
      mojom::AccessibilityServiceClient* ax_service_client,
      mojo::PendingAssociatedReceiver<mojom::Automation> automation);
  void ConfigureOSState();
  void ConfigureSpeechRecognition(
      mojom::AccessibilityServiceClient* ax_service_client);
  void ConfigureTts(mojom::AccessibilityServiceClient* ax_service_client);
  void ConfigureUserInput(mojom::AccessibilityServiceClient* ax_service_client);
  void ConfigureUserInterface(
      mojom::AccessibilityServiceClient* ax_service_client);

  // |file_loader_remote| must outlive this object.
  void ConfigureFileLoader(
      mojo::Remote<mojom::AccessibilityFileLoader>* file_loader_remote);

  void FinishContextSetUp();

  // Instructs V8Environment to create a devtools agent.
  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent);

  // Called by V8Environment when JS wants to bind a Mojo interface.
  void BindInterface(const std::string& interface_name,
                     mojo::GenericPendingReceiver pending_receiver);

  // Executes the module at |file_path|, invoking |callback| when the operation
  // is done.
  void ExecuteModule(base::FilePath file_path, base::OnceClosure on_complete);

  // Loads the file at |path|, and invokes |callback| once that is done. Note
  // that |callback| is wrapped with a base::SequenceBoundCallback, which causes
  // the callback to be invoked in the sequence in which the
  // base::SequenceBoundCallback was constructed.
  // Caller is responsible for checking if the resulting base::File is valid.
  void LoadFile(base::FilePath path,
                base::SequenceBound<V8Environment::OnFileLoadedCallback>
                    sequence_bound_callback);

  // Allow tests to expose additional Mojo interfaces to JS.
  void AddInterfaceForTest(std::unique_ptr<InterfaceBinder> interface_binder);
  void RunScriptForTest(const std::string& script,
                        base::OnceClosure on_complete);

  // The files added here will be returned in fifo order in response to calls to
  // |LoadFile()|.
  void AddFileForTest(base::File file) {
    files_for_test_.push(std::move(file));
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  base::SequenceBound<V8Environment> v8_env_;
  // The Mojo interfaces that are exposed to JS. When JS wants to bind a Mojo
  // interface, the first matching InterfaceBinder will be used.
  std::vector<std::unique_ptr<InterfaceBinder>> interface_binders_;

  // Interface used to load files.
  raw_ptr<mojo::Remote<mojom::AccessibilityFileLoader>> file_loader_remote_;

  std::queue<base::File> files_for_test_;

  base::WeakPtrFactory<V8Manager> weak_factory_{this};
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_V8_MANAGER_H_
