// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/callback.h"
#include "base/run_loop.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/sensor/sensor_test_utils.h"
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

SensorTestContext::SensorTestContext() {
  // Sensor's constructor has a check for this that could be removed in the
  // future.
  testing_scope_.GetDocument().SetSecureContextStateForTesting(
      SecureContextState::kSecure);
  // Necessary for SensorProxy::ShouldSuspendUpdates() to work correctly.
  testing_scope_.GetPage().GetFocusController().SetFocused(true);

  testing_scope_.GetDocument().GetBrowserInterfaceBroker().SetBinderForTesting(
      device::mojom::blink::SensorProvider::Name_,
      WTF::BindRepeating(&SensorTestContext::BindSensorProviderRequest,
                         WTF::Unretained(this)));
}

SensorTestContext::~SensorTestContext() {
  testing_scope_.GetDocument().GetBrowserInterfaceBroker().SetBinderForTesting(
      device::mojom::blink::SensorProvider::Name_, {});
}

ExecutionContext* SensorTestContext::GetExecutionContext() const {
  return testing_scope_.GetExecutionContext();
}

void SensorTestContext::BindSensorProviderRequest(
    mojo::ScopedMessagePipeHandle handle) {
  sensor_provider_.Bind(
      device::mojom::SensorProviderRequest(std::move(handle)));
}

// SensorTestUtils

// static
void SensorTestUtils::WaitForEvent(EventTarget* event_target,
                                   const WTF::AtomicString& event_type) {
  base::RunLoop run_loop;
  auto* event_listener =
      MakeGarbageCollected<SyncEventListener>(run_loop.QuitClosure());
  event_target->addEventListener(event_type, event_listener);
  run_loop.Run();
  event_target->removeEventListener(event_type, event_listener);
}

}  // namespace blink
