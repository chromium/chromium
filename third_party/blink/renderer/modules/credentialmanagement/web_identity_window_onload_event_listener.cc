// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/web_identity_window_onload_event_listener.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/before_unload_event.h"
#include "third_party/blink/renderer/modules/credentialmanagement/web_identity_requester.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WebIdentityWindowOnloadEventListener::WebIdentityWindowOnloadEventListener(
    Document* document,
    WebIdentityRequester* requester)
    : document_(document), requester_(requester) {}

// WebIdentityWindowOnloadEventListener::Invoke() is not guaranteed to be called
// after all of the JavaScript "load" handlers.
void WebIdentityWindowOnloadEventListener::Invoke(
    ExecutionContext* execution_context,
    Event* event) {
  DCHECK_EQ(event->type(), event_type_names::kLoad);

  // document_ becomes null when its current execution context is no longer
  // valid and requester_ becomes null when CredentialsContainer is destroyed.
  if (!document_ || !requester_)
    return;

  // If the FedCmMultipleIdentityProviders flag is disabled, we use the
  // WebIdentityWindowOnloadEventListener purely for metrics purposes.
  // Specifically, we want to gather metrics on how much of a delay the window
  // onload event introduces.
  if (!RuntimeEnabledFeatures::FedCmMultipleIdentityProvidersEnabled(
          execution_context)) {
    document_->GetTaskRunner(TaskType::kInternalDefault)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&WebIdentityRequester::StopDelayTimer,
                                 WrapPersistent(requester_.Get()),
                                 /*timer_started_before_onload=*/true));
    return;
  }

  // Post a task to WebIdentityRequester::RequestToken() to guarantee that
  // WebIdentityRequester::RequestToken() is called after all the Javascript
  // "load" handlers.
  document_->GetTaskRunner(TaskType::kInternalDefault)
      ->PostTask(FROM_HERE, WTF::BindOnce(&WebIdentityRequester::RequestToken,
                                          WrapPersistent(requester_.Get())));
}

void WebIdentityWindowOnloadEventListener::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(requester_);
  NativeEventListener::Trace(visitor);
}

}  // namespace blink
