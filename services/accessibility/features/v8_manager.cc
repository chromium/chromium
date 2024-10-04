// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/accessibility/features/v8_manager.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "gin/converter.h"
#include "gin/function_template.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "services/accessibility/assistive_technology_controller_impl.h"
#include "services/accessibility/features/autoclick_client_interface_binder.h"
#include "services/accessibility/features/automation_client_interface_binder.h"
#include "services/accessibility/features/automation_internal_bindings.h"
#include "services/accessibility/features/devtools/os_devtools_agent.h"
#include "services/accessibility/features/interface_binder.h"
#include "services/accessibility/features/mojo/mojo.h"
#include "services/accessibility/features/speech_recognition_interface_binder.h"
#include "services/accessibility/features/sync_os_state_api_bindings.h"
#include "services/accessibility/features/tts_interface_binder.h"
#include "services/accessibility/features/user_input_interface_binder.h"
#include "services/accessibility/features/user_interface_interface_binder.h"
#include "services/accessibility/features/v8_bindings_utils.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom-forward.h"
#include "services/accessibility/public/mojom/file_loader.mojom.h"
#include "v8-data.h"
#include "v8-exception.h"
#include "v8-local-handle.h"
#include "v8-message.h"
#include "v8-microtask-queue.h"
#include "v8-persistent-handle.h"
#include "v8-platform.h"
#include "v8-primitive.h"
#include "v8-promise.h"
#include "v8-script.h"
#include "v8-value.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-template.h"

namespace ax {
namespace {

using v8::Context;
using v8::Local;
using v8::MaybeLocal;
using v8::Module;
using v8::ModuleRequest;

// The index into the global template's internal fields that
// stores a pointer to this V8Environment. This allows any class with
// a v8::Context to access this V8Environment.
static const int kV8ContextWrapperIndex = 0;

// Initial size of the module map to store modules.
static const size_t kModuleMapSize = 10;

// Returns the directory part of `path` and removes the trailing '/'.
std::string DirName(const std::string& path) {
  size_t last_slash = path.find_last_of('/');
  CHECK(last_slash != std::string::npos) << "Path must contain at least one /";
  return path.substr(0, last_slash);
}

std::string ToSTLString(v8::Isolate* isolate, Local<v8::String> v8_str) {
  v8::String::Utf8Value utf8(isolate, v8_str);
  CHECK(*utf8);
  return *utf8;
}

// Callback used during module instantiation. Please see
// v8::Module::Instantiate.
MaybeLocal<Module> ResolveModuleCallback(
    Local<Context> context,
    Local<v8::String> specifier,
    Local<v8::FixedArray> import_assertions,
    Local<v8::Module> referrer) {
  V8Environment* v8_env = V8Environment::GetFromContext(context);
  CHECK(v8_env);

  v8::Isolate* isolate = context->GetIsolate();
  std::optional<std::string> referrer_identifier =
      v8_env->GetIdentifierFromModule(v8::Global<Module>(isolate, referrer));
  CHECK(referrer_identifier.has_value());
  std::string normalized_identifier = V8Environment::NormalizeRelativePath(
      ToSTLString(isolate, specifier), DirName(*referrer_identifier));
  return v8_env->GetModuleFromIdentifier(normalized_identifier);
}

}  // namespace

void V8Environment::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent) {
  if (!devtools_agent_) {
    devtools_agent_ =
        std::make_unique<OSDevToolsAgent>(*this, std::move(main_runner_));
  }
  devtools_agent_->Connect(std::move(agent));
}

// static
base::SequenceBound<V8Environment> V8Environment::Create(
    base::WeakPtr<V8Manager> manager) {
  // Create task runner for running V8. The Isolate should only ever be accessed
  // on this thread.
  auto v8_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  // Get a reference to the current SequencedTaskRunner for posting tasks back
  // to the constructor and current thread.
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  auto main_runner = base::SequencedTaskRunner::GetCurrentDefault();
  base::SequenceBound<V8Environment> result(
      std::move(v8_runner), std::move(main_runner), std::move(manager));
  return result;
}

// static
V8Environment* V8Environment::GetFromContext(Local<Context> context) {
  Local<v8::Object> global_proxy = context->Global();
  CHECK_LT(kV8ContextWrapperIndex, global_proxy->InternalFieldCount());
  V8Environment* v8_env = reinterpret_cast<V8Environment*>(
      global_proxy->GetAlignedPointerFromInternalField(kV8ContextWrapperIndex));
  CHECK(v8_env);
  return v8_env;
}

V8Environment::V8Environment(
    scoped_refptr<base::SequencedTaskRunner> main_runner,
    base::WeakPtr<V8Manager> manager)
    : main_runner_(std::move(main_runner)), manager_(std::move(manager)) {
  CreateIsolate();
  module_to_identifier_map_ = std::make_unique<ModuleToIdentifierMap>(
      kModuleMapSize, ModuleGlobalHash(isolate_holder_->isolate()));
}

V8Environment::~V8Environment() {
  if (!isolate_holder_) {
    return;
  }

  NotifyIsolateWillDestroy();
  isolate_holder_->isolate()->TerminateExecution();

  // These maps must be destroyed before context and isolate because they hold
  // references to global objects.
  devtools_agent_.reset();
  identifier_to_module_map_.clear();
  module_to_identifier_map_.reset();

  context_holder_.reset();
  isolate_holder_.reset();
}

void V8Environment::InstallAutomation(
    mojo::PendingAssociatedReceiver<mojom::Automation> automation) {
  automation_bindings_ =
      std::make_unique<AutomationInternalBindings>(this, std::move(automation));
}

void V8Environment::InstallOSState() {
  os_state_needed_ = true;
}

void V8Environment::ExecuteScript(const std::string& script,
                                  base::OnceCallback<void()> on_complete) {
  bool result = BindingsIsolateHolder::ExecuteScriptInContext(script);
  DCHECK(result);
  // FIXME: For callers, it might be nice to bounce back to the calling thread
  // rather than exposing V8Environment's internal implementation thread.
  std::move(on_complete).Run();
}

void V8Environment::ExecuteModule(base::FilePath file_path,
                                  base::OnceClosure on_complete) {
  CHECK(!module_evaluation_in_progress_)
      << "A module is already being evaluated by this instance.";
  module_evaluation_in_progress_ = true;
  on_complete_ = std::move(on_complete);
  CHECK(num_unloaded_modules_ == 0);
  num_unloaded_modules_++;  // Root module is still unloaded.
  root_module_identifier_ = file_path.MaybeAsASCII();
  CHECK(!root_module_identifier_->empty());
  RequestModuleContents(file_path);
}

void V8Environment::RequestModuleContents(base::FilePath file_path) {
  std::string module_identifier = file_path.MaybeAsASCII();
  CHECK(!module_identifier.empty());

  // This callback will be wrapped with a base::SequenceBound so that it can be
  // invoked by the manager sequence, but will run in this sequence.
  OnFileLoadedCallback callback =
      base::BindOnce(&V8Environment::OnFileLoaded, base::Unretained(this),
                     std::move(module_identifier));
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V8Manager::LoadFile, manager_, file_path,
                     base::SequenceBound<OnFileLoadedCallback>(
                         base::SingleThreadTaskRunner::GetCurrentDefault(),
                         std::move(callback))));
}

void V8Environment::OnFileLoaded(std::string module_identifier,
                                 base::File file) {
  v8::Isolate::Scope isolate_scope(GetIsolate());
  v8::HandleScope handle_scope(GetIsolate());
  Local<Context> context = GetContext();
  Context::Scope context_scope(context);
  v8::TryCatch trycatch(GetIsolate());

  if (!file.IsValid()) {
    // TODO(b:314185597): Handle modules that fail to be evaluated.
    return HandleModuleError("file is not valid.");
  }

  std::string data;
  data.resize(file.GetLength());
  int bytes_read = file.Read(0, data.data(), file.GetLength());
  if (bytes_read != file.GetLength()) {
    // TODO(b:314185597): Handle modules that fail to be evaluated.
    return HandleModuleError("Error reading file.");
  }

  Local<v8::String> source_text =
      v8::String::NewFromUtf8(GetIsolate(), data.c_str(),
                              v8::NewStringType::kNormal, data.size())
          .ToLocalChecked();

  Local<v8::String> resource_name =
      v8::String::NewFromUtf8(GetIsolate(), module_identifier.c_str())
          .ToLocalChecked();

  v8::ScriptOrigin origin(
      resource_name, /*resource_line_offset =*/0,
      /*resource_column_offset=*/0, /*resource_is_shared_cross_origin=*/false,
      /*script_id=*/-1,
      /*source_map_url=*/Local<v8::Value>(), /*resource_is_opaque=*/false,
      /*is_wasm=*/false, /*is_module=*/true);
  v8::ScriptCompiler::Source source(source_text, origin);
  Local<Module> module;
  if (!v8::ScriptCompiler::CompileModule(GetIsolate(), &source)
           .ToLocal(&module)) {
    CHECK(trycatch.HasCaught());
    // TODO(b:314185597): Handle modules that fail to be evaluated.
    return HandleModuleError(ExceptionToString(trycatch));
  }
  num_unloaded_modules_--;

  const std::string module_directory_name = DirName(module_identifier);
  CHECK(identifier_to_module_map_
            .insert(std::make_pair(module_identifier,
                                   v8::Global<Module>(GetIsolate(), module)))
            .second);
  CHECK(module_to_identifier_map_
            ->insert(std::make_pair(v8::Global<Module>(GetIsolate(), module),
                                    module_identifier))
            .second);

  Local<v8::FixedArray> module_requests = module->GetModuleRequests();
  for (int i = 0, length = module_requests->Length(); i < length; ++i) {
    Local<ModuleRequest> module_request =
        module_requests->Get(context, i).As<ModuleRequest>();
    Local<v8::String> v8_specifier = module_request->GetSpecifier();
    std::string specifier = ToSTLString(GetIsolate(), v8_specifier);

    std::string normalized_identifier =
        NormalizeRelativePath(specifier, module_directory_name);
    auto it = identifier_to_module_map_.find(normalized_identifier);
    if (it == identifier_to_module_map_.end()) {
      num_unloaded_modules_++;
      RequestModuleContents(base::FilePath(normalized_identifier));
    }
  }

  if (num_unloaded_modules_ == 0) {
    EvaluateModule();
  }
}

void V8Environment::EvaluateModule() {
  CHECK(module_evaluation_in_progress_ && num_unloaded_modules_ == 0);
  v8::HandleScope handle_scope(GetIsolate());
  Local<Context> context = GetContext();
  Context::Scope context_scope(context);
  v8::TryCatch trycatch(GetIsolate());

  auto root_module_it =
      identifier_to_module_map_.find(*root_module_identifier_);
  CHECK(root_module_it != identifier_to_module_map_.end());
  Local<Module> root_module = root_module_it->second.Get(GetIsolate());

  MaybeLocal<v8::Value> maybe_result;
  if (root_module->InstantiateModule(context, ResolveModuleCallback)
          .FromMaybe(false)) {
    CHECK_EQ(v8::Module::kInstantiated, root_module->GetStatus());
    maybe_result = root_module->Evaluate(context);
    CHECK(!maybe_result.IsEmpty());
  } else {
    CHECK(trycatch.HasCaught());
    // TODO(b:314185597): Handle modules that fail to be evaluated.
    return HandleModuleError(ExceptionToString(trycatch));
  }
  Local<v8::Value> value;
  Local<v8::Promise> promise;
  if (maybe_result.ToLocal(&value)) {
    promise = value.As<v8::Promise>();
  } else {
    CHECK(trycatch.HasCaught());
    // TODO(b:314185597): Handle modules that fail to be evaluated.
    return HandleModuleError(ExceptionToString(trycatch));
  }

  // TODO(b:314187876): check for background tasks and run them.
  while (promise->State() == v8::Promise::kPending) {
    context->GetMicrotaskQueue()->PerformCheckpoint(GetIsolate());
  }

  // TODO(b:314185597): Handle modules that fail to be evaluated or promises
  // that are rejected.
  CHECK_EQ(v8::Module::kEvaluated, root_module->GetStatus());

  if (promise->State() == v8::Promise::kFulfilled) {
    module_evaluation_in_progress_ = false;
    root_module_identifier_ = std::nullopt;
    main_runner_->PostTask(FROM_HERE, std::move(on_complete_));
  } else {
    // TODO(b:314185597): Handle modules that fail to be evaluated.
    return HandleModuleError(
        "Promise is still not fullfilled after module evaluation.");
  }
}

void V8Environment::HandleModuleError(const std::string& message) {
  module_evaluation_in_progress_ = false;
  num_unloaded_modules_ = 0;
  root_module_identifier_ = std::nullopt;
  HandleError(message);
}

// static
std::string V8Environment::NormalizeRelativePath(
    const std::string& relative_path,
    const std::string& base_dir) {
  CHECK(!(!relative_path.empty() && relative_path[0] == '/'))
      << "Relative path can't be an absolute path.";
  CHECK(!base_dir.empty())
      << "The base directory to resolve relative path can't be empty.";
  CHECK(*base_dir.rbegin() != '/')
      << "The base directory name can't end with a /.";
  const int base_dir_depth =
      1 + std::count(base_dir.begin(), base_dir.end(), '/');

  std::string path = base_dir + '/' + relative_path;
  std::vector<std::string> parts;
  std::istringstream part_stream(path);
  std::string part;
  int relative_path_parents = 0;
  while (std::getline(part_stream, part, '/')) {
    if (part == "..") {
      relative_path_parents++;
      CHECK(relative_path_parents <= base_dir_depth)
          << "The relative path can't reference a parent of the base "
             "directory.";
      if (!parts.empty()) {
        parts.pop_back();
      }
    } else if (part != ".") {
      parts.push_back(part);
    }
  }

  std::ostringstream os;

  // At least we need the file name and some directory, since `base_dir` can't
  // be empty.
  CHECK(parts.size() > 1);
  std::copy(parts.begin(), parts.end() - 1,
            std::ostream_iterator<std::string>(os, "/"));

  // Copies the file name potion of the path.
  os << *parts.rbegin();
  return os.str();
}

MaybeLocal<Module> V8Environment::GetModuleFromIdentifier(
    const std::string& identifier) {
  auto it = identifier_to_module_map_.find(identifier);
  if (it != identifier_to_module_map_.end()) {
    return it->second.Get(GetIsolate());
  }

  // This function is used in the callback to obtain an imported module once a
  // module is being instantiated. Returning an empty `MaybeLocal` indicates
  // that an error occurred, but allows the `Instantiate` function to finish, at
  // the same time throwing the appropriate js error.
  return MaybeLocal<Module>();
}

std::optional<std::string> V8Environment::GetIdentifierFromModule(
    v8::Global<v8::Module> module) {
  auto identifier_it = module_to_identifier_map_->find(module);
  if (identifier_it != module_to_identifier_map_->end()) {
    return identifier_it->second;
  }
  return std::nullopt;
}

v8::Isolate* V8Environment::GetIsolate() const {
  return isolate_holder_ ? isolate_holder_->isolate() : nullptr;
}

Local<Context> V8Environment::GetContext() const {
  return context_holder_->context();
}

void V8Environment::BindInterface(
    const std::string& interface_name,
    mojo::GenericPendingReceiver pending_receiver) {
  main_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V8Manager::BindInterface, manager_,
                                interface_name, std::move(pending_receiver)));
}

void V8Environment::CreateIsolate() {
  std::unique_ptr<v8::Isolate::CreateParams> params =
      gin::IsolateHolder::getDefaultIsolateParams();
  isolate_holder_ = std::make_unique<gin::IsolateHolder>(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      gin::IsolateHolder::kSingleThread,
      gin::IsolateHolder::IsolateType::kUtility, std::move(params));
}

void V8Environment::AddV8Bindings() {
  CHECK(isolate_holder_) << "V8 has not been started, cannot bind.";

  v8::Isolate* isolate = isolate_holder_->isolate();

  // Enter isolate scope.
  v8::Isolate::Scope isolate_scope(isolate);

  // Creates and enters stack-allocated handle scope.
  // All the Local handles (Local<>) in this function will belong to this
  // HandleScope and will be garbage collected when it goes out of scope in this
  // C++ function.
  v8::HandleScope handle_scope(isolate);

  // Create a template for the global object where we set the
  // built-in global functions.
  Local<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New(isolate);
  global_template->SetInternalFieldCount(kV8ContextWrapperIndex + 1);

  // Create a template for the global "chrome" object.
  // We add this because APIs are found in the chrome namespace in JS,
  // like chrome.automation, etc.
  Local<v8::ObjectTemplate> chrome_template = v8::ObjectTemplate::New(isolate);
  global_template->Set(isolate, "chrome", chrome_template);

  // Add automation bindings if needed.
  if (automation_bindings_) {
    Local<v8::ObjectTemplate> automation_template =
        v8::ObjectTemplate::New(isolate);
    automation_bindings_->AddAutomationRoutesToTemplate(&automation_template);
    global_template->Set(isolate, "nativeAutomationInternal",
                         automation_template);
  }

  // Add chrome.runtime.
  v8::Local<v8::ObjectTemplate> runtime_template =
      v8::ObjectTemplate::New(isolate);
  chrome_template->Set(isolate, "runtime", runtime_template);

  // Adds atpconsole.log/warn/error.
  // TODO(crbug.com/1355633): Deprecate and use console.log/warn/error instead.
  BindingsUtils::AddAtpConsoleTemplate(isolate, global_template);

  // Add TextEncoder and TextDecoder, which are used by Mojo to pass strings,
  // as well as some Accessibility features.
  BindingsUtils::AddCallHandlerToTemplate(
      isolate, global_template, "TextEncoder",
      BindingsUtils::CreateTextEncoderCallback);
  BindingsUtils::AddCallHandlerToTemplate(
      isolate, global_template, "TextDecoder",
      BindingsUtils::CreateTextDecoderCallback);

  // TODO(crbug.com/1355633): Add other API bindings to the global template.
  if (os_state_needed_) {
    Local<v8::ObjectTemplate> sync_os_state_template =
        v8::ObjectTemplate::New(isolate);
    sync_os_state_template->Set(
        GetIsolate(), "getDisplayNameForLocale",
        gin::CreateFunctionTemplate(
            GetIsolate(), base::BindRepeating(&GetDisplayNameForLocale)));
    chrome_template->Set(GetIsolate(), "syncOSState", sync_os_state_template);
  }

  // Add the global template to the current context.
  Local<Context> context =
      Context::New(isolate, /*extensions=*/nullptr, global_template);
  context_holder_ = std::make_unique<gin::ContextHolder>(isolate);
  context_holder_->SetContext(context);

  // Make a pointer to `this` in the current context.
  // This allows Mojo to use the context to find `this` and bind interfaces
  // on it.
  Local<v8::Object> global_proxy = context->Global();
  DCHECK_EQ(kV8ContextWrapperIndex + 1, global_proxy->InternalFieldCount());

  global_proxy->SetAlignedPointerInInternalField(kV8ContextWrapperIndex, this);

  // Create a template for the "Mojo" object in the context scope.
  Context::Scope context_scope(context);
  gin::Handle<Mojo> mojo = Mojo::Create(context);
  global_proxy->Set(context, gin::StringToV8(isolate, "Mojo"), mojo.ToV8())
      .Check();

  // Initialize self as the global template. This will allow mojo to work with
  // modules without a shim.
  global_proxy->Set(context, gin::StringToV8(isolate, "self"), global_proxy)
      .Check();

  // TODO(crbug.com/1355633): At this point we could load in API Javascript
  // using ExecuteScript.
}

V8Manager::V8Manager() {
  // Note: this can't be in the initializer list because WeakPtrFactory fields
  // must be declared after all other fields.
  v8_env_ = V8Environment::Create(weak_factory_.GetWeakPtr());
}

V8Manager::~V8Manager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void V8Manager::ConfigureAutomation(
    mojom::AccessibilityServiceClient* ax_service_client,
    mojo::PendingAssociatedReceiver<mojom::Automation> automation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  v8_env_.AsyncCall(&V8Environment::InstallAutomation)
      .WithArgs(std::move(automation));
  interface_binders_.push_back(
      std::make_unique<AutomationClientInterfaceBinder>(ax_service_client));
}

void V8Manager::ConfigureAutoclick(
    mojom::AccessibilityServiceClient* ax_service_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/262637071): load the Autoclick JS shim into V8 using
  // v8_env_.AsyncCall.
  interface_binders_.push_back(
      std::make_unique<AutoclickClientInterfaceBinder>(ax_service_client));
}

void V8Manager::ConfigureSpeechRecognition(
    mojom::AccessibilityServiceClient* ax_service_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  interface_binders_.push_back(
      std::make_unique<SpeechRecognitionInterfaceBinder>(ax_service_client));
}

void V8Manager::ConfigureOSState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  v8_env_.AsyncCall(&V8Environment::InstallOSState);
}

void V8Manager::ConfigureTts(
    mojom::AccessibilityServiceClient* ax_service_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/262637071): load the TTS JS shim into V8 using v8_env_.AsyncCall.
  interface_binders_.push_back(
      std::make_unique<TtsInterfaceBinder>(ax_service_client));
}

void V8Manager::ConfigureUserInput(
    mojom::AccessibilityServiceClient* ax_service_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/262637071): load the AccessibilityPrivate JS shim into V8 using
  // v8_env_.AsyncCall if it isn't already loaded.
  interface_binders_.push_back(
      std::make_unique<UserInputInterfaceBinder>(ax_service_client));
}

void V8Manager::ConfigureUserInterface(
    mojom::AccessibilityServiceClient* ax_service_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/262637071): load the AccessibilityPrivate JS shim into V8 using
  // v8_env_.AsyncCall if it isn't already loaded.
  interface_binders_.push_back(
      std::make_unique<UserInterfaceInterfaceBinder>(ax_service_client));
}

void V8Manager::ConfigureFileLoader(
    mojo::Remote<mojom::AccessibilityFileLoader>* file_loader_remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(file_loader_remote);
  file_loader_remote_ = file_loader_remote;
}

void V8Manager::FinishContextSetUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  v8_env_.AsyncCall(&V8Environment::AddV8Bindings);
}

// Instructs V8Environment to create a devtools agent.
void V8Manager::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  v8_env_.AsyncCall(&V8Environment::ConnectDevToolsAgent)
      .WithArgs(std::move(agent));
}

void V8Manager::AddInterfaceForTest(
    std::unique_ptr<InterfaceBinder> interface_binder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  interface_binders_.push_back(std::move(interface_binder));
}

void V8Manager::BindInterface(const std::string& interface_name,
                              mojo::GenericPendingReceiver pending_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b:262637071): Add mappings for bindings for other C++/JS mojom APIs.
  // TODO(b:262637071): We may need to use associated remotes/receivers to avoid
  // messages coming in an unpredicted order compared to the Extensions system.
  for (auto& binder : interface_binders_) {
    if (binder->MatchesInterface(interface_name)) {
      binder->BindReceiver(std::move(pending_receiver));
      return;
    }
  }
  LOG(ERROR) << "Couldn't find Receiver for interface " << interface_name
             << ". Ensure it's installed in the isolate with "
             << "V8Manager::AddInterface and available for binding in "
             << "V8Manager::BindInterface.";
}

void V8Manager::RunScriptForTest(const std::string& script,
                                 base::OnceClosure on_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  v8_env_.AsyncCall(&V8Environment::ExecuteScript)
      .WithArgs(script, std::move(on_complete));
}

void V8Manager::ExecuteModule(base::FilePath file_path,
                              base::OnceClosure on_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  v8_env_.AsyncCall(&V8Environment::ExecuteModule)
      .WithArgs(file_path, std::move(on_complete));
}

void V8Manager::LoadFile(
    base::FilePath path,
    base::SequenceBound<V8Environment::OnFileLoadedCallback>
        sequence_bound_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note: the callback will be invoked in the sequence of the V8Environment,
  // due to the base::SequenceBoundCallback.
  ax::mojom::AccessibilityFileLoader::LoadCallback callback = base::BindOnce(
      [](base::SequenceBound<V8Environment::OnFileLoadedCallback>
             sequence_bound_callback,
         base::File file) {
        sequence_bound_callback.PostTaskWithThisObject(base::BindOnce(
            [](base::File file, V8Environment::OnFileLoadedCallback* callback) {
              std::move(*callback).Run(std::move(file));
            },
            std::move(file)));
      },
      std::move(sequence_bound_callback));
  if (!files_for_test_.empty()) {
    base::File file = std::move(files_for_test_.front());
    files_for_test_.pop();
    std::move(callback).Run(std::move(file));
    return;
  }

  (*file_loader_remote_)->Load(path, std::move(callback));
}

}  // namespace ax
