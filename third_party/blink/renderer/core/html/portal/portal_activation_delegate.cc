// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/portal_activation_delegate.h"

#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class PromiseActivationDelegate
    : public GarbageCollected<PromiseActivationDelegate>,
      public PortalActivationDelegate {
 public:
  PromiseActivationDelegate(ScriptPromiseResolver* resolver,
                            const ExceptionContext& exception_context)
      : resolver_(resolver), exception_context_(exception_context) {}

  virtual ~PromiseActivationDelegate() = default;

  void ActivationDidSucceed() final { resolver_->Resolve(); }

  void ActivationDidFail(const String& message) final {
    ScriptState* script_state = resolver_->GetScriptState();
    ScriptState::Scope scope(script_state);
    ExceptionState exception_state(script_state->GetIsolate(),
                                   exception_context_);
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      message);
    resolver_->Reject(exception_state);
  }

  void ActivationWasAbandoned() final { resolver_->Detach(); }

  void Trace(Visitor* visitor) const final { visitor->Trace(resolver_); }

 private:
  Member<ScriptPromiseResolver> resolver_;

  // Needed to reconstruct ExceptionState. Ideally these would be bundled.
  // See https://crbug.com/991544.
  ExceptionContext exception_context_;
};

}  // namespace

PortalActivationDelegate* PortalActivationDelegate::ForPromise(
    ScriptPromiseResolver* resolver,
    const ExceptionContext& exception_context) {
  return MakeGarbageCollected<PromiseActivationDelegate>(resolver,
                                                         exception_context);
}

namespace {

class ConsoleActivationDelegate
    : public GarbageCollected<ConsoleActivationDelegate>,
      public PortalActivationDelegate {
 public:
  explicit ConsoleActivationDelegate(ConsoleLogger* logger) : logger_(logger) {}

  void ActivationDidSucceed() final {}

  void ActivationDidFail(const String& message) final {
    logger_->AddConsoleMessage(mojom::blink::ConsoleMessageSource::kRendering,
                               mojom::blink::ConsoleMessageLevel::kWarning,
                               message);
  }

  void ActivationWasAbandoned() final {}

  void Trace(Visitor* visitor) const final { visitor->Trace(logger_); }

 private:
  Member<ConsoleLogger> logger_;
};

}  // namespace

PortalActivationDelegate* PortalActivationDelegate::ForConsole(
    ConsoleLogger* logger) {
  return MakeGarbageCollected<ConsoleActivationDelegate>(logger);
}

}  // namespace blink
