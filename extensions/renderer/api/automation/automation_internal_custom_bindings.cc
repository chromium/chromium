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
#include "base/i18n/string_search.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
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
#include "ui/accessibility/ax_language_detection.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/platform/automation/automation_api_util.h"
#include "ui/accessibility/platform/automation/automation_ax_tree_wrapper.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

void ThrowInvalidArgumentsException(
    AutomationInternalCustomBindings* automation_bindings,
    bool is_fatal = true) {
  v8::Isolate* isolate = automation_bindings->GetIsolate();
  automation_bindings->GetIsolate()->ThrowException(
      v8::String::NewFromUtf8Literal(
          isolate,
          "Invalid arguments to AutomationInternalCustomBindings function"));

  if (is_fatal) {
    LOG(FATAL)
        << "Invalid arguments to AutomationInternalCustomBindings function"
        << automation_bindings->context()->GetStackTraceAsString();
  }
}

v8::Local<v8::String> CreateV8String(v8::Isolate* isolate,
                                     base::StringPiece str) {
  return gin::StringToSymbol(isolate, str);
}

v8::Local<v8::Object> RectToV8Object(v8::Isolate* isolate,
                                     const gfx::Rect& rect) {
  return gin::DataObjectBuilder(isolate)
      .Set("left", rect.x())
      .Set("top", rect.y())
      .Set("width", rect.width())
      .Set("height", rect.height())
      .Build();
}

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

//
// Helper class that helps implement bindings for a JavaScript function
// that takes a single input argument consisting of a Tree ID. Looks up
// the ui::AutomationAXTreeWrapper and passes it to the function passed to the
// constructor.
//

typedef void (*TreeIDFunction)(v8::Isolate* isolate,
                               v8::ReturnValue<v8::Value> result,
                               ui::AutomationAXTreeWrapper* tree_wrapper);

class TreeIDWrapper : public base::RefCountedThreadSafe<TreeIDWrapper> {
 public:
  TreeIDWrapper(AutomationInternalCustomBindings* automation_bindings,
                TreeIDFunction function)
      : automation_bindings_(automation_bindings), function_(function) {}

  void Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = automation_bindings_->GetIsolate();
    if (args.Length() != 1 || !args[0]->IsString())
      ThrowInvalidArgumentsException(automation_bindings_);

    ui::AXTreeID tree_id =
        ui::AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
    ui::AutomationAXTreeWrapper* tree_wrapper =
        automation_bindings_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
    if (!tree_wrapper)
      return;

    // The root can be null if this is called from an onTreeChange callback.
    if (!tree_wrapper->ax_tree()->root())
      return;

    function_(isolate, args.GetReturnValue(), tree_wrapper);
  }

 private:
  virtual ~TreeIDWrapper() {}

  friend class base::RefCountedThreadSafe<TreeIDWrapper>;

  AutomationInternalCustomBindings* automation_bindings_;
  TreeIDFunction function_;
};

//
// Helper class that helps implement bindings for a JavaScript function
// that takes two input arguments: a tree ID and node ID. Looks up the
// ui::AutomationAXTreeWrapper and the AXNode and passes them to the function
// passed to the constructor.
//
typedef std::function<void(v8::Isolate* isolate,
                           v8::ReturnValue<v8::Value> result,
                           ui::AutomationAXTreeWrapper* tree_wrapper,
                           ui::AXNode* node)>
    NodeIDFunction;

class NodeIDWrapper : public base::RefCountedThreadSafe<NodeIDWrapper> {
 public:
  NodeIDWrapper(AutomationInternalCustomBindings* automation_bindings,
                NodeIDFunction function)
      : automation_bindings_(automation_bindings), function_(function) {}

  void Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = automation_bindings_->GetIsolate();
    if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsNumber())
      ThrowInvalidArgumentsException(automation_bindings_);

    v8::Local<v8::Context> context =
        automation_bindings_->context()->v8_context();
    ui::AXTreeID tree_id =
        ui::AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
    int node_id = args[1]->Int32Value(context).FromMaybe(0);

    ui::AutomationAXTreeWrapper* tree_wrapper =
        automation_bindings_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
    if (!tree_wrapper)
      return;

    ui::AXNode* node = tree_wrapper->GetUnignoredNodeFromId(node_id);
    if (!node)
      return;

    function_(isolate, args.GetReturnValue(), tree_wrapper, node);
  }

 private:
  virtual ~NodeIDWrapper() {}

  friend class base::RefCountedThreadSafe<NodeIDWrapper>;

  AutomationInternalCustomBindings* automation_bindings_;
  NodeIDFunction function_;
};

//
// Helper class that helps implement bindings for a JavaScript function
// that takes three input arguments: a tree ID, node ID, and string
// argument. Looks up the ui::AutomationAXTreeWrapper and the AXNode and passes
// them to the function passed to the constructor.
//

typedef void (*NodeIDPlusAttributeFunction)(v8::Isolate* isolate,
                                            v8::ReturnValue<v8::Value> result,
                                            ui::AXTree* tree,
                                            ui::AXNode* node,
                                            const std::string& attribute);

class NodeIDPlusAttributeWrapper
    : public base::RefCountedThreadSafe<NodeIDPlusAttributeWrapper> {
 public:
  NodeIDPlusAttributeWrapper(
      AutomationInternalCustomBindings* automation_bindings,
      NodeIDPlusAttributeFunction function)
      : automation_bindings_(automation_bindings), function_(function) {}

  void Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = automation_bindings_->GetIsolate();
    if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsNumber() ||
        !args[2]->IsString()) {
      ThrowInvalidArgumentsException(automation_bindings_);
    }

    v8::Local<v8::Context> context =
        automation_bindings_->context()->v8_context();
    ui::AXTreeID tree_id =
        ui::AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
    int node_id = args[1]->Int32Value(context).FromMaybe(0);
    std::string attribute = *v8::String::Utf8Value(isolate, args[2]);

    ui::AutomationAXTreeWrapper* tree_wrapper =
        automation_bindings_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
    if (!tree_wrapper)
      return;

    ui::AXNode* node = tree_wrapper->GetUnignoredNodeFromId(node_id);
    if (!node)
      return;

    function_(isolate, args.GetReturnValue(), tree_wrapper->ax_tree(), node,
              attribute);
  }

 private:
  virtual ~NodeIDPlusAttributeWrapper() {}

  friend class base::RefCountedThreadSafe<NodeIDPlusAttributeWrapper>;

  AutomationInternalCustomBindings* automation_bindings_;
  NodeIDPlusAttributeFunction function_;
};

//
// Helper class that helps implement bindings for a JavaScript function
// that takes four input arguments: a tree ID, node ID, and integer start
// and end indices. Looks up the ui::AutomationAXTreeWrapper and the AXNode and
// passes them to the function passed to the constructor.
//

typedef std::function<void(v8::Isolate* isolate,
                           v8::ReturnValue<v8::Value> result,
                           ui::AutomationAXTreeWrapper* tree_wrapper,
                           ui::AXNode* node,
                           int start,
                           int end,
                           bool clipped)>
    NodeIDPlusRangeFunction;

class NodeIDPlusRangeWrapper
    : public base::RefCountedThreadSafe<NodeIDPlusRangeWrapper> {
 public:
  NodeIDPlusRangeWrapper(AutomationInternalCustomBindings* automation_bindings,
                         NodeIDPlusRangeFunction function)
      : automation_bindings_(automation_bindings), function_(function) {}

  void Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = automation_bindings_->GetIsolate();
    if (args.Length() < 5 || !args[0]->IsString() || !args[1]->IsNumber() ||
        !args[2]->IsNumber() || !args[3]->IsNumber() || !args[4]->IsBoolean()) {
      ThrowInvalidArgumentsException(automation_bindings_);
    }

    v8::Local<v8::Context> context =
        automation_bindings_->context()->v8_context();
    ui::AXTreeID tree_id =
        ui::AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
    int node_id = args[1]->Int32Value(context).FromMaybe(0);
    int start = args[2]->Int32Value(context).FromMaybe(0);
    int end = args[3]->Int32Value(context).FromMaybe(0);
    bool clipped = args[4]->BooleanValue(isolate);

    ui::AutomationAXTreeWrapper* tree_wrapper =
        automation_bindings_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
    if (!tree_wrapper)
      return;

    ui::AXNode* node = tree_wrapper->GetUnignoredNodeFromId(node_id);
    if (!node)
      return;

    function_(isolate, args.GetReturnValue(), tree_wrapper, node, start, end,
              clipped);
  }

 private:
  virtual ~NodeIDPlusRangeWrapper() {}

  friend class base::RefCountedThreadSafe<NodeIDPlusRangeWrapper>;

  AutomationInternalCustomBindings* automation_bindings_;
  NodeIDPlusRangeFunction function_;
};

typedef std::function<void(v8::Isolate* isolate,
                           v8::ReturnValue<v8::Value> result,
                           ui::AutomationAXTreeWrapper* tree_wrapper,
                           ui::AXNode* node,
                           const std::string& strVal,
                           bool boolVal)>
    NodeIDPlusStringBoolFunction;

class NodeIDPlusStringBoolWrapper
    : public base::RefCountedThreadSafe<NodeIDPlusStringBoolWrapper> {
 public:
  NodeIDPlusStringBoolWrapper(
      AutomationInternalCustomBindings* automation_bindings,
      NodeIDPlusStringBoolFunction function)
      : automation_bindings_(automation_bindings), function_(function) {}

  void Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = automation_bindings_->GetIsolate();
    if (args.Length() < 4 || !args[0]->IsString() || !args[1]->IsNumber() ||
        !args[2]->IsString() || !args[3]->IsBoolean()) {
      ThrowInvalidArgumentsException(automation_bindings_);
    }

    v8::Local<v8::Context> context =
        automation_bindings_->context()->v8_context();
    ui::AXTreeID tree_id =
        ui::AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
    int node_id = args[1]->Int32Value(context).FromMaybe(0);
    std::string str_val = *v8::String::Utf8Value(isolate, args[2]);
    bool bool_val = args[3].As<v8::Boolean>()->Value();

    ui::AutomationAXTreeWrapper* tree_wrapper =
        automation_bindings_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
    if (!tree_wrapper)
      return;

    ui::AXNode* node = tree_wrapper->GetUnignoredNodeFromId(node_id);
    if (!node)
      return;

    function_(isolate, args.GetReturnValue(), tree_wrapper, node, str_val,
              bool_val);
  }

 private:
  virtual ~NodeIDPlusStringBoolWrapper() {}

  friend class base::RefCountedThreadSafe<NodeIDPlusStringBoolWrapper>;

  AutomationInternalCustomBindings* automation_bindings_;
  NodeIDPlusStringBoolFunction function_;
};

using NodeIDPlusDimensionsFunction =
    std::function<void(v8::Isolate* isolate,
                       v8::ReturnValue<v8::Value> result,
                       ui::AutomationAXTreeWrapper* tree_wrapper,
                       ui::AXNode* node,
                       int x,
                       int y,
                       int width,
                       int height)>;

class NodeIDPlusDimensionsWrapper
    : public base::RefCountedThreadSafe<NodeIDPlusDimensionsWrapper> {
 public:
  NodeIDPlusDimensionsWrapper(
      AutomationInternalCustomBindings* automation_bindings,
      NodeIDPlusDimensionsFunction function)
      : automation_bindings_(automation_bindings), function_(function) {}

  void Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = automation_bindings_->GetIsolate();
    if (args.Length() < 6 || !args[0]->IsString() || !args[1]->IsInt32() ||
        !args[2]->IsInt32() || !args[3]->IsInt32() || !args[4]->IsInt32() ||
        !args[5]->IsInt32()) {
      ThrowInvalidArgumentsException(automation_bindings_);
    }

    ui::AXTreeID tree_id =
        ui::AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
    int node_id = args[1].As<v8::Int32>()->Value();
    int x = args[2].As<v8::Int32>()->Value();
    int y = args[3].As<v8::Int32>()->Value();
    int width = args[4].As<v8::Int32>()->Value();
    int height = args[5].As<v8::Int32>()->Value();

    ui::AutomationAXTreeWrapper* tree_wrapper =
        automation_bindings_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
    if (!tree_wrapper)
      return;

    ui::AXNode* node = tree_wrapper->GetUnignoredNodeFromId(node_id);
    if (!node)
      return;

    function_(isolate, args.GetReturnValue(), tree_wrapper, node, x, y, width,
              height);
  }

 private:
  virtual ~NodeIDPlusDimensionsWrapper() {}

  friend class base::RefCountedThreadSafe<NodeIDPlusDimensionsWrapper>;

  AutomationInternalCustomBindings* automation_bindings_;
  NodeIDPlusDimensionsFunction function_;
};

typedef std::function<void(v8::Isolate* isolate,
                           v8::ReturnValue<v8::Value> result,
                           ui::AutomationAXTreeWrapper* tree_wrapper,
                           ui::AXNode* node,
                           api::automation::EventType event_type)>
    NodeIDPlusEventFunction;

class NodeIDPlusEventWrapper
    : public base::RefCountedThreadSafe<NodeIDPlusEventWrapper> {
 public:
  NodeIDPlusEventWrapper(AutomationInternalCustomBindings* automation_bindings,
                         NodeIDPlusEventFunction function)
      : automation_bindings_(automation_bindings), function_(function) {}

  void Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = automation_bindings_->GetIsolate();
    if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsInt32() ||
        !args[2]->IsString()) {
      // The extension system does not perform argument validation in js, so an
      // extension author can do something like node.addEventListener(undefined)
      // and reach here. Do not crash the process.
      ThrowInvalidArgumentsException(automation_bindings_,
                                     false /* is_fatal */);
      return;
    }

    ui::AXTreeID tree_id =
        ui::AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
    int node_id = args[1].As<v8::Int32>()->Value();
    auto event_type = api::automation::ParseEventType(
        *v8::String::Utf8Value(isolate, args[2]));
    if (event_type == api::automation::EVENT_TYPE_NONE) {
      ThrowInvalidArgumentsException(automation_bindings_,
                                     false /* is_fatal */);
      return;
    }

    ui::AutomationAXTreeWrapper* tree_wrapper =
        automation_bindings_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
    if (!tree_wrapper)
      return;

    ui::AXNode* node = tree_wrapper->GetUnignoredNodeFromId(node_id);
    if (!node)
      return;

    function_(isolate, args.GetReturnValue(), tree_wrapper, node, event_type);
  }

 private:
  virtual ~NodeIDPlusEventWrapper() {}

  friend class base::RefCountedThreadSafe<NodeIDPlusEventWrapper>;

  AutomationInternalCustomBindings* automation_bindings_;
  NodeIDPlusEventFunction function_;
};

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
#define ROUTE_FUNCTION(FN)                                       \
  RouteHandlerFunction(                                          \
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

  // Bindings that take a Tree ID and return a property of the tree.

  RouteTreeIDFunction(
      "GetRootID", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                      ui::AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(
            v8::Integer::New(isolate, tree_wrapper->ax_tree()->root()->id()));
      });
  RouteTreeIDFunction(
      "GetPublicRoot",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper) {
        tree_wrapper = tree_wrapper->GetTreeWrapperWithUnignoredRoot();
        if (!tree_wrapper)
          return;

        gin::DataObjectBuilder response(isolate);
        response.Set("treeId", tree_wrapper->GetTreeID().ToString());
        response.Set("nodeId", tree_wrapper->ax_tree()->root()->id());
        result.Set(response.Build());
      });
  RouteTreeIDFunction(
      "GetDocURL", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                      ui::AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::String::NewFromUtf8(
                       isolate, tree_wrapper->ax_tree()->data().url.c_str())
                       .ToLocalChecked());
      });
  RouteTreeIDFunction(
      "GetDocTitle", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                        ui::AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::String::NewFromUtf8(
                       isolate, tree_wrapper->ax_tree()->data().title.c_str())
                       .ToLocalChecked());
      });
  RouteTreeIDFunction(
      "GetDocLoaded",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(
            v8::Boolean::New(isolate, tree_wrapper->ax_tree()->data().loaded));
      });
  RouteTreeIDFunction(
      "GetDocLoadingProgress",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->ax_tree()->data().loading_progress));
      });
  RouteTreeIDFunction(
      "GetIsSelectionBackward",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper) {
        const ui::AXNode* anchor = tree_wrapper->GetNodeFromTree(
            tree_wrapper->GetTreeID(),
            tree_wrapper->GetUnignoredSelection().anchor_object_id);
        if (!anchor)
          return;

        result.Set(v8::Boolean::New(
            isolate, tree_wrapper->ax_tree()->data().sel_is_backward));
      });
  RouteTreeIDFunction(
      "GetAnchorObjectID",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->GetUnignoredSelection().anchor_object_id));
      });
  RouteTreeIDFunction(
      "GetAnchorOffset",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->GetUnignoredSelection().anchor_offset));
      });
  RouteTreeIDFunction(
      "GetAnchorAffinity",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(CreateV8String(
            isolate,
            ui::ToString(
                tree_wrapper->GetUnignoredSelection().anchor_affinity)));
      });
  RouteTreeIDFunction(
      "GetFocusObjectID",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->GetUnignoredSelection().focus_object_id));
      });
  RouteTreeIDFunction(
      "GetFocusOffset",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->GetUnignoredSelection().focus_offset));
      });
  RouteTreeIDFunction(
      "GetFocusAffinity",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(CreateV8String(
            isolate,
            ui::ToString(
                tree_wrapper->GetUnignoredSelection().focus_affinity)));
      });
  RouteTreeIDFunction(
      "GetSelectionStartObjectID",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper) {
        ui::AXSelection unignored_selection =
            tree_wrapper->GetUnignoredSelection();
        int32_t start_object_id = unignored_selection.is_backward
                                      ? unignored_selection.focus_object_id
                                      : unignored_selection.anchor_object_id;
        result.Set(v8::Number::New(isolate, start_object_id));
      });
  RouteTreeIDFunction(
      "GetSelectionStartOffset",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper) {
        ui::AXSelection unignored_selection =
            tree_wrapper->GetUnignoredSelection();
        int start_offset = unignored_selection.is_backward
                               ? unignored_selection.focus_offset
                               : unignored_selection.anchor_offset;
        result.Set(v8::Number::New(isolate, start_offset));
      });
  RouteTreeIDFunction(
      "GetSelectionStartAffinity",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper) {
        ui::AXSelection unignored_selection =
            tree_wrapper->GetUnignoredSelection();
        ax::mojom::TextAffinity start_affinity =
            unignored_selection.is_backward
                ? unignored_selection.focus_affinity
                : unignored_selection.anchor_affinity;
        result.Set(CreateV8String(isolate, ui::ToString(start_affinity)));
      });
  RouteTreeIDFunction(
      "GetSelectionEndObjectID",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper) {
        ui::AXSelection unignored_selection =
            tree_wrapper->GetUnignoredSelection();
        int32_t end_object_id = unignored_selection.is_backward
                                    ? unignored_selection.anchor_object_id
                                    : unignored_selection.focus_object_id;
        result.Set(v8::Number::New(isolate, end_object_id));
      });
  RouteTreeIDFunction(
      "GetSelectionEndOffset",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper) {
        ui::AXSelection unignored_selection =
            tree_wrapper->GetUnignoredSelection();
        int end_offset = unignored_selection.is_backward
                             ? unignored_selection.anchor_offset
                             : unignored_selection.focus_offset;
        result.Set(v8::Number::New(isolate, end_offset));
      });
  RouteTreeIDFunction(
      "GetSelectionEndAffinity",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper) {
        ui::AXSelection unignored_selection =
            tree_wrapper->GetUnignoredSelection();
        ax::mojom::TextAffinity end_affinity =
            unignored_selection.is_backward
                ? unignored_selection.anchor_affinity
                : unignored_selection.focus_affinity;
        result.Set(CreateV8String(isolate, ui::ToString(end_affinity)));
      });

  // Bindings that take a Tree ID and Node ID and return a property of the node.

  RouteNodeIDFunction(
      "GetParentID",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ui::AXNode* parent = GetParent(node, &tree_wrapper);
        if (parent) {
          gin::DataObjectBuilder response(isolate);
          response.Set("treeId", tree_wrapper->GetTreeID().ToString());
          response.Set("nodeId", parent->id());
          result.Set(response.Build());
        }
      });
  RouteNodeIDFunction(
      "GetChildCount",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        size_t child_count;
        std::vector<ui::AXNode*> child_roots = GetRootsOfChildTree(node);
        if (child_roots.empty())
          child_count = node->GetUnignoredChildCount();
        else
          child_count = child_roots.size();

        result.Set(
            v8::Integer::New(isolate, static_cast<int32_t>(child_count)));
      });
  RouteNodeIDFunction(
      "GetIndexInParent",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        result.Set(v8::Integer::New(
            isolate, static_cast<int32_t>(node->GetUnignoredIndexInParent())));
      });
  RouteNodeIDFunction(
      "GetRole",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const std::string& role_name = ui::ToString(node->GetRole());
        result.Set(v8::String::NewFromUtf8(isolate, role_name.c_str())
                       .ToLocalChecked());
      });
  RouteNodeIDFunction(
      "GetLocation",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        gfx::Rect global_clipped_bounds =
            ComputeGlobalNodeBounds(tree_wrapper, node);
        result.Set(RectToV8Object(isolate, global_clipped_bounds));
      });
  RouteNodeIDFunction(
      "GetUnclippedLocation",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool offscreen = false;
        gfx::Rect global_unclipped_bounds =
            ComputeGlobalNodeBounds(tree_wrapper, node, gfx::RectF(),
                                    &offscreen, false /* clip_bounds */);
        result.Set(RectToV8Object(isolate, global_unclipped_bounds));
      });
  RouteNodeIDFunction(
      "GetLineStartOffsets",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const std::vector<int> line_starts =
            node->GetIntListAttribute(ax::mojom::IntListAttribute::kLineStarts);
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Array> array_result(
            v8::Array::New(isolate, line_starts.size()));
        for (size_t i = 0; i < line_starts.size(); ++i) {
          array_result
              ->CreateDataProperty(context, static_cast<uint32_t>(i),
                                   v8::Integer::New(isolate, line_starts[i]))
              .Check();
        }
        result.Set(array_result);
      });
  RouteNodeIDFunction(
      "GetChildIDs",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::vector<int> child_ids;
        ui::AXTreeID tree_id;
        std::vector child_roots = GetRootsOfChildTree(node);
        if (!child_roots.empty()) {
          tree_id = child_roots[0]->tree()->GetAXTreeID();
          for (ui::AXNode* child_root : child_roots)
            child_ids.push_back(child_root->id());
        } else {
          for (auto iter = node->UnignoredChildrenBegin();
               iter != node->UnignoredChildrenEnd(); ++iter) {
            child_ids.push_back(iter->id());
            tree_id = iter->tree()->GetAXTreeID();
          }
        }

        gin::DataObjectBuilder response(isolate);
        response.Set("treeId", tree_id.ToString());
        response.Set("nodeIds", child_ids);
        result.Set(response.Build());
      });
  RouteNodeIDFunction(
      "GetWordStartOffsets",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::vector<int> word_starts = ui::GetWordStartOffsets(
            node->GetString16Attribute(ax::mojom::StringAttribute::kName));
        result.Set(gin::ConvertToV8(isolate, word_starts));
      });
  RouteNodeIDFunction(
      "GetWordEndOffsets",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::vector<int> word_ends = ui::GetWordEndOffsets(
            node->GetString16Attribute(ax::mojom::StringAttribute::kName));
        result.Set(gin::ConvertToV8(isolate, word_ends));
      });
  RouteNodeIDFunction(
      "GetSentenceStartOffsets",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const std::vector<int>& sentence_starts =
            AutomationTreeManagerOwner::CalculateSentenceBoundary(
                tree_wrapper, node, true /* start_boundary */);
        result.Set(gin::ConvertToV8(isolate, sentence_starts));
      });
  RouteNodeIDFunction(
      "GetSentenceEndOffsets",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const std::vector<int>& sentence_ends =
            AutomationTreeManagerOwner::CalculateSentenceBoundary(
                tree_wrapper, node, false /* start_boundary */);
        result.Set(gin::ConvertToV8(isolate, sentence_ends));
      });
  RouteNodeIDFunction("GetMarkers", [](v8::Isolate* isolate,
                                       v8::ReturnValue<v8::Value> result,
                                       ui::AutomationAXTreeWrapper*
                                           tree_wrapper,
                                       ui::AXNode* node) {
    if (!node->HasIntListAttribute(
            ax::mojom::IntListAttribute::kMarkerStarts) ||
        !node->HasIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds) ||
        !node->HasIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes)) {
      return;
    }

    const std::vector<int32_t>& marker_starts =
        node->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts);
    const std::vector<int32_t>& marker_ends =
        node->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds);
    const std::vector<int32_t>& marker_types =
        node->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes);

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
  });
  // Bindings that take a Tree ID and Node ID and string attribute name
  // and return a property of the node.

  RouteNodeIDPlusRangeFunction(
      "GetBoundsForRange",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node,
             int start, int end, bool clipped) {
        if (node->GetRole() != ax::mojom::Role::kInlineTextBox)
          return;

        // Use character offsets to compute the local bounds of this subrange.
        gfx::RectF local_bounds(0, 0,
                                node->data().relative_bounds.bounds.width(),
                                node->data().relative_bounds.bounds.height());
        const std::string& name =
            node->GetStringAttribute(ax::mojom::StringAttribute::kName);
        std::vector<int> character_offsets = node->GetIntListAttribute(
            ax::mojom::IntListAttribute::kCharacterOffsets);
        int len =
            static_cast<int>(std::min(name.size(), character_offsets.size()));
        if (start >= 0 && start <= end && end <= len) {
          int start_offset = start > 0 ? character_offsets[start - 1] : 0;
          int end_offset = end > 0 ? character_offsets[end - 1] : 0;

          switch (node->data().GetTextDirection()) {
            case ax::mojom::WritingDirection::kLtr:
            default:
              local_bounds.set_x(local_bounds.x() + start_offset);
              local_bounds.set_width(end_offset - start_offset);
              break;
            case ax::mojom::WritingDirection::kRtl:
              local_bounds.set_x(local_bounds.x() + local_bounds.width() -
                                 end_offset);
              local_bounds.set_width(end_offset - start_offset);
              break;
            case ax::mojom::WritingDirection::kTtb:
              local_bounds.set_y(local_bounds.y() + start_offset);
              local_bounds.set_height(end_offset - start_offset);
              break;
            case ax::mojom::WritingDirection::kBtt:
              local_bounds.set_y(local_bounds.y() + local_bounds.height() -
                                 end_offset);
              local_bounds.set_height(end_offset - start_offset);
              break;
          }
        }

        // Convert from local to global coordinates second, after subsetting,
        // because the local to global conversion might involve matrix
        // transformations.
        gfx::Rect global_bounds =
            ComputeGlobalNodeBounds(tree_wrapper, node, local_bounds, nullptr,
                                    clipped /* clip_bounds */);
        result.Set(RectToV8Object(isolate, global_bounds));
      });

  RouteNodeIDPlusDimensionsFunction(
      "ComputeGlobalBounds",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node, int x,
             int y, int width, int height) {
        gfx::RectF local_bounds(x, y, width, height);

        // Convert from local coordinates in Android window, to global
        // coordinates spanning entire screen.
        gfx::Rect global_bounds = ComputeGlobalNodeBounds(
            tree_wrapper, node, local_bounds, nullptr, false /* clip_bounds */);
        result.Set(RectToV8Object(isolate, global_bounds));
      });

  // Bindings that take a Tree ID and Node ID and string attribute name
  // and return a property of the node.

  RouteNodeIDPlusAttributeFunction(
      "GetStringAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        auto attribute =
            ui::ParseAXEnum<ax::mojom::StringAttribute>(attribute_name.c_str());
        const char* attr_value;
        if (attribute == ax::mojom::StringAttribute::kFontFamily ||
            attribute == ax::mojom::StringAttribute::kLanguage) {
          attr_value = node->GetInheritedStringAttribute(attribute).c_str();
        } else if (!node->HasStringAttribute(attribute)) {
          return;
        } else {
          attr_value = node->GetStringAttribute(attribute).c_str();
        }

        result.Set(
            v8::String::NewFromUtf8(isolate, attr_value).ToLocalChecked());
      });
  RouteNodeIDPlusAttributeFunction(
      "GetBoolAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        auto attribute =
            ui::ParseAXEnum<ax::mojom::BoolAttribute>(attribute_name.c_str());
        bool attr_value;
        if (!node->GetBoolAttribute(attribute, &attr_value))
          return;

        result.Set(v8::Boolean::New(isolate, attr_value));
      });
  RouteNodeIDPlusAttributeFunction(
      "GetIntAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        auto attribute =
            ui::ParseAXEnum<ax::mojom::IntAttribute>(attribute_name.c_str());
        int attr_value;

        if (attribute == ax::mojom::IntAttribute::kPosInSet &&
            node->GetPosInSet()) {
          attr_value = *node->GetPosInSet();
        } else if (attribute == ax::mojom::IntAttribute::kSetSize &&
                   node->GetSetSize()) {
          attr_value = *node->GetSetSize();
        } else if (!node->GetIntAttribute(attribute, &attr_value)) {
          return;
        }

        result.Set(v8::Integer::New(isolate, attr_value));
      });
  RouteNodeIDPlusAttributeFunction(
      "GetIntAttributeReverseRelations",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        auto attribute =
            ui::ParseAXEnum<ax::mojom::IntAttribute>(attribute_name.c_str());
        std::set<int32_t> ids =
            tree->GetReverseRelations(attribute, node->id());
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Array> array_result(v8::Array::New(isolate, ids.size()));
        size_t count = 0;
        for (int32_t id : ids) {
          array_result
              ->CreateDataProperty(context, static_cast<uint32_t>(count++),
                                   v8::Integer::New(isolate, id))
              .Check();
        }
        result.Set(array_result);
      });
  RouteNodeIDPlusAttributeFunction(
      "GetFloatAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        auto attribute =
            ui::ParseAXEnum<ax::mojom::FloatAttribute>(attribute_name.c_str());
        float attr_value;

        if (!node->GetFloatAttribute(attribute, &attr_value))
          return;

        double intpart, fracpart;
        fracpart = modf(attr_value, &intpart);
        double value_precision_2 =
            intpart + std::round(fracpart * 100) / 100.0f;
        result.Set(v8::Number::New(isolate, value_precision_2));
      });
  RouteNodeIDPlusAttributeFunction(
      "GetIntListAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        auto attribute = ui::ParseAXEnum<ax::mojom::IntListAttribute>(
            attribute_name.c_str());
        if (!node->HasIntListAttribute(attribute))
          return;
        const std::vector<int32_t>& attr_value =
            node->GetIntListAttribute(attribute);

        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Array> array_result(
            v8::Array::New(isolate, attr_value.size()));
        for (size_t i = 0; i < attr_value.size(); ++i)
          array_result
              ->CreateDataProperty(context, static_cast<uint32_t>(i),
                                   v8::Integer::New(isolate, attr_value[i]))
              .Check();
        result.Set(array_result);
      });
  RouteNodeIDPlusAttributeFunction(
      "GetIntListAttributeReverseRelations",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        auto attribute = ui::ParseAXEnum<ax::mojom::IntListAttribute>(
            attribute_name.c_str());
        std::set<int32_t> ids =
            tree->GetReverseRelations(attribute, node->id());
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Array> array_result(v8::Array::New(isolate, ids.size()));
        size_t count = 0;
        for (int32_t id : ids) {
          array_result
              ->CreateDataProperty(context, static_cast<uint32_t>(count++),
                                   v8::Integer::New(isolate, id))
              .Check();
        }
        result.Set(array_result);
      });
  RouteNodeIDPlusAttributeFunction(
      "GetHtmlAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        std::string attribute_value;
        if (!node->GetHtmlAttribute(attribute_name.c_str(), &attribute_value))
          return;

        result.Set(v8::String::NewFromUtf8(isolate, attribute_value.c_str())
                       .ToLocalChecked());
      });
  RouteNodeIDFunction(
      "GetNameFrom",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ax::mojom::NameFrom name_from = node->data().GetNameFrom();
        const std::string& name_from_str = ui::ToString(name_from);
        result.Set(v8::String::NewFromUtf8(isolate, name_from_str.c_str())
                       .ToLocalChecked());
      });
  RouteNodeIDFunction("GetName", [this](
                                     v8::Isolate* isolate,
                                     v8::ReturnValue<v8::Value> result,
                                     ui::AutomationAXTreeWrapper* tree_wrapper,
                                     ui::AXNode* node) {
    const char* name = nullptr;
    if (node->GetRole() == ax::mojom::Role::kPortal &&
        node->data().GetNameFrom() == ax::mojom::NameFrom::kNone) {
      // Portals are not expected to have multiple child roots.
      if (const auto& child_roots = GetRootsOfChildTree(node);
          !child_roots.empty()) {
        name = child_roots[0]
                   ->GetStringAttribute(ax::mojom::StringAttribute::kName)
                   .c_str();
      }
    }

    if (!name && node->HasStringAttribute(ax::mojom::StringAttribute::kName)) {
      name =
          node->GetStringAttribute(ax::mojom::StringAttribute::kName).c_str();
    }

    if (name)
      result.Set(v8::String::NewFromUtf8(isolate, name).ToLocalChecked());
  });
  RouteNodeIDFunction(
      "GetDescriptionFrom",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ax::mojom::DescriptionFrom description_from =
            static_cast<ax::mojom::DescriptionFrom>(node->GetIntAttribute(
                ax::mojom::IntAttribute::kDescriptionFrom));
        std::string description_from_str = ui::ToString(description_from);
        result.Set(
            v8::String::NewFromUtf8(isolate, description_from_str.c_str())
                .ToLocalChecked());
      });
  RouteNodeIDFunction(
      "GetImageAnnotation",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
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
        result.Set(v8::String::NewFromUtf8(isolate, status_string.c_str())
                       .ToLocalChecked());
      });
  RouteNodeIDFunction(
      "GetSubscript",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value =
            node->GetIntAttribute(ax::mojom::IntAttribute::kTextPosition) ==
            static_cast<int32_t>(ax::mojom::TextPosition::kSubscript);
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetSuperscript",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value =
            node->GetIntAttribute(ax::mojom::IntAttribute::kTextPosition) ==
            static_cast<int32_t>(ax::mojom::TextPosition::kSuperscript);
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetBold",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value = node->data().HasTextStyle(ax::mojom::TextStyle::kBold);
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetItalic",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value = node->data().HasTextStyle(ax::mojom::TextStyle::kItalic);
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetUnderline",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value =
            node->data().HasTextStyle(ax::mojom::TextStyle::kUnderline);
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetLineThrough",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value =
            node->data().HasTextStyle(ax::mojom::TextStyle::kLineThrough);
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetDetectedLanguage",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const std::string& detectedLanguage = node->GetLanguage();
        result.Set(v8::String::NewFromUtf8(isolate, detectedLanguage.c_str())
                       .ToLocalChecked());
      });

  RouteNodeIDPlusAttributeFunction(
      "GetLanguageAnnotationForStringAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        auto attr =
            ui::ParseAXEnum<ax::mojom::StringAttribute>(attribute_name.c_str());
        if (attr == ax::mojom::StringAttribute::kNone) {
          // Set result as empty array.
          result.Set(v8::Array::New(isolate, 0));
          return;
        }
        std::vector<ui::AXLanguageSpan> language_annotation =
            tree->language_detection_manager
                ->GetLanguageAnnotationForStringAttribute(*node, attr);
        const std::string& attribute_value = node->GetStringAttribute(attr);
        // Build array.
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Array> array_result(
            v8::Array::New(isolate, language_annotation.size()));
        std::vector<size_t> offsets_for_adjustment(2, 0);
        for (size_t i = 0; i < language_annotation.size(); ++i) {
          offsets_for_adjustment[0] =
              static_cast<size_t>(language_annotation[i].start_index);
          offsets_for_adjustment[1] =
              static_cast<size_t>(language_annotation[i].end_index);
          // Convert UTF-8 offsets into UTF-16 offsets, since these objects
          // will be used in Javascript.
          base::UTF8ToUTF16AndAdjustOffsets(attribute_value,
                                            &offsets_for_adjustment);

          gin::DataObjectBuilder span(isolate);
          span.Set("startIndex", static_cast<int>(offsets_for_adjustment[0]));
          span.Set("endIndex", static_cast<int>(offsets_for_adjustment[1]));
          span.Set("language", language_annotation[i].language);
          span.Set("probability", language_annotation[i].probability);
          array_result
              ->CreateDataProperty(context, static_cast<uint32_t>(i),
                                   span.Build())
              .Check();
        }
        result.Set(array_result);
      });

  RouteNodeIDFunction(
      "GetCustomActions",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const std::vector<int32_t>& custom_action_ids =
            node->GetIntListAttribute(
                ax::mojom::IntListAttribute::kCustomActionIds);
        if (custom_action_ids.empty()) {
          result.SetUndefined();
          return;
        }

        const std::vector<std::string>& custom_action_descriptions =
            node->GetStringListAttribute(
                ax::mojom::StringListAttribute::kCustomActionDescriptions);
        if (custom_action_ids.size() != custom_action_descriptions.size()) {
          NOTREACHED();
          return;
        }

        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Array> custom_actions(
            v8::Array::New(isolate, custom_action_ids.size()));
        for (size_t i = 0; i < custom_action_ids.size(); i++) {
          gin::DataObjectBuilder custom_action(isolate);
          custom_action.Set("id", custom_action_ids[i]);
          custom_action.Set("description", custom_action_descriptions[i]);
          custom_actions
              ->CreateDataProperty(context, static_cast<uint32_t>(i),
                                   custom_action.Build())
              .Check();
        }
        result.Set(custom_actions);
      });
  RouteNodeIDFunction(
      "GetStandardActions",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::vector<std::string> standard_actions;
        for (uint32_t action = static_cast<uint32_t>(ax::mojom::Action::kNone);
             action <= static_cast<uint32_t>(ax::mojom::Action::kMaxValue);
             ++action) {
          if (node->data().HasAction(static_cast<ax::mojom::Action>(action))) {
            standard_actions.push_back(
                ui::ToString(static_cast<ax::mojom::Action>(action)));
          }
        }

        // TODO(crbug/955633): Set doDefault, increment, and decrement directly
        //     on the AXNode.
        // The doDefault action is implied by having a default action verb.
        int default_action_verb =
            static_cast<int>(ax::mojom::DefaultActionVerb::kNone);
        if (node->GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb,
                                  &default_action_verb) &&
            default_action_verb !=
                static_cast<int>(ax::mojom::DefaultActionVerb::kNone)) {
          standard_actions.push_back(ui::ToString(
              static_cast<ax::mojom::Action>(ax::mojom::Action::kDoDefault)));
        }

        // Increment and decrement are available when the role is a slider or
        // spin button.
        auto role = node->GetRole();
        if (role == ax::mojom::Role::kSlider ||
            role == ax::mojom::Role::kSpinButton) {
          standard_actions.push_back(ui::ToString(
              static_cast<ax::mojom::Action>(ax::mojom::Action::kIncrement)));
          standard_actions.push_back(ui::ToString(
              static_cast<ax::mojom::Action>(ax::mojom::Action::kDecrement)));
        }

        auto actions_result = v8::Array::New(isolate, standard_actions.size());
        for (size_t i = 0; i < standard_actions.size(); i++) {
          const v8::Maybe<bool>& did_set_value = actions_result->Set(
              isolate->GetCurrentContext(), i,
              v8::String::NewFromUtf8(isolate, standard_actions[i].c_str())
                  .ToLocalChecked());

          bool did_set_value_result = false;
          if (!did_set_value.To(&did_set_value_result) || !did_set_value_result)
            return;
        }
        result.Set(actions_result);
      });
  RouteNodeIDFunction(
      "GetChecked",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const ax::mojom::CheckedState checked_state =
            static_cast<ax::mojom::CheckedState>(
                node->GetIntAttribute(ax::mojom::IntAttribute::kCheckedState));
        if (checked_state != ax::mojom::CheckedState::kNone) {
          const std::string& checked_str = ui::ToString(checked_state);
          result.Set(v8::String::NewFromUtf8(isolate, checked_str.c_str())
                         .ToLocalChecked());
        }
      });
  RouteNodeIDFunction(
      "GetRestriction",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const ax::mojom::Restriction restriction =
            node->data().GetRestriction();
        if (restriction != ax::mojom::Restriction::kNone) {
          const std::string& restriction_str = ui::ToString(restriction);
          result.Set(v8::String::NewFromUtf8(isolate, restriction_str.c_str())
                         .ToLocalChecked());
        }
      });
  RouteNodeIDFunction(
      "GetDefaultActionVerb",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ax::mojom::DefaultActionVerb default_action_verb =
            static_cast<ax::mojom::DefaultActionVerb>(node->GetIntAttribute(
                ax::mojom::IntAttribute::kDefaultActionVerb));
        if (default_action_verb == ax::mojom::DefaultActionVerb::kNone)
          return;

        const std::string& default_action_verb_str =
            ui::ToString(default_action_verb);
        result.Set(
            v8::String::NewFromUtf8(isolate, default_action_verb_str.c_str())
                .ToLocalChecked());
      });
  RouteNodeIDFunction(
      "GetHasPopup",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ax::mojom::HasPopup has_popup = node->data().GetHasPopup();
        const std::string& has_popup_str = ui::ToString(has_popup);
        result.Set(v8::String::NewFromUtf8(isolate, has_popup_str.c_str())
                       .ToLocalChecked());
      });
  RouteNodeIDFunction(
      "GetAriaCurrentState",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ax::mojom::AriaCurrentState current_state =
            static_cast<ax::mojom::AriaCurrentState>(node->GetIntAttribute(
                ax::mojom::IntAttribute::kAriaCurrentState));
        if (current_state == ax::mojom::AriaCurrentState::kNone)
          return;
        const std::string& current_state_string = ui::ToString(current_state);
        result.Set(
            v8::String::NewFromUtf8(isolate, current_state_string.c_str())
                .ToLocalChecked());
      });
  RouteNodeIDFunction(
      "GetInvalidState",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ax::mojom::InvalidState invalid_state = node->GetInvalidState();
        if (invalid_state == ax::mojom::InvalidState::kNone)
          return;
        const std::string& invalid_state_string = ui::ToString(invalid_state);
        result.Set(
            v8::String::NewFromUtf8(isolate, invalid_state_string.c_str())
                .ToLocalChecked());
      });
  RouteNodeIDFunction(
      "GetIsButton",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value = ui::IsButton(node->GetRole());
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetIsCheckBox",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value = ui::IsCheckBox(node->GetRole());
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetIsComboBox",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value = ui::IsComboBox(node->GetRole());
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetIsImage",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value = ui::IsImage(node->GetRole());
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDPlusStringBoolFunction(
      "GetNextTextMatch",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node,
             const std::string& search_str, bool backward) {
        std::u16string search_str_16 = base::UTF8ToUTF16(search_str);
        auto next =
            backward ? &AutomationInternalCustomBindings::GetPreviousInTreeOrder
                     : &AutomationInternalCustomBindings::GetNextInTreeOrder;
        ui::AutomationAXTreeWrapper** target_tree_wrapper = &tree_wrapper;
        while (true) {
          node = (this->*next)(node, target_tree_wrapper);

          // We explicitly disallow searches in the desktop tree.
          if ((*target_tree_wrapper)->IsDesktopTree())
            return;

          if (!node)
            return;

          std::u16string name;
          if (!node->GetString16Attribute(ax::mojom::StringAttribute::kName,
                                          &name))
            continue;

          if (base::i18n::StringSearchIgnoringCaseAndAccents(
                  search_str_16, name, nullptr, nullptr)) {
            gin::DataObjectBuilder response(isolate);
            response.Set("treeId",
                         (*target_tree_wrapper)->GetTreeID().ToString());
            response.Set("nodeId", node->id());
            result.Set(response.Build());
            return;
          }
        }
      });
  RouteNodeIDFunction(
      "GetTableColumnCount",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        if (node->GetTableColCount())
          result.Set(*node->GetTableColCount());
      });
  RouteNodeIDFunction(
      "GetTableRowCount",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        if (node->GetTableRowCount())
          result.Set(*node->GetTableRowCount());
      });
  RouteNodeIDFunction(
      "GetTableCellColumnHeaders",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::vector<int32_t> col_headers = node->GetTableCellColHeaderNodeIds();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Array> array_result(
            v8::Array::New(isolate, col_headers.size()));
        for (size_t i = 0; i < col_headers.size(); ++i)
          array_result
              ->CreateDataProperty(context, static_cast<uint32_t>(i),
                                   v8::Integer::New(isolate, col_headers[i]))
              .Check();
        result.Set(array_result);
      });
  RouteNodeIDFunction(
      "GetTableCellRowHeaders",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::vector<int32_t> row_headers = node->GetTableCellRowHeaderNodeIds();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Array> array_result(
            v8::Array::New(isolate, row_headers.size()));
        for (size_t i = 0; i < row_headers.size(); ++i)
          array_result
              ->CreateDataProperty(context, static_cast<uint32_t>(i),
                                   v8::Integer::New(isolate, row_headers[i]))
              .Check();
        result.Set(array_result);
      });
  RouteNodeIDFunction(
      "GetTableCellColumnIndex",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        if (node->GetTableCellColIndex())
          result.Set(*node->GetTableCellColIndex());
      });
  RouteNodeIDFunction(
      "GetTableCellRowIndex",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        if (node->GetTableCellRowIndex())
          result.Set(*node->GetTableCellRowIndex());
      });
  RouteNodeIDFunction(
      "GetTableCellAriaColumnIndex",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        if (node->GetTableCellAriaColIndex())
          result.Set(*node->GetTableCellAriaColIndex());
      });
  RouteNodeIDFunction(
      "GetTableCellAriaRowIndex",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        if (node->GetTableCellAriaRowIndex())
          result.Set(*node->GetTableCellAriaRowIndex());
      });
  RouteNodeIDFunction(
      "SetAccessibilityFocus",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ui::AXTreeID tree_id = tree_wrapper->GetTreeID();
        if (tree_id != accessibility_focused_tree_id() &&
            accessibility_focused_tree_id() != ui::AXTreeIDUnknown()) {
          ui::AutomationAXTreeWrapper* previous_tree_wrapper =
              GetAutomationAXTreeWrapperFromTreeID(
                  accessibility_focused_tree_id());
          if (previous_tree_wrapper) {
            previous_tree_wrapper->SetAccessibilityFocus(ui::kInvalidAXNodeID);
          }
        }
        SetAccessibilityFocusedTreeID(tree_id);
        tree_wrapper->SetAccessibilityFocus(node->id());
      });
  RouteNodeIDFunction(
      "GetSortDirection",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        if (node->HasIntAttribute(ax::mojom::IntAttribute::kSortDirection)) {
          const std::string& sort_direction_str = ui::ToString(
              static_cast<ax::mojom::SortDirection>(node->GetIntAttribute(
                  ax::mojom::IntAttribute::kSortDirection)));
          result.Set(
              v8::String::NewFromUtf8(isolate, sort_direction_str.c_str())
                  .ToLocalChecked());
        }
      });
  RouteNodeIDFunction("GetValue", [](v8::Isolate* isolate,
                                     v8::ReturnValue<v8::Value> result,
                                     ui::AutomationAXTreeWrapper* tree_wrapper,
                                     ui::AXNode* node) {
    const std::string value_str = node->GetValueForControl();
    result.Set(
        v8::String::NewFromUtf8(isolate, value_str.c_str()).ToLocalChecked());
  });
  RouteNodeIDPlusEventFunction(
      "EventListenerAdded",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node,
             api::automation::EventType event_type) {
        tree_wrapper->EventListenerAdded(
            AutomationEventTypeToAXEventTuple(event_type), node);
        TreeEventListenersChanged(tree_wrapper);
      });
  RouteNodeIDPlusEventFunction(
      "EventListenerRemoved",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             ui::AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node,
             api::automation::EventType event_type) {
        tree_wrapper->EventListenerRemoved(
            AutomationEventTypeToAXEventTuple(event_type), node);
        TreeEventListenersChanged(tree_wrapper);
      });
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
    ThrowInvalidArgumentsException(this);
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
    ThrowInvalidArgumentsException(this);
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
    ThrowInvalidArgumentsException(this);
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
    ThrowInvalidArgumentsException(this);
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
    ThrowInvalidArgumentsException(this);
    return;
  }

  SetDesktopTreeId(ui::AXTreeID::FromString(
      *v8::String::Utf8Value(args.GetIsolate(), args[0])));
}

void AutomationInternalCustomBindings::GetHtmlAttributes(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = GetIsolate();
  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsNumber())
    ThrowInvalidArgumentsException(this);

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
    ThrowInvalidArgumentsException(this);

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
    ThrowInvalidArgumentsException(this);
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

void AutomationInternalCustomBindings::UpdateOverallTreeChangeObserverFilter() {
  tree_change_observer_overall_filter_ = 0;
  for (const auto& observer : tree_change_observers_)
    tree_change_observer_overall_filter_ |= 1 << observer.filter;
}

void AutomationInternalCustomBindings::RouteTreeIDFunction(
    const std::string& name,
    TreeIDFunction callback) {
  scoped_refptr<TreeIDWrapper> wrapper = new TreeIDWrapper(this, callback);
  RouteHandlerFunction(name, base::BindRepeating(&TreeIDWrapper::Run, wrapper));
}

void AutomationInternalCustomBindings::RouteNodeIDFunction(
    const std::string& name,
    NodeIDFunction callback) {
  auto wrapper = base::MakeRefCounted<NodeIDWrapper>(this, callback);
  RouteHandlerFunction(name, base::BindRepeating(&NodeIDWrapper::Run, wrapper));
}

void AutomationInternalCustomBindings::RouteNodeIDPlusAttributeFunction(
    const std::string& name,
    NodeIDPlusAttributeFunction callback) {
  auto wrapper =
      base::MakeRefCounted<NodeIDPlusAttributeWrapper>(this, callback);
  RouteHandlerFunction(
      name, base::BindRepeating(&NodeIDPlusAttributeWrapper::Run, wrapper));
}

void AutomationInternalCustomBindings::RouteNodeIDPlusRangeFunction(
    const std::string& name,
    NodeIDPlusRangeFunction callback) {
  auto wrapper = base::MakeRefCounted<NodeIDPlusRangeWrapper>(this, callback);
  RouteHandlerFunction(
      name, base::BindRepeating(&NodeIDPlusRangeWrapper::Run, wrapper));
}

void AutomationInternalCustomBindings::RouteNodeIDPlusStringBoolFunction(
    const std::string& name,
    NodeIDPlusStringBoolFunction callback) {
  auto wrapper =
      base::MakeRefCounted<NodeIDPlusStringBoolWrapper>(this, callback);
  RouteHandlerFunction(
      name, base::BindRepeating(&NodeIDPlusStringBoolWrapper::Run, wrapper));
}

void AutomationInternalCustomBindings::RouteNodeIDPlusDimensionsFunction(
    const std::string& name,
    NodeIDPlusDimensionsFunction callback) {
  auto wrapper =
      base::MakeRefCounted<NodeIDPlusDimensionsWrapper>(this, callback);
  RouteHandlerFunction(
      name, base::BindRepeating(&NodeIDPlusDimensionsWrapper::Run, wrapper));
}

void AutomationInternalCustomBindings::RouteNodeIDPlusEventFunction(
    const std::string& name,
    NodeIDPlusEventFunction callback) {
  auto wrapper = base::MakeRefCounted<NodeIDPlusEventWrapper>(this, callback);
  RouteHandlerFunction(
      name, base::BindRepeating(&NodeIDPlusEventWrapper::Run, wrapper));
}

void AutomationInternalCustomBindings::GetChildIDAtIndex(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 3 || !args[2]->IsNumber()) {
    ThrowInvalidArgumentsException(this);
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
