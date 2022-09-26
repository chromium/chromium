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
#include "base/no_destructor.h"
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
#include "extensions/renderer/api/automation/automation_position.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "ipc/message_filter.h"
#include "third_party/blink/public/strings/grit/blink_accessibility_strings.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/platform/automation/automation_api_util.h"
#include "ui/accessibility/platform/automation/automation_ax_tree_wrapper.h"
#include "ui/accessibility/platform/automation/automation_tree_manager_owner.h"
#include "ui/accessibility/platform/automation/automation_v8_bindings.h"
#include "ui/accessibility/platform/automation/automation_v8_router.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

api::automation::MarkerType ConvertMarkerTypeFromAXToAutomation(
    ax::mojom::MarkerType ax) {
  switch (ax) {
    case ax::mojom::MarkerType::kNone:
      return api::automation::MARKER_TYPE_NONE;
    case ax::mojom::MarkerType::kSpelling:
      return api::automation::MARKER_TYPE_SPELLING;
    case ax::mojom::MarkerType::kGrammar:
      return api::automation::MARKER_TYPE_GRAMMAR;
    case ax::mojom::MarkerType::kTextMatch:
      return api::automation::MARKER_TYPE_TEXTMATCH;
    case ax::mojom::MarkerType::kActiveSuggestion:
      return api::automation::MARKER_TYPE_ACTIVESUGGESTION;
    case ax::mojom::MarkerType::kSuggestion:
      return api::automation::MARKER_TYPE_SUGGESTION;
    case ax::mojom::MarkerType::kHighlight:
      return api::automation::MARKER_TYPE_HIGHLIGHT;
  }
}

// TODO(crbug.com/1357889): Move this and other converters between
// automation and AX types to a utility file.
api::automation::TreeChangeType ConvertToAutomationTreeChangeType(
    ax::mojom::Mutation change_type) {
  switch (change_type) {
    case ax::mojom::Mutation::kNone:
      return api::automation::TREE_CHANGE_TYPE_NONE;
    case ax::mojom::Mutation::kNodeCreated:
      return api::automation::TREE_CHANGE_TYPE_NODECREATED;
    case ax::mojom::Mutation::kSubtreeCreated:
      return api::automation::TREE_CHANGE_TYPE_SUBTREECREATED;
    case ax::mojom::Mutation::kNodeChanged:
      return api::automation::TREE_CHANGE_TYPE_NODECHANGED;
    case ax::mojom::Mutation::kTextChanged:
      return api::automation::TREE_CHANGE_TYPE_TEXTCHANGED;
    case ax::mojom::Mutation::kNodeRemoved:
      return api::automation::TREE_CHANGE_TYPE_NODEREMOVED;
    case ax::mojom::Mutation::kSubtreeUpdateEnd:
      return api::automation::TREE_CHANGE_TYPE_SUBTREEUPDATEEND;
    default:
      NOTREACHED();
  }
}

// Maps a key, a stringification of values in ui::AXEventGenerator::Event or
// ax::mojom::Event into a value, automation::api::EventType. The runtime
// invariant is that there should be exactly the same number of values in the
// map as is the size of api::automation::EventType.
api::automation::EventType AXEventToAutomationEventType(
    ax::mojom::Event event_type) {
  static base::NoDestructor<std::vector<api::automation::EventType>> enum_map;
  if (enum_map->empty()) {
    for (int i = static_cast<int>(ax::mojom::Event::kMinValue);
         i <= static_cast<int>(ax::mojom::Event::kMaxValue); i++) {
      auto ax_event_type = static_cast<ax::mojom::Event>(i);
      if (ui::ShouldIgnoreAXEventForAutomation(ax_event_type) ||
          ax_event_type == ax::mojom::Event::kNone) {
        enum_map->emplace_back(api::automation::EVENT_TYPE_NONE);
        continue;
      }

      const char* val = ui::ToString(ax_event_type);
      api::automation::EventType automation_event_type =
          api::automation::ParseEventType(val);
      if (automation_event_type == api::automation::EVENT_TYPE_NONE)
        NOTREACHED() << "Missing mapping from ax::mojom::Event: " << val;

      enum_map->emplace_back(automation_event_type);
    }
  }

  return (*enum_map)[static_cast<int>(event_type)];
}

api::automation::EventType AXGeneratedEventToAutomationEventType(
    ui::AXEventGenerator::Event event_type) {
  static base::NoDestructor<std::vector<api::automation::EventType>> enum_map;
  if (enum_map->empty()) {
    for (int i = 0;
         i <= static_cast<int>(ui::AXEventGenerator::Event::MAX_VALUE); i++) {
      auto ax_event_type = static_cast<ui::AXEventGenerator::Event>(i);
      if (ui::ShouldIgnoreGeneratedEventForAutomation(ax_event_type)) {
        enum_map->emplace_back(api::automation::EVENT_TYPE_NONE);
        continue;
      }

      const char* val = ui::ToString(ax_event_type);
      api::automation::EventType automation_event_type =
          api::automation::ParseEventType(val);
      if (automation_event_type == api::automation::EVENT_TYPE_NONE)
        NOTREACHED() << "Missing mapping from ui::AXEventGenerator::Event: "
                     << val;

      enum_map->emplace_back(automation_event_type);
    }
  }

  return (*enum_map)[static_cast<int>(event_type)];
}

std::tuple<ax::mojom::Event, ui::AXEventGenerator::Event>
AutomationEventTypeToAXEventTuple(api::automation::EventType event_type) {
  const char* val = api::automation::ToString(event_type);
  ax::mojom::Event ax_event = ax::mojom::Event::kNone;
  ui::MaybeParseAXEnum<ax::mojom::Event>(val, &ax_event);
  ui::AXEventGenerator::Event generated_event =
      ui::AXEventGenerator::Event::NONE;
  ui::MaybeParseGeneratedEvent(val, &generated_event);
  return std::tuple<ax::mojom::Event, ui::AXEventGenerator::Event>(
      ax_event, generated_event);
}

}  // namespace

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
      tree_change_observer_overall_filter_(0),
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
// It's safe to use base::Unretained(this) here because these bindings
// will only be called on a valid AutomationInternalCustomBindings instance
// and none of the functions have any side effects.
// TODO(crbug.com/1357889): Move ROUTE_FUNCTION functions into
// ui::AutomationV8Bindings if possible.
#define ROUTE_FUNCTION(FN)                                       \
  ObjectBackedNativeHandler::RouteHandlerFunction(               \
      #FN, "automation",                                         \
      base::BindRepeating(&AutomationInternalCustomBindings::FN, \
                          base::Unretained(this)))
  ROUTE_FUNCTION(IsInteractPermitted);
  ROUTE_FUNCTION(GetSchemaAdditions);
  ROUTE_FUNCTION(StartCachingAccessibilityTrees);
  ROUTE_FUNCTION(StopCachingAccessibilityTrees);
  ROUTE_FUNCTION(DestroyAccessibilityTree);
  ROUTE_FUNCTION(AddTreeChangeObserver);
  ROUTE_FUNCTION(RemoveTreeChangeObserver);
  ROUTE_FUNCTION(GetChildIDAtIndex);
  ROUTE_FUNCTION(GetFocus);
  ROUTE_FUNCTION(GetHtmlAttributes);
  ROUTE_FUNCTION(GetState);
  ROUTE_FUNCTION(CreateAutomationPosition);
  ROUTE_FUNCTION(GetAccessibilityFocus);
  ROUTE_FUNCTION(SetDesktopID);
#undef ROUTE_FUNCTION

  // This should only be called once.
  DCHECK(!automation_v8_bindings_);
  automation_v8_bindings_ =
      std::make_unique<ui::AutomationV8Bindings>(this, this);
  automation_v8_bindings_->AddV8Routes();

  automation_v8_bindings_->RouteNodeIDFunction(
      "GetMarkers",
      base::BindRepeating([](v8::Isolate* isolate,
                             v8::ReturnValue<v8::Value> result,
                             ui::AutomationAXTreeWrapper* tree_wrapper,
                             ui::AXNode* node) {
        if (!node->HasIntListAttribute(
                ax::mojom::IntListAttribute::kMarkerStarts) ||
            !node->HasIntListAttribute(
                ax::mojom::IntListAttribute::kMarkerEnds) ||
            !node->HasIntListAttribute(
                ax::mojom::IntListAttribute::kMarkerTypes)) {
          return;
        }

        const std::vector<int32_t>& marker_starts = node->GetIntListAttribute(
            ax::mojom::IntListAttribute::kMarkerStarts);
        const std::vector<int32_t>& marker_ends =
            node->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds);
        const std::vector<int32_t>& marker_types = node->GetIntListAttribute(
            ax::mojom::IntListAttribute::kMarkerTypes);

        std::vector<v8::Local<v8::Object>> markers;
        for (size_t i = 0; i < marker_types.size(); ++i) {
          gin::DataObjectBuilder marker_obj(isolate);
          marker_obj.Set("startOffset", marker_starts[i]);
          marker_obj.Set("endOffset", marker_ends[i]);

          gin::DataObjectBuilder flags(isolate);
          int32_t marker_type = marker_types[i];
          int32_t marker_pos = 1;
          while (marker_type) {
            if (marker_type & 1) {
              flags.Set(
                  api::automation::ToString(ConvertMarkerTypeFromAXToAutomation(
                      static_cast<ax::mojom::MarkerType>(marker_pos))),
                  true);
            }
            marker_type = marker_type >> 1;
            marker_pos = marker_pos << 1;
          }

          marker_obj.Set("flags", flags.Build());
          markers.push_back(marker_obj.Build());
        }

        result.Set(gin::ConvertToV8(isolate, markers));
      }));

  automation_v8_bindings_->RouteNodeIDFunction(
      "GetImageAnnotation",
      base::BindRepeating(&AutomationInternalCustomBindings::GetImageAnnotation,
                          base::Unretained(this)));
}

void AutomationInternalCustomBindings::Invalidate() {
  ObjectBackedNativeHandler::Invalidate();

  if (message_filter_)
    message_filter_->Detach();

  auto& child_tree_id_reverse_map =
      ui::AutomationAXTreeWrapper::GetChildTreeIDReverseMap();
  base::EraseIf(
      child_tree_id_reverse_map,
      [this](
          const std::pair<ui::AXTreeID, ui::AutomationAXTreeWrapper*>& pair) {
        return pair.second->owner() == this;
      });

  ClearCachedAutomationTreeWrappers();
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

void AutomationInternalCustomBindings::IsInteractPermitted(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  const Extension* extension = context()->extension();
  CHECK(extension);
  const AutomationInfo* automation_info = AutomationInfo::Get(extension);
  CHECK(automation_info);
  args.GetReturnValue().Set(
      v8::Boolean::New(GetIsolate(), automation_info->interact));
}

void AutomationInternalCustomBindings::StartCachingAccessibilityTrees(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
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

void AutomationInternalCustomBindings::StopCachingAccessibilityTrees(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  message_filter_->Detach();
  message_filter_.reset();
  tree_change_observers_.clear();
  ClearCachedAutomationTreeWrappers();
  ui::AutomationAXTreeWrapper::GetChildTreeIDReverseMap().clear();
}

void AutomationInternalCustomBindings::GetSchemaAdditions(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = GetIsolate();

  gin::DataObjectBuilder name_from_type(isolate);
  for (int32_t i = static_cast<int32_t>(ax::mojom::NameFrom::kNone);
       i <= static_cast<int32_t>(ax::mojom::NameFrom::kMaxValue); ++i) {
    name_from_type.Set(
        i,
        base::StringPiece(ui::ToString(static_cast<ax::mojom::NameFrom>(i))));
  }

  gin::DataObjectBuilder restriction(isolate);
  for (int32_t i = static_cast<int32_t>(ax::mojom::Restriction::kNone);
       i <= static_cast<int32_t>(ax::mojom::Restriction::kMaxValue); ++i) {
    restriction.Set(i, base::StringPiece(ui::ToString(
                           static_cast<ax::mojom::Restriction>(i))));
  }

  gin::DataObjectBuilder description_from_type(isolate);
  for (int32_t i = static_cast<int32_t>(ax::mojom::DescriptionFrom::kNone);
       i <= static_cast<int32_t>(ax::mojom::DescriptionFrom::kMaxValue); ++i) {
    description_from_type.Set(
        i, base::StringPiece(
               ui::ToString(static_cast<ax::mojom::DescriptionFrom>(i))));
  }

  args.GetReturnValue().Set(
      gin::DataObjectBuilder(isolate)
          .Set("NameFromType", name_from_type.Build())
          .Set("Restriction", restriction.Build())
          .Set("DescriptionFromType", description_from_type.Build())
          .Build());
}

void AutomationInternalCustomBindings::DestroyAccessibilityTree(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 1 || !args[0]->IsString()) {
    ThrowInvalidArgumentsException();
    return;
  }

  ui::AXTreeID tree_id = ui::AXTreeID::FromString(
      *v8::String::Utf8Value(args.GetIsolate(), args[0]));
  auto& child_tree_id_reverse_map =
      ui::AutomationAXTreeWrapper::GetChildTreeIDReverseMap();
  base::EraseIf(
      child_tree_id_reverse_map,
      [tree_id](
          const std::pair<ui::AXTreeID, ui::AutomationAXTreeWrapper*>& pair) {
        return pair.first == tree_id || pair.second->GetTreeID() == tree_id;
      });

  if (tree_id == accessibility_focused_tree_id())
    SetAccessibilityFocusedTreeID(ui::AXTreeIDUnknown());

  RemoveAutomationTreeWrapperFromCache(tree_id);
  trees_with_event_listeners_.erase(tree_id);
}

void AutomationInternalCustomBindings::AddTreeChangeObserver(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 2 || !args[0]->IsNumber() || !args[1]->IsString()) {
    ThrowInvalidArgumentsException();
    return;
  }

  TreeChangeObserver observer;
  observer.id = args[0]->Int32Value(context()->v8_context()).FromMaybe(0);
  std::string filter_str = *v8::String::Utf8Value(args.GetIsolate(), args[1]);
  observer.filter = api::automation::ParseTreeChangeObserverFilter(filter_str);

  tree_change_observers_.push_back(observer);
  UpdateOverallTreeChangeObserverFilter();
}

void AutomationInternalCustomBindings::RemoveTreeChangeObserver(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // The argument is an integer key for an object which is automatically
  // converted to a string.
  if (args.Length() != 1 || !args[0]->IsString()) {
    ThrowInvalidArgumentsException();
    return;
  }

  int observer_id = args[0]->Int32Value(context()->v8_context()).FromMaybe(0);

  for (auto iter = tree_change_observers_.begin();
       iter != tree_change_observers_.end(); ++iter) {
    if (iter->id == observer_id) {
      tree_change_observers_.erase(iter);
      break;
    }
  }

  UpdateOverallTreeChangeObserverFilter();
}

void AutomationInternalCustomBindings::GetFocus(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 0) {
    ThrowInvalidArgumentsException();
    return;
  }

  ui::AutomationAXTreeWrapper* desktop_tree =
      GetAutomationAXTreeWrapperFromTreeID(desktop_tree_id());
  ui::AutomationAXTreeWrapper* focused_wrapper = nullptr;
  ui::AXNode* focused_node = nullptr;
  if (desktop_tree &&
      !GetFocusInternal(desktop_tree, &focused_wrapper, &focused_node))
    return;

  if (!desktop_tree) {
    focused_wrapper = GetAutomationAXTreeWrapperFromTreeID(focus_tree_id());
    if (!focused_wrapper)
      return;

    focused_node = focused_wrapper->GetNodeFromTree(
        focused_wrapper->GetTreeID(), focus_id());
    if (!focused_node)
      return;
  }

  args.GetReturnValue().Set(
      gin::DataObjectBuilder(GetIsolate())
          .Set("treeId", focused_wrapper->GetTreeID().ToString())
          .Set("nodeId", focused_node->id())
          .Build());
}

void AutomationInternalCustomBindings::GetAccessibilityFocus(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ui::AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(accessibility_focused_tree_id());
  if (!tree_wrapper)
    return;

  ui::AXNode* node = tree_wrapper->GetAccessibilityFocusedNode();
  if (!node)
    return;

  args.GetReturnValue().Set(
      gin::DataObjectBuilder(GetIsolate())
          .Set("treeId", accessibility_focused_tree_id().ToString())
          .Set("nodeId", node->id())
          .Build());
}

void AutomationInternalCustomBindings::SetDesktopID(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 1 || !args[0]->IsString()) {
    ThrowInvalidArgumentsException();
    return;
  }

  SetDesktopTreeId(ui::AXTreeID::FromString(
      *v8::String::Utf8Value(args.GetIsolate(), args[0])));
}

void AutomationInternalCustomBindings::GetHtmlAttributes(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = GetIsolate();
  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsNumber())
    ThrowInvalidArgumentsException();

  ui::AXTreeID tree_id =
      ui::AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
  int node_id = args[1]->Int32Value(context()->v8_context()).FromMaybe(0);

  ui::AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper)
    return;

  ui::AXNode* node =
      tree_wrapper->GetNodeFromTree(tree_wrapper->GetTreeID(), node_id);
  if (!node)
    return;

  gin::DataObjectBuilder dst(isolate);
  for (const auto& pair : node->data().html_attributes)
    dst.Set(pair.first, pair.second);
  args.GetReturnValue().Set(dst.Build());
}

void AutomationInternalCustomBindings::GetState(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = GetIsolate();
  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsNumber())
    ThrowInvalidArgumentsException();

  ui::AXTreeID tree_id =
      ui::AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
  int node_id = args[1]->Int32Value(context()->v8_context()).FromMaybe(0);

  ui::AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper)
    return;

  ui::AXNode* node =
      tree_wrapper->GetNodeFromTree(tree_wrapper->GetTreeID(), node_id);
  if (!node)
    return;

  gin::DataObjectBuilder state(isolate);
  uint32_t state_pos = 0, state_shifter = node->data().state;
  while (state_shifter) {
    if (state_shifter & 1)
      state.Set(ui::ToString(static_cast<ax::mojom::State>(state_pos)), true);
    state_shifter = state_shifter >> 1;
    state_pos++;
  }
  ui::AutomationAXTreeWrapper* top_tree_wrapper = nullptr;
  ui::AutomationAXTreeWrapper* walker = tree_wrapper;
  while (walker && walker != top_tree_wrapper) {
    top_tree_wrapper = walker;
    GetParent(walker->ax_tree()->root(), &walker);
  }

  const bool focused = tree_wrapper->IsInFocusChain(node->id());
  if (focused) {
    state.Set(api::automation::ToString(api::automation::STATE_TYPE_FOCUSED),
              true);
  }

  bool offscreen = false;
  ComputeGlobalNodeBounds(tree_wrapper, node, gfx::RectF(), &offscreen);
  if (offscreen)
    state.Set(api::automation::ToString(api::automation::STATE_TYPE_OFFSCREEN),
              true);

  args.GetReturnValue().Set(state.Build());
}

void AutomationInternalCustomBindings::CreateAutomationPosition(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = GetIsolate();
  if (args.Length() < 4 || !args[0]->IsString() /* tree id */ ||
      !args[1]->IsInt32() /* node id */ || !args[2]->IsInt32() /* offset */ ||
      !args[3]->IsBoolean() /* is upstream affinity */) {
    ThrowInvalidArgumentsException();
  }

  ui::AXTreeID tree_id =
      ui::AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
  int node_id = args[1]->Int32Value(context()->v8_context()).ToChecked();

  ui::AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper)
    return;

  ui::AXNode* node = tree_wrapper->ax_tree()->GetFromId(node_id);
  if (!node)
    return;

  int offset = args[2]->Int32Value(context()->v8_context()).ToChecked();
  bool is_upstream = args[3]->BooleanValue(isolate);

  gin::Handle<AutomationPosition> handle = gin::CreateHandle(
      isolate, new AutomationPosition(*node, offset, is_upstream));
  args.GetReturnValue().Set(handle.ToV8().As<v8::Object>());
}

void AutomationInternalCustomBindings::GetImageAnnotation(
    v8::Isolate* isolate,
    v8::ReturnValue<v8::Value> result,
    ui::AutomationAXTreeWrapper* tree_wrapper,
    ui::AXNode* node) {
  std::string status_string = std::string();
  auto status = node->data().GetImageAnnotationStatus();
  switch (status) {
    case ax::mojom::ImageAnnotationStatus::kNone:
    case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
    case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
      break;

    case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
    case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
    case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
    case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
      status_string = GetLocalizedStringForImageAnnotationStatus(status);
      break;
    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      status_string = node->GetStringAttribute(
          ax::mojom::StringAttribute::kImageAnnotation);
      break;
  }
  if (status_string.empty())
    return;
  result.Set(
      v8::String::NewFromUtf8(isolate, status_string.c_str()).ToLocalChecked());
}

void AutomationInternalCustomBindings::UpdateOverallTreeChangeObserverFilter() {
  tree_change_observer_overall_filter_ = 0;
  for (const auto& observer : tree_change_observers_)
    tree_change_observer_overall_filter_ |= 1 << observer.filter;
}

void AutomationInternalCustomBindings::GetChildIDAtIndex(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 3 || !args[2]->IsNumber()) {
    ThrowInvalidArgumentsException();
    return;
  }

  ui::AXTreeID tree_id = ui::AXTreeID::FromString(
      *v8::String::Utf8Value(args.GetIsolate(), args[0]));
  int node_id = args[1]->Int32Value(context()->v8_context()).FromMaybe(0);

  ui::AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper)
    return;

  ui::AXNode* node =
      tree_wrapper->GetNodeFromTree(tree_wrapper->GetTreeID(), node_id);
  if (!node)
    return;

  int index = args[2]->Int32Value(context()->v8_context()).FromMaybe(0);

  // Check for child roots.
  std::vector<ui::AXNode*> child_roots = GetRootsOfChildTree(node);

  if (index < 0)
    return;

  ui::AXNode* child_node = nullptr;
  if (!child_roots.empty() && static_cast<size_t>(index) < child_roots.size()) {
    child_node = child_roots[index];
  } else if (static_cast<size_t>(index) >= node->GetUnignoredChildCount()) {
    return;
  } else {
    child_node = node->GetUnignoredChildAtIndex(static_cast<size_t>(index));
  }

  DCHECK(child_node);

  gin::DataObjectBuilder response(GetIsolate());
  response.Set("treeId", child_node->tree()->GetAXTreeID().ToString());
  response.Set("nodeId", child_node->id());
  args.GetReturnValue().Set(response.Build());
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
    bindings_system_->DispatchEventInContext(
        "automationInternal.onAccessibilityTreeSerializationError", args,
        nullptr, context());
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

  // At this point, don't bother dispatching to js if the node is ignored. A js
  // client shouldn't process ignored nodes.
  if (node->IsIgnored())
    return false;

  // Likewise, don't process tree changes on ignored trees.
  auto* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree->GetAXTreeID());
  if (!tree_wrapper || tree_wrapper->IsTreeIgnored())
    return false;

  bool has_filter = false;
  if (tree_change_observer_overall_filter_ &
      (1
       << api::automation::TREE_CHANGE_OBSERVER_FILTER_LIVEREGIONTREECHANGES)) {
    if (node->HasStringAttribute(
            ax::mojom::StringAttribute::kContainerLiveStatus) ||
        node->GetRole() == ax::mojom::Role::kAlert ||
        change_type == ax::mojom::Mutation::kSubtreeUpdateEnd) {
      has_filter = true;
    }
  }

  if (tree_change_observer_overall_filter_ &
      (1 << api::automation::TREE_CHANGE_OBSERVER_FILTER_TEXTMARKERCHANGES)) {
    if (node->HasIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes))
      has_filter = true;
  }

  if (tree_change_observer_overall_filter_ &
      (1 << api::automation::TREE_CHANGE_OBSERVER_FILTER_ALLTREECHANGES))
    has_filter = true;

  if (!has_filter)
    return false;

  ui::AXTreeID tree_id = tree->GetAXTreeID();
  bool did_send_event = false;
  for (const auto& observer : tree_change_observers_) {
    switch (observer.filter) {
      case api::automation::TREE_CHANGE_OBSERVER_FILTER_NOTREECHANGES:
      default:
        continue;
      case api::automation::TREE_CHANGE_OBSERVER_FILTER_LIVEREGIONTREECHANGES:
        if (!node->HasStringAttribute(
                ax::mojom::StringAttribute::kContainerLiveStatus) &&
            node->GetRole() != ax::mojom::Role::kAlert &&
            change_type != ax::mojom::Mutation::kSubtreeUpdateEnd) {
          continue;
        }
        break;
      case api::automation::TREE_CHANGE_OBSERVER_FILTER_TEXTMARKERCHANGES:
        if (!node->HasIntListAttribute(
                ax::mojom::IntListAttribute::kMarkerTypes))
          continue;
        break;
      case api::automation::TREE_CHANGE_OBSERVER_FILTER_ALLTREECHANGES:
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
    bindings_system_->DispatchEventInContext("automationInternal.onTreeChange",
                                             args, nullptr, context());
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
  bindings_system_->DispatchEventInContext(
      "automationInternal.onAccessibilityEvent", args, nullptr, context());

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
    HandlerFunction handler_function) {
  ObjectBackedNativeHandler::RouteHandlerFunction(name, handler_function);
}

std::tuple<ax::mojom::Event, ui::AXEventGenerator::Event>
AutomationInternalCustomBindings::ParseEventType(
    const std::string& event_type) const {
  return AutomationEventTypeToAXEventTuple(
      api::automation::ParseEventType(event_type));
}

void AutomationInternalCustomBindings::SendChildTreeIDEvent(
    ui::AXTreeID child_tree_id) {
  base::Value::List args;
  args.Append(child_tree_id.ToString());
  bindings_system_->DispatchEventInContext("automationInternal.onChildTreeID",
                                           args, nullptr, context());
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

  bindings_system_->DispatchEventInContext("automationInternal.onNodesRemoved",
                                           args, nullptr, context());
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

void AutomationInternalCustomBindings::TreeEventListenersChanged(
    ui::AutomationAXTreeWrapper* tree_wrapper) {
  if (tree_wrapper->EventListenerCount() != 0) {
    trees_with_event_listeners_.insert(tree_wrapper->GetTreeID());
    return;
  }

  if (trees_with_event_listeners_.empty())
    return;

  trees_with_event_listeners_.erase(tree_wrapper->GetTreeID());
  if (!trees_with_event_listeners_.empty())
    return;

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
  if (!trees_with_event_listeners_.empty())
    return;

  bindings_system_->DispatchEventInContext(
      "automationInternal.onAllAutomationEventListenersRemoved",
      base::Value::List(), nullptr, context());
}

}  // namespace extensions
