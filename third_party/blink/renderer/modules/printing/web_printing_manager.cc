// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/printing/web_printing_manager.h"

#include "printing/buildflags/buildflags.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

const char WebPrintingManager::kSupplementName[] = "PrintingManager";

WebPrintingManager* WebPrintingManager::GetWebPrintingManager(
    NavigatorBase& navigator) {
  WebPrintingManager* printing_manager =
      Supplement<NavigatorBase>::From<WebPrintingManager>(navigator);
  if (!printing_manager) {
    printing_manager = MakeGarbageCollected<WebPrintingManager>(navigator);
    ProvideTo(navigator, printing_manager);
  }
  return printing_manager;
}

WebPrintingManager::WebPrintingManager(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      printing_service_(navigator.GetExecutionContext()) {}

void WebPrintingManager::Trace(Visitor* visitor) const {
  visitor->Trace(printing_service_);
  ScriptWrappable::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
}

mojom::blink::WebPrintingService* WebPrintingManager::GetPrintingService() {
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
  if (!printing_service_.is_bound()) {
    auto* execution_context = GetSupplementable()->GetExecutionContext();
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        printing_service_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kNetworking)));
  }
  return printing_service_.get();
#else
  return nullptr;
#endif
}

}  // namespace blink
