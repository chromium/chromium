// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webshare/navigator_share.h"

#include <stdint.h>

#include <utility>

#include "base/files/safe_base_name.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_share_data.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

namespace blink {

namespace {

constexpr size_t kMaxSharedFileCount = 10;
constexpr uint32_t kMaxSharedFileBytes = 50U * 1024 * 1024;

constexpr uint32_t kMaxTitleLength = 16U * 1024;
#if BUILDFLAG(IS_ANDROID)
constexpr uint32_t kMaxTextLength = 120U * 1024;
#else
constexpr uint32_t kMaxTextLength = 1U * 1024 * 1024;
#endif
constexpr uint32_t kMaxUrlLength = 16U * 1024;

// Gets the human-friendly error message for a ShareError. |error| must not be
// ShareError::OK.
String ErrorToString(mojom::blink::ShareError error) {
  switch (error) {
    case mojom::blink::ShareError::OK:
      NOTREACHED_IN_MIGRATION();
      break;
    case mojom::blink::ShareError::INTERNAL_ERROR:
      return "Share failed";
    case mojom::blink::ShareError::PERMISSION_DENIED:
      return "Permission denied";
    case mojom::blink::ShareError::CANCELED:
      return "Share canceled";
  }
  NOTREACHED_IN_MIGRATION();
  return String();
}

bool HasFiles(const ShareData& data) {
  if (!data.hasFiles())
    return false;

  return !data.files().empty();
}

// Returns true unless |share(data)| would reject with TypeError.
// Populates |url| with the result of running the URL parser on |data.url|.
// If the return value is false and |exception_state| is non null, throws
// TypeError.
//
// https://w3c.github.io/web-share/level-2/#canshare-method
// https://w3c.github.io/web-share/level-2/#share-method
bool CanShareInternal(const LocalDOMWindow& window,
                      const ShareData& data,
                      KURL& url,
                      ExceptionState* exception_state) {
  if (!data.hasTitle() && !data.hasText() && !data.hasUrl() &&
      !HasFiles(data)) {
    if (exception_state) {
      exception_state->ThrowTypeError(
          "No known share data fields supplied. If using only new fields "
          "(other than title, text and url), you must feature-detect "
          "them first.");
    }
    return false;
  }

  if (data.hasUrl()) {
    url = window.CompleteURL(data.url());
    if (!url.IsValid() ||
        (!url.ProtocolIsInHTTPFamily() &&
         url.Protocol() != window.document()->BaseURL().Protocol())) {
      if (exception_state) {
        exception_state->ThrowTypeError("Invalid URL");
      }
      return false;
    }
  }

  return true;
}

}  // namespace

class NavigatorShare::ShareClientImpl final
    : public GarbageCollected<ShareClientImpl> {
 public:
  ShareClientImpl(NavigatorShare*,
                  bool has_files,
                  ScriptPromiseResolver<IDLUndefined>*);

  void Callback(mojom::blink::ShareError);

  void OnConnectionError();

  void Trace(Visitor* visitor) const {
    visitor->Trace(navigator_);
    visitor->Trace(resolver_);
  }

 private:
  WeakMember<NavigatorShare> navigator_;
  bool has_files_;
  Member<ScriptPromiseResolver<IDLUndefined>> resolver_;
  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;
};

NavigatorShare::ShareClientImpl::ShareClientImpl(
    NavigatorShare* navigator_share,
    bool has_files,
    ScriptPromiseResolver<IDLUndefined>* resolver)
    : navigator_(navigator_share),
      has_files_(has_files),
      resolver_(resolver),
      feature_handle_for_scheduler_(
          ExecutionContext::From(resolver_->GetScriptState())
              ->GetScheduler()
              ->RegisterFeature(
                  SchedulingPolicy::Feature::kWebShare,
                  {SchedulingPolicy::DisableBackForwardCache()})) {}

void NavigatorShare::ShareClientImpl::Callback(mojom::blink::ShareError error) {
  if (navigator_) {
    DCHECK(navigator_->clients_.Contains(this));
    navigator_->clients_.erase(this);
  }

  if (error == mojom::blink::ShareError::OK) {
    UseCounter::Count(ExecutionContext::From(resolver_->GetScriptState()),
                      has_files_
                          ? WebFeature::kWebShareSuccessfulContainingFiles
                          : WebFeature::kWebShareSuccessfulWithoutFiles);
    resolver_->Resolve();
  } else {
    UseCounter::Count(ExecutionContext::From(resolver_->GetScriptState()),
                      has_files_
                          ? WebFeature::kWebShareUnsuccessfulContainingFiles
                          : WebFeature::kWebShareUnsuccessfulWithoutFiles);
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        (error == mojom::blink::ShareError::PERMISSION_DENIED)
            ? DOMExceptionCode::kNotAllowedError
            : DOMExceptionCode::kAbortError,
        ErrorToString(error)));
  }
}

void NavigatorShare::ShareClientImpl::OnConnectionError() {
  resolver_->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError,
      "Internal error: could not connect to Web Share interface."));
}

NavigatorShare& NavigatorShare::From(Navigator& navigator) {
  NavigatorShare* supplement =
      Supplement<Navigator>::From<NavigatorShare>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorShare>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

void NavigatorShare::Trace(Visitor* visitor) const {
  visitor->Trace(service_remote_);
  visitor->Trace(clients_);
  Supplement<Navigator>::Trace(visitor);
}

const char NavigatorShare::kSupplementName[] = "NavigatorShare";

bool NavigatorShare::canShare(ScriptState* script_state,
                              const ShareData* data) {
  if (!script_state->ContextIsValid())
    return false;

  if (!ExecutionContext::From(script_state)
           ->IsFeatureEnabled(
               mojom::blink::PermissionsPolicyFeature::kWebShare)) {
    return false;
  }

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  KURL unused_url;
  return CanShareInternal(*window, *data, unused_url, nullptr);
}

bool NavigatorShare::canShare(ScriptState* script_state,
                              Navigator& navigator,
                              const ShareData* data) {
  return From(navigator).canShare(script_state, data);
}

ScriptPromise<IDLUndefined> NavigatorShare::share(
    ScriptState* script_state,
    const ShareData* data,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Internal error: window frame is missing (the navigator may be "
        "detached).");
    return EmptyPromise();
  }

  LocalDOMWindow* const window = LocalDOMWindow::From(script_state);
  ExecutionContext* const execution_context =
      ExecutionContext::From(script_state);

  if (!execution_context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kWebShare)) {
    window->CountUse(WebFeature::kWebSharePolicyDisallow);
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      "Permission denied");
    return EmptyPromise();
  }
  window->CountUse(WebFeature::kWebSharePolicyAllow);

// This is due to a limitation on Android, where we sometimes are not advised
// when the share completes. This goes against the web share spec to work around
// the platform-specific bug, it is explicitly skipping section §2.1.2 step 2 of
// the Web Share spec. https://www.w3.org/TR/web-share/#share-method
#if !BUILDFLAG(IS_ANDROID)
  if (!clients_.empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "An earlier share has not yet completed.");
    return EmptyPromise();
  }
#endif

  if (!LocalFrame::ConsumeTransientUserActivation(window->GetFrame())) {
    VLOG(1) << "Share without transient activation (user gesture)";
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Must be handling a user gesture to perform a share request.");
    return EmptyPromise();
  }

  if (window->GetFrame()->IsInFencedFrameTree()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Web Share is not allowed in a fenced frame tree.");
    return EmptyPromise();
  }

  KURL url;
  if (!CanShareInternal(*window, *data, url, &exception_state)) {
    DCHECK(exception_state.HadException());
    return EmptyPromise();
  }

  if (!service_remote_.is_bound()) {
    // See https://bit.ly/2S0zRAS for task types.
    window->GetFrame()->GetBrowserInterfaceBroker().GetInterface(
        service_remote_.BindNewPipeAndPassReceiver(
            window->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    service_remote_.set_disconnect_handler(WTF::BindOnce(
        &NavigatorShare::OnConnectionError, WrapWeakPersistent(this)));
    DCHECK(service_remote_.is_bound());
  }

  if ((data->hasTitle() && data->title().length() > kMaxTitleLength) ||
      (data->hasText() && data->text().length() > kMaxTextLength) ||
      (data->hasUrl() && data->url().length() > kMaxUrlLength)) {
    execution_context->AddConsoleMessage(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kWarning, "Share too large");
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      "Permission denied");
    return EmptyPromise();
  }

  bool has_files = HasFiles(*data);
  WTF::Vector<mojom::blink::SharedFilePtr> files;
  uint64_t total_bytes = 0;
  if (has_files) {
    files.ReserveInitialCapacity(data->files().size());
    for (const blink::Member<blink::File>& file : data->files()) {
      std::optional<base::SafeBaseName> name =
          base::SafeBaseName::Create(StringToFilePath(file->name()));
      if (!name) {
        execution_context->AddConsoleMessage(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning, "Unsafe file name");
        exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                          "Permission denied");
        return EmptyPromise();
      }

      total_bytes += file->size();
      files.push_back(mojom::blink::SharedFile::New(
          *name, file->GetBlobDataHandleWithKnownSize()));
    }

    if (files.size() > kMaxSharedFileCount ||
        total_bytes > kMaxSharedFileBytes) {
      execution_context->AddConsoleMessage(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning, "Share too large");
      exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                        "Permission denied");
      return EmptyPromise();
    }
  }

  if (has_files)
    UseCounter::Count(execution_context, WebFeature::kWebShareContainingFiles);
  if (data->hasTitle())
    UseCounter::Count(execution_context, WebFeature::kWebShareContainingTitle);
  if (data->hasText())
    UseCounter::Count(execution_context, WebFeature::kWebShareContainingText);
  if (data->hasUrl())
    UseCounter::Count(execution_context, WebFeature::kWebShareContainingUrl);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());

  ShareClientImpl* client =
      MakeGarbageCollected<ShareClientImpl>(this, has_files, resolver);
  clients_.insert(client);
  auto promise = resolver->Promise();

  service_remote_->Share(
      data->hasTitle() ? data->title() : g_empty_string,
      data->hasText() ? data->text() : g_empty_string, url, std::move(files),
      WTF::BindOnce(&ShareClientImpl::Callback, WrapPersistent(client)));

  return promise;
}

ScriptPromise<IDLUndefined> NavigatorShare::share(
    ScriptState* script_state,
    Navigator& navigator,
    const ShareData* data,
    ExceptionState& exception_state) {
  return From(navigator).share(script_state, data, exception_state);
}

void NavigatorShare::OnConnectionError() {
  HeapHashSet<Member<ShareClientImpl>> clients;
  clients_.swap(clients);
  for (auto& client : clients) {
    client->OnConnectionError();
  }
  service_remote_.reset();
}

}  // namespace blink
