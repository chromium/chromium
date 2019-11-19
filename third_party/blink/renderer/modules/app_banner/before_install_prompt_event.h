// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_APP_BANNER_BEFORE_INSTALL_PROMPT_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_APP_BANNER_BEFORE_INSTALL_PROMPT_EVENT_H_

#include <utility>
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/app_banner/app_banner.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/app_banner/app_banner_prompt_result.h"
#include "third_party/blink/renderer/modules/event_modules.h"

namespace blink {

class BeforeInstallPromptEvent;
class BeforeInstallPromptEventInit;

using UserChoiceProperty =
    ScriptPromiseProperty<Member<BeforeInstallPromptEvent>,
                          Member<AppBannerPromptResult>,
                          ToV8UndefinedGenerator>;

class BeforeInstallPromptEvent final
    : public Event,
      public mojom::blink::AppBannerEvent,
      public ActiveScriptWrappable<BeforeInstallPromptEvent>,
      public ContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(BeforeInstallPromptEvent, Dispose);
  USING_GARBAGE_COLLECTED_MIXIN(BeforeInstallPromptEvent);

 public:
  BeforeInstallPromptEvent(const AtomicString& name,
                           LocalFrame&,
                           mojo::PendingRemote<mojom::blink::AppBannerService>,
                           mojo::PendingReceiver<mojom::blink::AppBannerEvent>,
                           const Vector<String>& platforms);
  BeforeInstallPromptEvent(ExecutionContext*,
                           const AtomicString& name,
                           const BeforeInstallPromptEventInit*);
  ~BeforeInstallPromptEvent() override;

  static BeforeInstallPromptEvent* Create(
      const AtomicString& name,
      LocalFrame& frame,
      mojo::PendingRemote<mojom::blink::AppBannerService> service_remote,
      mojo::PendingReceiver<mojom::blink::AppBannerEvent> event_receiver,
      const Vector<String>& platforms) {
    return MakeGarbageCollected<BeforeInstallPromptEvent>(
        name, frame, std::move(service_remote), std::move(event_receiver),
        platforms);
  }

  static BeforeInstallPromptEvent* Create(
      ExecutionContext* execution_context,
      const AtomicString& name,
      const BeforeInstallPromptEventInit* init) {
    return MakeGarbageCollected<BeforeInstallPromptEvent>(execution_context,
                                                          name, init);
  }

  void Dispose();

  Vector<String> platforms() const;
  ScriptPromise userChoice(ScriptState*);
  ScriptPromise prompt(ScriptState*);

  const AtomicString& InterfaceName() const override;
  void preventDefault() override;

  // ScriptWrappable
  bool HasPendingActivity() const override;

  void Trace(blink::Visitor*) override;

 private:
  // mojom::blink::AppBannerEvent methods:
  void BannerAccepted(const String& platform) override;
  void BannerDismissed() override;

  mojo::Remote<mojom::blink::AppBannerService> banner_service_remote_;
  mojo::Receiver<mojom::blink::AppBannerEvent> receiver_{this};
  Vector<String> platforms_;
  Member<UserChoiceProperty> user_choice_;
};

DEFINE_TYPE_CASTS(BeforeInstallPromptEvent,
                  Event,
                  event,
                  event->InterfaceName() ==
                      event_interface_names::kBeforeInstallPromptEvent,
                  event.InterfaceName() ==
                      event_interface_names::kBeforeInstallPromptEvent);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_APP_BANNER_BEFORE_INSTALL_PROMPT_EVENT_H_
