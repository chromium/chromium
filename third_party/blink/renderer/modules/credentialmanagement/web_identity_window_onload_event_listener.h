// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_WEB_IDENTITY_WINDOW_ONLOAD_EVENT_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_WEB_IDENTITY_WINDOW_ONLOAD_EVENT_LISTENER_H_

#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/modules/credentialmanagement/web_identity_requester.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class Document;
class Event;
class ExecutionContext;
class WebIdentityRequester;

// Helper class used to set up a window onload listener for documents with FedCM
// multiple IDP support enabled. More specifically, to support multiple identity
// providers through multiple navigator.credentials.get calls, we need to wait
// for all the get calls in window onload to be loaded before displaying the
// FedCM prompt.
class WebIdentityWindowOnloadEventListener : public NativeEventListener {
 public:
  WebIdentityWindowOnloadEventListener(Document*, WebIdentityRequester*);

  void Trace(Visitor* visitor) const override;

 private:
  // Invoked when the window onload event has concluded.
  void Invoke(ExecutionContext*, Event* event) override;

  Member<Document> document_;
  Member<WebIdentityRequester> requester_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_WEB_IDENTITY_WINDOW_ONLOAD_EVENT_LISTENER_H_
