// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/subapps/sub_apps.h"

#include <utility>

#include "base/check.h"
#include "base/types/expected.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sub_apps_add_response.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sub_apps_list_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sub_apps_remove_response.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8-isolate.h"

namespace blink {

using mojom::blink::SubAppsService;
using mojom::blink::SubAppsServiceAddResultPtr;
using mojom::blink::SubAppsServiceAddResultType;
using mojom::blink::SubAppsServiceListResultEntryPtr;
using mojom::blink::SubAppsServiceRemoveResultPtr;
using mojom::blink::SubAppsServiceRemoveResultType;
using mojom::blink::SubAppsServiceResultCode;

namespace {

DOMException* MojoAddResultTypeToDOMException(
    SubAppsServiceAddResultType result_type) {
  switch (result_type) {
    case SubAppsServiceAddResultType::kSuccess:
      return nullptr;
    case SubAppsServiceAddResultType::kScopeOverlap:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kConstraintError,
          "The scope of the sub-app overlaps with an existing app");
    case SubAppsServiceAddResultType::kRecursiveInstall:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kConstraintError, "Cannot install parent app");
    case SubAppsServiceAddResultType::kInvalidManifest:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kDataError,
          "The web manifest of the sub-app is invalid.");
    case SubAppsServiceAddResultType::kAlreadyInstalled:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          "The sub-app is already installed.");
    case SubAppsServiceAddResultType::kGenericError:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError,
          "A generic failure occurred during installation.");
  }
}

DOMException* MojoRemoveResultTypeToDOMException(
    SubAppsServiceRemoveResultType result_type) {
  switch (result_type) {
    case SubAppsServiceRemoveResultType::kSuccess:
      return nullptr;
    case SubAppsServiceRemoveResultType::kNotFound:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError,
          "The sub-app is not installed under this parent app.");
    case SubAppsServiceRemoveResultType::kGenericError:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError,
          "A generic failure occurred during removal.");
  }
}

v8::Local<v8::Value> MojoResultCodeToDOMException(
    SubAppsServiceResultCode result,
    v8::Isolate* isolate) {
  switch (result) {
    case SubAppsServiceResultCode::kWrongContext:
      return V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kNotSupportedError,
          "The API was called in a wrong context. Possibly, the current app "
          "was removed, or it is a sub app");

    case SubAppsServiceResultCode::kUserDeclined:
      return V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kNotAllowedError,
          "The user declined the installation prompt.");

    case SubAppsServiceResultCode::kTotalLimitExceeded:
      return V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kQuotaExceededError,
          "The limit for sub app installs per parent app is exceeded");

    case SubAppsServiceResultCode::kPerPromptLimitExceeded:
      return V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kQuotaExceededError,
          "The limit for sub app installs per single prompt is exceeded");

    case SubAppsServiceResultCode::kWebAppsNotUserInstallable:
      return V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kNotSupportedError,
          "Web apps are not user installable");

    case SubAppsServiceResultCode::kGenericError:
      return V8ThrowDOMException::CreateOrDie(isolate,
                                              DOMExceptionCode::kOperationError,
                                              "Generic error has occurred");
  }
}

HeapVector<std::pair<String, Member<SubAppsListResult>>> ListResultsFromMojo(
    Vector<SubAppsServiceListResultEntryPtr> sub_apps_list_mojo) {
  HeapVector<std::pair<String, Member<SubAppsListResult>>> sub_apps_list_idl;
  for (auto& sub_app_entry : sub_apps_list_mojo) {
    SubAppsListResult* list_result = SubAppsListResult::Create();
    list_result->setAppName(std::move(sub_app_entry->app_name));
    sub_apps_list_idl.emplace_back(std::move(sub_app_entry->manifest_id),
                                   list_result);
  }
  return sub_apps_list_idl;
}

bool IsValidPath(const String& path) {
  if (KURL(path).IsValid() || !path.starts_with('/') || path.empty() ||
      path.starts_with("//")) {
    return false;
  }

  KURL dummy_base("https://example.com/");
  KURL resolved_url(dummy_base, path);

  return resolved_url.IsValid();
}

}  // namespace

// static
const char SubApps::kSupplementName[] = "SubApps";

SubApps::SubApps(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      service_(window.GetExecutionContext()) {}

// static
SubApps* SubApps::subApps(LocalDOMWindow& window) {
  SubApps* subapps = Supplement<LocalDOMWindow>::From<SubApps>(window);
  if (!subapps) {
    subapps = MakeGarbageCollected<SubApps>(window);
    ProvideTo(window, subapps);
  }
  return subapps;
}

void SubApps::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(service_);
}

HeapMojoRemote<SubAppsService>& SubApps::GetService() {
  if (!service_.is_bound()) {
    auto* context = GetSupplementable()->GetExecutionContext();
    context->GetBrowserInterfaceBroker().GetInterface(
        service_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    // In case the other endpoint gets disconnected, we want to reset our end of
    // the pipe as well so that we don't remain connected to a half-open pipe.
    service_.set_disconnect_handler(
        BindOnce(&SubApps::OnConnectionError, WrapWeakPersistent(this)));
  }
  return service_;
}

void SubApps::OnConnectionError() {
  service_.reset();
}

ScriptPromise<SubAppsAddResponse> SubApps::add(
    ScriptState* script_state,
    const Vector<String>& install_paths,
    ExceptionState& exception_state) {
  // [SecureContext] from the IDL ensures this.
  DCHECK(ExecutionContext::From(script_state)->IsSecureContext());

  if (!CheckPreconditionsMaybeThrow(script_state, exception_state)) {
    return ScriptPromise<SubAppsAddResponse>();
  }

  // Check that the arguments are root-relative paths.
  for (const auto& install_path : install_paths) {
    if (!IsValidPath(install_path)) {
      exception_state.ThrowTypeError("Arguments must be root-relative paths.");
      return ScriptPromise<SubAppsAddResponse>();
    }
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<SubAppsAddResponse>>(
          script_state);
  ScriptPromise<SubAppsAddResponse> promise = resolver->Promise();

  GetService()->Add(
      install_paths,
      resolver->WrapCallbackInScriptScope(
          BindOnce([](ScriptPromiseResolver<SubAppsAddResponse>* resolver,
                      base::expected<Vector<SubAppsServiceAddResultPtr>,
                                     SubAppsServiceResultCode> result) {
            if (!result.has_value()) {
              resolver->Reject(MojoResultCodeToDOMException(
                  result.error(), resolver->GetScriptState()->GetIsolate()));
              return;
            }

            SubAppsAddResponse* response = SubAppsAddResponse::Create();
            Vector<std::pair<String, String>> installed_apps;
            HeapVector<std::pair<String, Member<DOMException>>> failed_apps;

            for (const auto& add_result : result.value()) {
              if (add_result->result_type ==
                  SubAppsServiceAddResultType::kSuccess) {
                installed_apps.emplace_back(add_result->install_path,
                                            add_result->manifest_id);
              } else {
                failed_apps.emplace_back(
                    add_result->install_path,
                    MojoAddResultTypeToDOMException(add_result->result_type));
              }
            }

            response->setInstalledApps(installed_apps);
            response->setFailedApps(failed_apps);
            resolver->Resolve(response);
          })));

  return promise;
}

ScriptPromise<IDLRecord<IDLUSVString, SubAppsListResult>> SubApps::list(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!CheckPreconditionsMaybeThrow(script_state, exception_state)) {
    return ScriptPromise<IDLRecord<IDLUSVString, SubAppsListResult>>();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLRecord<IDLUSVString, SubAppsListResult>>>(
      script_state);
  GetService()->List(resolver->WrapCallbackInScriptScope(BindOnce(
      [](ScriptPromiseResolver<IDLRecord<IDLUSVString, SubAppsListResult>>*
             resolver,
         base::expected<Vector<SubAppsServiceListResultEntryPtr>,
                        SubAppsServiceResultCode> result) {
        if (result.has_value()) {
          resolver->Resolve(ListResultsFromMojo(std::move(result.value())));
        } else {
          resolver->Reject(MojoResultCodeToDOMException(
              result.error(), resolver->GetScriptState()->GetIsolate()));
        }
      })));

  return resolver->Promise();
}

ScriptPromise<SubAppsRemoveResponse> SubApps::remove(
    ScriptState* script_state,
    const Vector<String>& manifest_ids,
    ExceptionState& exception_state) {
  if (!CheckPreconditionsMaybeThrow(script_state, exception_state)) {
    return ScriptPromise<SubAppsRemoveResponse>();
  }

  // Check that the arguments are root-relative paths.
  for (const auto& manifest_id : manifest_ids) {
    if (!IsValidPath(manifest_id)) {
      exception_state.ThrowTypeError("Arguments must be root-relative paths.");
      return ScriptPromise<SubAppsRemoveResponse>();
    }
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<SubAppsRemoveResponse>>(
          script_state);
  ScriptPromise<SubAppsRemoveResponse> promise = resolver->Promise();

  GetService()->Remove(
      manifest_ids,
      resolver->WrapCallbackInScriptScope(
          BindOnce([](ScriptPromiseResolver<SubAppsRemoveResponse>* resolver,
                      base::expected<Vector<SubAppsServiceRemoveResultPtr>,
                                     SubAppsServiceResultCode> result) {
            if (!result.has_value()) {
              resolver->Reject(MojoResultCodeToDOMException(
                  result.error(), resolver->GetScriptState()->GetIsolate()));
              return;
            }

            SubAppsRemoveResponse* response = SubAppsRemoveResponse::Create();
            Vector<String> removed_apps;
            HeapVector<std::pair<String, Member<DOMException>>> failed_apps;

            for (const auto& remove_result : result.value()) {
              if (remove_result->result_type ==
                  SubAppsServiceRemoveResultType::kSuccess) {
                removed_apps.push_back(remove_result->manifest_id);
              } else {
                failed_apps.emplace_back(remove_result->manifest_id,
                                         MojoRemoveResultTypeToDOMException(
                                             remove_result->result_type));
              }
            }

            response->setRemovedApps(removed_apps);
            response->setFailedApps(failed_apps);
            resolver->Resolve(response);
          })));

  return promise;
}

bool SubApps::CheckPreconditionsMaybeThrow(ScriptState* script_state,
                                           ExceptionState& exception_state) {
  if (!ExecutionContext::From(script_state)
           ->IsFeatureEnabled(
               network::mojom::PermissionsPolicyFeature::kSubApps)) {
    exception_state.ThrowSecurityError(
        "The executing top-level browsing context is not granted the "
        "\"sub-apps\" permissions policy.");
    return false;
  }

  LocalDOMWindow* const window = GetSupplementable();

  if (!window->GetFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "The object is no longer associated to a document.");
    return false;
  }

  if (!window->GetFrame()->IsMainFrame() ||
      window->GetFrame()->GetPage()->IsPrerendering() ||
      window->GetFrame()->IsInFencedFrameTree()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "API is only supported in primary top-level browsing contexts.");
    return false;
  }

  return true;
}

}  // namespace blink
