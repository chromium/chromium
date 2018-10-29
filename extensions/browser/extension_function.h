// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_FUNCTION_H_
#define EXTENSIONS_BROWSER_EXTENSION_FUNCTION_H_

#include <stddef.h>

#include <list>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/console_message_level.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "ipc/ipc_message.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

class ExtensionFunction;
class UIThreadExtensionFunction;
class IOThreadExtensionFunction;

namespace base {
class ListValue;
class Value;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}

namespace extensions {
class ExtensionFunctionDispatcher;
class IOThreadExtensionMessageFilter;
class QuotaLimitHeuristic;
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

#define EXTENSION_FUNCTION_ERROR(error) \
  do {                                  \
    error_ = error;                     \
    this->SetBadMessage();              \
    return ValidationFailure(this);     \
  } while (0)

// Declares a callable extension function with the given |name|. You must also
// supply a unique |histogramvalue| used for histograms of extension function
// invocation (add new ones at the end of the enum in
// extension_function_histogram_value.h).
#define DECLARE_EXTENSION_FUNCTION(name, histogramvalue)                     \
 public:                                                                     \
  static constexpr const char* function_name() { return name; }              \
                                                                             \
 public:                                                                     \
  static constexpr extensions::functions::HistogramValue histogram_value() { \
    return extensions::functions::histogramvalue;                            \
  }

// Traits that describe how ExtensionFunction should be deleted. This just calls
// the virtual "Destruct" method on ExtensionFunction, allowing derived classes
// to override the behavior.
struct ExtensionFunctionDeleteTraits {
 public:
  static void Destruct(const ExtensionFunction* x);
};

// Abstract base class for extension functions the ExtensionFunctionDispatcher
// knows how to dispatch to.
class ExtensionFunction
    : public base::RefCountedThreadSafe<ExtensionFunction,
                                        ExtensionFunctionDeleteTraits> {
 public:
  enum ResponseType {
    // The function has succeeded.
    SUCCEEDED,
    // The function has failed.
    FAILED,
    // The input message is malformed.
    BAD_MESSAGE
  };

  using ResponseCallback = base::Callback<void(
      ResponseType type,
      const base::ListValue& results,
      const std::string& error,
      extensions::functions::HistogramValue histogram_value)>;

  ExtensionFunction();

  virtual UIThreadExtensionFunction* AsUIThreadExtensionFunction();
  virtual IOThreadExtensionFunction* AsIOThreadExtensionFunction();

  // Returns true if the function has permission to run.
  //
  // The default implementation is to check the Extension's permissions against
  // what this function requires to run, but some APIs may require finer
  // grained control, such as tabs.executeScript being allowed for active tabs.
  //
  // This will be run after the function has been set up but before Run().
  virtual bool HasPermission();

  // The result of a function call.
  //
  // Use NoArguments(), OneArgument(), ArgumentList(), or Error()
  // rather than this class directly.
  class ResponseValueObject {
   public:
    virtual ~ResponseValueObject() {}

    // Returns true for success, false for failure.
    virtual bool Apply() = 0;

   protected:
    void SetFunctionResults(ExtensionFunction* function,
                            std::unique_ptr<base::ListValue> results);
    void SetFunctionError(ExtensionFunction* function,
                          const std::string& error);
  };
  typedef std::unique_ptr<ResponseValueObject> ResponseValue;

  // The action to use when returning from RunAsync.
  //
  // Use RespondNow() or RespondLater() or AlreadyResponded() rather than this
  // class directly.
  class ResponseActionObject {
   public:
    virtual ~ResponseActionObject() {}

    virtual void Execute() = 0;
  };
  typedef std::unique_ptr<ResponseActionObject> ResponseAction;

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

  // Runs the extension function if PreRunValidation() succeeds.
  ResponseAction RunWithValidation();

  // Runs the function and returns the action to take when the caller is ready
  // to respond.
  //
  // Typical return values might be:
  //   * RespondNow(NoArguments())
  //   * RespondNow(OneArgument(42))
  //   * RespondNow(ArgumentList(my_result.ToValue()))
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
  virtual ResponseAction Run() WARN_UNUSED_RESULT = 0;

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
  virtual void OnQuotaExceeded(const std::string& violation_error);

  // Specifies the raw arguments to the function, as a JSON value.
  // TODO(dcheng): This should take a const ref.
  virtual void SetArgs(const base::ListValue* args);

  // Retrieves the results of the function as a ListValue.
  const base::ListValue* GetResultList() const;

  // Retrieves any error string from the function.
  virtual const std::string& GetError() const;

  virtual void SetBadMessage();

  // Specifies the name of the function. A long-lived string (such as a string
  // literal) must be provided.
  void set_name(const char* name) { name_ = name; }
  const char* name() const { return name_; }

  void set_profile_id(void* profile_id) { profile_id_ = profile_id; }
  void* profile_id() const { return profile_id_; }

  void set_extension(
      const scoped_refptr<const extensions::Extension>& extension) {
    extension_ = extension;
  }
  const extensions::Extension* extension() const { return extension_.get(); }
  const std::string& extension_id() const {
    DCHECK(extension())
        << "extension_id() called without an Extension. If " << name()
        << " is allowed to be called without any Extension then you should "
        << "check extension() first. If not, there is a bug in the Extension "
        << "platform, so page somebody in extensions/OWNERS";
    return extension_->id();
  }

  void set_request_id(int request_id) { request_id_ = request_id; }
  int request_id() { return request_id_; }

  void set_source_url(const GURL& source_url) { source_url_ = source_url; }
  const GURL& source_url() { return source_url_; }

  void set_has_callback(bool has_callback) { has_callback_ = has_callback; }
  bool has_callback() { return has_callback_; }

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

  void set_response_callback(const ResponseCallback& callback) {
    response_callback_ = callback;
  }

  void set_source_context_type(extensions::Feature::Context type) {
    source_context_type_ = type;
  }
  extensions::Feature::Context source_context_type() const {
    return source_context_type_;
  }

  void set_source_process_id(int source_process_id) {
    source_process_id_ = source_process_id;
  }
  int source_process_id() const {
    return source_process_id_;
  }

  ResponseType* response_type() const { return response_type_.get(); }

  bool did_respond() const { return did_respond_; }

  // Sets did_respond_ to true so that the function won't DCHECK if it never
  // sends a response. Typically, this shouldn't be used, even in testing. It's
  // only for when you want to test functionality that doesn't exercise the
  // Run() aspect of an extension function.
  void ignore_did_respond_for_testing() { did_respond_ = true; }
  // Same as above, but global. Yuck. Do not add any more uses of this.
  static bool ignore_all_did_respond_for_testing_do_not_use;

 protected:
  friend struct ExtensionFunctionDeleteTraits;

  // ResponseValues.
  //
  // Success, no arguments to pass to caller.
  ResponseValue NoArguments();
  // Success, a single argument |arg| to pass to caller.
  ResponseValue OneArgument(std::unique_ptr<base::Value> arg);
  // Success, two arguments |arg1| and |arg2| to pass to caller.
  // Note that use of this function may imply you
  // should be using the generated Result struct and ArgumentList.
  ResponseValue TwoArguments(std::unique_ptr<base::Value> arg1,
                             std::unique_ptr<base::Value> arg2);
  // Success, a list of arguments |results| to pass to caller.
  // - a std::unique_ptr<> for convenience, since callers usually get this from
  //   the result of a Create(...) call on the generated Results struct. For
  //   example, alarms::Get::Results::Create(alarm).
  ResponseValue ArgumentList(std::unique_ptr<base::ListValue> results);
  // Error. chrome.runtime.lastError.message will be set to |error|.
  ResponseValue Error(const std::string& error);
  // Error with formatting. Args are processed using
  // ErrorUtils::FormatErrorMessage, that is, each occurrence of * is replaced
  // by the corresponding |s*|:
  // Error("Error in *: *", "foo", "bar") <--> Error("Error in foo: bar").
  ResponseValue Error(const std::string& format, const std::string& s1);
  ResponseValue Error(const std::string& format,
                      const std::string& s1,
                      const std::string& s2);
  ResponseValue Error(const std::string& format,
                      const std::string& s1,
                      const std::string& s2,
                      const std::string& s3);
  // Error with a list of arguments |args| to pass to caller.
  // Using this ResponseValue indicates something is wrong with the API.
  // It shouldn't be possible to have both an error *and* some arguments.
  // Some legacy APIs do rely on it though, like webstorePrivate.
  ResponseValue ErrorWithArguments(std::unique_ptr<base::ListValue> args,
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
  ResponseAction RespondNow(ResponseValue result) WARN_UNUSED_RESULT;
  // Don't respond now, but promise to call Respond(...) later.
  ResponseAction RespondLater() WARN_UNUSED_RESULT;
  // Respond() was already called before Run() finished executing.
  //
  // Assume Run() uses some helper system that accepts callback that Respond()s.
  // If that helper system calls the synchronously in some cases, then use
  // this return value in those cases.
  //
  // FooExtensionFunction::Run() {
  //   Helper::FetchResults(..., base::Bind(&Success));
  //   if (did_respond()) return AlreadyResponded();
  //   return RespondLater();
  // }
  // FooExtensionFunction::Success() {
  //   Respond(...);
  // }
  //
  // Helper::FetchResults(..., callback) {
  //   if (...)
  //     callback.Run(..);  // Synchronously call |callback|.
  //   else
  //     // Asynchronously call |callback|.
  // }
  ResponseAction AlreadyResponded() WARN_UNUSED_RESULT;

  // This is the return value of the EXTENSION_FUNCTION_VALIDATE macro, which
  // needs to work from Run(), RunAsync(), and RunSync(). The former of those
  // has a different return type (ResponseAction) than the latter two (bool).
  static ResponseAction ValidationFailure(ExtensionFunction* function)
      WARN_UNUSED_RESULT;

  // If RespondLater() was returned from Run(), functions must at some point
  // call Respond() with |result| as their result.
  //
  // More specifically: call this iff Run() has already executed, it returned
  // RespondLater(), and Respond(...) hasn't already been called.
  void Respond(ResponseValue result);

  virtual ~ExtensionFunction();

  // Helper method for ExtensionFunctionDeleteTraits. Deletes this object.
  virtual void Destruct() const = 0;

  // Called after the response is sent, allowing the function to perform any
  // additional work or cleanup.
  virtual void OnResponded() {}

  // Return true if the argument to this function at |index| was provided and
  // is non-null.
  bool HasOptionalArgument(size_t index);

  // The extension that called this function.
  scoped_refptr<const extensions::Extension> extension_;

  // The arguments to the API. Only non-null if argument were specified.
  std::unique_ptr<base::ListValue> args_;

 private:
  friend class ResponseValueObject;

  // Call with true to indicate success, false to indicate failure. If this
  // failed, |error_| should be set.
  void SendResponseImpl(bool success);

  base::ElapsedTimer timer_;

  // The results of the API. This should be populated through the Respond()/
  // RespondNow() methods. In legacy implementations, this is set directly, and
  // should be set before calling SendResponse().
  std::unique_ptr<base::ListValue> results_;

  // Any detailed error from the API. This should be populated by the derived
  // class before Run() returns.
  std::string error_;

  // The callback to run once the function has done execution.
  ResponseCallback response_callback_;

  // Id of this request, used to map the response back to the caller.
  int request_id_;

  // The id of the profile of this function's extension.
  void* profile_id_;

  // The name of this function.
  const char* name_;

  // The URL of the frame which is making this request
  GURL source_url_;

  // True if the js caller provides a callback function to receive the response
  // of this call.
  bool has_callback_;

  // True if this callback should include information from incognito contexts
  // even if our profile_ is non-incognito. Note that in the case of a "split"
  // mode extension, this will always be false, and we will limit access to
  // data from within the same profile_ (either incognito or not).
  bool include_incognito_information_;

  // True if the call was made in response of user gesture.
  bool user_gesture_;

  // Any class that gets a malformed message should set this to true before
  // returning.  Usually we want to kill the message sending process.
  bool bad_message_;

  // The sample value to record with the histogram API when the function
  // is invoked.
  extensions::functions::HistogramValue histogram_value_;

  // The type of the JavaScript context where this call originated.
  extensions::Feature::Context source_context_type_;

  // The process ID of the page that triggered this function call, or -1
  // if unknown.
  int source_process_id_;

  // The response type of the function, if the response has been sent.
  std::unique_ptr<ResponseType> response_type_;

  // Whether this function has responded.
  // TODO(devlin): Replace this with response_type_ != null.
  bool did_respond_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionFunction);
};

// Extension functions that run on the UI thread. Most functions fall into
// this category.
class UIThreadExtensionFunction : public ExtensionFunction {
 public:
  UIThreadExtensionFunction();

  UIThreadExtensionFunction* AsUIThreadExtensionFunction() override;

  bool PreRunValidation(std::string* error) override;
  void SetBadMessage() final;

  // Called when a message was received.
  // Should return true if it processed the message.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Set the browser context which contains the extension that has originated
  // this function call.
  void set_browser_context(content::BrowserContext* context) {
    context_ = context;
  }
  content::BrowserContext* browser_context() const { return context_; }

  void SetRenderFrameHost(content::RenderFrameHost* render_frame_host);
  content::RenderFrameHost* render_frame_host() const {
    return render_frame_host_;
  }

  void set_dispatcher(const base::WeakPtr<
      extensions::ExtensionFunctionDispatcher>& dispatcher) {
    dispatcher_ = dispatcher;
  }
  extensions::ExtensionFunctionDispatcher* dispatcher() const {
    return dispatcher_.get();
  }

  void set_service_worker_version_id(int64_t version_id) {
    service_worker_version_id_ = version_id;
  }

  // Returns the web contents associated with the sending |render_frame_host_|.
  // This can be null.
  content::WebContents* GetSenderWebContents();

 protected:
  // Emits a message to the extension's devtools console.
  void WriteToConsole(content::ConsoleMessageLevel level,
                      const std::string& message);

  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<UIThreadExtensionFunction>;

  ~UIThreadExtensionFunction() override;

  void OnResponded() override;

  // Sets the Blob UUIDs whose ownership is being transferred to the renderer.
  void SetTransferredBlobUUIDs(const std::vector<std::string>& blob_uuids);

  // The BrowserContext of this function's extension.
  // TODO(devlin): Grr... protected members. Move this to be private.
  content::BrowserContext* context_;

 private:
  class RenderFrameHostTracker;

  void Destruct() const override;

  bool is_from_service_worker() const {
    return service_worker_version_id_ !=
           blink::mojom::kInvalidServiceWorkerVersionId;
  }

  // The dispatcher that will service this extension function call.
  base::WeakPtr<extensions::ExtensionFunctionDispatcher> dispatcher_;

  // The RenderFrameHost we will send responses to.
  content::RenderFrameHost* render_frame_host_;

  // If this ExtensionFunction was called by an extension Service Worker, then
  // this contains the worker's version id.
  int64_t service_worker_version_id_;

  std::unique_ptr<RenderFrameHostTracker> tracker_;

  // The blobs transferred to the renderer process.
  std::vector<std::string> transferred_blob_uuids_;

  DISALLOW_COPY_AND_ASSIGN(UIThreadExtensionFunction);
};

// Extension functions that run on the IO thread. This type of function avoids
// a roundtrip to and from the UI thread (because communication with the
// extension process happens on the IO thread). It's intended to be used when
// performance is critical (e.g. the webRequest API which can block network
// requests). Generally, UIThreadExtensionFunction is more appropriate and will
// be easier to use and interface with the rest of the browser.
// To use this, specify `"forIOThread": true` in the function's schema.
class IOThreadExtensionFunction : public ExtensionFunction {
 public:
  IOThreadExtensionFunction();

  IOThreadExtensionFunction* AsIOThreadExtensionFunction() override;
  void SetBadMessage() final;

  void set_ipc_sender(
      base::WeakPtr<extensions::IOThreadExtensionMessageFilter> ipc_sender,
      int routing_id) {
    ipc_sender_ = ipc_sender;
    routing_id_ = routing_id;
  }

  base::WeakPtr<extensions::IOThreadExtensionMessageFilter> ipc_sender_weak()
      const {
    return ipc_sender_;
  }

  int routing_id() const { return routing_id_; }

  void set_extension_info_map(const extensions::InfoMap* extension_info_map) {
    extension_info_map_ = extension_info_map;
  }
  const extensions::InfoMap* extension_info_map() const {
    return extension_info_map_.get();
  }

 protected:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;
  friend class base::DeleteHelper<IOThreadExtensionFunction>;

  ~IOThreadExtensionFunction() override;

  void Destruct() const override;

 private:
  base::WeakPtr<extensions::IOThreadExtensionMessageFilter> ipc_sender_;
  int routing_id_;

  scoped_refptr<const extensions::InfoMap> extension_info_map_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadExtensionFunction);
};

#endif  // EXTENSIONS_BROWSER_EXTENSION_FUNCTION_H_
