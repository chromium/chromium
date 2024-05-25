// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_FUNCTION_H_
#define EXTENSIONS_BROWSER_EXTENSION_FUNCTION_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/pass_key.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/service_worker/service_worker_keepalive.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/constants.h"
#include "extensions/common/context_data.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/extra_response_data.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/stack_frame.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-forward.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-forward.h"

namespace base {
class Value;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}

namespace extensions {
class ExtensionFunctionDispatcher;
}

#ifdef NDEBUG
#define EXTENSION_FUNCTION_VALIDATE(test) \
  do {                                    \
    if (!(test)) {                        \
      this->SetBadMessage();              \
      return ValidationFailure(this);     \
    }                                     \
  } while (0)
#else   // NDEBUG
#define EXTENSION_FUNCTION_VALIDATE(test) CHECK(test)
#endif  // NDEBUG

#ifdef NDEBUG
#define EXTENSION_FUNCTION_PRERUN_VALIDATE(test) \
  do {                                           \
    if (!(test)) {                               \
      this->SetBadMessage();                     \
      return false;                              \
    }                                            \
  } while (0)
#else  // NDEBUG
#define EXTENSION_FUNCTION_PRERUN_VALIDATE(test) CHECK(test)
#endif  // NDEBUG

// Declares a callable extension function with the given |name|. You must also
// supply a unique |histogramvalue| used for histograms of extension function
// invocation (add new ones at the end of the enum in
// extension_function_histogram_value.h).
// TODO(devlin): This would be nicer if instead we defined the constructor
// for the ExtensionFunction since the histogram value and name should never
// change. Then, we could get rid of the set_ methods for those values on
// ExtensionFunction, and there'd be no possibility of having them be
// "wrong" for a given function. Unfortunately, that would require updating
// each ExtensionFunction and construction site, which, while possible, is
// quite costly.
#define DECLARE_EXTENSION_FUNCTION(name, histogramvalue)               \
 public:                                                               \
  static constexpr const char* static_function_name() { return name; } \
                                                                       \
 public:                                                               \
  static constexpr extensions::functions::HistogramValue               \
  static_histogram_value() {                                           \
    return extensions::functions::histogramvalue;                      \
  }

// Abstract base class for extension functions the ExtensionFunctionDispatcher
// knows how to dispatch to.
// NOTE: If you see a crash in an ExtensionFunction implementation and want to
// know which extension triggered the crash, look for crash keys
// extension-function-caller-1, 2, and 3.
class ExtensionFunction : public base::RefCountedThreadSafe<
                              ExtensionFunction,
                              content::BrowserThread::DeleteOnUIThread> {
 public:
  enum ResponseType {
    // The function has succeeded.
    SUCCEEDED,
    // The function has failed.
    FAILED,
    // The input message is malformed.
    BAD_MESSAGE
  };

  using ResponseCallback = base::OnceCallback<void(
      ResponseType type,
      base::Value::List results,
      const std::string& error,
      extensions::mojom::ExtraResponseDataPtr response_data)>;

  ExtensionFunction();

  ExtensionFunction(const ExtensionFunction&) = delete;
  ExtensionFunction& operator=(const ExtensionFunction&) = delete;

  static void EnsureShutdownNotifierFactoryBuilt();

  // Returns true if the function has permission to run.
  //
  // This checks the Extension's permissions against the features declared in
  // the *_features.json files. Note that some functions may perform additional
  // checks in Run(), such as for specific host permissions or user gestures.
  bool HasPermission() const;

  // Sends |error| as an error response.
  void RespondWithError(std::string error);

  using PassKey = base::PassKey<ExtensionFunction>;

  // The result of a function call.
  //
  // Use NoArguments(), WithArguments(), ArgumentList(), or Error()
  // rather than this class directly.
  class ResponseValue {
   public:
    ResponseValue(bool success, PassKey);
    ResponseValue(ResponseValue&& other);
    ResponseValue& operator=(ResponseValue&& other) = delete;
    ResponseValue(const ResponseValue&) = delete;
    ResponseValue& operator=(const ResponseValue&) = delete;
    ~ResponseValue();

    // Returns true for success, false for failure.
    bool success() const { return success_; }

   private:
    const bool success_;
  };

  // The action type used to hold a callback to be used by ResponseAction, when
  // returning from RunAsync.
  class RespondNowAction {
   public:
    using SendResponseCallback = base::OnceCallback<void(bool)>;
    RespondNowAction(ResponseValue result, SendResponseCallback send_response);
    RespondNowAction(RespondNowAction&& other);
    RespondNowAction& operator=(RespondNowAction&& other) = delete;
    ~RespondNowAction();

    // Executes the send response callback.
    void Execute();

   private:
    ResponseValue result_;
    SendResponseCallback send_response_;
  };

  // The action to use when returning from RunAsync.
  //
  // Use RespondNow() or RespondLater() or AlreadyResponded() rather than this
  // class directly.

  class ResponseAction {
   public:
    explicit ResponseAction(PassKey);
    ResponseAction(RespondNowAction action, PassKey);
    ResponseAction(ResponseAction&& other);
    ResponseAction& operator=(ResponseAction&& other) = delete;
    ~ResponseAction();

    // Executes whatever respond action it may be holding.
    void Execute();

   private:
    // An action object responsible for handling the sending of the response.
    std::optional<RespondNowAction> action_;
  };

  // Helper class for tests to force all ExtensionFunction::user_gesture()
  // calls to return true as long as at least one instance of this class
  // exists.
  class ScopedUserGestureForTests {
   public:
    ScopedUserGestureForTests();
    ~ScopedUserGestureForTests();
  };

  // A string used in the case of an unknown error being detected.
  // DON'T USE THIS. It's only here during conversion to flag cases where errors
  // aren't already set.
  // TODO(devlin): Remove this if/when all functions are updated to return real
  // errors.
  static const char kUnknownErrorDoNotUse[];

  // Called before Run() in order to perform a common verification check so that
  // APIs subclassing this don't have to roll their own RunSafe() variants.
  // If this returns false, then Run() is never called, and the function
  // responds immediately with an error (note that error must be non-empty in
  // this case). If this returns true, execution continues on to Run().
  virtual bool PreRunValidation(std::string* error);

  // Runs the extension function if PreRunValidation() succeeds. This should be
  // called at most once over the lifetime of an ExtensionFunction.
  ResponseAction RunWithValidation();

  // Runs the function and returns the action to take when the caller is ready
  // to respond. Callers can expect this is called at most once for the lifetime
  // of an ExtensionFunction.
  //
  // Typical return values might be:
  //   * RespondNow(NoArguments())
  //   * RespondNow(ArgumentList(my_result.ToValue()))
  //   * RespondNow(WithArguments(42))
  //   * RespondNow(WithArguments(42, "value", false))
  //   * RespondNow(Error("Warp core breach"))
  //   * RespondNow(Error("Warp core breach on *", GetURL()))
  //   * RespondLater(), then later,
  //     * Respond(NoArguments())
  //     * ... etc.
  //
  //
  // Callers must call Execute() on the return ResponseAction at some point,
  // exactly once.
  //
  // ExtensionFunction implementations are encouraged to just implement Run.
  [[nodiscard]] virtual ResponseAction Run() = 0;

  // Gets whether quota should be applied to this individual function
  // invocation. This is different to GetQuotaLimitHeuristics which is only
  // invoked once and then cached.
  //
  // Returns false by default.
  virtual bool ShouldSkipQuotaLimiting() const;

  // Optionally adds one or multiple QuotaLimitHeuristic instances suitable for
  // this function to |heuristics|. The ownership of the new QuotaLimitHeuristic
  // instances is passed to the owner of |heuristics|.
  // No quota limiting by default.
  //
  // Only called once per lifetime of the QuotaService.
  virtual void GetQuotaLimitHeuristics(
      extensions::QuotaLimitHeuristics* heuristics) const {}

  // Called when the quota limit has been exceeded. The default implementation
  // returns an error.
  virtual void OnQuotaExceeded(std::string violation_error);

  // Specifies the raw arguments to the function, as a JSON value.
  void SetArgs(base::Value::List args);

  // Retrieves the results of the function as a base::Value::List for testing
  // purposes.
  const base::Value::List* GetResultListForTest() const;

  std::unique_ptr<extensions::ContextData> GetContextData() const;

  // Retrieves any error string from the function.
  virtual const std::string& GetError() const;

  void SetBadMessage();

  // Specifies the name of the function. A long-lived string (such as a string
  // literal) must be provided.
  void SetName(const char* name);
  const char* name() const { return name_; }

  int context_id() const { return context_id_; }

  void set_extension(
      const scoped_refptr<const extensions::Extension>& extension) {
    extension_ = extension;
  }
  const extensions::Extension* extension() const { return extension_.get(); }
  const extensions::ExtensionId& extension_id() const {
    DCHECK(extension())
        << "extension_id() called without an Extension. If " << name()
        << " is allowed to be called without any Extension then you should "
        << "check extension() first. If not, there is a bug in the Extension "
        << "platform, so page somebody in extensions/OWNERS";
    return extension_->id();
  }

  void set_request_uuid(base::Uuid uuid) { request_uuid_ = std::move(uuid); }
  const base::Uuid& request_uuid() const { return request_uuid_; }

  void set_source_url(const GURL& source_url) { source_url_ = source_url; }
  const GURL& source_url() const { return source_url_; }

  void set_has_callback(bool has_callback) { has_callback_ = has_callback; }
  bool has_callback() const { return has_callback_; }

  void set_include_incognito_information(bool include) {
    include_incognito_information_ = include;
  }
  bool include_incognito_information() const {
    return include_incognito_information_;
  }

  // Note: consider using ScopedUserGestureForTests instead of calling
  // set_user_gesture directly.
  void set_user_gesture(bool user_gesture) { user_gesture_ = user_gesture; }
  bool user_gesture() const;

  void set_histogram_value(
      extensions::functions::HistogramValue histogram_value) {
    histogram_value_ = histogram_value; }
  extensions::functions::HistogramValue histogram_value() const {
    return histogram_value_; }

  void set_response_callback(ResponseCallback callback) {
    response_callback_ = std::move(callback);
  }

  void set_source_context_type(extensions::mojom::ContextType type) {
    source_context_type_ = type;
  }
  extensions::mojom::ContextType source_context_type() const {
    return source_context_type_;
  }

  void set_source_process_id(int source_process_id) {
    source_process_id_ = source_process_id;
  }
  int source_process_id() const {
    return source_process_id_;
  }

  void set_worker_id(extensions::WorkerId worker_id) {
    worker_id_ = std::move(worker_id);
  }
  const std::optional<extensions::WorkerId>& worker_id() const {
    return worker_id_;
  }

  int64_t service_worker_version_id() const {
    return worker_id_ ? worker_id_->version_id
                      : blink::mojom::kInvalidServiceWorkerVersionId;
  }

  void set_service_worker_keepalive(
      std::unique_ptr<extensions::ServiceWorkerKeepalive> keepalive) {
    service_worker_keepalive_ = std::move(keepalive);
  }
  // Out-of-line because the release of the keepalive can invoke significant
  // work.
  void ResetServiceWorkerKeepalive();

  bool is_from_service_worker() const { return worker_id_.has_value(); }

  ResponseType* response_type() const { return response_type_.get(); }

  bool did_respond() const { return did_respond_; }

  // Set the browser context which contains the extension that has originated
  // this function call. Only meant for testing; if unset, uses the
  // BrowserContext from dispatcher().
  void SetBrowserContextForTesting(content::BrowserContext* context);
  content::BrowserContext* browser_context() const;

  void SetRenderFrameHost(content::RenderFrameHost* render_frame_host);
  content::RenderFrameHost* render_frame_host() const {
    return render_frame_host_;
  }

  void SetDispatcher(
      const base::WeakPtr<extensions::ExtensionFunctionDispatcher>& dispatcher);
  extensions::ExtensionFunctionDispatcher* dispatcher() const {
    return dispatcher_.get();
  }

  int worker_thread_id() const {
    return worker_id_ ? worker_id_->thread_id : extensions::kMainThreadId;
  }

  // Returns the web contents associated with the sending |render_frame_host_|.
  // This can be null.
  content::WebContents* GetSenderWebContents();

  // Returns whether this API call should allow the extension service worker (if
  // any) to stay alive beyond the typical 5 minute-per-task limit (i.e.,
  // indicates this API is expected to potentially take longer than 5 minutes
  // to execute).
  // The default implementation returns false. In general, this should only
  // return true for APIs that trigger some sort of user prompt. If you are
  // unsure, please consult the extensions team.
  virtual bool ShouldKeepWorkerAliveIndefinitely();

  // Notifies the function that the renderer received the reply from the
  // browser. The function will only receive this notification if it registers
  // via `AddResponseTarget()`.
  virtual void OnResponseAck();

  // Sets did_respond_ to true so that the function won't DCHECK if it never
  // sends a response. Typically, this shouldn't be used, even in testing. It's
  // only for when you want to test functionality that doesn't exercise the
  // Run() aspect of an extension function.
  void ignore_did_respond_for_testing() { did_respond_ = true; }

  void preserve_results_for_testing() { preserve_results_for_testing_ = true; }

  // Same as above, but global. Yuck. Do not add any more uses of this.
  static bool ignore_all_did_respond_for_testing_do_not_use;

  void set_js_callstack(extensions::StackTrace js_callstack) {
    js_callstack_ = std::move(js_callstack);
  }

  const std::optional<extensions::StackTrace>& js_callstack() const {
    return js_callstack_;
  }

 protected:
  // ResponseValues.
  //
  // Success, no arguments to pass to caller.
  ResponseValue NoArguments();
  // Success, a list of arguments |results| to pass to caller.
  ResponseValue ArgumentList(base::Value::List results);

  // Success, a variadic list of arguments to pass to the caller.
  template <typename... Args>
  ResponseValue WithArguments(Args&&... args) {
    static_assert(sizeof...(Args) > 0,
                  "Use NoArguments(), as there are no arguments in this call.");

    base::Value::List params;
    params.reserve(sizeof...(Args));
    (params.Append(std::forward<Args&&>(args)), ...);
    return ArgumentList(std::move(params));
  }

  // Error. chrome.runtime.lastError.message will be set to |error|.
  ResponseValue Error(std::string error);
  // Error with formatting. Args are processed using
  // ErrorUtils::FormatErrorMessage, that is, each occurrence of * is replaced
  // by the corresponding |s*|:
  // Error("Error in *: *", "foo", "bar") <--> Error("Error in foo: bar").
  template <typename... Args>
  ResponseValue Error(const std::string& format, const Args&... args) {
    return CreateErrorResponseValue(
        extensions::ErrorUtils::FormatErrorMessage(format, args...));
  }
  // Error with a list of arguments |args| to pass to caller.
  // Using this ResponseValue indicates something is wrong with the API.
  // It shouldn't be possible to have both an error *and* some arguments.
  // Some legacy APIs do rely on it though, like webstorePrivate.
  ResponseValue ErrorWithArguments(base::Value::List args,
                                   const std::string& error);
  // Bad message. A ResponseValue equivalent to EXTENSION_FUNCTION_VALIDATE(),
  // so this will actually kill the renderer and not respond at all.
  ResponseValue BadMessage();

  // ResponseActions.
  //
  // These are exclusively used as return values from Run(). Call Respond(...)
  // to respond at any other time - but as described below, only after Run()
  // has already executed, and only if it returned RespondLater().
  //
  // Respond to the extension immediately with |result|.
  [[nodiscard]] ResponseAction RespondNow(ResponseValue result);
  // Don't respond now, but promise to call Respond(...) later.
  [[nodiscard]] ResponseAction RespondLater();
  // Respond() was already called before Run() finished executing.
  //
  // Assume Run() uses some helper system that accepts callback that Respond()s.
  // If that helper system calls the synchronously in some cases, then use
  // this return value in those cases.
  //
  // FooExtensionFunction::Run() {
  //   Helper::FetchResults(..., base::BindOnce(&Success));
  //   if (did_respond()) return AlreadyResponded();
  //   return RespondLater();
  // }
  // FooExtensionFunction::Success() {
  //   Respond(...);
  // }
  //
  // Helper::FetchResults(..., base::OnceCallback callback) {
  //   if (...)
  //     std::move(callback).Run(..);  // Synchronously call |callback|.
  //   else
  //     // Asynchronously call |callback|.
  // }
  [[nodiscard]] ResponseAction AlreadyResponded();

  // This is the return value of the EXTENSION_FUNCTION_VALIDATE macro, which
  // needs to work from Run(), RunAsync(), and RunSync(). The former of those
  // has a different return type (ResponseAction) than the latter two (bool).
  [[nodiscard]] static ResponseAction ValidationFailure(
      ExtensionFunction* function);

  // If RespondLater() was returned from Run(), functions must at some point
  // call Respond() with |result| as their result.
  //
  // More specifically: call this iff Run() has already executed, it returned
  // RespondLater(), and Respond(...) hasn't already been called.
  void Respond(ResponseValue result);

  // Adds this instance to the set of targets waiting for an ACK from the
  // renderer.
  void AddResponseTarget();

  virtual ~ExtensionFunction();

  // Called after the response is sent, allowing the function to perform any
  // additional work or cleanup.
  virtual void OnResponded();

  // Called when the `browser_context_` associated with this ExtensionFunction
  // is shutting down. Immediately after this call, `browser_context_` will be
  // set to null. Subclasses should override this method to perform any cleanup
  // that needs to happen before the context shuts down, such as removing
  // observers of KeyedServices.
  virtual void OnBrowserContextShutdown() {}

  // Return true if the argument to this function at |index| was provided and
  // is non-null.
  bool HasOptionalArgument(size_t index);

  // Emits a message to the extension's devtools console.
  void WriteToConsole(blink::mojom::ConsoleMessageLevel level,
                      const std::string& message);

  // Reports an inspector issue to the issues tab in Chrome DevTools
  void ReportInspectorIssue(blink::mojom::InspectorIssueInfoPtr info);

  // Sets the Blobs whose ownership is being transferred to the renderer.
  void SetTransferredBlobs(std::vector<blink::mojom::SerializedBlobPtr> blobs);

  bool has_args() const { return args_.has_value(); }

  const base::Value::List& args() const {
    DCHECK(args_);
    return *args_;
  }

  base::Value::List& mutable_args() {
    DCHECK(args_);
    return *args_;
  }

  // The extension that called this function.
  scoped_refptr<const extensions::Extension> extension_;

 private:
  ResponseValue CreateArgumentListResponse(base::Value::List result);
  ResponseValue CreateErrorWithArgumentsResponse(base::Value::List result,
                                                 const std::string& error);
  ResponseValue CreateErrorResponseValue(std::string error);
  ResponseValue CreateBadMessageResponse();

  void SetFunctionResults(base::Value::List results);
  void SetFunctionError(std::string error);

  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<ExtensionFunction>;
  friend class ResponseValueObject;
  class RenderFrameHostTracker;

  // Called on BrowserContext shutdown.
  void Shutdown();

  // Call with true to indicate success, false to indicate failure. If this
  // failed, |error_| should be set.
  void SendResponseImpl(bool success);

  // The arguments to the API. Only non-null if arguments were specified.
  std::optional<base::Value::List> args_;

  base::ElapsedTimer timer_;

  // The results of the API. This should be populated through the Respond()/
  // RespondNow() methods. In legacy implementations, this is set directly, and
  // should be set before calling SendResponse().
  std::optional<base::Value::List> results_;

  // Any detailed error from the API. This should be populated by the derived
  // class before Run() returns.
  std::string error_;

  // The callback to run once the function has done execution.
  ResponseCallback response_callback_;

  // UUID for this request.
  base::Uuid request_uuid_;

  // The name of this function.
  const char* name_ = nullptr;

  // The URL of the frame which is making this request
  GURL source_url_;

  // True if the js caller provides a callback function to receive the response
  // of this call.
  bool has_callback_ = false;

  // True if this callback should include information from incognito contexts
  // even if our profile_ is non-incognito. Note that in the case of a "split"
  // mode extension, this will always be false, and we will limit access to
  // data from within the same profile_ (either incognito or not).
  bool include_incognito_information_ = false;

  // True if the call was made in response of user gesture.
  bool user_gesture_ = false;

  // Any class that gets a malformed message should set this to true before
  // returning.  Usually we want to kill the message sending process.
  bool bad_message_ = false;

  // Set to true when RunWithValidation() is called, to look for callers using
  // the method more than once on a single ExtensionFunction. Note that some
  // ExtensionFunction objects may be created but not run, for example due to
  // quota limits.
  bool did_run_ = false;

  // The sample value to record with the histogram API when the function
  // is invoked.
  extensions::functions::HistogramValue histogram_value_ =
      extensions::functions::UNKNOWN;

  // The type of the JavaScript context where this call originated.
  extensions::mojom::ContextType source_context_type_ =
      extensions::mojom::ContextType::kUnspecified;

  // The context ID of the browser context where this call originated.
  int context_id_ = extensions::kUnspecifiedContextId;

  // The process ID of the page that triggered this function call, or -1
  // if unknown.
  int source_process_id_ = -1;

  // Set to the ID of the calling worker if this function was invoked by an
  // extension service worker context.
  std::optional<extensions::WorkerId> worker_id_;

  // A keepalive for the associated service worker. Only populated if this was
  // triggered by an extension service worker. In a unique_ptr instead of an
  // optional because it's unclear if the pre-allocated memory overhead is
  // worthwhile (given the number of calls from e.g. webui).
  std::unique_ptr<extensions::ServiceWorkerKeepalive> service_worker_keepalive_;

  // The response type of the function, if the response has been sent.
  std::unique_ptr<ResponseType> response_type_;

  // Whether this function has responded.
  // TODO(devlin): Replace this with response_type_ != null.
  bool did_respond_ = false;

  // If set to true, preserves |results_|, even after SendResponseImpl() was
  // called.
  //
  // SendResponseImpl() moves the results out of |this| through
  // ResponseCallback, and calling this method avoids that. This is nececessary
  // for tests that use test_utils::RunFunction*(), as those tests typically
  // retrieve the result afterwards through GetResultListForTest().
  // TODO(crbug.com/40803310): Remove this once GetResultListForTest() is
  // removed after ensuring consumers only use RunFunctionAndReturnResult() to
  // retrieve the results.
  bool preserve_results_for_testing_ = false;

  // The dispatcher that will service this extension function call.
  base::WeakPtr<extensions::ExtensionFunctionDispatcher> dispatcher_;

  // Obtained via |dispatcher_| when it is set. It automatically resets to
  // nullptr when the BrowserContext is shutdown (much like a WeakPtr).
  raw_ptr<content::BrowserContext> browser_context_ = nullptr;
  raw_ptr<content::BrowserContext> browser_context_for_testing_ = nullptr;

  // Subscription for a callback that runs when the BrowserContext* is
  // destroyed.
  base::CallbackListSubscription shutdown_subscription_;

  // The RenderFrameHost we will send responses to.
  raw_ptr<content::RenderFrameHost> render_frame_host_ = nullptr;

  std::unique_ptr<RenderFrameHostTracker> tracker_;

  // The blobs transferred to the renderer process.
  std::vector<blink::mojom::SerializedBlobPtr> transferred_blobs_;

  // The JS call stack snapshot captured at function invocation time.
  std::optional<extensions::StackTrace> js_callstack_;
};

#endif  // EXTENSIONS_BROWSER_EXTENSION_FUNCTION_H_
