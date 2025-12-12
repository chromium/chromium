// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/app_banner/app_banner_controller.h"

#include <memory>
#include <utility>
#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/app_banner/before_install_prompt_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"

namespace blink {

// static
const char AppBannerController::kSupplementName[] = "AppBannerController";

// static
AppBannerController* AppBannerController::From(LocalDOMWindow& window) {
  return Supplement<LocalDOMWindow>::From<AppBannerController>(window);
}

// static
void AppBannerController::BindReceiver(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::AppBannerController> receiver) {
  DCHECK(frame && frame->DomWindow());
  auto& window = *frame->DomWindow();
  auto* controller = AppBannerController::From(window);
  if (!controller) {
    controller = MakeGarbageCollected<AppBannerController>(
        base::PassKey<AppBannerController>(), window);
    Supplement<LocalDOMWindow>::ProvideTo(window, controller);
  }
  controller->Bind(std::move(receiver));
}

AppBannerController::AppBannerController(base::PassKey<AppBannerController>,
                                         LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window), receiver_(this, &window) {}

void AppBannerController::Bind(
    mojo::PendingReceiver<mojom::blink::AppBannerController> receiver) {
  // We only expect one BannerPromptRequest() to ever be in flight at a time,
  // and there shouldn't never be multiple callers bound at a time.
  receiver_.reset();
  // See https://bit.ly/2S0zRAS for task types.
  receiver_.Bind(std::move(receiver), GetSupplementable()->GetTaskRunner(
                                          TaskType::kMiscPlatformAPI));
}

void AppBannerController::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

void AppBannerController::BannerPromptRequest(
    mojo::PendingRemote<mojom::blink::AppBannerService> service_remote,
    mojo::PendingReceiver<mojom::blink::AppBannerEvent> event_receiver,
    const Vector<String>& platforms,
    BannerPromptRequestCallback callback) {
  // TODO(hajimehoshi): Add tests for the case the frame is detached.
  // TODO(http://crbug/1289079): Test that prompt() behaves correctly when
  // called in pagehide().

  mojom::AppBannerPromptReply reply =
      GetSupplementable()->DispatchEvent(*BeforeInstallPromptEvent::Create(
          event_type_names::kBeforeinstallprompt, *GetSupplementable(),
          std::move(service_remote), std::move(event_receiver), platforms)) ==
              DispatchEventResult::kNotCanceled
          ? mojom::AppBannerPromptReply::NONE
          : mojom::AppBannerPromptReply::CANCEL;

  std::move(callback).Run(reply);
}

}  // namespace blink
