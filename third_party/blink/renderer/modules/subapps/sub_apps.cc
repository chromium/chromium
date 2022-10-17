// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/subapps/sub_apps.h"

#include <utility>

#include "base/check.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sub_apps_add_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sub_apps_list_info.h"
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
using mojom::blink::SubAppsServiceAddInfo;
using mojom::blink::SubAppsServiceAddInfoPtr;
using mojom::blink::SubAppsServiceAddResultCode;
using mojom::blink::SubAppsServiceAddResultPtr;
using mojom::blink::SubAppsServiceListInfoPtr;
using mojom::blink::SubAppsServiceListResultPtr;
using mojom::blink::SubAppsServiceResult;

namespace {

Vector<std::pair<String, String>> AddResultsFromMojo(
    Vector<SubAppsServiceAddResultPtr> add_results_mojo) {
  Vector<std::pair<String, String>> add_results_idl;
  for (auto& add_result : add_results_mojo) {
    std::string result;

    switch (add_result->result_code) {
      case SubAppsServiceAddResultCode::kSuccessNewInstall:
        result = "success-new-install";
        break;
      case SubAppsServiceAddResultCode::kSuccessAlreadyInstalled:
        result = "success-already-installed";
        break;
      case SubAppsServiceAddResultCode::kUserInstallDeclined:
        result = "user-install-declined";
        break;
      case SubAppsServiceAddResultCode::kExpectedAppIdCheckFailed:
        result = "expected-app-id-check-failed";
        break;
      case SubAppsServiceAddResultCode::kParentAppUninstalled:
        result = "parent-app-uninstalled";
        break;
      case SubAppsServiceAddResultCode::kInstallUrlInvalid:
        result = "install-url-invalid";
        break;
      case SubAppsServiceAddResultCode::kNotValidManifestForWebApp:
        result = "invalid-manifest-for-web-app";
        break;
      case SubAppsServiceAddResultCode::kFailure:
        result = "failure";
        break;
    }
    add_results_idl.emplace_back(add_result->unhashed_app_id, result);
  }
  return add_results_idl;
}

Vector<SubAppsServiceAddInfoPtr> AddOptionsToMojo(
    HeapVector<std::pair<String, Member<SubAppsAddOptions>>> sub_apps_idl) {
  Vector<SubAppsServiceAddInfoPtr> sub_apps_mojo;
  for (auto& [unhashed_app_id, add_options] : sub_apps_idl) {
    sub_apps_mojo.emplace_back(SubAppsServiceAddInfo::New(
        unhashed_app_id, KURL(add_options->installUrl())));
  }
  return sub_apps_mojo;
}

HeapVector<std::pair<String, Member<SubAppsListInfo>>> ListResultsFromMojo(
    Vector<SubAppsServiceListInfoPtr> sub_apps_mojo) {
  HeapVector<std::pair<String, Member<SubAppsListInfo>>> subapps;
  for (auto& sub_app : sub_apps_mojo) {
    SubAppsListInfo* list_info = SubAppsListInfo::Create();
    list_info->setAppName(std::move(sub_app->app_name));
    subapps.emplace_back(std::move(sub_app->unhashed_app_id), list_info);
  }
  return subapps;
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
    const HeapVector<std::pair<String, Member<SubAppsAddOptions>>>& sub_apps,
    ExceptionState& exception_state) {
  // [SecureContext] from the IDL ensures this.
  DCHECK(ExecutionContext::From(script_state)->IsSecureContext());

  if (!CheckPreconditionsMaybeThrow(exception_state)) {
    return ScriptPromise();
  }

  Navigator* const navigator = GetSupplementable();
  const SecurityOrigin* frame_origin = navigator->DomWindow()
                                           ->GetFrame()
                                           ->GetSecurityContext()
                                           ->GetSecurityOrigin();

  // Check that each sub app's install url has the same origin as the parent
  // app, throw exception otherwise.
  for (const auto& [unhashed_app_id, add_options] : sub_apps) {
    KURL sub_app_install_url(add_options->installUrl());
    if (!frame_origin->IsSameOriginWith(
            SecurityOrigin::Create(sub_app_install_url).get())) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kURLMismatchError,
          "Install path must be a fully qualified URL matching the origin of "
          "the caller.");
      return ScriptPromise();
    }
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  GetService()->Add(
      AddOptionsToMojo(std::move(sub_apps)),
      resolver->WrapCallbackInScriptScope(
          WTF::BindOnce([](ScriptPromiseResolver* resolver,
                           Vector<SubAppsServiceAddResultPtr> results_mojo) {
            for (const auto& add_result : results_mojo) {
              if (add_result->result_code !=
                      SubAppsServiceAddResultCode::kSuccessNewInstall &&
                  add_result->result_code !=
                      SubAppsServiceAddResultCode::kSuccessAlreadyInstalled) {
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
        if (result->code == SubAppsServiceResult::kSuccess) {
          resolver->Resolve(ListResultsFromMojo(std::move(result->sub_apps)));
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
                              const String& unhashed_app_id,
                              ExceptionState& exception_state) {
  if (!CheckPreconditionsMaybeThrow(exception_state)) {
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  GetService()->Remove(
      unhashed_app_id,
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          [](ScriptPromiseResolver* resolver, SubAppsServiceResult result) {
            if (result == SubAppsServiceResult::kSuccess) {
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
