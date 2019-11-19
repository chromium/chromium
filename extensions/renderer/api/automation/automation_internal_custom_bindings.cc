// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "base/i18n/string_search.h"
#include "base/macros.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/automation.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "ipc/message_filter.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_language_detection.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace extensions {

namespace {

void ThrowInvalidArgumentsException(
    AutomationInternalCustomBindings* automation_bindings) {
  v8::Isolate* isolate = automation_bindings->GetIsolate();
  automation_bindings->GetIsolate()->ThrowException(
      v8::String::NewFromUtf8(
          isolate,
          "Invalid arguments to AutomationInternalCustomBindings function",
          v8::NewStringType::kNormal)
          .ToLocalChecked());

  LOG(FATAL) << "Invalid arguments to AutomationInternalCustomBindings function"
             << automation_bindings->context()->GetStackTraceAsString();
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

// Adjust the bounding box of a node from local to global coordinates,
// walking up the parent hierarchy to offset by frame offsets and
// scroll offsets.
// If |clip_bounds| is false, the bounds of the node will not be clipped
// to the ancestors bounding boxes if needed. Regardless of clipping, results
// are returned in global coordinates.
static gfx::Rect ComputeGlobalNodeBounds(AutomationAXTreeWrapper* tree_wrapper,
                                         ui::AXNode* node,
                                         gfx::RectF local_bounds = gfx::RectF(),
                                         bool* offscreen = nullptr,
                                         bool clip_bounds = true) {
  gfx::RectF bounds = local_bounds;

  while (node) {
    bounds = tree_wrapper->tree()->RelativeToTreeBounds(node, bounds, offscreen,
                                                        clip_bounds);

    if (!tree_wrapper->owner())
      break;

    AutomationAXTreeWrapper* previous_tree_wrapper = tree_wrapper;
    ui::AXNode* parent = tree_wrapper->owner()->GetParent(
        tree_wrapper->tree()->root(), &tree_wrapper);
    if (parent == node)
      break;

    // All trees other than the desktop tree are scaled by the device
    // scale factor. When crossing out of another tree into the desktop
    // tree, unscale the bounds by the device scale factor.
    if (!previous_tree_wrapper->IsDesktopTree() &&
        tree_wrapper->IsDesktopTree()) {
      float scale_factor = tree_wrapper->owner()->GetDeviceScaleFactor();
      if (scale_factor > 0)
        bounds.Scale(1.0 / scale_factor);
    }

    node = parent;
  }

  return gfx::ToEnclosingRect(bounds);
}

//
// Helper class that helps implement bindings for a JavaScript function
// that takes a single input argument consisting of a Tree ID. Looks up
// the AutomationAXTreeWrapper and passes it to the function passed to the
// constructor.
//

typedef void (*TreeIDFunction)(v8::Isolate* isolate,
                               v8::ReturnValue<v8::Value> result,
                               AutomationAXTreeWrapper* tree_wrapper);

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
    AutomationAXTreeWrapper* tree_wrapper =
        automation_bindings_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
    if (!tree_wrapper)
      return;

    // The root can be null if this is called from an onTreeChange callback.
    if (!tree_wrapper->tree()->root())
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
// AutomationAXTreeWrapper and the AXNode and passes them to the function passed
// to the constructor.
//
typedef std::function<void(v8::Isolate* isolate,
                           v8::ReturnValue<v8::Value> result,
                           AutomationAXTreeWrapper* tree_wrapper,
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

    AutomationAXTreeWrapper* tree_wrapper =
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
// argument. Looks up the AutomationAXTreeWrapper and the AXNode and passes them
// to the function passed to the constructor.
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

    AutomationAXTreeWrapper* tree_wrapper =
        automation_bindings_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
    if (!tree_wrapper)
      return;

    ui::AXNode* node = tree_wrapper->GetUnignoredNodeFromId(node_id);
    if (!node)
      return;

    function_(isolate, args.GetReturnValue(), tree_wrapper->tree(), node,
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
// and end indices. Looks up the AutomationAXTreeWrapper and the AXNode and
// passes them to the function passed to the constructor.
//

typedef void (*NodeIDPlusRangeFunction)(v8::Isolate* isolate,
                                        v8::ReturnValue<v8::Value> result,
                                        AutomationAXTreeWrapper* tree_wrapper,
                                        ui::AXNode* node,
                                        int start,
                                        int end);

class NodeIDPlusRangeWrapper
    : public base::RefCountedThreadSafe<NodeIDPlusRangeWrapper> {
 public:
  NodeIDPlusRangeWrapper(AutomationInternalCustomBindings* automation_bindings,
                         NodeIDPlusRangeFunction function)
      : automation_bindings_(automation_bindings), function_(function) {}

  void Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = automation_bindings_->GetIsolate();
    if (args.Length() < 4 || !args[0]->IsString() || !args[1]->IsNumber() ||
        !args[2]->IsNumber() || !args[3]->IsNumber()) {
      ThrowInvalidArgumentsException(automation_bindings_);
    }

    v8::Local<v8::Context> context =
        automation_bindings_->context()->v8_context();
    ui::AXTreeID tree_id =
        ui::AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
    int node_id = args[1]->Int32Value(context).FromMaybe(0);
    int start = args[2]->Int32Value(context).FromMaybe(0);
    int end = args[3]->Int32Value(context).FromMaybe(0);

    AutomationAXTreeWrapper* tree_wrapper =
        automation_bindings_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
    if (!tree_wrapper)
      return;

    ui::AXNode* node = tree_wrapper->GetUnignoredNodeFromId(node_id);
    if (!node)
      return;

    function_(isolate, args.GetReturnValue(), tree_wrapper, node, start, end);
  }

 private:
  virtual ~NodeIDPlusRangeWrapper() {}

  friend class base::RefCountedThreadSafe<NodeIDPlusRangeWrapper>;

  AutomationInternalCustomBindings* automation_bindings_;
  NodeIDPlusRangeFunction function_;
};

typedef std::function<void(v8::Isolate* isolate,
                           v8::ReturnValue<v8::Value> result,
                           AutomationAXTreeWrapper* tree_wrapper,
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

    AutomationAXTreeWrapper* tree_wrapper =
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
    void (*)(v8::Isolate* isolate,
             v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper,
             ui::AXNode* node,
             int x,
             int y,
             int width,
             int height);

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

    AutomationAXTreeWrapper* tree_wrapper =
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

  DISALLOW_COPY_AND_ASSIGN(AutomationMessageFilter);
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
  ROUTE_FUNCTION(DestroyAccessibilityTree);
  ROUTE_FUNCTION(AddTreeChangeObserver);
  ROUTE_FUNCTION(RemoveTreeChangeObserver);
  ROUTE_FUNCTION(GetChildIDAtIndex);
  ROUTE_FUNCTION(GetFocus);
  ROUTE_FUNCTION(GetHtmlAttributes);
  ROUTE_FUNCTION(GetState);
#undef ROUTE_FUNCTION

  // Bindings that take a Tree ID and return a property of the tree.

  RouteTreeIDFunction("GetRootID", [](v8::Isolate* isolate,
                                      v8::ReturnValue<v8::Value> result,
                                      AutomationAXTreeWrapper* tree_wrapper) {
    result.Set(v8::Integer::New(isolate, tree_wrapper->tree()->root()->id()));
  });
  RouteTreeIDFunction("GetDocURL", [](v8::Isolate* isolate,
                                      v8::ReturnValue<v8::Value> result,
                                      AutomationAXTreeWrapper* tree_wrapper) {
    result.Set(v8::String::NewFromUtf8(isolate,
                                       tree_wrapper->tree()->data().url.c_str(),
                                       v8::NewStringType::kNormal)
                   .ToLocalChecked());
  });
  RouteTreeIDFunction(
      "GetDocTitle", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                        AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::String::NewFromUtf8(
                       isolate, tree_wrapper->tree()->data().title.c_str(),
                       v8::NewStringType::kNormal)
                       .ToLocalChecked());
      });
  RouteTreeIDFunction(
      "GetDocLoaded",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(
            v8::Boolean::New(isolate, tree_wrapper->tree()->data().loaded));
      });
  RouteTreeIDFunction(
      "GetDocLoadingProgress",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->tree()->data().loading_progress));
      });
  RouteTreeIDFunction(
      "GetIsSelectionBackward",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        const ui::AXNode* anchor = tree_wrapper->tree()->GetFromId(
            tree_wrapper->GetUnignoredSelection().anchor_object_id);
        if (!anchor)
          return;

        result.Set(v8::Boolean::New(
            isolate, tree_wrapper->tree()->data().sel_is_backward));
      });
  RouteTreeIDFunction(
      "GetAnchorObjectID",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->GetUnignoredSelection().anchor_object_id));
      });
  RouteTreeIDFunction(
      "GetAnchorOffset",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->GetUnignoredSelection().anchor_offset));
      });
  RouteTreeIDFunction(
      "GetAnchorAffinity",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(CreateV8String(
            isolate,
            ui::ToString(
                tree_wrapper->GetUnignoredSelection().anchor_affinity)));
      });
  RouteTreeIDFunction(
      "GetFocusObjectID",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->GetUnignoredSelection().focus_object_id));
      });
  RouteTreeIDFunction(
      "GetFocusOffset",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->GetUnignoredSelection().focus_offset));
      });
  RouteTreeIDFunction(
      "GetFocusAffinity",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(CreateV8String(
            isolate,
            ui::ToString(
                tree_wrapper->GetUnignoredSelection().focus_affinity)));
      });
  RouteTreeIDFunction(
      "GetSelectionStartObjectID",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        ui::AXTree::Selection unignored_selection =
            tree_wrapper->GetUnignoredSelection();
        int32_t start_object_id = unignored_selection.is_backward
                                      ? unignored_selection.focus_object_id
                                      : unignored_selection.anchor_object_id;
        result.Set(v8::Number::New(isolate, start_object_id));
      });
  RouteTreeIDFunction(
      "GetSelectionStartOffset",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        ui::AXTree::Selection unignored_selection =
            tree_wrapper->GetUnignoredSelection();
        int start_offset = unignored_selection.is_backward
                               ? unignored_selection.focus_offset
                               : unignored_selection.anchor_offset;
        result.Set(v8::Number::New(isolate, start_offset));
      });
  RouteTreeIDFunction(
      "GetSelectionStartAffinity",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        ui::AXTree::Selection unignored_selection =
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
         AutomationAXTreeWrapper* tree_wrapper) {
        ui::AXTree::Selection unignored_selection =
            tree_wrapper->GetUnignoredSelection();
        int32_t end_object_id = unignored_selection.is_backward
                                    ? unignored_selection.anchor_object_id
                                    : unignored_selection.focus_object_id;
        result.Set(v8::Number::New(isolate, end_object_id));
      });
  RouteTreeIDFunction(
      "GetSelectionEndOffset",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        ui::AXTree::Selection unignored_selection =
            tree_wrapper->GetUnignoredSelection();
        int end_offset = unignored_selection.is_backward
                             ? unignored_selection.anchor_offset
                             : unignored_selection.focus_offset;
        result.Set(v8::Number::New(isolate, end_offset));
      });
  RouteTreeIDFunction(
      "GetSelectionEndAffinity",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        ui::AXTree::Selection unignored_selection =
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
             AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ui::AXNode* parent = GetParent(node, &tree_wrapper);
        if (parent) {
          gin::DataObjectBuilder response(isolate);
          response.Set("treeId", tree_wrapper->tree_id().ToString());
          response.Set("nodeId", parent->id());
          result.Set(response.Build());
        }
      });
  RouteNodeIDFunction(
      "GetChildCount",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        size_t child_count;
        if (GetRootOfChildTree(&node, &tree_wrapper))
          child_count = 1;
        else
          child_count = node->GetUnignoredChildCount();

        result.Set(v8::Integer::New(isolate, int32_t{child_count}));
      });
  RouteNodeIDFunction(
      "GetIndexInParent",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        result.Set(v8::Integer::New(
            isolate, int32_t{node->GetUnignoredIndexInParent()}));
      });
  RouteNodeIDFunction(
      "GetRole", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                    AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::string role_name = ui::ToString(node->data().role);
        result.Set(v8::String::NewFromUtf8(isolate, role_name.c_str(),
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked());
      });
  RouteNodeIDFunction(
      "GetLocation",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        gfx::Rect global_clipped_bounds =
            ComputeGlobalNodeBounds(tree_wrapper, node);
        result.Set(RectToV8Object(isolate, global_clipped_bounds));
      });
  RouteNodeIDFunction(
      "GetUnclippedLocation",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool offscreen = false;
        gfx::Rect global_unclipped_bounds =
            ComputeGlobalNodeBounds(tree_wrapper, node, gfx::RectF(),
                                    &offscreen, false /* clip_bounds */);
        result.Set(RectToV8Object(isolate, global_unclipped_bounds));
      });
  RouteNodeIDFunction(
      "GetLineStartOffsets",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const std::vector<int> line_starts =
            node->GetOrComputeLineStartOffsets();
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
             AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::vector<int> child_ids;
        if (GetRootOfChildTree(&node, &tree_wrapper)) {
          child_ids.push_back(node->id());
        } else {
          size_t child_count = node->GetUnignoredChildCount();
          for (size_t i = 0; i < child_count; ++i)
            child_ids.push_back(node->GetUnignoredChildAtIndex(i)->id());
        }

        gin::DataObjectBuilder response(isolate);
        response.Set("treeId", tree_wrapper->tree_id().ToString());
        response.Set("nodeIds", child_ids);
        result.Set(response.Build());
      });
  RouteNodeIDFunction(
      "GetWordStartOffsets",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::vector<int> word_starts = ui::GetWordStartOffsets(
            node->GetString16Attribute(ax::mojom::StringAttribute::kName));
        result.Set(gin::ConvertToV8(isolate, word_starts));
      });
  RouteNodeIDFunction(
      "GetWordEndOffsets",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::vector<int> word_ends = ui::GetWordEndOffsets(
            node->GetString16Attribute(ax::mojom::StringAttribute::kName));
        result.Set(gin::ConvertToV8(isolate, word_ends));
      });
  // Bindings that take a Tree ID and Node ID and string attribute name
  // and return a property of the node.

  RouteNodeIDPlusRangeFunction(
      "GetBoundsForRange",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node, int start,
         int end) {
        if (node->data().role != ax::mojom::Role::kInlineTextBox)
          return;

        // Use character offsets to compute the local bounds of this subrange.
        gfx::RectF local_bounds(0, 0,
                                node->data().relative_bounds.bounds.width(),
                                node->data().relative_bounds.bounds.height());
        std::string name =
            node->data().GetStringAttribute(ax::mojom::StringAttribute::kName);
        std::vector<int> character_offsets = node->data().GetIntListAttribute(
            ax::mojom::IntListAttribute::kCharacterOffsets);
        int len =
            static_cast<int>(std::min(name.size(), character_offsets.size()));
        if (start >= 0 && start <= end && end <= len) {
          int start_offset = start > 0 ? character_offsets[start - 1] : 0;
          int end_offset = end > 0 ? character_offsets[end - 1] : 0;

          switch (node->data().GetTextDirection()) {
            case ax::mojom::TextDirection::kLtr:
            default:
              local_bounds.set_x(local_bounds.x() + start_offset);
              local_bounds.set_width(end_offset - start_offset);
              break;
            case ax::mojom::TextDirection::kRtl:
              local_bounds.set_x(local_bounds.x() + local_bounds.width() -
                                 end_offset);
              local_bounds.set_width(end_offset - start_offset);
              break;
            case ax::mojom::TextDirection::kTtb:
              local_bounds.set_y(local_bounds.y() + start_offset);
              local_bounds.set_height(end_offset - start_offset);
              break;
            case ax::mojom::TextDirection::kBtt:
              local_bounds.set_y(local_bounds.y() + local_bounds.height() -
                                 end_offset);
              local_bounds.set_height(end_offset - start_offset);
              break;
          }
        }

        // Convert from local to global coordinates second, after subsetting,
        // because the local to global conversion might involve matrix
        // transformations.
        gfx::Rect global_bounds = ComputeGlobalNodeBounds(
            tree_wrapper, node, local_bounds, nullptr, true /* clip_bounds */);
        result.Set(RectToV8Object(isolate, global_bounds));
      });

  RouteNodeIDPlusDimensionsFunction(
      "ComputeGlobalBounds",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node, int x, int y,
         int width, int height) {
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
        ax::mojom::StringAttribute attribute =
            ui::ParseStringAttribute(attribute_name.c_str());
        std::string attr_value;
        if (attribute == ax::mojom::StringAttribute::kFontFamily ||
            attribute == ax::mojom::StringAttribute::kLanguage) {
          attr_value = node->GetInheritedStringAttribute(attribute);
        } else if (!node->data().GetStringAttribute(attribute, &attr_value)) {
          return;
        }

        result.Set(v8::String::NewFromUtf8(isolate, attr_value.c_str(),
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked());
      });
  RouteNodeIDPlusAttributeFunction(
      "GetBoolAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        ax::mojom::BoolAttribute attribute =
            ui::ParseBoolAttribute(attribute_name.c_str());
        bool attr_value;
        if (!node->data().GetBoolAttribute(attribute, &attr_value))
          return;

        result.Set(v8::Boolean::New(isolate, attr_value));
      });
  RouteNodeIDPlusAttributeFunction(
      "GetIntAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        ax::mojom::IntAttribute attribute =
            ui::ParseIntAttribute(attribute_name.c_str());
        int attr_value;

        if (attribute == ax::mojom::IntAttribute::kPosInSet &&
            node->GetPosInSet()) {
          attr_value = *node->GetPosInSet();
        } else if (attribute == ax::mojom::IntAttribute::kSetSize &&
                   node->GetSetSize()) {
          attr_value = *node->GetSetSize();
        } else if (!node->data().GetIntAttribute(attribute, &attr_value)) {
          return;
        }

        result.Set(v8::Integer::New(isolate, attr_value));
      });
  RouteNodeIDPlusAttributeFunction(
      "GetIntAttributeReverseRelations",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        ax::mojom::IntAttribute attribute =
            ui::ParseIntAttribute(attribute_name.c_str());
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
        ax::mojom::FloatAttribute attribute =
            ui::ParseFloatAttribute(attribute_name.c_str());
        float attr_value;

        if (!node->data().GetFloatAttribute(attribute, &attr_value))
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
        ax::mojom::IntListAttribute attribute =
            ui::ParseIntListAttribute(attribute_name.c_str());
        if (!node->data().HasIntListAttribute(attribute))
          return;
        const std::vector<int32_t>& attr_value =
            node->data().GetIntListAttribute(attribute);

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
        ax::mojom::IntListAttribute attribute =
            ui::ParseIntListAttribute(attribute_name.c_str());
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
        std::string attr_value;
        if (!node->data().GetHtmlAttribute(attribute_name.c_str(), &attr_value))
          return;

        result.Set(v8::String::NewFromUtf8(isolate, attr_value.c_str(),
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked());
      });
  RouteNodeIDFunction(
      "GetNameFrom",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ax::mojom::NameFrom name_from = static_cast<ax::mojom::NameFrom>(
            node->data().GetIntAttribute(ax::mojom::IntAttribute::kNameFrom));
        std::string name_from_str = ui::ToString(name_from);
        result.Set(v8::String::NewFromUtf8(isolate, name_from_str.c_str(),
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked());
      });
  RouteNodeIDFunction(
      "GetDescriptionFrom",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ax::mojom::DescriptionFrom description_from =
            static_cast<ax::mojom::DescriptionFrom>(
                node->data().GetIntAttribute(
                    ax::mojom::IntAttribute::kDescriptionFrom));
        std::string description_from_str = ui::ToString(description_from);
        result.Set(v8::String::NewFromUtf8(isolate,
                                           description_from_str.c_str(),
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked());
      });
  RouteNodeIDFunction(
      "GetImageAnnotation",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::string status_string;
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
        result.Set(v8::String::NewFromUtf8(isolate, status_string.c_str(),
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked());
      });
  RouteNodeIDFunction("GetSubscript", [](v8::Isolate* isolate,
                                         v8::ReturnValue<v8::Value> result,
                                         AutomationAXTreeWrapper* tree_wrapper,
                                         ui::AXNode* node) {
    bool value =
        node->data().GetIntAttribute(ax::mojom::IntAttribute::kTextPosition) ==
        static_cast<int32_t>(ax::mojom::TextPosition::kSubscript);
    result.Set(v8::Boolean::New(isolate, value));
  });
  RouteNodeIDFunction(
      "GetSuperscript",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value =
            node->data().GetIntAttribute(
                ax::mojom::IntAttribute::kTextPosition) ==
            static_cast<int32_t>(ax::mojom::TextPosition::kSuperscript);
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetBold", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                    AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value = node->data().HasTextStyle(ax::mojom::TextStyle::kBold);
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetItalic", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                      AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value = node->data().HasTextStyle(ax::mojom::TextStyle::kItalic);
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction("GetUnderline", [](v8::Isolate* isolate,
                                         v8::ReturnValue<v8::Value> result,
                                         AutomationAXTreeWrapper* tree_wrapper,
                                         ui::AXNode* node) {
    bool value = node->data().HasTextStyle(ax::mojom::TextStyle::kUnderline);
    result.Set(v8::Boolean::New(isolate, value));
  });
  RouteNodeIDFunction(
      "GetLineThrough",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        bool value =
            node->data().HasTextStyle(ax::mojom::TextStyle::kLineThrough);
        result.Set(v8::Boolean::New(isolate, value));
      });
  RouteNodeIDFunction(
      "GetDetectedLanguage",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::string detectedLanguage = node->GetLanguage();
        result.Set(v8::String::NewFromUtf8(isolate, detectedLanguage.c_str(),
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked());
      });

  RouteNodeIDPlusAttributeFunction(
      "GetLanguageAnnotationForStringAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         ui::AXTree* tree, ui::AXNode* node,
         const std::string& attribute_name) {
        ax::mojom::StringAttribute attr =
            ui::ParseStringAttribute(attribute_name.c_str());
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
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const std::vector<int32_t>& custom_action_ids =
            node->data().GetIntListAttribute(
                ax::mojom::IntListAttribute::kCustomActionIds);
        if (custom_action_ids.empty()) {
          result.SetUndefined();
          return;
        }

        const std::vector<std::string>& custom_action_descriptions =
            node->data().GetStringListAttribute(
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
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::vector<std::string> standard_actions;
        for (uint32_t action = static_cast<uint32_t>(ax::mojom::Action::kNone);
             action <= static_cast<uint32_t>(ax::mojom::Action::kMaxValue);
             ++action) {
          if (node->data().HasAction(static_cast<ax::mojom::Action>(action)))
            standard_actions.push_back(
                ToString(static_cast<api::automation::ActionType>(action)));
        }

        // TODO(crbug/955633): Set doDefault, increment, and decrement directly
        //     on the AXNode.
        // The doDefault action is implied by having a default action verb.
        int default_action_verb =
            static_cast<int>(ax::mojom::DefaultActionVerb::kNone);
        if (node->data().GetIntAttribute(
                ax::mojom::IntAttribute::kDefaultActionVerb,
                &default_action_verb) &&
            default_action_verb !=
                static_cast<int>(ax::mojom::DefaultActionVerb::kNone)) {
          standard_actions.push_back(
              ToString(static_cast<api::automation::ActionType>(
                  ax::mojom::Action::kDoDefault)));
        }

        // Increment and decrement are available when the role is a slider or
        // spin button.
        std::string role_string;
        node->GetStringAttribute(ax::mojom::StringAttribute::kRole,
                                 &role_string);
        ax::mojom::Role role = ui::ParseRole(role_string.c_str());
        if (role == ax::mojom::Role::kSlider ||
            role == ax::mojom::Role::kSpinButton) {
          standard_actions.push_back(
              ToString(static_cast<api::automation::ActionType>(
                  ax::mojom::Action::kIncrement)));
          standard_actions.push_back(
              ToString(static_cast<api::automation::ActionType>(
                  ax::mojom::Action::kDecrement)));
        }

        auto actions_result = v8::Array::New(isolate, standard_actions.size());
        for (size_t i = 0; i < standard_actions.size(); i++) {
          const v8::Maybe<bool>& did_set_value = actions_result->Set(
              isolate->GetCurrentContext(), i,
              v8::String::NewFromUtf8(isolate, standard_actions[i].c_str(),
                                      v8::NewStringType::kNormal)
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
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const ax::mojom::CheckedState checked_state =
            static_cast<ax::mojom::CheckedState>(node->data().GetIntAttribute(
                ax::mojom::IntAttribute::kCheckedState));
        if (checked_state != ax::mojom::CheckedState::kNone) {
          const std::string checked_str = ui::ToString(checked_state);
          result.Set(v8::String::NewFromUtf8(isolate, checked_str.c_str(),
                                             v8::NewStringType::kNormal)
                         .ToLocalChecked());
        }
      });
  RouteNodeIDFunction(
      "GetRestriction",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        const ax::mojom::Restriction restriction =
            node->data().GetRestriction();
        if (restriction != ax::mojom::Restriction::kNone) {
          const std::string restriction_str = ui::ToString(restriction);
          result.Set(v8::String::NewFromUtf8(isolate, restriction_str.c_str(),
                                             v8::NewStringType::kNormal)
                         .ToLocalChecked());
        }
      });
  RouteNodeIDFunction(
      "GetDefaultActionVerb",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ax::mojom::DefaultActionVerb default_action_verb =
            static_cast<ax::mojom::DefaultActionVerb>(
                node->data().GetIntAttribute(
                    ax::mojom::IntAttribute::kDefaultActionVerb));
        std::string default_action_verb_str = ui::ToString(default_action_verb);
        result.Set(v8::String::NewFromUtf8(isolate,
                                           default_action_verb_str.c_str(),
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked());
      });
  RouteNodeIDFunction(
      "GetHasPopup",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        ax::mojom::HasPopup has_popup = node->data().GetHasPopup();
        std::string has_popup_str = ui::ToString(has_popup);
        result.Set(v8::String::NewFromUtf8(isolate, has_popup_str.c_str(),
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked());
      });
  RouteNodeIDPlusStringBoolFunction(
      "GetNextTextMatch",
      [this](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node,
             const std::string& search_str, bool backward) {
        base::string16 search_str_16 = base::UTF8ToUTF16(search_str);
        auto next =
            backward ? &AutomationInternalCustomBindings::GetPreviousInTreeOrder
                     : &AutomationInternalCustomBindings::GetNextInTreeOrder;
        AutomationAXTreeWrapper** target_tree_wrapper = &tree_wrapper;
        while (true) {
          node = (this->*next)(node, target_tree_wrapper);

          // We explicitly disallow searches in the desktop tree.
          if ((*target_tree_wrapper)->IsDesktopTree())
            return;

          if (!node)
            return;

          base::string16 name;
          if (!node->GetString16Attribute(ax::mojom::StringAttribute::kName,
                                          &name))
            continue;

          if (base::i18n::StringSearchIgnoringCaseAndAccents(
                  search_str_16, name, nullptr, nullptr)) {
            gin::DataObjectBuilder response(isolate);
            response.Set("treeId",
                         (*target_tree_wrapper)->tree_id().ToString());
            response.Set("nodeId", node->id());
            result.Set(response.Build());
            return;
          }
        }
      });
  RouteNodeIDFunction(
      "GetTableCellColumnHeaders",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::vector<int32_t> col_headers;
        node->GetTableCellColHeaderNodeIds(&col_headers);
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
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        std::vector<int32_t> row_headers;
        node->GetTableCellRowHeaderNodeIds(&row_headers);
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
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        if (node->GetTableCellColIndex())
          result.Set(*node->GetTableCellColIndex());
      });
  RouteNodeIDFunction(
      "GetTableCellRowIndex",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        if (node->GetTableCellRowIndex())
          result.Set(*node->GetTableCellRowIndex());
      });
  RouteNodeIDFunction(
      "GetTableCellAriaColumnIndex",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        if (node->GetTableCellAriaColIndex())
          result.Set(*node->GetTableCellAriaColIndex());
      });
  RouteNodeIDFunction(
      "GetTableCellAriaRowIndex",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper, ui::AXNode* node) {
        if (node->GetTableCellAriaRowIndex())
          result.Set(*node->GetTableCellAriaRowIndex());
      });
}

void AutomationInternalCustomBindings::Invalidate() {
  ObjectBackedNativeHandler::Invalidate();

  if (message_filter_)
    message_filter_->Detach();

  tree_id_to_tree_wrapper_map_.clear();
}

// http://crbug.com/784266
// clang-format off
void AutomationInternalCustomBindings::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(AutomationInternalCustomBindings, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AccessibilityEventBundle,
                        OnAccessibilityEvents)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AccessibilityLocationChange,
                        OnAccessibilityLocationChange)
  IPC_END_MESSAGE_MAP()
}  // clang-format on

AutomationAXTreeWrapper* AutomationInternalCustomBindings::
    GetAutomationAXTreeWrapperFromTreeID(ui::AXTreeID tree_id) const {
  const auto iter = tree_id_to_tree_wrapper_map_.find(tree_id);
  if (iter == tree_id_to_tree_wrapper_map_.end())
    return nullptr;

  return iter->second.get();
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
      AutomationAXTreeWrapper::GetChildTreeIDReverseMap();
  base::EraseIf(
      child_tree_id_reverse_map,
      [tree_id](const std::pair<ui::AXTreeID, AutomationAXTreeWrapper*>& pair) {
        return pair.first == tree_id || pair.second->tree_id() == tree_id;
      });

  auto iter = tree_id_to_tree_wrapper_map_.find(tree_id);
  if (iter == tree_id_to_tree_wrapper_map_.end())
    return;

  AutomationAXTreeWrapper* tree_wrapper = iter->second.get();
  axtree_to_tree_wrapper_map_.erase(tree_wrapper->tree());
  tree_id_to_tree_wrapper_map_.erase(tree_id);
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

bool AutomationInternalCustomBindings::GetFocusInternal(
    AutomationAXTreeWrapper* tree_wrapper,
    AutomationAXTreeWrapper** out_tree_wrapper,
    ui::AXNode** out_node) {
  int focus_id = tree_wrapper->tree()->data().focus_id;
  ui::AXNode* focus = tree_wrapper->tree()->GetFromId(focus_id);
  if (!focus)
    return false;

  // If the focused node is the owner of a child tree, that indicates
  // a node within the child tree is the one that actually has focus.
  while (focus->data().HasStringAttribute(
      ax::mojom::StringAttribute::kChildTreeId)) {
    // Try to keep following focus recursively, by letting |tree_id| be the
    // new subtree to search in, while keeping |focus_tree_id| set to the tree
    // where we know we found a focused node.
    ui::AXTreeID child_tree_id =
        ui::AXTreeID::FromString(focus->data().GetStringAttribute(
            ax::mojom::StringAttribute::kChildTreeId));

    AutomationAXTreeWrapper* child_tree_wrapper =
        GetAutomationAXTreeWrapperFromTreeID(child_tree_id);
    if (!child_tree_wrapper)
      break;

    // If |child_tree_wrapper| is a frame tree that indicates a focused frame,
    // jump to that frame if possible.
    ui::AXTreeID focused_tree_id =
        child_tree_wrapper->tree()->data().focused_tree_id;
    if (focused_tree_id != ui::AXTreeIDUnknown() &&
        !child_tree_wrapper->IsDesktopTree()) {
      AutomationAXTreeWrapper* focused_tree_wrapper =
          GetAutomationAXTreeWrapperFromTreeID(
              child_tree_wrapper->tree()->data().focused_tree_id);
      if (focused_tree_wrapper)
        child_tree_wrapper = focused_tree_wrapper;
    }

    int child_focus_id = child_tree_wrapper->tree()->data().focus_id;
    ui::AXNode* child_focus =
        child_tree_wrapper->tree()->GetFromId(child_focus_id);
    if (!child_focus)
      break;

    focus = child_focus;
    tree_wrapper = child_tree_wrapper;
  }

  *out_tree_wrapper = tree_wrapper;
  *out_node = focus;
  return true;
}

void AutomationInternalCustomBindings::GetFocus(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 1 || !args[0]->IsString()) {
    ThrowInvalidArgumentsException(this);
    return;
  }

  ui::AXTreeID tree_id = ui::AXTreeID::FromString(
      *v8::String::Utf8Value(args.GetIsolate(), args[0]));
  AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper)
    return;

  AutomationAXTreeWrapper* focused_tree_wrapper = nullptr;
  ui::AXNode* focused_node = nullptr;
  if (!GetFocusInternal(tree_wrapper, &focused_tree_wrapper, &focused_node))
    return;

  args.GetReturnValue().Set(
      gin::DataObjectBuilder(GetIsolate())
          .Set("treeId", focused_tree_wrapper->tree_id().ToString())
          .Set("nodeId", focused_node->id())
          .Build());
}

void AutomationInternalCustomBindings::GetHtmlAttributes(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = GetIsolate();
  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsNumber())
    ThrowInvalidArgumentsException(this);

  ui::AXTreeID tree_id =
      ui::AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
  int node_id = args[1]->Int32Value(context()->v8_context()).FromMaybe(0);

  AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper)
    return;

  ui::AXNode* node = tree_wrapper->tree()->GetFromId(node_id);
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

  AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper)
    return;

  ui::AXNode* node = tree_wrapper->tree()->GetFromId(node_id);
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
  AutomationAXTreeWrapper* top_tree_wrapper = nullptr;
  AutomationAXTreeWrapper* walker = tree_wrapper;
  while (walker && walker != top_tree_wrapper) {
    top_tree_wrapper = walker;
    GetParent(walker->tree()->root(), &walker);
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

void AutomationInternalCustomBindings::UpdateOverallTreeChangeObserverFilter() {
  tree_change_observer_overall_filter_ = 0;
  for (const auto& observer : tree_change_observers_)
    tree_change_observer_overall_filter_ |= 1 << observer.filter;
}

ui::AXNode* AutomationInternalCustomBindings::GetParent(
    ui::AXNode* node,
    AutomationAXTreeWrapper** in_out_tree_wrapper) const {
  if (node->GetUnignoredParent())
    return node->GetUnignoredParent();

  AutomationAXTreeWrapper* parent_tree_wrapper = nullptr;

  ui::AXTreeID parent_tree_id =
      (*in_out_tree_wrapper)->tree()->data().parent_tree_id;
  if (parent_tree_id != ui::AXTreeIDUnknown()) {
    // If the tree specifies its parent tree ID, use that. That provides
    // some additional security guarantees, so a tree can't be "claimed"
    // by something else.
    parent_tree_wrapper = GetAutomationAXTreeWrapperFromTreeID(parent_tree_id);
  } else {
    // Otherwise if it was unspecified, check to see if another tree listed
    // this one as its child, and then we know the parent.
    parent_tree_wrapper = AutomationAXTreeWrapper::GetParentOfTreeId(
        (*in_out_tree_wrapper)->tree_id());
  }

  if (!parent_tree_wrapper)
    return nullptr;

  std::set<int32_t> host_node_ids =
      parent_tree_wrapper->tree()->GetNodeIdsForChildTreeId(
          (*in_out_tree_wrapper)->tree_id());

#if !defined(NDEBUG)
  if (host_node_ids.size() > 1)
    DLOG(WARNING) << "Multiple nodes claim the same child tree id.";
#endif

  for (int32_t host_node_id : host_node_ids) {
    ui::AXNode* host_node =
        parent_tree_wrapper->tree()->GetFromId(host_node_id);
    if (host_node) {
      DCHECK_EQ((*in_out_tree_wrapper)->tree_id(),
                ui::AXTreeID::FromString(host_node->GetStringAttribute(
                    ax::mojom::StringAttribute::kChildTreeId)));
      *in_out_tree_wrapper = parent_tree_wrapper;
      return host_node;
    }
  }

  return nullptr;
}

bool AutomationInternalCustomBindings::GetRootOfChildTree(
    ui::AXNode** in_out_node,
    AutomationAXTreeWrapper** in_out_tree_wrapper) const {
  std::string child_tree_id_str;
  if (!(*in_out_node)
           ->GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId,
                                &child_tree_id_str))
    return false;

  AutomationAXTreeWrapper* child_tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(
          ui::AXTreeID::FromString(child_tree_id_str));
  if (!child_tree_wrapper || !child_tree_wrapper->tree()->root())
    return false;

  *in_out_tree_wrapper = child_tree_wrapper;
  *in_out_node = child_tree_wrapper->tree()->root();
  return true;
}

ui::AXNode* AutomationInternalCustomBindings::GetNextInTreeOrder(
    ui::AXNode* start,
    AutomationAXTreeWrapper** in_out_tree_wrapper) const {
  ui::AXNode* walker = start;
  if (walker->GetUnignoredChildCount())
    return walker->GetUnignoredChildAtIndex(0);

  // We also have to check child tree id.
  if (GetRootOfChildTree(&walker, in_out_tree_wrapper))
    return walker;

  // Find the next branch forward.
  ui::AXNode* parent;
  while ((parent = GetParent(walker, in_out_tree_wrapper))) {
    if ((walker->GetUnignoredIndexInParent() + 1) <
        parent->GetUnignoredChildCount()) {
      return parent->GetUnignoredChildAtIndex(
          walker->GetUnignoredIndexInParent() + 1);
    }
    walker = parent;
  }

  return nullptr;
}

ui::AXNode* AutomationInternalCustomBindings::GetPreviousInTreeOrder(
    ui::AXNode* start,
    AutomationAXTreeWrapper** in_out_tree_wrapper) const {
  ui::AXNode* walker = start;

  ui::AXNode* parent = GetParent(start, in_out_tree_wrapper);

  // No more nodes.
  if (!parent)
    return nullptr;

  // No previous sibling; parent is previous.
  if (walker->GetUnignoredIndexInParent() == 0)
    return parent;

  walker =
      parent->GetUnignoredChildAtIndex(walker->GetUnignoredIndexInParent() - 1);

  // Walks to deepest last child.
  while (true) {
    if (walker->GetUnignoredChildCount()) {
      walker = walker->GetUnignoredChildAtIndex(
          walker->GetUnignoredChildCount() - 1);
    } else if (!GetRootOfChildTree(&walker, in_out_tree_wrapper)) {
      break;
    }
  }
  return walker;
}

float AutomationInternalCustomBindings::GetDeviceScaleFactor() const {
  return context()->GetRenderFrame()->GetDeviceScaleFactor();
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

void AutomationInternalCustomBindings::GetChildIDAtIndex(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 3 || !args[2]->IsNumber()) {
    ThrowInvalidArgumentsException(this);
    return;
  }

  ui::AXTreeID tree_id = ui::AXTreeID::FromString(
      *v8::String::Utf8Value(args.GetIsolate(), args[0]));
  int node_id = args[1]->Int32Value(context()->v8_context()).FromMaybe(0);

  const auto iter = tree_id_to_tree_wrapper_map_.find(tree_id);
  if (iter == tree_id_to_tree_wrapper_map_.end())
    return;

  AutomationAXTreeWrapper* tree_wrapper = iter->second.get();
  ui::AXNode* node = tree_wrapper->tree()->GetFromId(node_id);
  if (!node)
    return;

  int index = args[2]->Int32Value(context()->v8_context()).FromMaybe(0);
  int child_id;

  // Check for a child tree, which is guaranteed to always be the only child.
  if (index == 0 && GetRootOfChildTree(&node, &tree_wrapper))
    child_id = node->id();
  else if (index < 0 || size_t{index} >= node->GetUnignoredChildCount())
    return;
  else
    child_id = node->GetUnignoredChildAtIndex(size_t{index})->id();

  gin::DataObjectBuilder response(GetIsolate());
  response.Set("treeId", tree_wrapper->tree_id().ToString());
  response.Set("nodeId", child_id);
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
  AutomationAXTreeWrapper* tree_wrapper;
  auto iter = tree_id_to_tree_wrapper_map_.find(tree_id);
  if (iter == tree_id_to_tree_wrapper_map_.end()) {
    tree_wrapper = new AutomationAXTreeWrapper(tree_id, this);
    tree_id_to_tree_wrapper_map_.insert(
        std::make_pair(tree_id, base::WrapUnique(tree_wrapper)));
    axtree_to_tree_wrapper_map_.insert(
        std::make_pair(tree_wrapper->tree(), tree_wrapper));
  } else {
    tree_wrapper = iter->second.get();
  }

  if (!tree_wrapper->OnAccessibilityEvents(event_bundle, is_active_profile)) {
    LOG(ERROR) << tree_wrapper->tree()->error();
    base::ListValue args;
    args.AppendString(tree_id.ToString());
    bindings_system_->DispatchEventInContext(
        "automationInternal.onAccessibilityTreeSerializationError", &args,
        nullptr, context());
    return;
  }
}

void AutomationInternalCustomBindings::OnAccessibilityLocationChange(
    const ExtensionMsg_AccessibilityLocationChangeParams& params) {
  ui::AXTreeID tree_id = params.tree_id;
  auto iter = tree_id_to_tree_wrapper_map_.find(tree_id);
  if (iter == tree_id_to_tree_wrapper_map_.end())
    return;
  AutomationAXTreeWrapper* tree_wrapper = iter->second.get();
  ui::AXNode* node = tree_wrapper->tree()->GetFromId(params.id);
  if (!node)
    return;
  node->SetLocation(params.new_location.offset_container_id,
                    params.new_location.bounds,
                    params.new_location.transform.get());
}

bool AutomationInternalCustomBindings::SendTreeChangeEvent(
    api::automation::TreeChangeType change_type,
    ui::AXTree* tree,
    ui::AXNode* node) {
  // Don't send tree change events when it's not the active profile.
  if (!is_active_profile_)
    return false;

  // Notify custom bindings when there's an unloaded tree; js will enable the
  // renderer and wait for it to load.
  std::string child_tree_id_str;
  if (node->data().GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId,
                                      &child_tree_id_str)) {
    ui::AXTreeID child_tree_id = ui::AXTreeID::FromString(child_tree_id_str);
    auto* tree_wrapper = GetAutomationAXTreeWrapperFromTreeID(child_tree_id);
    if (!tree_wrapper || !tree_wrapper->tree()->data().loaded)
      SendChildTreeIDEvent(child_tree_id);
  }

  bool has_filter = false;
  if (tree_change_observer_overall_filter_ &
      (1
       << api::automation::TREE_CHANGE_OBSERVER_FILTER_LIVEREGIONTREECHANGES)) {
    if (node->data().HasStringAttribute(
            ax::mojom::StringAttribute::kContainerLiveStatus) ||
        node->data().role == ax::mojom::Role::kAlert ||
        change_type == api::automation::TREE_CHANGE_TYPE_SUBTREEUPDATEEND) {
      has_filter = true;
    }
  }

  if (tree_change_observer_overall_filter_ &
      (1 << api::automation::TREE_CHANGE_OBSERVER_FILTER_TEXTMARKERCHANGES)) {
    if (node->data().HasIntListAttribute(
            ax::mojom::IntListAttribute::kMarkerTypes))
      has_filter = true;
  }

  if (tree_change_observer_overall_filter_ &
      (1 << api::automation::TREE_CHANGE_OBSERVER_FILTER_ALLTREECHANGES))
    has_filter = true;

  if (!has_filter)
    return false;

  auto iter = axtree_to_tree_wrapper_map_.find(tree);
  if (iter == axtree_to_tree_wrapper_map_.end())
    return false;

  ui::AXTreeID tree_id = iter->second->tree_id();
  bool did_send_event = false;
  for (const auto& observer : tree_change_observers_) {
    switch (observer.filter) {
      case api::automation::TREE_CHANGE_OBSERVER_FILTER_NOTREECHANGES:
      default:
        continue;
      case api::automation::TREE_CHANGE_OBSERVER_FILTER_LIVEREGIONTREECHANGES:
        if (!node->data().HasStringAttribute(
                ax::mojom::StringAttribute::kContainerLiveStatus) &&
            node->data().role != ax::mojom::Role::kAlert &&
            change_type != api::automation::TREE_CHANGE_TYPE_SUBTREEUPDATEEND) {
          continue;
        }
        break;
      case api::automation::TREE_CHANGE_OBSERVER_FILTER_TEXTMARKERCHANGES:
        if (!node->data().HasIntListAttribute(
                ax::mojom::IntListAttribute::kMarkerTypes))
          continue;
        break;
      case api::automation::TREE_CHANGE_OBSERVER_FILTER_ALLTREECHANGES:
        break;
    }

    did_send_event = true;
    base::ListValue args;
    args.AppendInteger(observer.id);
    args.AppendString(tree_id.ToString());
    args.AppendInteger(node->id());
    args.AppendString(ToString(change_type));
    bindings_system_->DispatchEventInContext("automationInternal.onTreeChange",
                                             &args, nullptr, context());
  }

  return did_send_event;
}

void AutomationInternalCustomBindings::SendAutomationEvent(
    ui::AXTreeID tree_id,
    const gfx::Point& mouse_location,
    const ui::AXEvent& event,
    api::automation::EventType event_type) {
  auto event_params = std::make_unique<base::DictionaryValue>();
  event_params->SetString("treeID", tree_id.ToString());
  event_params->SetInteger("targetID", event.id);
  event_params->SetString("eventType", api::automation::ToString(event_type));
  event_params->SetString("eventFrom", ui::ToString(event.event_from));
  event_params->SetInteger("actionRequestID", event.action_request_id);
  event_params->SetInteger("mouseX", mouse_location.x());
  event_params->SetInteger("mouseY", mouse_location.y());
  base::ListValue args;
  args.Append(std::move(event_params));
  bindings_system_->DispatchEventInContext(
      "automationInternal.onAccessibilityEvent", &args, nullptr, context());
}

void AutomationInternalCustomBindings::MaybeSendFocusAndBlur(
    AutomationAXTreeWrapper* tree,
    const ExtensionMsg_AccessibilityEventBundleParams& event_bundle) {
  // Only send focus or blur if we got one of these events from the originating
  // renderer. While sending events purely based upon whether the targeted
  // focus/blur node changed may work, we end up firing too many events since
  // intermediate states trigger more events than likely necessary. Also, the
  // |event_from| field is only properly associated with focus/blur when the
  // event type is also focus/blur.
  base::Optional<ax::mojom::EventFrom> event_from;
  for (const auto& event : event_bundle.events) {
    if (event.event_type == ax::mojom::Event::kBlur ||
        event.event_type == ax::mojom::Event::kFocus)
      event_from = event.event_from;
  }

  if (!event_from) {
    // There was no explicit focus/blur; return early.
    // Make an exception for the desktop tree, where we can reliably infer
    // focus/blur even without an explicit event.
    if (!tree->IsDesktopTree())
      return;

    event_from = ax::mojom::EventFrom::kNone;
  }

  // Get the root-most tree.
  AutomationAXTreeWrapper* root_tree = tree;
  while (
      (tree = AutomationAXTreeWrapper::GetParentOfTreeId(root_tree->tree_id())))
    root_tree = tree;

  ui::AXNode* new_node = nullptr;
  AutomationAXTreeWrapper* new_wrapper = nullptr;
  if (!GetFocusInternal(root_tree, &new_wrapper, &new_node))
    return;

  ui::AXNode* old_node = nullptr;
  AutomationAXTreeWrapper* old_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(focus_tree_id_);
  if (old_wrapper)
    old_node = old_wrapper->tree()->GetFromId(focus_id_);

  if (new_wrapper == old_wrapper && new_node == old_node)
    return;

  // Blur previous focus.
  if (old_node) {
    ui::AXEvent blur_event;
    blur_event.id = old_node->id();
    blur_event.event_from = *event_from;
    SendAutomationEvent(old_wrapper->tree_id(), event_bundle.mouse_location,
                        blur_event, api::automation::EVENT_TYPE_BLUR);

    focus_id_ = -1;
    focus_tree_id_ = ui::AXTreeIDUnknown();
  }

  // New focus.
  if (new_node) {
    ui::AXEvent focus_event;
    focus_event.id = new_node->id();
    focus_event.event_from = *event_from;
    SendAutomationEvent(new_wrapper->tree_id(), event_bundle.mouse_location,
                        focus_event, api::automation::EVENT_TYPE_FOCUS);
    focus_id_ = new_node->id();
    focus_tree_id_ = new_wrapper->tree_id();
  }
}

void AutomationInternalCustomBindings::SendChildTreeIDEvent(
    ui::AXTreeID child_tree_id) {
  base::ListValue args;
  args.AppendString(child_tree_id.ToString());
  bindings_system_->DispatchEventInContext("automationInternal.onChildTreeID",
                                           &args, nullptr, context());
}

void AutomationInternalCustomBindings::SendNodesRemovedEvent(
    ui::AXTree* tree,
    const std::vector<int>& ids) {
  auto iter = axtree_to_tree_wrapper_map_.find(tree);
  if (iter == axtree_to_tree_wrapper_map_.end())
    return;

  ui::AXTreeID tree_id = iter->second->tree_id();

  base::ListValue args;
  args.AppendString(tree_id.ToString());
  {
    auto nodes = std::make_unique<base::ListValue>();
    for (auto id : ids)
      nodes->AppendInteger(id);
    args.Append(std::move(nodes));
  }

  bindings_system_->DispatchEventInContext("automationInternal.onNodesRemoved",
                                           &args, nullptr, context());
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

}  // namespace extensions
