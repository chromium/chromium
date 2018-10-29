// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_FETCH_BACKGROUND_FETCH_RECORD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_FETCH_BACKGROUND_FETCH_RECORD_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Request;
class Response;

class MODULES_EXPORT BackgroundFetchRecord final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  BackgroundFetchRecord(Request* request, Response* response, bool aborted);
  ~BackgroundFetchRecord() override;

  Request* request() const;
  ScriptPromise responseReady(ScriptState* script_state);

  void Trace(blink::Visitor* visitor) override;

 private:
  using ResponseReadyProperty =
      ScriptPromiseProperty<Member<BackgroundFetchRecord>,
                            Member<Response>,
                            Member<DOMException>>;
  Member<Request> request_;
  Member<Response> response_;
  Member<ResponseReadyProperty> response_ready_property_;

  // Whether this record belongs to a fetch that was aborted.
  bool aborted_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_FETCH_BACKGROUND_FETCH_RECORD_H_
