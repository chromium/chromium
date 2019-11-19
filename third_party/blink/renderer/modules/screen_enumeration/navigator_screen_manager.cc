// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_enumeration/navigator_screen_manager.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/renderer/modules/screen_enumeration/screen_manager.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

template <typename T>
class NavigatorScreenManagerImpl final
    : public GarbageCollected<NavigatorScreenManagerImpl<T>>,
      public Supplement<T> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorScreenManagerImpl<T>);

 public:
  static const char kSupplementName[];

  static NavigatorScreenManagerImpl<T>& From(T& supplementable) {
    NavigatorScreenManagerImpl<T>* supplement =
        Supplement<T>::template From<NavigatorScreenManagerImpl<T>>(
            supplementable);
    if (!supplement) {
      supplement =
          MakeGarbageCollected<NavigatorScreenManagerImpl<T>>(supplementable);
      Supplement<T>::ProvideTo(supplementable, supplement);
    }
    return *supplement;
  }

  explicit NavigatorScreenManagerImpl(T& supplementable)
      : Supplement<T>(supplementable) {}

  NavigatorScreenManagerImpl(const NavigatorScreenManagerImpl&) = delete;
  NavigatorScreenManagerImpl& operator=(const NavigatorScreenManagerImpl&) =
      delete;

  ScreenManager* GetScreen(ExecutionContext* execution_context) {
    if (!screen_manager_) {
      mojo::Remote<mojom::blink::ScreenEnumeration> backend;
      execution_context->GetBrowserInterfaceBroker().GetInterface(
          backend.BindNewPipeAndPassReceiver());
      screen_manager_ = MakeGarbageCollected<ScreenManager>(std::move(backend));
    }
    return screen_manager_;
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(screen_manager_);
    Supplement<T>::Trace(visitor);
  }

 private:
  Member<ScreenManager> screen_manager_;
};

// static
template <typename T>
const char NavigatorScreenManagerImpl<T>::kSupplementName[] =
    "NavigatorScreenManager";

}  // namespace

// static
ScreenManager* NavigatorScreenManager::screen(Navigator& navigator) {
  LocalFrame* local_frame = navigator.GetFrame();
  if (!local_frame) {
    return nullptr;
  }

  ExecutionContext* execution_context =
      local_frame->DomWindow()->GetExecutionContext();
  return NavigatorScreenManagerImpl<Navigator>::From(navigator).GetScreen(
      execution_context);
}

// static
ScreenManager* NavigatorScreenManager::screen(ScriptState* script_state,
                                              WorkerNavigator& navigator) {
  return NavigatorScreenManagerImpl<WorkerNavigator>::From(navigator).GetScreen(
      ExecutionContext::From(script_state));
}

}  // namespace blink
