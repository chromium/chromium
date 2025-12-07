// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer_test_utils.h"

#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/pressure_update.mojom-blink.h"
#include "third_party/blink/public/mojom/compute_pressure/web_pressure_update.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

FakePressureService::FakePressureService() = default;
FakePressureService::~FakePressureService() = default;

void FakePressureService::BindRequest(mojo::ScopedMessagePipeHandle handle) {
  mojo::PendingReceiver<mojom::blink::WebPressureManager> receiver(
      std::move(handle));
  DCHECK(!manager_receiver_.is_bound());
  manager_receiver_.Bind(std::move(receiver));
  manager_receiver_.set_disconnect_handler(
      BindOnce(&FakePressureService::OnConnectionError, Unretained(this)));
}

void FakePressureService::AddClient(
    device::mojom::blink::PressureSource source,
    mojo::PendingAssociatedRemote<mojom::blink::WebPressureClient> client,
    AddClientCallback callback) {
  client_remote_.Bind(std::move(client));

  std::move(callback).Run(
      device::mojom::blink::PressureManagerAddClientResult::kOk);
}

void FakePressureService::SendUpdate(
    mojom::blink::WebPressureUpdatePtr update) {
  client_remote_->OnPressureUpdated(std::move(update));
}

void FakePressureService::OnConnectionError() {
  manager_receiver_.reset();
  client_remote_.reset();
}

ComputePressureTestingContext::ComputePressureTestingContext(
    FakePressureService* mock_pressure_service) {
  DomWindow()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::WebPressureManager::Name_,
      BindRepeating(&FakePressureService::BindRequest,
                    Unretained(mock_pressure_service)));
}

ComputePressureTestingContext::~ComputePressureTestingContext() {
  // Remove the testing binder to avoid crashes between tests caused by
  // our mocks rebinding an already-bound Binding.
  // See https://crbug.com/1010116 for more information.
  DomWindow()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::WebPressureManager::Name_, {});
}

LocalDOMWindow* ComputePressureTestingContext::DomWindow() {
  return testing_scope_.GetFrame().DomWindow();
}

ScriptState* ComputePressureTestingContext::GetScriptState() {
  return testing_scope_.GetScriptState();
}

ExceptionState& ComputePressureTestingContext::GetExceptionState() {
  return testing_scope_.GetExceptionState();
}

}  // namespace blink
