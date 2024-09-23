// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_CONTEXT_H_
#define EXTENSIONS_RENDERER_SCRIPT_CONTEXT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/unguessable_token.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/script_constants.h"
#include "extensions/common/stack_frame.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/safe_builtins.h"
#include "third_party/blink/public/web/web_script_execution_callback.h"
#include "url/gurl.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-script.h"

namespace blink {
class WebDocumentLoader;
class WebLocalFrame;
}

namespace content {
class RenderFrame;
}

namespace extensions {
enum class CheckAliasStatus;
class Extension;

// Extensions wrapper for a v8::Context.
//
// v8::Contexts can be constructed on any thread, and must only be accessed or
// destroyed that thread.
//
// Note that ScriptContexts bound to worker threads will not have the full
// functionality as those bound to the main RenderThread.
class ScriptContext {
 public:
  using RunScriptExceptionHandler =
      base::OnceCallback<void(const v8::TryCatch&)>;

  ScriptContext(const v8::Local<v8::Context>& context,
                blink::WebLocalFrame* frame,
                const mojom::HostID& host_id,
                const Extension* extension,
                std::optional<int> blink_isolated_world_id,
                mojom::ContextType context_type,
                const Extension* effective_extension,
                mojom::ContextType effective_context_type);

  ScriptContext(const ScriptContext&) = delete;
  ScriptContext& operator=(const ScriptContext&) = delete;

  ~ScriptContext();

  // Returns whether |url| from any Extension in |extension_set| is sandboxed,
  // as declared in each Extension's manifest.
  // TODO(kalman): Delete this when crbug.com/466373 is fixed.
  // See comment in HasAccessOrThrowError.
  static bool IsSandboxedPage(const GURL& url);

  // Initializes |module_system| and associates it with this context.
  void SetModuleSystem(std::unique_ptr<ModuleSystem> module_system);

  // Clears the WebLocalFrame for this contexts and invalidates the associated
  // ModuleSystem.
  void Invalidate();

  // Registers |observer| to be run when this context is invalidated. Closures
  // are run immediately when Invalidate() is called, not in a message loop.
  void AddInvalidationObserver(base::OnceClosure observer);

  // Returns true if this context is still valid, false if it isn't.
  // A context becomes invalid via Invalidate().
  bool is_valid() const { return is_valid_; }

  v8::Local<v8::Context> v8_context() const {
    return v8::Local<v8::Context>::New(isolate_, v8_context_);
  }

  const mojom::HostID& host_id() const { return host_id_; }

  const Extension* extension() const { return extension_.get(); }

  const Extension* effective_extension() const {
    return effective_extension_.get();
  }

  blink::WebLocalFrame* web_frame() const { return web_frame_; }

  mojom::ContextType context_type() const { return context_type_; }

  mojom::ContextType effective_context_type() const {
    return effective_context_type_;
  }

  const base::UnguessableToken& context_id() const { return context_id_; }

  ModuleSystem* module_system() { return module_system_.get(); }

  SafeBuiltins* safe_builtins() { return &safe_builtins_; }

  const SafeBuiltins* safe_builtins() const { return &safe_builtins_; }

  // Returns the ID of the extension associated with this context, or empty
  // string if there is no such extension.
  const std::string& GetExtensionID() const;

  // Returns the RenderFrame associated with this context. Can return NULL if
  // the context is in the process of being destroyed.
  content::RenderFrame* GetRenderFrame() const;

  // Safely calls the v8::Function, respecting the page load deferrer and
  // possibly executing asynchronously.
  // Doesn't catch exceptions; callers must do that if they want.
  // USE THESE METHODS RATHER THAN v8::Function::Call WHEREVER POSSIBLE.
  void SafeCallFunction(const v8::Local<v8::Function>& function,
                        int argc,
                        v8::Local<v8::Value> argv[]);
  void SafeCallFunction(const v8::Local<v8::Function>& function,
                        int argc,
                        v8::Local<v8::Value> argv[],
                        blink::WebScriptExecutionCallback callback);

  // Returns the availability of the API |api_name|.
  Feature::Availability GetAvailability(const std::string& api_name);
  // Returns the availability of the API |api_name|.
  // |check_alias| Whether API that has an alias that is available should be
  // considered available (even if the API itself is not available).
  Feature::Availability GetAvailability(const std::string& api_name,
                                        CheckAliasStatus check_alias);

  // Returns a string description of the type of context this is.
  std::string GetContextTypeDescription() const;

  // Returns a string description of the effective type of context this is.
  std::string GetEffectiveContextTypeDescription() const;

  v8::Isolate* isolate() const { return isolate_; }

  // Get the URL of this context's web frame.
  //
  // TODO(kalman): Remove this and replace with a GetOrigin() call which reads
  // of WebDocument::getSecurityOrigin():
  //  - The URL can change (e.g. pushState) but the origin cannot. Luckily it
  //    appears as though callers don't make security decisions based on the
  //    result of url() so it's not a problem... yet.
  //  - Origin is the correct check to be making.
  //  - It might let us remove the about:blank resolving?
  const GURL& url() const { return url_; }

  const GURL& service_worker_scope() const;

  int64_t service_worker_version_id() const {
    return service_worker_version_id_;
  }

  bool IsForServiceWorker() const;

  // Sets the URL of this ScriptContext. Usually this will automatically be set
  // on construction, unless this isn't constructed with enough information to
  // determine the URL (e.g. frame was null).
  // TODO(kalman): Make this a constructor parameter (as an origin).
  void set_url(const GURL& url) { url_ = url; }
  void set_service_worker_scope(const GURL& scope) {
    service_worker_scope_ = scope;
  }
  void set_service_worker_version_id(int64_t service_worker_version_id) {
    service_worker_version_id_ = service_worker_version_id;
  }

  // Returns whether the API |api| or any part of the API could be available in
  // this context without taking into account the context's extension.
  // |check_alias| Whether the API should be considered available if it has an
  // alias that is available.
  bool IsAnyFeatureAvailableToContext(const extensions::Feature& api,
                                      CheckAliasStatus check_alias);

  // Scope which maps a frame to a document loader. This is used by various
  // static methods below, which need to account for "just about to load"
  // document when retrieving URL.
  class ScopedFrameDocumentLoader {
   public:
    ScopedFrameDocumentLoader(blink::WebLocalFrame* frame,
                              blink::WebDocumentLoader* document_loader);

    ScopedFrameDocumentLoader(const ScopedFrameDocumentLoader&) = delete;
    ScopedFrameDocumentLoader& operator=(const ScopedFrameDocumentLoader&) =
        delete;

    ~ScopedFrameDocumentLoader();

   private:
    raw_ptr<blink::WebLocalFrame> frame_;
    raw_ptr<blink::WebDocumentLoader> document_loader_;
  };

  // TODO(devlin): Move all these Get*URL*() methods out of here? While they are
  // vaguely ScriptContext related, there's enough here that they probably
  // warrant another class or utility file.

  // Utility to get the URL we will match against for a frame. If the frame has
  // committed, this is the commited URL. Otherwise it is the provisional URL.
  // The returned URL may be invalid.
  static GURL GetDocumentLoaderURLForFrame(const blink::WebLocalFrame* frame);

  // Similar to GetDocumentLoaderURLForFrame, but only returns the data source
  // URL if the frame's document url is empty and the frame has a security
  // origin that allows access to the data source url.
  // TODO(asargent/devlin) - there may be places that should switch to using
  // this instead of GetDocumentLoaderURLForFrame.
  static GURL GetAccessCheckedFrameURL(const blink::WebLocalFrame* frame);

  // Used to determine the "effective" URL in context classification, such as to
  // associate an about:blank frame in an extension context with its extension.
  // If |document_url| is an about: or data: URL, returns the URL of the first
  // frame without an about: or data: URL that matches the initiator origin.
  // This may not be the immediate parent. Returns |document_url| if it is not
  // an about: URL, if |match_about_blank| is false, or if a suitable parent
  // cannot be found.
  // Will not check parent contexts that cannot be accessed (as is the case
  // for sandboxed frames).
  static GURL GetEffectiveDocumentURLForContext(blink::WebLocalFrame* frame,
                                                const GURL& document_url,
                                                bool match_about_blank);

  // Used to determine the "effective" URL for extension script injection.
  // If |document_url| is an about: or data: URL, returns the URL of the first
  // frame without an about: or data: URL that matches the initiator origin.
  // This may not be the immediate parent. Returns |document_url| if it is not
  // an about: or data: URL, if |match_origin_as_fallback| is set to not match,
  // or if a suitable parent cannot be found.
  // Considers parent contexts that cannot be accessed (as is the case for
  // sandboxed frames).
  static GURL GetEffectiveDocumentURLForInjection(
      blink::WebLocalFrame* frame,
      const GURL& document_url,
      MatchOriginAsFallbackBehavior match_origin_as_fallback);

  // Grants a set of content capabilities to this context.
  void set_content_capabilities(APIPermissionSet capabilities) {
    content_capabilities_ = std::move(capabilities);
  }

  // Indicates if this context has an effective API permission either by being
  // a context for an extension which has that permission, or by being a web
  // context which has been granted the corresponding capability by an
  // extension.
  bool HasAPIPermission(mojom::APIPermissionID permission) const;

  // Throws an Error in this context's JavaScript context, if this context does
  // not have access to |name|. Returns true if this context has access (i.e.
  // no exception thrown), false if it does not (i.e. an exception was thrown).
  bool HasAccessOrThrowError(const std::string& name);

  // Returns a string representation of this ScriptContext, for debugging.
  std::string GetDebugString() const;

  // Gets the current stack trace as a multi-line string to be logged.
  std::string GetStackTraceAsString() const;

  // Gets the current stack trace in a structured form instead of a string.
  std::optional<StackTrace> GetStackTrace(int frame_limit);

  // Generate a unique integer value. This is only unique within this instance.
  int32_t GetNextIdFromCounter() { return id_counter++; }

  // Runs |code|, labelling the script that gets created as |name| (the name is
  // used in the devtools and stack traces). |exception_handler| will be called
  // re-entrantly if an exception is thrown during the script's execution.
  v8::Local<v8::Value> RunScript(
      v8::Local<v8::String> name,
      v8::Local<v8::String> code,
      RunScriptExceptionHandler exception_handler,
      v8::ScriptCompiler::NoCacheReason no_cache_reason =
          v8::ScriptCompiler::NoCacheReason::kNoCacheNoReason);

 private:
  // Whether this context is valid.
  bool is_valid_;

  // The v8 context the bindings are accessible to.
  v8::Global<v8::Context> v8_context_;

  // The WebLocalFrame associated with this context. This can be NULL because
  // this object can outlive is destroyed asynchronously.
  raw_ptr<blink::WebLocalFrame> web_frame_;

  // The HostID associated with this context. For extensions, the HostID
  // HostType should match kExtensions and the ID should match
  // |extension()->id()|.
  const mojom::HostID host_id_;

  // The extension associated with this context, or NULL if there is none. This
  // might be a hosted app in the case that this context is hosting a web URL.
  scoped_refptr<const Extension> extension_;

  // The ID of the isolated world with which this context is associated, if
  // any.  This is predominantly set for user script and content script
  // contexts, but may be set for others, such as when something injects into a
  // <webview>.
  const std::optional<int> blink_isolated_world_id_;

  // The type of context.
  mojom::ContextType context_type_;

  // The effective extension associated with this context, or NULL if there is
  // none. This is different from the above extension if this context is in an
  // about:blank iframe for example.
  scoped_refptr<const Extension> effective_extension_;

  // The type of context.
  mojom::ContextType effective_context_type_;

  // A globally-unique ID for the script context.
  base::UnguessableToken context_id_;

  // Owns and structures the JS that is injected to set up extension bindings.
  std::unique_ptr<ModuleSystem> module_system_;

  // Contains safe copies of builtin objects like Function.prototype.
  SafeBuiltins safe_builtins_;

  // The set of capabilities granted to this context by extensions.
  APIPermissionSet content_capabilities_;

  // A list of base::OnceClosure instances as an observer interface for
  // invalidation.
  std::vector<base::OnceClosure> invalidate_observers_;

  raw_ptr<v8::Isolate> isolate_;

  GURL url_;

  GURL service_worker_scope_;

  int64_t service_worker_version_id_;

  // A counter to generate unique IDs. IDs must start at 1.
  int32_t id_counter = 1;

  base::ThreadChecker thread_checker_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPT_CONTEXT_H_
