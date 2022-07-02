// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/subapps/sub_apps.h"

#include <utility>

#include "base/check.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sub_apps_add_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
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

using mojom::blink::SubAppsServiceListResultPtr;
using mojom::blink::SubAppsServiceResult;

namespace {

Vector<std::pair<String, String>> ResultsFromMojo(
    Vector<mojom::blink::SubAppsServiceAddResultPtr> sub_apps_mojo) {
  Vector<std::pair<String, String>> subapps;
  for (const auto& pair : sub_apps_mojo) {
    std::string result;

    switch (pair->result_code) {
      case mojom::blink::SubAppsServiceAddResultCode::kSuccessNewInstall:
        result = "success-new-install";
        break;
      case mojom::blink::SubAppsServiceAddResultCode::kSuccessAlreadyInstalled:
        result = "success-already-installed";
        break;
      case mojom::blink::SubAppsServiceAddResultCode::kUserInstallDeclined:
        result = "user-install-declined";
        break;
      case mojom::blink::SubAppsServiceAddResultCode::kExpectedAppIdCheckFailed:
        result = "expected-app-id-check-failed";
        break;
      case mojom::blink::SubAppsServiceAddResultCode::kParentAppUninstalled:
        result = "parent-app-uninstalled";
        break;
      case mojom::blink::SubAppsServiceAddResultCode::kFailure:
        result = "failure";
        break;
    }
    subapps.emplace_back(pair->unhashed_app_id, result);
  }
  return subapps;
}

Vector<mojom::blink::SubAppsServiceAddInfoPtr> InstallParamsToMojo(
    const HeapVector<std::pair<String, Member<SubAppsAddOptions>>>
        sub_apps_idl) {
  Vector<mojom::blink::SubAppsServiceAddInfoPtr> subapps;
  for (const auto& [unhashed_app_id, install_options] : sub_apps_idl) {
    mojom::blink::SubAppsServiceAddInfoPtr mojom_pair =
        mojom::blink::SubAppsServiceAddInfo::New();
    mojom_pair->unhashed_app_id = unhashed_app_id;
    mojom_pair->install_url = KURL(install_options->installUrl());
    subapps.push_back(std::move(mojom_pair));
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

HeapMojoRemote<mojom::blink::SubAppsService>& SubApps::GetService() {
  if (!service_.is_bound()) {
    auto* context = GetSupplementable()->GetExecutionContext();
    context->GetBrowserInterfaceBroker().GetInterface(
        service_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    // In case the other endpoint gets disconnected, we want to reset our end of
    // the pipe as well so that we don't remain connected to a half-open pipe.
    service_.set_disconnect_handler(
        WTF::Bind(&SubApps::OnConnectionError, WrapWeakPersistent(this)));
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
  for (const auto& [manifest_id, install_options] : sub_apps) {
    KURL sub_app_install_url(install_options->installUrl());
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
      InstallParamsToMojo(sub_apps),
      resolver->WrapCallbackInScriptScope(WTF::Bind(
          [](ScriptPromiseResolver* resolver,
             Vector<mojom::blink::SubAppsServiceAddResultPtr> mojom_results) {
            resolver->Resolve(ResultsFromMojo(std::move(mojom_results)));
          })));
  return resolver->Promise();
}

ScriptPromise SubApps::list(ScriptState* script_state,
                            ExceptionState& exception_state) {
  if (!CheckPreconditionsMaybeThrow(exception_state)) {
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  GetService()->List(resolver->WrapCallbackInScriptScope(WTF::Bind(
      [](ScriptPromiseResolver* resolver, SubAppsServiceListResultPtr result) {
        if (result->code == SubAppsServiceResult::kSuccess) {
          resolver->Resolve(result->sub_app_ids);
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
      resolver->WrapCallbackInScriptScope(WTF::Bind(
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
