// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/automation/automation_internal_custom_bindings.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/common/api/automation.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/automation.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/renderer/api/automation/automation_api_converters.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "extensions/renderer/script_context.h"
#include "ipc/message_filter.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/platform/automation/automation_api_util.h"
#include "ui/accessibility/platform/automation/automation_ax_tree_wrapper.h"
#include "ui/accessibility/platform/automation/automation_tree_manager_owner.h"
#include "ui/accessibility/platform/automation/automation_v8_bindings.h"
#include "ui/accessibility/platform/automation/automation_v8_router.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/auto_image_annotation_strings.h"

namespace extensions {

AutomationInternalCustomBindings::AutomationInternalCustomBindings(
    ScriptContext* context,
    NativeExtensionBindingsSystem* bindings_system)
    : ObjectBackedNativeHandler(context),
      bindings_system_(bindings_system),
      should_ignore_context_(false),
      automation_v8_bindings_(
          std::make_unique<ui::AutomationV8Bindings>(this, this)) {
  // We will ignore this instance if the extension has a background page and
  // this context is not that background page. In all other cases, we will have
  // multiple instances floating around in the same process.
  if (context && context->extension()) {
    const GURL background_page_url =
        extensions::BackgroundInfo::GetBackgroundURL(context->extension());
    should_ignore_context_ =
        background_page_url != "" && background_page_url != context->url();
  }
}

AutomationInternalCustomBindings::~AutomationInternalCustomBindings() = default;

void AutomationInternalCustomBindings::AddRoutes() {
  automation_v8_bindings_->AddV8Routes();

  // Extensions specific routes.
  ObjectBackedNativeHandler::RouteHandlerFunction(
      "IsInteractPermitted", "automation",
      base::BindRepeating(
          &AutomationInternalCustomBindings::IsInteractPermitted,
          base::Unretained(this)));
}

void AutomationInternalCustomBindings::Invalidate() {
  ObjectBackedNativeHandler::Invalidate();
  receiver_.reset();
  AutomationTreeManagerOwner::Invalidate();
}

ui::AutomationV8Bindings*
AutomationInternalCustomBindings::GetAutomationV8Bindings() const {
  DCHECK(automation_v8_bindings_);
  return automation_v8_bindings_.get();
}

void AutomationInternalCustomBindings::IsInteractPermitted(
    const v8::FunctionCallbackInfo<v8::Value>& args) const {
  const Extension* extension = context()->extension();
  CHECK(extension);
  const AutomationInfo* automation_info = AutomationInfo::Get(extension);
  CHECK(automation_info);
  args.GetReturnValue().Set(automation_info->desktop);
}

void AutomationInternalCustomBindings::StartCachingAccessibilityTrees() {
  if (should_ignore_context_)
    return;

  if (!receiver_.is_bound()) {
    bindings_system_->GetIPCMessageSender()->SendBindAutomationIPC(
        context(), receiver_.BindNewEndpointAndPassRemote());
  }
}

void AutomationInternalCustomBindings::StopCachingAccessibilityTrees() {
  receiver_.reset();
}

//
// Handle accessibility events from the browser process.
//

void AutomationInternalCustomBindings::ThrowInvalidArgumentsException(
    bool is_fatal) const {
  GetIsolate()->ThrowException(v8::String::NewFromUtf8Literal(
      GetIsolate(),
      "Invalid arguments to AutomationInternalCustomBindings function"));

  if (!is_fatal)
    return;

  LOG(FATAL) << "Invalid arguments to AutomationInternalCustomBindings function"
             << context()->GetStackTraceAsString();
}

v8::Isolate* AutomationInternalCustomBindings::GetIsolate() const {
  return ObjectBackedNativeHandler::GetIsolate();
}

v8::Local<v8::Context> AutomationInternalCustomBindings::GetContext() const {
  return context()->v8_context();
}

void AutomationInternalCustomBindings::RouteHandlerFunction(
    const std::string& name,
    scoped_refptr<ui::V8HandlerFunctionWrapper> handler_function_wrapper) {
  ObjectBackedNativeHandler::RouteHandlerFunction(
      name, "automation",
      base::BindRepeating(&ui::V8HandlerFunctionWrapper::RunV8,
                          handler_function_wrapper));
}

ui::TreeChangeObserverFilter
AutomationInternalCustomBindings::ParseTreeChangeObserverFilter(
    const std::string& filter) const {
  return ConvertAutomationTreeChangeObserverFilter(
      api::automation::ParseTreeChangeObserverFilter(filter));
}

std::string AutomationInternalCustomBindings::GetMarkerTypeString(
    ax::mojom::MarkerType type) const {
  return api::automation::ToString(ConvertMarkerTypeFromAXToAutomation(type));
}

std::string AutomationInternalCustomBindings::GetFocusedStateString() const {
  return api::automation::ToString(api::automation::StateType::kFocused);
}

std::string AutomationInternalCustomBindings::GetOffscreenStateString() const {
  return api::automation::ToString(api::automation::StateType::kOffscreen);
}

void AutomationInternalCustomBindings::DispatchEvent(
    const std::string& event_name,
    const base::Value::List& event_args) const {
  bindings_system_->DispatchEventInContext(event_name, event_args, nullptr,
                                           context());
}

std::string
AutomationInternalCustomBindings::GetLocalizedStringForImageAnnotationStatus(
    ax::mojom::ImageAnnotationStatus status) const {
  int message_id = 0;
  switch (status) {
    case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
      message_id = IDS_AX_IMAGE_ELIGIBLE_FOR_ANNOTATION;
      break;
    case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
      message_id = IDS_AX_IMAGE_ANNOTATION_PENDING;
      break;
    case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
      message_id = IDS_AX_IMAGE_ANNOTATION_ADULT;
      break;
    case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
    case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
      message_id = IDS_AX_IMAGE_ANNOTATION_NO_DESCRIPTION;
      break;
    case ax::mojom::ImageAnnotationStatus::kNone:
    case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
    case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      return std::string();
  }

  DCHECK(message_id);

  return l10n_util::GetStringUTF8(message_id);
}

std::string AutomationInternalCustomBindings::GetTreeChangeTypeString(
    ax::mojom::Mutation change_type) const {
  return ToString(ConvertToAutomationTreeChangeType(change_type));
}

std::string AutomationInternalCustomBindings::GetEventTypeString(
    const std::tuple<ax::mojom::Event, ui::AXEventGenerator::Event>& event_type)
    const {
  ui::AXEventGenerator::Event generated_event = std::get<1>(event_type);
  // Resolve the proper event based on generated or non-generated event sources.
  api::automation::EventType automation_event_type =
      generated_event != ui::AXEventGenerator::Event::NONE
          ? AXGeneratedEventToAutomationEventType(generated_event)
          : AXEventToAutomationEventType(std::get<0>(event_type));
  return api::automation::ToString(automation_event_type);
}

void AutomationInternalCustomBindings::NotifyTreeEventListenersChanged() {
  // This task is posted because we need to wait for any pending mutations
  // to be processed before sending the event.
  auto callback =
      base::BindOnce(&AutomationInternalCustomBindings::
                         MaybeSendOnAllAutomationEventListenersRemoved,
                     weak_ptr_factory_.GetWeakPtr());

  if (context()->IsForServiceWorker()) {
    content::WorkerThread::PostTask(content::WorkerThread::GetCurrentId(),
                                    std::move(callback));
  } else {
    context()
        ->web_frame()
        ->GetTaskRunner(blink::TaskType::kInternalDefault)
        ->PostTask(FROM_HERE, std::move(callback));
  }
}

}  // namespace extensions
