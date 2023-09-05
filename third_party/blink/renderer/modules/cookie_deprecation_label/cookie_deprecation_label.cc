// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cookie_deprecation_label/cookie_deprecation_label.h"

#include <utility>

#include "base/check.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/cookie_deprecation_label/cookie_deprecation_label.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
const char CookieDeprecationLabel::kSupplementName[] = "CookieDeprecation";

// static
CookieDeprecationLabel* CookieDeprecationLabel::cookieDeprecationLabel(
    Navigator& navigator) {
  auto* supplement =
      Supplement<Navigator>::From<CookieDeprecationLabel>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<CookieDeprecationLabel>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement;
}

CookieDeprecationLabel::CookieDeprecationLabel(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      label_document_service_(navigator.DomWindow()) {}

CookieDeprecationLabel::~CookieDeprecationLabel() = default;

mojom::blink::CookieDeprecationLabelDocumentService*
CookieDeprecationLabel::GetDocumentService(ScriptState* script_state) {
  if (!label_document_service_.is_bound()) {
    ExecutionContext::From(script_state)
        ->GetBrowserInterfaceBroker()
        .GetInterface(label_document_service_.BindNewPipeAndPassReceiver(
            ExecutionContext::From(script_state)
                ->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return label_document_service_.get();
}

ScriptPromise CookieDeprecationLabel::getValue(ScriptState* script_state) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  GetDocumentService(script_state)
      ->GetValue(WTF::BindOnce(
          [](ScriptPromiseResolver* resolver, const String& label) {
            DCHECK(resolver);
            // The label will be null if cookie deprecation label is not allowed
            // for the profile.
            if (label.IsNull()) {
              resolver->Reject();
            } else {
              resolver->Resolve(label);
            }
          },
          WrapPersistent(resolver)));

  return promise;
}

void CookieDeprecationLabel::Trace(Visitor* visitor) const {
  visitor->Trace(label_document_service_);
  Supplement<Navigator>::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
