// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/permissions/testing/internals_permission.h"

#include <utility>

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission_automation.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/testing/internals.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// static
ScriptPromise InternalsPermission::setPermission(
    ScriptState* script_state,
    Internals&,
    const ScriptValue& raw_descriptor,
    const String& state,
    const String& origin,
    const String& embedding_origin,
    ExceptionState& exception_state) {
  mojom::blink::PermissionDescriptorPtr descriptor =
      ParsePermissionDescriptor(script_state, raw_descriptor, exception_state);
  if (exception_state.HadException() || !script_state->ContextIsValid())
    return ScriptPromise();

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  KURL url;
  if (origin.IsNull()) {
    const SecurityOrigin* security_origin = window->GetSecurityOrigin();
    if (security_origin->IsOpaque()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotAllowedError,
          "Unable to set permission for an opaque origin.");
      return ScriptPromise();
    }
    url = KURL(security_origin->ToString());
    DCHECK(url.IsValid());
  } else {
    url = KURL(origin);
    if (!url.IsValid()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                        "'" + origin + "' is not a valid URL.");
      return ScriptPromise();
    }
  }

  KURL embedding_url;
  if (embedding_origin.IsNull()) {
    Frame& top_frame = window->GetFrame()->Tree().Top();
    const SecurityOrigin* top_security_origin =
        top_frame.GetSecurityContext()->GetSecurityOrigin();
    if (top_security_origin->IsOpaque()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotAllowedError,
          "Unable to set permission for an opaque embedding origin.");
      return ScriptPromise();
    }
    embedding_url = KURL(top_security_origin->ToString());
  } else {
    embedding_url = KURL(embedding_origin);
    if (!embedding_url.IsValid()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "'" + embedding_origin + "' is not a valid URL.");
      return ScriptPromise();
    }
  }

  mojo::Remote<test::mojom::blink::PermissionAutomation> permission_automation;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      permission_automation.BindNewPipeAndPassReceiver());
  DCHECK(permission_automation.is_bound());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  auto* raw_permission_automation = permission_automation.get();
  raw_permission_automation->SetPermission(
      std::move(descriptor), ToPermissionStatus(state.Utf8()), url,
      embedding_url,
      WTF::Bind(
          // While we only really need |resolver|, we also take the
          // mojo::Remote<> so that it remains alive after this function exits.
          [](ScriptPromiseResolver* resolver,
             mojo::Remote<test::mojom::blink::PermissionAutomation>,
             bool success) {
            if (success)
              resolver->Resolve();
            else
              resolver->Reject();
          },
          WrapPersistent(resolver), std::move(permission_automation)));

  return promise;
}

}  // namespace blink
