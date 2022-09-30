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

#include "base/bind.h"
#include "base/containers/cxx20_erase.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/api/automation.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/automation.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/renderer/api/automation/automation_api_converters.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "extensions/renderer/script_context.h"
#include "ipc/message_filter.h"
#include "third_party/blink/public/strings/grit/blink_accessibility_strings.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/platform/automation/automation_api_util.h"
#include "ui/accessibility/platform/automation/automation_ax_tree_wrapper.h"
#include "ui/accessibility/platform/automation/automation_tree_manager_owner.h"
#include "ui/accessibility/platform/automation/automation_v8_bindings.h"
#include "ui/accessibility/platform/automation/automation_v8_router.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

class AutomationMessageFilter : public IPC::MessageFilter {
 public:
  AutomationMessageFilter(
      AutomationInternalCustomBindings* owner,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : owner_(owner), removed_(false), task_runner_(std::move(task_runner)) {
    DCHECK(owner);
    content::RenderThread::Get()->AddFilter(this);
  }

  AutomationMessageFilter(const AutomationMessageFilter&) = delete;
  AutomationMessageFilter& operator=(const AutomationMessageFilter&) = delete;

  void Detach() {
    owner_ = nullptr;
    Remove();
  }

  // IPC::MessageFilter
  bool OnMessageReceived(const IPC::Message& message) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AutomationMessageFilter::OnMessageReceivedOnRenderThread, this,
            message));

    // Always return false in case there are multiple
    // AutomationInternalCustomBindings instances attached to the same thread.
    return false;
  }

  void OnFilterRemoved() override { removed_ = true; }

 private:
  void OnMessageReceivedOnRenderThread(const IPC::Message& message) {
    if (owner_)
      owner_->OnMessageReceived(message);
  }

  ~AutomationMessageFilter() override { Remove(); }

  void Remove() {
    if (!removed_) {
      removed_ = true;
      content::RenderThread::Get()->RemoveFilter(this);
    }
  }

  AutomationInternalCustomBindings* owner_;
  bool removed_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

AutomationInternalCustomBindings::AutomationInternalCustomBindings(
    ScriptContext* context,
    NativeExtensionBindingsSystem* bindings_system)
    : ObjectBackedNativeHandler(context),
      is_active_profile_(true),
      bindings_system_(bindings_system),
      should_ignore_context_(false) {
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

AutomationInternalCustomBindings::~AutomationInternalCustomBindings() {}

void AutomationInternalCustomBindings::AddRoutes() {
  // This should only be called once.
  DCHECK(!automation_v8_bindings_);
  automation_v8_bindings_ =
      std::make_unique<ui::AutomationV8Bindings>(this, this);
  automation_v8_bindings_->AddV8Routes();
}

void AutomationInternalCustomBindings::Invalidate() {
  ObjectBackedNativeHandler::Invalidate();

  if (message_filter_)
    message_filter_->Detach();

  AutomationTreeManagerOwner::Invalidate();
}

void AutomationInternalCustomBindings::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(AutomationInternalCustomBindings, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AccessibilityEventBundle,
                        OnAccessibilityEvents)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AccessibilityLocationChange,
                        OnAccessibilityLocationChange)
  IPC_END_MESSAGE_MAP()
}

bool AutomationInternalCustomBindings::IsInteractPermitted() const {
  const Extension* extension = context()->extension();
  CHECK(extension);
  const AutomationInfo* automation_info = AutomationInfo::Get(extension);
  CHECK(automation_info);
  return automation_info->interact;
}

void AutomationInternalCustomBindings::StartCachingAccessibilityTrees() {
  if (should_ignore_context_)
    return;

  if (!message_filter_) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        context()->web_frame()->GetTaskRunner(
            blink::TaskType::kInternalDefault);
    message_filter_ = base::MakeRefCounted<AutomationMessageFilter>(
        this, std::move(task_runner));
  }
}

void AutomationInternalCustomBindings::StopCachingAccessibilityTrees() {
  message_filter_->Detach();
  message_filter_.reset();
}

//
// Handle accessibility events from the browser process.
//

void AutomationInternalCustomBindings::OnAccessibilityEvents(
    const ExtensionMsg_AccessibilityEventBundleParams& event_bundle,
    bool is_active_profile) {
  is_active_profile_ = is_active_profile;
  ui::AXTreeID tree_id = event_bundle.tree_id;
  ui::AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  bool is_new_tree = tree_wrapper == nullptr;
  if (is_new_tree) {
    tree_wrapper = new ui::AutomationAXTreeWrapper(tree_id, this);
    CacheAutomationTreeWrapperForTreeID(tree_id, tree_wrapper);
  }

  if (!tree_wrapper->OnAccessibilityEvents(
          event_bundle.tree_id, event_bundle.updates, event_bundle.events,
          event_bundle.mouse_location, is_active_profile)) {
    DLOG(ERROR) << tree_wrapper->ax_tree()->error();
    base::Value::List args;
    args.Append(tree_id.ToString());
    DispatchEvent("automationInternal.onAccessibilityTreeSerializationError",
                  args);
    return;
  }

  // Send an initial event to ensure the js-side objects get created for new
  // trees.
  if (is_new_tree) {
    ui::AXEvent initial_event;
    initial_event.id = -1;
    initial_event.event_from = ax::mojom::EventFrom::kNone;
    initial_event.event_type = ax::mojom::Event::kNone;
    SendAutomationEvent(tree_id, gfx::Point(), initial_event);
  }

  // After handling events in js, if the client did not add any event listeners,
  // shut things down.
  TreeEventListenersChanged(tree_wrapper);
}

void AutomationInternalCustomBindings::OnAccessibilityLocationChange(
    const ExtensionMsg_AccessibilityLocationChangeParams& params) {
  ui::AXTreeID tree_id = params.tree_id;
  ui::AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper)
    return;
  ui::AXNode* node =
      tree_wrapper->GetNodeFromTree(tree_wrapper->GetTreeID(), params.id);
  if (!node)
    return;

  absl::optional<gfx::Rect> previous_accessibility_focused_global_bounds =
      GetAccessibilityFocusedLocation();

  node->SetLocation(params.new_location.offset_container_id,
                    params.new_location.bounds,
                    params.new_location.transform.get());

  if (previous_accessibility_focused_global_bounds.has_value() &&
      previous_accessibility_focused_global_bounds !=
          GetAccessibilityFocusedLocation()) {
    SendAccessibilityFocusedLocationChange(gfx::Point());
  }
}

bool AutomationInternalCustomBindings::SendTreeChangeEvent(
    ax::mojom::Mutation change_type,
    ui::AXTree* tree,
    ui::AXNode* node) {
  // Don't send tree change events when it's not the active profile.
  if (!is_active_profile_)
    return false;

  // Notify custom bindings when there's an unloaded tree; js will enable the
  // renderer and wait for it to load.
  std::string child_tree_id_str;
  if (node->GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId,
                               &child_tree_id_str)) {
    ui::AXTreeID child_tree_id = ui::AXTreeID::FromString(child_tree_id_str);
    auto* tree_wrapper = GetAutomationAXTreeWrapperFromTreeID(child_tree_id);
    if (!tree_wrapper || !tree_wrapper->ax_tree()->data().loaded)
      SendChildTreeIDEvent(child_tree_id);
  }

  if (!ShouldSendTreeChangeEvent(change_type, tree, node))
    return false;

  ui::AXTreeID tree_id = tree->GetAXTreeID();
  bool did_send_event = false;
  for (const auto& observer : tree_change_observers()) {
    switch (observer.filter) {
      case ui::TreeChangeObserverFilter::kNoTreeChanges:
      default:
        continue;
      case ui::TreeChangeObserverFilter::kLiveRegionTreeChanges:
        if (!node->HasStringAttribute(
                ax::mojom::StringAttribute::kContainerLiveStatus) &&
            node->GetRole() != ax::mojom::Role::kAlert &&
            change_type != ax::mojom::Mutation::kSubtreeUpdateEnd) {
          continue;
        }
        break;
      case ui::TreeChangeObserverFilter::kTextMarkerChanges:
        if (!node->HasIntListAttribute(
                ax::mojom::IntListAttribute::kMarkerTypes))
          continue;
        break;
      case ui::TreeChangeObserverFilter::kAllTreeChanges:
        break;
    }

    api::automation::TreeChangeType automation_change_type =
        ConvertToAutomationTreeChangeType(change_type);
    did_send_event = true;
    base::Value::List args;
    args.Append(observer.id);
    args.Append(tree_id.ToString());
    args.Append(node->id());
    args.Append(ToString(automation_change_type));
    DispatchEvent("automationInternal.onTreeChange", args);
  }

  return did_send_event;
}

void AutomationInternalCustomBindings::SendAutomationEvent(
    ui::AXTreeID tree_id,
    const gfx::Point& mouse_location,
    const ui::AXEvent& event,
    absl::optional<ui::AXEventGenerator::Event> generated_event_type) {
  ui::AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper)
    return;

  // Resolve the proper event based on generated or non-generated event sources.
  api::automation::EventType automation_event_type =
      generated_event_type.has_value()
          ? AXGeneratedEventToAutomationEventType(*generated_event_type)
          : AXEventToAutomationEventType(event.event_type);
  const char* automation_event_type_str =
      api::automation::ToString(automation_event_type);

  // These events get used internally to trigger other behaviors in js.
  bool fire_event =
      automation_event_type == api::automation::EVENT_TYPE_NONE ||
      automation_event_type == api::automation::EVENT_TYPE_HITTESTRESULT ||
      automation_event_type ==
          api::automation::EVENT_TYPE_MEDIASTARTEDPLAYING ||
      automation_event_type == api::automation::EVENT_TYPE_MEDIASTOPPEDPLAYING;

  // If we don't explicitly recognize the event type, require a valid, unignored
  // node target.
  ui::AXNode* node =
      tree_wrapper->GetNodeFromTree(tree_wrapper->GetTreeID(), event.id);
  if (!fire_event && (!node || node->data().IsIgnored()))
    return;

  while (node && tree_wrapper && !fire_event) {
    if (tree_wrapper->HasEventListener(
            AutomationEventTypeToAXEventTuple(automation_event_type), node)) {
      fire_event = true;
      break;
    }
    node = GetParent(node, &tree_wrapper);
  }

  if (!fire_event)
    return;

  base::Value event_params(base::Value::Type::DICTIONARY);
  event_params.SetKey("treeID", base::Value(tree_id.ToString()));
  event_params.SetKey("targetID", base::Value(event.id));
  event_params.SetKey("eventType", base::Value(automation_event_type_str));

  event_params.SetKey("eventFrom", base::Value(ui::ToString(event.event_from)));
  event_params.SetKey("eventFromAction",
                      base::Value(ui::ToString(event.event_from_action)));
  event_params.SetKey("actionRequestID", base::Value(event.action_request_id));
  event_params.SetKey("mouseX", base::Value(mouse_location.x()));
  event_params.SetKey("mouseY", base::Value(mouse_location.y()));

  // Populate intents.
  base::Value value_intents(base::Value::Type::LIST);
  for (const auto& intent : event.event_intents) {
    base::Value dict(base::Value::Type::DICTIONARY);
    dict.SetKey("command", base::Value(ui::ToString(intent.command)));
    dict.SetKey("inputEventType",
                base::Value(ui::ToString(intent.input_event_type)));
    dict.SetKey("textBoundary",
                base::Value(ui::ToString(intent.text_boundary)));
    dict.SetKey("moveDirection",
                base::Value(ui::ToString(intent.move_direction)));
    value_intents.Append(std::move(dict));
  }

  event_params.SetKey("intents", std::move(value_intents));

  base::Value::List args;
  args.Append(std::move(event_params));
  DispatchEvent("automationInternal.onAccessibilityEvent", args);

  if (!notify_event_for_testing_.is_null())
    notify_event_for_testing_.Run(automation_event_type);
}

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
    AutomationV8Router::HandlerFunction handler_function) {
  ObjectBackedNativeHandler::RouteHandlerFunction(name, handler_function);
}

void AutomationInternalCustomBindings::RouteHandlerFunction(
    const std::string& name,
    const std::string& api_name,
    AutomationV8Router::HandlerFunction handler_function) {
  ObjectBackedNativeHandler::RouteHandlerFunction(name, api_name,
                                                  handler_function);
}

std::tuple<ax::mojom::Event, ui::AXEventGenerator::Event>
AutomationInternalCustomBindings::ParseEventType(
    const std::string& event_type) const {
  return AutomationEventTypeToAXEventTuple(
      api::automation::ParseEventType(event_type));
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
  return api::automation::ToString(api::automation::STATE_TYPE_FOCUSED);
}

std::string AutomationInternalCustomBindings::GetOffscreenStateString() const {
  return api::automation::ToString(api::automation::STATE_TYPE_OFFSCREEN);
}

void AutomationInternalCustomBindings::DispatchEvent(
    const std::string& event_name,
    const base::Value::List& event_args) const {
  bindings_system_->DispatchEventInContext(event_name, event_args, nullptr,
                                           context());
}

void AutomationInternalCustomBindings::SendChildTreeIDEvent(
    ui::AXTreeID child_tree_id) {
  base::Value::List args;
  args.Append(child_tree_id.ToString());
  DispatchEvent("automationInternal.onChildTreeID", args);
}

void AutomationInternalCustomBindings::SendNodesRemovedEvent(
    ui::AXTree* tree,
    const std::vector<int>& ids) {
  ui::AXTreeID tree_id = tree->GetAXTreeID();
  base::Value::List args;
  args.Append(tree_id.ToString());
  {
    base::Value nodes(base::Value::Type::LIST);
    for (auto id : ids)
      nodes.Append(id);
    args.Append(std::move(nodes));
  }

  DispatchEvent("automationInternal.onNodesRemoved", args);
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

void AutomationInternalCustomBindings::NotifyTreeEventListenersChanged() {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      context()->web_frame()->GetTaskRunner(blink::TaskType::kInternalDefault);
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&AutomationInternalCustomBindings::
                         MaybeSendOnAllAutomationEventListenersRemoved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AutomationInternalCustomBindings::
    MaybeSendOnAllAutomationEventListenersRemoved() {
  if (HasTreesWithEventListeners())
    return;

  DispatchEvent("automationInternal.onAllAutomationEventListenersRemoved",
                base::Value::List());
}

}  // namespace extensions
