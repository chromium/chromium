// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/sensor_test_utils.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/sensor/web_sensor_provider.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// An event listener that invokes |invocation_callback| when it is called.
class SyncEventListener final : public NativeEventListener {
 public:
  SyncEventListener(base::OnceClosure invocation_callback)
      : invocation_callback_(std::move(invocation_callback)) {}

  void Invoke(ExecutionContext*, Event*) override {
    DCHECK(invocation_callback_);
    std::move(invocation_callback_).Run();
  }

 private:
  base::OnceClosure invocation_callback_;
};

}  // namespace

// SensorTestContext

SensorTestContext::SensorTestContext()
    : testing_scope_(KURL("https://example.com")) {
  // Necessary for SensorProxy::ShouldSuspendUpdates() to work correctly.
  testing_scope_.GetPage().GetFocusController().SetFocused(true);

  testing_scope_.GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::WebSensorProvider::Name_,
      WTF::BindRepeating(&SensorTestContext::BindSensorProviderRequest,
                         WTF::Unretained(this)));
}

SensorTestContext::~SensorTestContext() {
  testing_scope_.GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::WebSensorProvider::Name_, {});
}

ExecutionContext* SensorTestContext::GetExecutionContext() const {
  return testing_scope_.GetExecutionContext();
}

ScriptState* SensorTestContext::GetScriptState() const {
  return testing_scope_.GetScriptState();
}

void SensorTestContext::BindSensorProviderRequest(
    mojo::ScopedMessagePipeHandle handle) {
  sensor_provider_.Bind(
      mojo::PendingReceiver<device::mojom::SensorProvider>(std::move(handle)));
}

// SensorTestUtils

// static
void SensorTestUtils::WaitForEvent(EventTarget* event_target,
                                   const WTF::AtomicString& event_type) {
  base::RunLoop run_loop;
  auto* event_listener =
      MakeGarbageCollected<SyncEventListener>(run_loop.QuitClosure());
  event_target->addEventListener(event_type, event_listener,
                                 /*use_capture=*/false);
  run_loop.Run();
  event_target->removeEventListener(event_type, event_listener,
                                    /*use_capture=*/false);
}

}  // namespace blink
