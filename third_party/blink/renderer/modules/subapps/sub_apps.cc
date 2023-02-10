// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/subapps/sub_apps.h"

#include <utility>

#include "base/check.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sub_apps_add_params.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sub_apps_list_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sub_apps_result_code.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using mojom::blink::SubAppsService;
using mojom::blink::SubAppsServiceAddParameters;
using mojom::blink::SubAppsServiceAddParametersPtr;
using mojom::blink::SubAppsServiceAddResultPtr;
using mojom::blink::SubAppsServiceListResultEntryPtr;
using mojom::blink::SubAppsServiceListResultPtr;
using mojom::blink::SubAppsServiceResultCode;

namespace {

const int kMaximumNumberOfSubappsPerAddCall = 7;

Vector<std::pair<String, V8SubAppsResultCode>> AddResultsFromMojo(
    Vector<SubAppsServiceAddResultPtr> add_results_mojo) {
  Vector<std::pair<String, V8SubAppsResultCode>> add_results_idl;
  for (auto& add_result : add_results_mojo) {
    auto result_code =
        add_result->result_code == SubAppsServiceResultCode::kSuccess
            ? V8SubAppsResultCode(V8SubAppsResultCode::Enum::kSuccess)
            : V8SubAppsResultCode(V8SubAppsResultCode::Enum::kFailure);
    add_results_idl.emplace_back(add_result->unhashed_app_id_path, result_code);
  }
  return add_results_idl;
}

Vector<SubAppsServiceAddParametersPtr> AddOptionsToMojo(
    HeapVector<std::pair<String, Member<SubAppsAddParams>>>
        sub_apps_to_add_idl) {
  Vector<SubAppsServiceAddParametersPtr> sub_apps_to_add_mojo;
  for (auto& [unhashed_app_id_path, add_params] : sub_apps_to_add_idl) {
    sub_apps_to_add_mojo.emplace_back(SubAppsServiceAddParameters::New(
        unhashed_app_id_path, add_params->installURL()));
  }
  return sub_apps_to_add_mojo;
}

HeapVector<std::pair<String, Member<SubAppsListResult>>> ListResultsFromMojo(
    Vector<SubAppsServiceListResultEntryPtr> sub_apps_list_mojo) {
  HeapVector<std::pair<String, Member<SubAppsListResult>>> sub_apps_list_idl;
  for (auto& sub_app_entry : sub_apps_list_mojo) {
    SubAppsListResult* list_result = SubAppsListResult::Create();
    list_result->setAppName(std::move(sub_app_entry->app_name));
    sub_apps_list_idl.emplace_back(
        std::move(sub_app_entry->unhashed_app_id_path), list_result);
  }
  return sub_apps_list_idl;
}

}  // namespace

// static
const char SubApps::kSupplementName[] = "SubApps";

SubApps::SubApps(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      service_(navigator.GetExecutionContext()) {}

// static
SubApps* SubApps::subApps(Navigator& navigator) {
  SubApps* subapps = Supplement<Navigator>::From<SubApps>(navigator);
  if (!subapps) {
    subapps = MakeGarbageCollected<SubApps>(navigator);
    ProvideTo(navigator, subapps);
  }
  return subapps;
}

void SubApps::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
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
        WTF::BindOnce(&SubApps::OnConnectionError, WrapWeakPersistent(this)));
  }
  return service_;
}

void SubApps::OnConnectionError() {
  service_.reset();
}

ScriptPromise SubApps::add(
    ScriptState* script_state,
    const HeapVector<std::pair<String, Member<SubAppsAddParams>>>&
        sub_apps_to_add,
    ExceptionState& exception_state) {
  // [SecureContext] from the IDL ensures this.
  DCHECK(ExecutionContext::From(script_state)->IsSecureContext());

  if (!CheckPreconditionsMaybeThrow(exception_state)) {
    return ScriptPromise();
  }

  LocalFrame* frame = GetSupplementable()->DomWindow()->GetFrame();
  // TODO(crbug.com/1326843): Maybe we don't need user activation if
  // the right policy is set.
  if (!LocalFrame::ConsumeTransientUserActivation(frame)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Unable to add sub-app. This API can only be called shortly after a "
        "user activation.");
    return ScriptPromise();
  }

  // TODO(crbug.com/1326843): Maybe we don't need to limit add() if the
  // right policy is set, we mainly want to avoid overwhelming the user with
  // a permissions prompt that lists dozens of apps to install.
  if (sub_apps_to_add.size() > kMaximumNumberOfSubappsPerAddCall) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "Unable to add sub-apps. The maximum number of apps added per call "
        "is " +
            String::Number(kMaximumNumberOfSubappsPerAddCall) + ", but " +
            String::Number(sub_apps_to_add.size()) + " were provided.");
    return ScriptPromise();
  }

  // Check that the arguments are root-relative paths.
  for (const auto& [unhashed_app_id_path, add_params] : sub_apps_to_add) {
    if (KURL(unhashed_app_id_path).IsValid() ||
        KURL(add_params->installURL()).IsValid()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "Arguments must be root-relative paths.");
      return ScriptPromise();
    }
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  GetService()->Add(
      AddOptionsToMojo(std::move(sub_apps_to_add)),
      resolver->WrapCallbackInScriptScope(
          WTF::BindOnce([](ScriptPromiseResolver* resolver,
                           Vector<SubAppsServiceAddResultPtr> results_mojo) {
            for (const auto& add_result : results_mojo) {
              if (add_result->result_code ==
                  SubAppsServiceResultCode::kFailure) {
                return resolver->Reject(
                    AddResultsFromMojo(std::move(results_mojo)));
              }
            }
            resolver->Resolve(AddResultsFromMojo(std::move(results_mojo)));
          })));
  return resolver->Promise();
}

ScriptPromise SubApps::list(ScriptState* script_state,
                            ExceptionState& exception_state) {
  if (!CheckPreconditionsMaybeThrow(exception_state)) {
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  GetService()->List(resolver->WrapCallbackInScriptScope(WTF::BindOnce(
      [](ScriptPromiseResolver* resolver, SubAppsServiceListResultPtr result) {
        if (result->result_code == SubAppsServiceResultCode::kSuccess) {
          resolver->Resolve(
              ListResultsFromMojo(std::move(result->sub_apps_list)));
        } else {
          resolver->Reject(V8ThrowDOMException::CreateOrDie(
              resolver->GetScriptState()->GetIsolate(),
              DOMExceptionCode::kOperationError,
              "Unable to list sub-apps. Check whether the calling app is "
              "installed."));
        }
      })));

  return resolver->Promise();
}

ScriptPromise SubApps::remove(ScriptState* script_state,
                              const String& unhashed_app_id_path,
                              ExceptionState& exception_state) {
  if (!CheckPreconditionsMaybeThrow(exception_state)) {
    return ScriptPromise();
  }

  // Check that the argument is a root-relative path.
  if (KURL(unhashed_app_id_path).IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Arguments must be root-relative paths.");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  GetService()->Remove(
      unhashed_app_id_path,
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          [](ScriptPromiseResolver* resolver, SubAppsServiceResultCode result) {
            if (result == SubAppsServiceResultCode::kSuccess) {
              resolver->Resolve();
            } else {
              resolver->Reject(V8ThrowDOMException::CreateOrDie(
                  resolver->GetScriptState()->GetIsolate(),
                  DOMExceptionCode::kOperationError,
                  "Unable to remove given sub-app. Check whether the calling "
                  "app is installed."));
            }
          })));

  return resolver->Promise();
}

bool SubApps::CheckPreconditionsMaybeThrow(ExceptionState& exception_state) {
  Navigator* const navigator = GetSupplementable();

  if (!navigator->DomWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The object is no longer associated to a document.");
    return false;
  }

  if (!navigator->DomWindow()->GetFrame()->IsMainFrame() ||
      navigator->DomWindow()->GetFrame()->GetPage()->IsPrerendering() ||
      navigator->DomWindow()->GetFrame()->IsInFencedFrameTree()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "API is only supported in primary top-level browsing contexts.");
    return false;
  }

  return true;
}

}  // namespace blink
