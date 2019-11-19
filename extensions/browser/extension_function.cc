// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_function.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_message_filter.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_messages.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-forward.h"

using content::BrowserThread;
using content::WebContents;
using extensions::ErrorUtils;
using extensions::ExtensionAPI;
using extensions::Feature;

namespace {

// Logs UMA about the performance for a given extension function run.
void LogUma(bool success,
            base::TimeDelta elapsed_time,
            extensions::functions::HistogramValue histogram_value) {
  // Note: Certain functions perform actions that are inherently slow - such as
  // anything waiting on user action. As such, we can't always assume that a
  // long execution time equates to a poorly-performing function.
  if (success) {
    if (elapsed_time < base::TimeDelta::FromMilliseconds(1)) {
      base::UmaHistogramSparse("Extensions.Functions.SucceededTime.LessThan1ms",
                               histogram_value);
    } else if (elapsed_time < base::TimeDelta::FromMilliseconds(5)) {
      base::UmaHistogramSparse("Extensions.Functions.SucceededTime.1msTo5ms",
                               histogram_value);
    } else if (elapsed_time < base::TimeDelta::FromMilliseconds(10)) {
      base::UmaHistogramSparse("Extensions.Functions.SucceededTime.5msTo10ms",
                               histogram_value);
    } else {
      base::UmaHistogramSparse("Extensions.Functions.SucceededTime.Over10ms",
                               histogram_value);
    }
    UMA_HISTOGRAM_TIMES("Extensions.Functions.SucceededTotalExecutionTime",
                        elapsed_time);
  } else {
    if (elapsed_time < base::TimeDelta::FromMilliseconds(1)) {
      base::UmaHistogramSparse("Extensions.Functions.FailedTime.LessThan1ms",
                               histogram_value);
    } else if (elapsed_time < base::TimeDelta::FromMilliseconds(5)) {
      base::UmaHistogramSparse("Extensions.Functions.FailedTime.1msTo5ms",
                               histogram_value);
    } else if (elapsed_time < base::TimeDelta::FromMilliseconds(10)) {
      base::UmaHistogramSparse("Extensions.Functions.FailedTime.5msTo10ms",
                               histogram_value);
    } else {
      base::UmaHistogramSparse("Extensions.Functions.FailedTime.Over10ms",
                               histogram_value);
    }
    UMA_HISTOGRAM_TIMES("Extensions.Functions.FailedTotalExecutionTime",
                        elapsed_time);
  }
}

void LogBadMessage(extensions::functions::HistogramValue histogram_value) {
  base::RecordAction(base::UserMetricsAction("BadMessageTerminate_EFD"));
  // Track the specific function's |histogram_value|, as this may indicate a
  // bug in that API's implementation.
  UMA_HISTOGRAM_ENUMERATION("Extensions.BadMessageFunctionName",
                            histogram_value,
                            extensions::functions::ENUM_BOUNDARY);
}

template <class T>
void ReceivedBadMessage(T* bad_message_sender,
                        extensions::bad_message::BadMessageReason reason,
                        extensions::functions::HistogramValue histogram_value) {
  LogBadMessage(histogram_value);
  // The renderer has done validation before sending extension api requests.
  // Therefore, we should never receive a request that is invalid in a way
  // that JSON validation in the renderer should have caught. It could be an
  // attacker trying to exploit the browser, so we crash the renderer instead.
  extensions::bad_message::ReceivedBadMessage(bad_message_sender, reason);
}

class ArgumentListResponseValue
    : public ExtensionFunction::ResponseValueObject {
 public:
  ArgumentListResponseValue(ExtensionFunction* function,
                            std::unique_ptr<base::ListValue> result) {
    SetFunctionResults(function, std::move(result));
    // It would be nice to DCHECK(error.empty()) but some legacy extension
    // function implementations... I'm looking at chrome.input.ime... do this
    // for some reason.
  }

  ~ArgumentListResponseValue() override {}

  bool Apply() override { return true; }
};

class ErrorWithArgumentsResponseValue : public ArgumentListResponseValue {
 public:
  ErrorWithArgumentsResponseValue(ExtensionFunction* function,
                                  std::unique_ptr<base::ListValue> result,
                                  const std::string& error)
      : ArgumentListResponseValue(function, std::move(result)) {
    SetFunctionError(function, error);
  }

  ~ErrorWithArgumentsResponseValue() override {}

  bool Apply() override { return false; }
};

class ErrorResponseValue : public ExtensionFunction::ResponseValueObject {
 public:
  ErrorResponseValue(ExtensionFunction* function, const std::string& error) {
    // It would be nice to DCHECK(!error.empty()) but too many legacy extension
    // function implementations don't set error but signal failure.
    SetFunctionError(function, error);
  }

  ~ErrorResponseValue() override {}

  bool Apply() override { return false; }
};

class BadMessageResponseValue : public ExtensionFunction::ResponseValueObject {
 public:
  explicit BadMessageResponseValue(ExtensionFunction* function) {
    function->SetBadMessage();
    NOTREACHED() << function->name() << ": bad message";
  }

  ~BadMessageResponseValue() override {}

  bool Apply() override { return false; }
};

class RespondNowAction : public ExtensionFunction::ResponseActionObject {
 public:
  typedef base::Callback<void(bool)> SendResponseCallback;
  RespondNowAction(ExtensionFunction::ResponseValue result,
                   const SendResponseCallback& send_response)
      : result_(std::move(result)), send_response_(send_response) {}
  ~RespondNowAction() override {}

  void Execute() override { send_response_.Run(result_->Apply()); }

 private:
  ExtensionFunction::ResponseValue result_;
  SendResponseCallback send_response_;
};

class RespondLaterAction : public ExtensionFunction::ResponseActionObject {
 public:
  ~RespondLaterAction() override {}

  void Execute() override {}
};

class AlreadyRespondedAction : public ExtensionFunction::ResponseActionObject {
 public:
  ~AlreadyRespondedAction() override {}

  void Execute() override {}
};

// Used in implementation of ScopedUserGestureForTests.
class UserGestureForTests {
 public:
  static UserGestureForTests* GetInstance();

  // Returns true if there is at least one ScopedUserGestureForTests object
  // alive.
  bool HaveGesture();

  // These should be called when a ScopedUserGestureForTests object is
  // created/destroyed respectively.
  void IncrementCount();
  void DecrementCount();

 private:
  UserGestureForTests();
  friend struct base::DefaultSingletonTraits<UserGestureForTests>;

  base::Lock lock_;  // for protecting access to |count_|
  int count_;
};

// static
UserGestureForTests* UserGestureForTests::GetInstance() {
  return base::Singleton<UserGestureForTests>::get();
}

UserGestureForTests::UserGestureForTests() : count_(0) {}

bool UserGestureForTests::HaveGesture() {
  base::AutoLock autolock(lock_);
  return count_ > 0;
}

void UserGestureForTests::IncrementCount() {
  base::AutoLock autolock(lock_);
  ++count_;
}

void UserGestureForTests::DecrementCount() {
  base::AutoLock autolock(lock_);
  --count_;
}


}  // namespace

void ExtensionFunction::ResponseValueObject::SetFunctionResults(
    ExtensionFunction* function,
    std::unique_ptr<base::ListValue> results) {
  DCHECK(!function->results_) << "Function " << function->name_
                              << "already has results set.";
  function->results_ = std::move(results);
}

void ExtensionFunction::ResponseValueObject::SetFunctionError(
    ExtensionFunction* function,
    const std::string& error) {
  DCHECK(function->error_.empty()) << "Function " << function->name_
                                   << "already has an error.";
  function->error_ = error;
}

// static
bool ExtensionFunction::ignore_all_did_respond_for_testing_do_not_use = false;

// static
const char ExtensionFunction::kUnknownErrorDoNotUse[] = "Unknown error.";

// Helper class to track the lifetime of ExtensionFunction's RenderFrameHost and
// notify the function when it is deleted, as well as forwarding any messages
// to the ExtensionFunction.
class ExtensionFunction::RenderFrameHostTracker
    : public content::WebContentsObserver {
 public:
  explicit RenderFrameHostTracker(ExtensionFunction* function)
      : content::WebContentsObserver(
            WebContents::FromRenderFrameHost(function->render_frame_host())),
        function_(function) {}

 private:
  // content::WebContentsObserver:
  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    if (render_frame_host == function_->render_frame_host())
      function_->SetRenderFrameHost(nullptr);
  }

  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override {
    return render_frame_host == function_->render_frame_host() &&
        function_->OnMessageReceived(message);
  }

  ExtensionFunction* function_;  // Owns us.

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostTracker);
};

ExtensionFunction::ExtensionFunction() = default;

ExtensionFunction::~ExtensionFunction() {
  if (dispatcher() && (render_frame_host() || is_from_service_worker())) {
    dispatcher()->OnExtensionFunctionCompleted(
        extension(), is_from_service_worker(), name());
  }

  // The extension function should always respond to avoid leaks in the
  // renderer, dangling callbacks, etc. The exception is if the system is
  // shutting down.
  extensions::ExtensionsBrowserClient* browser_client =
      extensions::ExtensionsBrowserClient::Get();
  DCHECK(!browser_client || browser_client->IsShuttingDown() || did_respond() ||
         ignore_all_did_respond_for_testing_do_not_use)
      << name();
}

bool ExtensionFunction::HasPermission() const {
  Feature::Availability availability =
      ExtensionAPI::GetSharedInstance()->IsAvailable(
          name_, extension_.get(), source_context_type_, source_url(),
          extensions::CheckAliasStatus::ALLOWED);
  return availability.is_available();
}

bool ExtensionFunction::PreRunValidation(std::string* error) {
  // TODO(crbug.com/625646) This is a partial fix to avoid crashes when certain
  // extension functions run during shutdown. Browser or Notification creation
  // for example create a ScopedKeepAlive, which hit a CHECK if the browser is
  // shutting down. This fixes the current problem as the known issues happen
  // through synchronous calls from Run(), but posted tasks will not be covered.
  // A possible fix would involve refactoring ExtensionFunction: unrefcount
  // here and use weakptrs for the tasks, then have it owned by something that
  // will be destroyed naturally in the course of shut down.
  if (extensions::ExtensionsBrowserClient::Get()->IsShuttingDown()) {
    *error = "The browser is shutting down.";
    return false;
  }

  return true;
}

ExtensionFunction::ResponseAction ExtensionFunction::RunWithValidation() {
  std::string error;
  if (!PreRunValidation(&error)) {
    DCHECK(!error.empty() || bad_message_);
    return bad_message_ ? ValidationFailure(this) : RespondNow(Error(error));
  }
  return Run();
}

bool ExtensionFunction::ShouldSkipQuotaLimiting() const {
  return false;
}

void ExtensionFunction::OnQuotaExceeded(const std::string& violation_error) {
  error_ = violation_error;
  SendResponseImpl(false);
}

void ExtensionFunction::SetArgs(base::Value args) {
  DCHECK(args.is_list());
  DCHECK(!args_.get());  // Should only be called once.
  args_ = base::ListValue::From(base::Value::ToUniquePtrValue(std::move(args)));
}

const base::ListValue* ExtensionFunction::GetResultList() const {
  return results_.get();
}

const std::string& ExtensionFunction::GetError() const {
  return error_;
}

void ExtensionFunction::SetBadMessage() {
  bad_message_ = true;

  if (render_frame_host()) {
    ReceivedBadMessage(render_frame_host()->GetProcess(),
                       is_from_service_worker()
                           ? extensions::bad_message::EFD_BAD_MESSAGE_WORKER
                           : extensions::bad_message::EFD_BAD_MESSAGE,
                       histogram_value());
  }
}

bool ExtensionFunction::user_gesture() const {
  return user_gesture_ || UserGestureForTests::GetInstance()->HaveGesture();
}

bool ExtensionFunction::OnMessageReceived(const IPC::Message& message) {
  return false;
}

void ExtensionFunction::SetRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  // An extension function from Service Worker does not have a RenderFrameHost.
  if (is_from_service_worker()) {
    DCHECK(!render_frame_host);
    return;
  }

  DCHECK_NE(render_frame_host_ == nullptr, render_frame_host == nullptr);
  render_frame_host_ = render_frame_host;
  tracker_.reset(render_frame_host ? new RenderFrameHostTracker(this)
                                   : nullptr);
}

content::WebContents* ExtensionFunction::GetSenderWebContents() {
  return render_frame_host_
             ? content::WebContents::FromRenderFrameHost(render_frame_host_)
             : nullptr;
}

ExtensionFunction::ResponseValue ExtensionFunction::NoArguments() {
  return ResponseValue(
      new ArgumentListResponseValue(this, std::make_unique<base::ListValue>()));
}

ExtensionFunction::ResponseValue ExtensionFunction::OneArgument(
    std::unique_ptr<base::Value> arg) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append(std::move(arg));
  return ResponseValue(new ArgumentListResponseValue(this, std::move(args)));
}

ExtensionFunction::ResponseValue ExtensionFunction::TwoArguments(
    std::unique_ptr<base::Value> arg1,
    std::unique_ptr<base::Value> arg2) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append(std::move(arg1));
  args->Append(std::move(arg2));
  return ResponseValue(new ArgumentListResponseValue(this, std::move(args)));
}

ExtensionFunction::ResponseValue ExtensionFunction::ArgumentList(
    std::unique_ptr<base::ListValue> args) {
  return ResponseValue(new ArgumentListResponseValue(this, std::move(args)));
}

ExtensionFunction::ResponseValue ExtensionFunction::Error(
    const std::string& error) {
  return ResponseValue(new ErrorResponseValue(this, error));
}

ExtensionFunction::ResponseValue ExtensionFunction::Error(
    const std::string& format,
    const std::string& s1) {
  return ResponseValue(
      new ErrorResponseValue(this, ErrorUtils::FormatErrorMessage(format, s1)));
}

ExtensionFunction::ResponseValue ExtensionFunction::Error(
    const std::string& format,
    const std::string& s1,
    const std::string& s2) {
  return ResponseValue(new ErrorResponseValue(
      this, ErrorUtils::FormatErrorMessage(format, s1, s2)));
}

ExtensionFunction::ResponseValue ExtensionFunction::Error(
    const std::string& format,
    const std::string& s1,
    const std::string& s2,
    const std::string& s3) {
  return ResponseValue(new ErrorResponseValue(
      this, ErrorUtils::FormatErrorMessage(format, s1, s2, s3)));
}

ExtensionFunction::ResponseValue ExtensionFunction::ErrorWithArguments(
    std::unique_ptr<base::ListValue> args,
    const std::string& error) {
  return ResponseValue(
      new ErrorWithArgumentsResponseValue(this, std::move(args), error));
}

ExtensionFunction::ResponseValue ExtensionFunction::BadMessage() {
  return ResponseValue(new BadMessageResponseValue(this));
}

ExtensionFunction::ResponseAction ExtensionFunction::RespondNow(
    ResponseValue result) {
  return ResponseAction(new RespondNowAction(
      std::move(result),
      base::Bind(&ExtensionFunction::SendResponseImpl, this)));
}

ExtensionFunction::ResponseAction ExtensionFunction::RespondLater() {
  return ResponseAction(new RespondLaterAction());
}

ExtensionFunction::ResponseAction ExtensionFunction::AlreadyResponded() {
  DCHECK(did_respond()) << "ExtensionFunction did not call Respond(),"
                           " but Run() returned AlreadyResponded()";
  return ResponseAction(new AlreadyRespondedAction());
}

// static
ExtensionFunction::ResponseAction ExtensionFunction::ValidationFailure(
    ExtensionFunction* function) {
  return function->RespondNow(function->BadMessage());
}

void ExtensionFunction::Respond(ResponseValue result) {
  SendResponseImpl(result->Apply());
}

void ExtensionFunction::OnResponded() {
  if (!transferred_blob_uuids_.empty()) {
    render_frame_host_->Send(
        new ExtensionMsg_TransferBlobs(transferred_blob_uuids_));
  }
}

bool ExtensionFunction::HasOptionalArgument(size_t index) {
  base::Value* value;
  return args_->Get(index, &value) && !value->is_none();
}

void ExtensionFunction::WriteToConsole(blink::mojom::ConsoleMessageLevel level,
                                       const std::string& message) {
  // Only the main frame handles dev tools messages.
  WebContents::FromRenderFrameHost(render_frame_host_)
      ->GetMainFrame()
      ->AddMessageToConsole(level, message);
}

void ExtensionFunction::SetTransferredBlobUUIDs(
    const std::vector<std::string>& blob_uuids) {
  DCHECK(transferred_blob_uuids_.empty());  // Should only be called once.
  transferred_blob_uuids_ = blob_uuids;
}

void ExtensionFunction::SendResponseImpl(bool success) {
  DCHECK(!response_callback_.is_null());
  DCHECK(!did_respond_) << name_;
  did_respond_ = true;

  ResponseType response = success ? SUCCEEDED : FAILED;
  if (bad_message_) {
    response = BAD_MESSAGE;
    LOG(ERROR) << "Bad extension message " << name_;
  }
  response_type_ = std::make_unique<ResponseType>(response);

  // If results were never set, we send an empty argument list.
  if (!results_)
    results_.reset(new base::ListValue());

  response_callback_.Run(response, *results_, GetError());
  LogUma(success, timer_.Elapsed(), histogram_value_);

  OnResponded();
}

ExtensionFunction::ScopedUserGestureForTests::ScopedUserGestureForTests() {
  UserGestureForTests::GetInstance()->IncrementCount();
}

ExtensionFunction::ScopedUserGestureForTests::~ScopedUserGestureForTests() {
  UserGestureForTests::GetInstance()->DecrementCount();
}
