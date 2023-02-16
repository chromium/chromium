// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/native_extension_bindings_system.h"

#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "components/crx_file/id_util.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/content_capabilities_handler.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/renderer/api/declarative_content_hooks_delegate.h"
#include "extensions/renderer/api/dom_hooks_delegate.h"
#include "extensions/renderer/api/feedback_private_hooks_delegate.h"
#include "extensions/renderer/api/i18n_hooks_delegate.h"
#include "extensions/renderer/api/runtime_hooks_delegate.h"
#include "extensions/renderer/api/web_request_hooks.h"
#include "extensions/renderer/api_activity_logger.h"
#include "extensions/renderer/bindings/api_binding_bridge.h"
#include "extensions/renderer/bindings/api_binding_hooks.h"
#include "extensions/renderer/bindings/api_binding_js_util.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/chrome_setting.h"
#include "extensions/renderer/console.h"
#include "extensions/renderer/content_setting.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extension_interaction_provider.h"
#include "extensions/renderer/extension_js_runner.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set_iterable.h"
#include "extensions/renderer/storage_area.h"
#include "extensions/renderer/worker_thread_util.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "gin/per_context_data.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-template.h"

namespace extensions {

namespace {

const char kBindingsSystemPerContextKey[] = "extension_bindings_system";

// Returns true if the given |api| is a "prefixed" api of the |root_api|; that
// is, if the api begins with the root.
// For example, 'app.runtime' is a prefixed api of 'app'.
// This is designed to be used as a utility when iterating over a sorted map, so
// assumes that |api| is lexicographically greater than |root_api|.
bool IsPrefixedAPI(base::StringPiece api, base::StringPiece root_api) {
  DCHECK_NE(api, root_api);
  DCHECK_GT(api, root_api);
  return base::StartsWith(api, root_api, base::CompareCase::SENSITIVE) &&
         api[root_api.size()] == '.';
}

// Returns the first different level of the api specification between the given
// |api_name| and |reference|. For an api_name of 'app.runtime' and a reference
// of 'app', this returns 'app.runtime'. For an api_name of
// 'cast.streaming.session' and a reference of 'cast', this returns
// 'cast.streaming'. If reference is empty, this simply returns the first layer;
// so given 'app.runtime' and no reference, this returns 'app'.
base::StringPiece GetFirstDifferentAPIName(base::StringPiece api_name,
                                           base::StringPiece reference) {
  base::StringPiece::size_type dot =
      api_name.find('.', reference.empty() ? 0 : reference.size() + 1);
  if (dot == base::StringPiece::npos)
    return api_name;
  return api_name.substr(0, dot);
}

struct BindingsSystemPerContextData : public base::SupportsUserData::Data {
  BindingsSystemPerContextData(
      base::WeakPtr<NativeExtensionBindingsSystem> bindings_system)
      : bindings_system(bindings_system) {}
  ~BindingsSystemPerContextData() override {}

  v8::Global<v8::Object> api_object;
  v8::Global<v8::Object> internal_apis;
  base::WeakPtr<NativeExtensionBindingsSystem> bindings_system;
};

// If a 'chrome' property exists on the context's global and is an object,
// returns that.
// If a 'chrome' property exists but isn't an object, returns an empty Local.
// If no 'chrome' property exists (or is undefined), creates a new
// object, assigns it to Global().chrome, and returns it.
v8::Local<v8::Object> GetOrCreateChrome(v8::Local<v8::Context> context) {
  // Ensure that the creation context for any new chrome object is |context|.
  v8::Context::Scope context_scope(context);

  // TODO(devlin): This is a little silly. We expect that this may do the wrong
  // thing if the window has set some other 'chrome' (as in the case of script
  // doing 'window.chrome = true'), but we don't really handle it. It could also
  // throw exceptions or have unintended side effects.
  // On the one hand, anyone writing that code is probably asking for trouble.
  // On the other, it'd be nice to avoid. I wonder if we can?
  v8::Local<v8::String> chrome_string =
      gin::StringToSymbol(context->GetIsolate(), "chrome");
  v8::Local<v8::Value> chrome_value;
  if (!context->Global()->Get(context, chrome_string).ToLocal(&chrome_value))
    return v8::Local<v8::Object>();

  v8::Local<v8::Object> chrome_object;
  if (chrome_value->IsUndefined()) {
    chrome_object = v8::Object::New(context->GetIsolate());
    v8::Maybe<bool> success = context->Global()->CreateDataProperty(
        context, chrome_string, chrome_object);
    if (!success.IsJust() || !success.FromJust())
      return v8::Local<v8::Object>();
  } else if (chrome_value->IsObject()) {
    v8::Local<v8::Object> obj = chrome_value.As<v8::Object>();
    // The creation context of the `chrome` property could be different if a
    // different context (such as the parent of an about:blank iframe) assigned
    // it. Since in this case we know that the chrome object is not the one we
    // created, do not use it for bindings. This also avoids weirdness of having
    // bindings created in one context stored on a chrome object from another.
    // TODO(devlin): There might be a way of detecting if the browser created
    // the chrome object. For instance, we could add a v8::Private to the
    // chrome object we construct, and check if it's present. Unfortunately, we
    // need to a) track down each place we create the chrome object (it's not
    // just in extensions) and also see how much that would break.
    if (obj->GetCreationContextChecked() == context)
      chrome_object = obj;
  }

  return chrome_object;
}

BindingsSystemPerContextData* GetBindingsDataFromContext(
    v8::Local<v8::Context> context) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  if (!per_context_data || !binding::IsContextValid(context))
    return nullptr;  // Context is shutting down.

  auto* data = static_cast<BindingsSystemPerContextData*>(
      per_context_data->GetUserData(kBindingsSystemPerContextKey));
  CHECK(data);
  if (!data->bindings_system) {
    NOTREACHED() << "Context outlived bindings system.";
    return nullptr;
  }

  return data;
}

void AddConsoleError(v8::Local<v8::Context> context, const std::string& error) {
  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  // Note: |script_context| may be null. During context tear down, we remove the
  // script context from the ScriptContextSet, so it's not findable by
  // GetScriptContextFromV8Context. In theory, we shouldn't be running any
  // bindings code after this point, but it seems that we are in at least some
  // places.
  // TODO(devlin): Investigate. At least one place this manifests is in
  // messaging binding tear down exhibited by
  // MessagingApiTest.MessagingBackgroundOnly.
  // console::AddMessage() can handle a null script context.
  console::AddMessage(script_context, blink::mojom::ConsoleMessageLevel::kError,
                      error);
}

// Returns the API schema indicated by |api_name|.
const base::Value::Dict& GetAPISchema(const std::string& api_name) {
  const base::Value::Dict* schema =
      ExtensionAPI::GetSharedInstance()->GetSchema(api_name);
  CHECK(schema) << api_name;
  return *schema;
}

// Returns true if the feature specified by |name| is available to the given
// |context|.
bool IsAPIFeatureAvailable(v8::Local<v8::Context> context,
                           const std::string& name) {
  ScriptContext* script_context = GetScriptContextFromV8ContextChecked(context);
  return script_context->GetAvailability(name).is_available();
}

// Returns true if the specified |context| is allowed to use promise based
// returns from APIs.
bool ArePromisesAllowed(v8::Local<v8::Context> context) {
  ScriptContext* script_context = GetScriptContextFromV8ContextChecked(context);
  const Extension* extension = script_context->extension();
  return (extension && extension->manifest_version() >= 3) ||
         script_context->context_type() == Feature::WEBUI_CONTEXT;
}

// Instantiates the binding object for the given |name|. |name| must specify a
// specific feature.
v8::Local<v8::Object> CreateRootBinding(v8::Local<v8::Context> context,
                                        ScriptContext* script_context,
                                        const std::string& name,
                                        APIBindingsSystem* bindings_system) {
  APIBindingHooks* hooks = nullptr;
  v8::Local<v8::Object> binding_object =
      bindings_system->CreateAPIInstance(name, context, &hooks);

  gin::Handle<APIBindingBridge> bridge_handle = gin::CreateHandle(
      context->GetIsolate(),
      new APIBindingBridge(hooks, context, binding_object,
                           script_context->GetExtensionID(),
                           script_context->GetContextTypeDescription()));
  v8::Local<v8::Value> native_api_bridge = bridge_handle.ToV8();
  script_context->module_system()->OnNativeBindingCreated(name,
                                                          native_api_bridge);

  return binding_object;
}

// Creates the binding object for the given |root_name|. This can be
// complicated, since APIs may have prefixed names, like 'app.runtime' or
// 'system.cpu'. This method accepts the first name (i.e., the key that we are
// looking for on the chrome object, such as 'app') and returns the fully
// instantiated binding, including prefixed APIs. That is, given 'app', this
// will instantiate 'app', 'app.runtime', and 'app.window'.
//
// NOTE(devlin): We could do the prefixed apis lazily; however, it's not clear
// how much of a win it would be. It's less overhead here than in the general
// case (instantiating a handful of APIs instead of all of them), and it's more
// likely they will be used (since the extension is already accessing the
// parent).
// TODO(devlin): We should be creating ObjectTemplates for these so that we only
// do this work once. APIBindings (for the single API) already do this.
v8::Local<v8::Object> CreateFullBinding(
    v8::Local<v8::Context> context,
    ScriptContext* script_context,
    APIBindingsSystem* bindings_system,
    const FeatureProvider* api_feature_provider,
    const std::string& root_name) {
  const FeatureMap& features = api_feature_provider->GetAllFeatures();
  auto lower = features.lower_bound(root_name);
  DCHECK(lower != features.end());

  // Some bindings have a prefixed name, like app.runtime, where 'app' and
  // 'app.runtime' are, in fact, separate APIs. It's also possible for a
  // context to have access to 'app.runtime', but not to 'app'. For this, we
  // either instantiate the 'app' binding fully (if the context has access), or
  // else use an empty object (so we can still instantiate 'app.runtime').
  v8::Local<v8::Object> root_binding;
  if (lower->first == root_name) {
    const Feature* feature = lower->second.get();
    if (script_context->IsAnyFeatureAvailableToContext(
            *feature, CheckAliasStatus::NOT_ALLOWED)) {
      // If this feature is an alias for a different API, use the other binding
      // as the basis for the API contents.
      const std::string& source_name =
          feature->source().empty() ? root_name : feature->source();
      root_binding = CreateRootBinding(context, script_context, source_name,
                                       bindings_system);
    }
    ++lower;
  }

  // Look for any bindings that would be on the same object. Any of these would
  // start with the same base name (e.g. 'app') + '.' (since '.' is < x for any
  // isalpha(x)).
  std::string upper = root_name + static_cast<char>('.' + 1);
  base::StringPiece last_binding_name;
  // The following loop is a little painful because we have crazy binding names
  // and syntaxes. The way this works is as follows:
  // Look at each feature after the root feature we passed in. If there exists
  // a (non-child) feature with a prefixed name, create the full binding for
  // the object that the next feature is on. Then, iterate past any features
  // already instantiated by that, and continue until there are no more features
  // prefixed by the root API.
  // As a concrete example, we can look at the cast APIs (cast and
  // cast.streaming.*)
  // Start with vanilla 'cast', and instantiate that.
  // Then iterate over features, and see 'cast.streaming.receiverSession'.
  // 'cast.streaming.receiverSession' is a prefixed API of 'cast', but we find
  // the first level of difference, which is 'cast.streaming', and instantiate
  // that object completely (through recursion).
  // The next feature is 'cast.streaming.rtpStream', but this is a prefixed API
  // of 'cast.streaming', which we just instantiated completely (including
  // 'cast.streaming.rtpStream'), so we continue.
  // Iterate until all cast.* features are created.
  // TODO(devlin): This is bonkers, but what's the better way? We could extract
  // this out to be a more readable Visitor implementation, but is it worth it
  // for this one place? Ideally, we'd have a less convoluted feature
  // representation (some kind of tree would make this trivial), but for now, we
  // have strings.
  // On the upside, most APIs are not prefixed at all, and this loop is never
  // entered.
  for (auto iter = lower; iter != features.end() && iter->first < upper;
       ++iter) {
    if (iter->second->IsInternal())
      continue;

    if (IsPrefixedAPI(iter->first, last_binding_name)) {
      // Instantiating |last_binding_name| must have already instantiated
      // iter->first.
      continue;
    }

    // If this API has a parent feature (and isn't marked 'noparent'),
    // then this must be a function or event, so we should not register.
    if (api_feature_provider->GetParent(*iter->second) != nullptr)
      continue;

    base::StringPiece binding_name =
        GetFirstDifferentAPIName(iter->first, root_name);

    v8::Local<v8::Object> nested_binding =
        CreateFullBinding(context, script_context, bindings_system,
                          api_feature_provider, std::string(binding_name));
    // It's possible that we don't create a binding if no features or
    // prefixed features are available to the context.
    if (nested_binding.IsEmpty())
      continue;

    if (root_binding.IsEmpty())
      root_binding = v8::Object::New(context->GetIsolate());

    // The nested api name contains a '.', e.g. 'app.runtime', but we want to
    // expose it on the object simply as 'runtime'.
    // Cache the last_binding_name now before mangling it.
    last_binding_name = binding_name;
    DCHECK_NE(base::StringPiece::npos, binding_name.rfind('.'));
    base::StringPiece accessor_name =
        binding_name.substr(binding_name.rfind('.') + 1);
    v8::Local<v8::String> nested_name =
        gin::StringToSymbol(context->GetIsolate(), accessor_name);
    v8::Maybe<bool> success =
        root_binding->CreateDataProperty(context, nested_name, nested_binding);
    if (!success.IsJust() || !success.FromJust())
      return v8::Local<v8::Object>();
  }

  return root_binding;
}

std::string GetContextOwner(v8::Local<v8::Context> context) {
  ScriptContext* script_context = GetScriptContextFromV8ContextChecked(context);
  const std::string& extension_id = script_context->GetExtensionID();
  bool id_is_valid = crx_file::id_util::IdIsValid(extension_id);
  CHECK(id_is_valid || script_context->url().is_valid());
  // Use only origin for URLs to match browser logic in EventListener::ForURL().
  return id_is_valid
             ? extension_id
             : url::Origin::Create(script_context->url()).GetURL().spec();
}

// Returns true if any portion of the runtime API is available to the given
// |context|. This is different than just checking features because runtime's
// availability depends on the installed extensions and the active URL (in the
// case of extensions communicating with external websites).
bool IsRuntimeAvailableToContext(ScriptContext* context) {
  // TODO(devlin): This doesn't seem thread-safe with ServiceWorkers?
  for (const auto& extension :
       *RendererExtensionRegistry::Get()->GetMainThreadExtensionSet()) {
    ExternallyConnectableInfo* info = static_cast<ExternallyConnectableInfo*>(
        extension->GetManifestData(manifest_keys::kExternallyConnectable));
    if (info && info->matches.MatchesURL(context->url())) {
      return true;
    }
  }
  return false;
}

// The APIs that could potentially be available to webpage-like contexts.
// This is the list of possible features; most web pages will not have access
// to these APIs.
// Note: `runtime` is not included here, since it's handled specially above.
const char* const kWebAvailableFeatures[] = {
    "app",
    "dashboardPrivate",
    "webstorePrivate",
    "management",
};

}  // namespace

NativeExtensionBindingsSystem::NativeExtensionBindingsSystem(
    std::unique_ptr<IPCMessageSender> ipc_message_sender)
    : ipc_message_sender_(std::move(ipc_message_sender)),
      api_system_(
          base::BindRepeating(&GetAPISchema),
          base::BindRepeating(&IsAPIFeatureAvailable),
          base::BindRepeating(&ArePromisesAllowed),
          base::BindRepeating(&NativeExtensionBindingsSystem::SendRequest,
                              base::Unretained(this)),
          std::make_unique<ExtensionInteractionProvider>(),
          base::BindRepeating(
              &NativeExtensionBindingsSystem::OnEventListenerChanged,
              base::Unretained(this)),
          base::BindRepeating(&GetContextOwner),
          base::BindRepeating(&APIActivityLogger::LogAPICall,
                              ipc_message_sender_.get()),
          base::BindRepeating(&AddConsoleError),
          APILastError(base::BindRepeating(&GetLastErrorParents),
                       base::BindRepeating(&AddConsoleError))),
      messaging_service_(this) {
  api_system_.RegisterCustomType(
      "storage.StorageArea",
      base::BindRepeating(&StorageArea::CreateStorageArea));
  api_system_.RegisterCustomType("types.ChromeSetting",
                                 base::BindRepeating(&ChromeSetting::Create));
  api_system_.RegisterCustomType("contentSettings.ContentSetting",
                                 base::BindRepeating(&ContentSetting::Create));
  api_system_.RegisterHooksDelegate("webRequest",
                                    std::make_unique<WebRequestHooks>());
  api_system_.RegisterHooksDelegate(
      "declarativeContent",
      std::make_unique<DeclarativeContentHooksDelegate>());
  api_system_.RegisterHooksDelegate("dom",
                                    std::make_unique<DOMHooksDelegate>());
  api_system_.RegisterHooksDelegate("i18n",
                                    std::make_unique<I18nHooksDelegate>());
  api_system_.RegisterHooksDelegate(
      "runtime", std::make_unique<RuntimeHooksDelegate>(&messaging_service_));
  api_system_.RegisterHooksDelegate(
      "feedbackPrivate", std::make_unique<FeedbackPrivateHooksDelegate>());
}

NativeExtensionBindingsSystem::~NativeExtensionBindingsSystem() = default;

void NativeExtensionBindingsSystem::DidCreateScriptContext(
    ScriptContext* context) {
  v8::Isolate* isolate = context->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> v8_context = context->v8_context();

  JSRunner::SetInstanceForContext(v8_context,
                                  std::make_unique<ExtensionJSRunner>(context));

  gin::PerContextData* per_context_data = gin::PerContextData::From(v8_context);
  DCHECK(per_context_data);
  DCHECK(!per_context_data->GetUserData(kBindingsSystemPerContextKey));

  auto data = std::make_unique<BindingsSystemPerContextData>(
      weak_factory_.GetWeakPtr());
  per_context_data->SetUserData(kBindingsSystemPerContextKey, std::move(data));

  if (get_internal_api_.IsEmpty()) {
    get_internal_api_.Set(
        isolate, v8::FunctionTemplate::New(
                     isolate, &NativeExtensionBindingsSystem::GetInternalAPI,
                     v8::Local<v8::Value>(), v8::Local<v8::Signature>(), 0,
                     v8::ConstructorBehavior::kThrow));
  }

  // Note: it's a shame we can't delay this (until, say, we knew an API would
  // actually be used), but it's needed for some of our crazier hooks, like
  // web/guest view.
  context->module_system()->SetGetInternalAPIHook(
      get_internal_api_.Get(isolate));
  context->module_system()->SetJSBindingUtilGetter(
      base::BindRepeating(&NativeExtensionBindingsSystem::GetJSBindingUtil,
                          weak_factory_.GetWeakPtr()));

  UpdateBindingsForContext(context);

  // Set the scripting params object for if we are running in a content script
  // context. This effectively checks that we are running in an isolated world
  // since main world script contexts have a different Feature::Context type.
  if (context->context_type() == Feature::CONTENT_SCRIPT_CONTEXT)
    SetScriptingParams(context);
}

void NativeExtensionBindingsSystem::WillReleaseScriptContext(
    ScriptContext* context) {
  v8::HandleScope handle_scope(context->isolate());
  v8::Local<v8::Context> v8_context = context->v8_context();
  api_system_.WillReleaseContext(v8_context);
  // Clear the JSRunner only after everything else has been notified that the
  // context is being released.
  JSRunner::ClearInstanceForContext(v8_context);
}

void NativeExtensionBindingsSystem::UpdateBindingsForContext(
    ScriptContext* context) {
  v8::Isolate* isolate = context->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> v8_context = context->v8_context();
  v8::Local<v8::Object> chrome = GetOrCreateChrome(v8_context);
  if (chrome.IsEmpty())
    return;

  DCHECK(GetBindingsDataFromContext(v8_context));

  auto set_accessor = [chrome, isolate,
                       v8_context](base::StringPiece accessor_name) {
    v8::Local<v8::String> api_name =
        gin::StringToSymbol(isolate, accessor_name);
    v8::Maybe<bool> success = chrome->SetLazyDataProperty(
        v8_context, api_name, &BindingAccessor, api_name);
    return success.IsJust() && success.FromJust();
  };

  auto set_restricted_accessor = [chrome, isolate,
                                  v8_context](base::StringPiece accessor_name) {
    v8::Local<v8::String> api_name =
        gin::StringToSymbol(isolate, accessor_name);
    v8::Maybe<bool> success = chrome->SetLazyDataProperty(
        v8_context, api_name, &ThrowDeveloperModeRestrictedError, api_name);
    return success.IsJust() && success.FromJust();
  };

  bool is_webpage = false;
  switch (context->context_type()) {
    case Feature::UNSPECIFIED_CONTEXT:
    case Feature::WEB_PAGE_CONTEXT:
    case Feature::BLESSED_WEB_PAGE_CONTEXT:
      is_webpage = true;
      break;
    case Feature::BLESSED_EXTENSION_CONTEXT:
    case Feature::LOCK_SCREEN_EXTENSION_CONTEXT:
    case Feature::OFFSCREEN_EXTENSION_CONTEXT:
    case Feature::UNBLESSED_EXTENSION_CONTEXT:
    case Feature::CONTENT_SCRIPT_CONTEXT:
    case Feature::WEBUI_CONTEXT:
    case Feature::WEBUI_UNTRUSTED_CONTEXT:
      is_webpage = false;
  }

  if (is_webpage) {
    // Hard-code registration of any APIs that are exposed to webpage-like
    // contexts, because it's more expensive to iterate over all the existing
    // features when only a handful could ever be available.
    // All of the same permission checks will still apply.
    // TODO(devlin): It could be interesting to apply this same logic to all
    // context types, especially on a given platform. Something to think about
    // for when we generate features.
    for (const char* feature_name : kWebAvailableFeatures) {
      if (context->GetAvailability(feature_name).is_available() &&
          !set_accessor(feature_name)) {
        LOG(ERROR) << "Failed to create API on Chrome object.";
        return;
      }
    }

    // Runtime is special (see IsRuntimeAvailableToContext()).
    if (IsRuntimeAvailableToContext(context) && !set_accessor("runtime"))
      LOG(ERROR) << "Failed to create API on Chrome object.";

    UpdateContentCapabilities(context);
    return;
  }

  FeatureCache::FeatureNameVector features =
      feature_cache_.GetAvailableFeatures(context->context_type(),
                                          context->extension(), context->url());
  base::StringPiece last_accessor;
  for (const std::string& feature : features) {
    // If we've already set up an accessor for the immediate property of the
    // chrome object, we don't need to do more.
    if (IsPrefixedAPI(feature, last_accessor))
      continue;

    // We've found an API that's available to the extension. Normally, we will
    // expose this under the name of the feature (e.g., 'tabs'), but in some
    // cases, this will be a prefixed API, such as 'app.runtime'. Find what the
    // property on the chrome object is named, and use that. So in the case of
    // 'app.runtime', we surface a getter for simply 'app'.
    //
    // TODO(devlin): UpdateBindingsForContext can be called during context
    // creation, but also when e.g. permissions change. Do we need to be
    // checking for whether or not the API already exists on the object as well
    // as if we need to remove any existing APIs?
    base::StringPiece accessor_name =
        GetFirstDifferentAPIName(feature, base::StringPiece());
    last_accessor = accessor_name;
    if (!set_accessor(accessor_name)) {
      LOG(ERROR) << "Failed to create API on Chrome object.";
      return;
    }
  }

  FeatureCache::FeatureNameVector dev_mode_features =
      feature_cache_.GetDeveloperModeRestrictedFeatures(
          context->context_type(), context->extension(), context->url());

  for (const std::string& feature : dev_mode_features) {
    base::StringPiece accessor_name =
        GetFirstDifferentAPIName(feature, base::StringPiece());
    // This code only works for restricting top-level features to developer
    // mode. For sub-features, this would result in overwriting the accessor
    // for the root API object and restricting the whole API.
    DCHECK_EQ(accessor_name, feature);
    if (!set_restricted_accessor(accessor_name)) {
      LOG(ERROR) << "Failed to create API on Chrome object.";
      return;
    }
  }
}

void NativeExtensionBindingsSystem::DispatchEventInContext(
    const std::string& event_name,
    const base::Value::List& event_args,
    const mojom::EventFilteringInfoPtr& filtering_info,
    ScriptContext* context) {
  v8::HandleScope handle_scope(context->isolate());
  v8::Context::Scope context_scope(context->v8_context());
  api_system_.FireEventInContext(event_name, context->v8_context(), event_args,
                                 filtering_info.Clone());
}

bool NativeExtensionBindingsSystem::HasEventListenerInContext(
    const std::string& event_name,
    ScriptContext* context) {
  v8::HandleScope handle_scope(context->isolate());
  return api_system_.event_handler()->HasListenerForEvent(
      event_name, context->v8_context());
}

void NativeExtensionBindingsSystem::HandleResponse(
    int request_id,
    bool success,
    const base::Value::List& response,
    const std::string& error,
    mojom::ExtraResponseDataPtr extra_data) {
  // Some API calls result in failure, but don't set an error. Use a generic and
  // unhelpful error string.
  // TODO(devlin): Track these down and fix them. See crbug.com/648275.
  api_system_.CompleteRequest(
      request_id, response,
      !success && error.empty() ? "Unknown error." : error,
      std::move(extra_data));
  ipc_message_sender_->SendOnRequestResponseReceivedIPC(request_id);
}

IPCMessageSender* NativeExtensionBindingsSystem::GetIPCMessageSender() {
  return ipc_message_sender_.get();
}

void NativeExtensionBindingsSystem::UpdateBindings(
    const ExtensionId& extension_id,
    bool permissions_changed,
    ScriptContextSetIterable* script_context_set) {
  if (permissions_changed)
    InvalidateFeatureCache(extension_id);
  script_context_set->ForEach(
      extension_id,
      base::BindRepeating(
          &NativeExtensionBindingsSystem::UpdateBindingsForContext,
          // Called synchronously.
          base::Unretained(this)));
}

void NativeExtensionBindingsSystem::OnExtensionRemoved(const ExtensionId& id) {
  feature_cache_.InvalidateExtension(id);
}

v8::Local<v8::Object> NativeExtensionBindingsSystem::GetAPIObjectForTesting(
    ScriptContext* context,
    const std::string& api_name) {
  return GetAPIHelper(context->v8_context(),
                      gin::StringToSymbol(context->isolate(), api_name));
}

void NativeExtensionBindingsSystem::BindingAccessor(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = info.Holder()->GetCreationContextChecked();

  // Force binding creation in the owning context (even if another context is
  // calling in). This is also important to ensure that objects created through
  // the initialization process are all instantiated for the owning context.
  // See https://crbug.com/819968.
  v8::Context::Scope context_scope(context);

  // We use info.Data() to store a real name here instead of using the provided
  // one to handle any weirdness from the caller (non-existent strings, etc).
  v8::Local<v8::String> api_name = info.Data().As<v8::String>();
  v8::Local<v8::Object> binding = GetAPIHelper(context, api_name);
  if (!binding.IsEmpty())
    info.GetReturnValue().Set(binding);
}

void NativeExtensionBindingsSystem::ThrowDeveloperModeRestrictedError(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  isolate->ThrowException(v8::Exception::Error(gin::StringToV8(
      isolate,
      base::StringPrintf(
          "The '%s' API is only available for users in developer mode.",
          gin::V8ToString(isolate, info.Data()).c_str()))));
  return;
}

// static
v8::Local<v8::Object> NativeExtensionBindingsSystem::GetAPIHelper(
    v8::Local<v8::Context> context,
    v8::Local<v8::String> api_name) {
  BindingsSystemPerContextData* data = GetBindingsDataFromContext(context);
  if (!data)
    return v8::Local<v8::Object>();

  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Object> apis;
  if (data->api_object.IsEmpty()) {
    apis = v8::Object::New(isolate);
    data->api_object = v8::Global<v8::Object>(isolate, apis);
  } else {
    apis = data->api_object.Get(isolate);
  }

  v8::Maybe<bool> has_property = apis->HasRealNamedProperty(context, api_name);
  if (!has_property.IsJust())
    return v8::Local<v8::Object>();

  if (has_property.FromJust()) {
    v8::Local<v8::Value> value =
        apis->GetRealNamedProperty(context, api_name).ToLocalChecked();
    DCHECK(value->IsObject());
    return value.As<v8::Object>();
  }

  ScriptContext* script_context = GetScriptContextFromV8ContextChecked(context);
  std::string api_name_string;
  CHECK(
      gin::Converter<std::string>::FromV8(isolate, api_name, &api_name_string));

  v8::Local<v8::Object> root_binding = CreateFullBinding(
      context, script_context, &data->bindings_system->api_system_,
      FeatureProvider::GetAPIFeatures(), api_name_string);
  if (root_binding.IsEmpty())
    return v8::Local<v8::Object>();

  v8::Maybe<bool> success =
      apis->CreateDataProperty(context, api_name, root_binding);
  if (!success.IsJust() || !success.FromJust())
    return v8::Local<v8::Object>();

  return root_binding;
}

v8::Local<v8::Object> NativeExtensionBindingsSystem::GetLastErrorParents(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object>* secondary_parent) {
  if (secondary_parent &&
      IsAPIFeatureAvailable(context, "extension.lastError")) {
    *secondary_parent = GetAPIHelper(
        context, gin::StringToSymbol(context->GetIsolate(), "extension"));
  }

  return GetAPIHelper(context,
                      gin::StringToSymbol(context->GetIsolate(), "runtime"));
}

// static
void NativeExtensionBindingsSystem::GetInternalAPI(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK_EQ(1, info.Length());
  CHECK(info[0]->IsString());

  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::String> v8_name = info[0].As<v8::String>();

  BindingsSystemPerContextData* data = GetBindingsDataFromContext(context);
  CHECK(data);

  v8::Local<v8::Object> internal_apis;
  if (data->internal_apis.IsEmpty()) {
    internal_apis = v8::Object::New(isolate);
    data->internal_apis.Reset(isolate, internal_apis);
  } else {
    internal_apis = data->internal_apis.Get(isolate);
  }

  v8::Maybe<bool> has_property =
      internal_apis->HasOwnProperty(context, v8_name);
  if (!has_property.IsJust())
    return;

  if (has_property.FromJust()) {
    // API was already instantiated.
    info.GetReturnValue().Set(
        internal_apis->GetRealNamedProperty(context, v8_name).ToLocalChecked());
    return;
  }

  std::string api_name = gin::V8ToString(isolate, info[0]);
  const Feature* feature = FeatureProvider::GetAPIFeature(api_name);
  ScriptContext* script_context = GetScriptContextFromV8ContextChecked(context);
  if (!feature || !script_context->IsAnyFeatureAvailableToContext(
                      *feature, CheckAliasStatus::NOT_ALLOWED)) {
    NOTREACHED();
    return;
  }

  CHECK(feature->IsInternal());

  // We don't need to go through CreateFullBinding here because internal APIs
  // are always acquired through getInternalBinding and specified by full name,
  // rather than through access on the chrome object. So we can just instantiate
  // a binding keyed with any name, even a prefixed one (e.g.
  // 'app.currentWindowInternal').
  v8::Local<v8::Object> api_binding = CreateRootBinding(
      context, script_context, api_name, &data->bindings_system->api_system_);

  if (api_binding.IsEmpty())
    return;

  v8::Maybe<bool> success =
      internal_apis->CreateDataProperty(context, v8_name, api_binding);
  if (!success.IsJust() || !success.FromJust())
    return;

  info.GetReturnValue().Set(api_binding);
}

void NativeExtensionBindingsSystem::SendRequest(
    std::unique_ptr<APIRequestHandler::Request> request,
    v8::Local<v8::Context> context) {
  ScriptContext* script_context = GetScriptContextFromV8ContextChecked(context);

  GURL url;
  blink::WebLocalFrame* frame = script_context->web_frame();
  if (frame && !frame->GetDocument().IsNull())
    url = frame->GetDocument().Url();
  else
    url = script_context->url();

  auto params = mojom::RequestParams::New();
  params->name = request->method_name;
  params->arguments = std::move(request->arguments_list);
  params->extension_id = script_context->GetExtensionID();
  params->source_url = url;
  params->request_id = request->request_id;
  params->has_callback = request->has_async_response_handler;
  params->user_gesture = request->has_user_gesture;
  // The IPC sender will update these members, if appropriate.
  params->worker_thread_id = kMainThreadId;
  params->service_worker_version_id =
      blink::mojom::kInvalidServiceWorkerVersionId;

  ipc_message_sender_->SendRequestIPC(script_context, std::move(params));
}

void NativeExtensionBindingsSystem::OnEventListenerChanged(
    const std::string& event_name,
    binding::EventListenersChanged change,
    const base::Value::Dict* filter,
    bool update_lazy_listeners,
    v8::Local<v8::Context> context) {
  ScriptContext* script_context = GetScriptContextFromV8ContextChecked(context);
  // We only remove a lazy listener if the listener removal was triggered
  // manually by the extension and the context is a lazy context.
  // Note: Check context_type() first to avoid accessing ExtensionFrameHelper on
  // a worker thread.
  bool is_lazy = update_lazy_listeners &&
                 (script_context->IsForServiceWorker() ||
                  ExtensionFrameHelper::IsContextForEventPage(script_context));

  switch (change) {
    case binding::EventListenersChanged::
        kFirstUnfilteredListenerForContextOwnerAdded:
      // Send a message to add a new listener since this is the first listener
      // for the context owner (e.g., extension).
      ipc_message_sender_->SendAddUnfilteredEventListenerIPC(script_context,
                                                             event_name);
      // Check if we need to add a lazy listener as well.
      [[fallthrough]];
    case binding::EventListenersChanged::
        kFirstUnfilteredListenerForContextAdded: {
      // If the listener is the first for the event page, we need to
      // specifically add a lazy listener.
      if (is_lazy) {
        ipc_message_sender_->SendAddUnfilteredLazyEventListenerIPC(
            script_context, event_name);
      }
      break;
    }
    case binding::EventListenersChanged::
        kLastUnfilteredListenerForContextOwnerRemoved:
      // Send a message to remove a listener since this is the last listener
      // for the context owner (e.g., extension).
      ipc_message_sender_->SendRemoveUnfilteredEventListenerIPC(script_context,
                                                                event_name);
      // Check if we need to remove a lazy listener as well.
      [[fallthrough]];
    case binding::EventListenersChanged::
        kLastUnfilteredListenerForContextRemoved: {
      // If the listener was the last for the event page, we need to remove
      // the lazy listener entry.
      if (is_lazy) {
        ipc_message_sender_->SendRemoveUnfilteredLazyEventListenerIPC(
            script_context, event_name);
      }
      break;
    }
    // TODO(https://crbug.com/873017): This is broken, since we'll only add or
    // remove a lazy listener if it was the first/last for the context owner.
    // This means that if an extension registers a filtered listener on a page
    // and *then* adds one in the event page, we won't properly add the listener
    // as lazy.  This is an issue for both native and JS bindings, so for now,
    // let's settle for parity.
    case binding::EventListenersChanged::
        kFirstListenerWithFilterForContextOwnerAdded:
      DCHECK(filter);
      ipc_message_sender_->SendAddFilteredEventListenerIPC(
          script_context, event_name, *filter, is_lazy);
      break;
    case binding::EventListenersChanged::
        kLastListenerWithFilterForContextOwnerRemoved:
      DCHECK(filter);
      ipc_message_sender_->SendRemoveFilteredEventListenerIPC(
          script_context, event_name, *filter, is_lazy);
      break;
  }
}

void NativeExtensionBindingsSystem::GetJSBindingUtil(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value>* binding_util_out) {
  gin::Handle<APIBindingJSUtil> handle = gin::CreateHandle(
      context->GetIsolate(),
      new APIBindingJSUtil(
          api_system_.type_reference_map(), api_system_.request_handler(),
          api_system_.event_handler(), api_system_.exception_handler()));
  *binding_util_out = handle.ToV8();
}

void NativeExtensionBindingsSystem::UpdateContentCapabilities(
    ScriptContext* context) {
  Feature::Context context_type = context->context_type();
  if (context_type != Feature::WEB_PAGE_CONTEXT &&
      context_type != Feature::BLESSED_WEB_PAGE_CONTEXT) {
    return;
  }

  // Must be called on main thread.
  DCHECK(!worker_thread_util::IsWorkerThread());

  APIPermissionSet permissions;
  for (const auto& extension :
       *RendererExtensionRegistry::Get()->GetMainThreadExtensionSet()) {
    blink::WebLocalFrame* web_frame = context->web_frame();
    GURL url = context->url();
    // We allow about:blank pages to take on the privileges of their parents if
    // they aren't sandboxed.
    if (web_frame && !web_frame->GetSecurityOrigin().IsOpaque()) {
      url = ScriptContext::GetEffectiveDocumentURLForContext(web_frame, url,
                                                             true);
    }
    const ContentCapabilitiesInfo& info =
        ContentCapabilitiesInfo::Get(extension.get());
    if (info.url_patterns.MatchesURL(url)) {
      APIPermissionSet new_permissions;
      APIPermissionSet::Union(permissions, info.permissions, &new_permissions);
      permissions = std::move(new_permissions);
    }
  }
  context->set_content_capabilities(std::move(permissions));
}

void NativeExtensionBindingsSystem::InvalidateFeatureCache(
    const ExtensionId& extension_id) {
  feature_cache_.InvalidateExtension(extension_id);
}

void NativeExtensionBindingsSystem::SetScriptingParams(ScriptContext* context) {
  if (!IsAPIFeatureAvailable(context->v8_context(), "scripting.globalParams"))
    return;

  v8::Local<v8::Object> scripting_object =
      GetAPIHelper(context->v8_context(),
                   gin::StringToSymbol(context->isolate(), "scripting"));
  if (scripting_object.IsEmpty())
    return;

  // Set the globalParams property on the chrome.scripting object if available.
  scripting_object
      ->CreateDataProperty(
          context->v8_context(),
          gin::StringToSymbol(context->isolate(), "globalParams"),
          gin::DataObjectBuilder(context->isolate()).Build())
      .Check();
}

}  // namespace extensions
