// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/pressure_client_impl.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer_manager.h"
#include "third_party/blink/renderer/modules/document_picture_in_picture/picture_in_picture_controller_impl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using device::mojom::blink::PressureSource;
using device::mojom::blink::PressureState;

namespace blink {

namespace {

V8PressureState::Enum PressureStateToV8PressureState(PressureState state) {
  switch (state) {
    case PressureState::kNominal:
      return V8PressureState::Enum::kNominal;
    case PressureState::kFair:
      return V8PressureState::Enum::kFair;
    case PressureState::kSerious:
      return V8PressureState::Enum::kSerious;
    case PressureState::kCritical:
      return V8PressureState::Enum::kCritical;
  }
  NOTREACHED_NORETURN();
}

V8PressureSource::Enum PressureSourceToV8PressureSource(PressureSource source) {
  switch (source) {
    case PressureSource::kCpu:
      return V8PressureSource::Enum::kCpu;
  }
  NOTREACHED_NORETURN();
}

}  // namespace

PressureClientImpl::PressureClientImpl(ExecutionContext* context,
                                       PressureObserverManager* manager)
    : ExecutionContextClient(context),
      manager_(manager),
      receiver_(this, context) {}

PressureClientImpl::~PressureClientImpl() = default;

void PressureClientImpl::OnPressureUpdated(
    device::mojom::blink::PressureUpdatePtr update) {
  if (!PassesPrivacyTest()) {
    return;
  }

  auto source = PressureSourceToV8PressureSource(update->source);
  // New observers may be created and added. Take a snapshot so as
  // to safely iterate.
  HeapVector<Member<blink::PressureObserver>> observers(observers_);
  for (const auto& observer : observers) {
    observer->OnUpdate(GetExecutionContext(), source,
                       PressureStateToV8PressureState(update->state),
                       ConvertTimeToDOMHighResTimeStamp(update->timestamp));
  }
}

void PressureClientImpl::AddObserver(PressureObserver* observer) {
  observers_.insert(observer);
}

void PressureClientImpl::RemoveObserver(PressureObserver* observer) {
  observers_.erase(observer);
  if (observers_.empty()) {
    Reset();
  }
}

mojo::PendingRemote<device::mojom::blink::PressureClient>
PressureClientImpl::BindNewPipeAndPassRemote() {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  auto remote = receiver_.BindNewPipeAndPassRemote(std::move(task_runner));
  receiver_.set_disconnect_handler(
      WTF::BindOnce(&PressureClientImpl::Reset, WrapWeakPersistent(this)));
  return remote;
}

void PressureClientImpl::Reset() {
  state_ = State::kUninitialized;
  observers_.clear();
  receiver_.reset();
}

void PressureClientImpl::Trace(Visitor* visitor) const {
  visitor->Trace(manager_);
  visitor->Trace(receiver_);
  visitor->Trace(observers_);
  ExecutionContextClient::Trace(visitor);
}

// https://w3c.github.io/compute-pressure/#dfn-passes-privacy-test
bool PressureClientImpl::PassesPrivacyTest() const {
  const ExecutionContext* context = GetExecutionContext();

  // TODO(crbug.com/1425053): Check for active needed worker.
  if (context->IsDedicatedWorkerGlobalScope() ||
      context->IsSharedWorkerGlobalScope()) {
    return true;
  }

  if (!DomWindow()) {
    return false;
  }

  LocalFrame* this_frame = DomWindow()->GetFrame();
  // 2. If associated document is not fully active, return false.
  if (context->IsContextDestroyed() || !this_frame) {
    return false;
  }

  // 4. If associated document is same-domain with initiators of active
  // Picture-in-Picture sessions, return true.
  //
  // TODO(crbug.com/1396177): A frame should be able to access to
  // PressureRecord if it is same-domain with initiators of active
  // Picture-in-Picture sessions. However, it is hard to implement now. In
  // current implementation, only the frame that triggers Picture-in-Picture
  // can access to PressureRecord.
  auto& pip_controller =
      PictureInPictureControllerImpl::From(*(this_frame->GetDocument()));
  if (pip_controller.PictureInPictureElement()) {
    return true;
  }

  // 5. If browsing context is capturing, return true.
  if (this_frame->IsCapturingMedia()) {
    return true;
  }

  // 7. If top-level browsing context does not have system focus, return false.
  CHECK(this_frame->GetPage());
  const auto& focus_controller = this_frame->GetPage()->GetFocusController();
  if (!focus_controller.IsFocused()) {
    return false;
  }

  // 8. Let focused document be the currently focused area's node document.
  const LocalFrame* focused_frame = focus_controller.FocusedFrame();
  if (!focused_frame) {
    return false;
  }

  // 9. If origin is same origin-domain with focused document, return true.
  // 10. Otherwise, return false.
  const SecurityOrigin* focused_frame_origin =
      focused_frame->GetSecurityContext()->GetSecurityOrigin();
  const SecurityOrigin* this_origin =
      this_frame->GetSecurityContext()->GetSecurityOrigin();
  return focused_frame_origin->CanAccess(this_origin);
}

}  // namespace blink
