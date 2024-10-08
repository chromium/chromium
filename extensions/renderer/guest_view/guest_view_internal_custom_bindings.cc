// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/guest_view_internal_custom_bindings.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "components/guest_view/renderer/guest_view_container.h"
#include "components/guest_view/renderer/guest_view_request.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension.h"
#include "extensions/renderer/script_context.h"
#include "ipc/ipc_sync_channel.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/web/web_custom_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "v8/include/v8.h"

using content::V8ValueConverter;

namespace {

// A map from view instance ID to view object (stored via weak V8 reference).
// Views are registered into this map via
// GuestViewInternalCustomBindings::RegisterView(), and accessed via
// GuestViewInternalCustomBindings::GetViewFromID().
using ViewMap = std::map<int, v8::Global<v8::Object>*>;
static base::LazyInstance<ViewMap>::DestructorAtExit weak_view_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace extensions {

namespace {

content::RenderFrame* GetRenderFrame(v8::Isolate* isolate,
                                     v8::Local<v8::Value> value) {
  v8::Local<v8::Context> context;
  if (!v8::Local<v8::Object>::Cast(value)->GetCreationContext(isolate).ToLocal(
          &context)) {
    if (context.IsEmpty())
      return nullptr;
  }
  blink::WebLocalFrame* frame = blink::WebLocalFrame::FrameForContext(context);
  if (!frame)
    return nullptr;
  return content::RenderFrame::FromWebFrame(frame);
}

class RenderFrameStatus final : public content::RenderFrameObserver {
 public:
  explicit RenderFrameStatus(content::RenderFrame* render_frame)
      : content::RenderFrameObserver(render_frame) {}
  ~RenderFrameStatus() final = default;

  bool is_ok() { return render_frame() != nullptr; }

  // RenderFrameObserver implementation.
  void OnDestruct() final {}
};

}  // namespace

struct GuestViewInternalCustomBindings::ViewHolder {
  mojo::Remote<guest_view::mojom::ViewHandle> keep_alive_handle_remote;
  int view_id;
};

GuestViewInternalCustomBindings::GuestViewInternalCustomBindings(
    ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

GuestViewInternalCustomBindings::~GuestViewInternalCustomBindings() = default;

void GuestViewInternalCustomBindings::AddRoutes() {
  RouteHandlerFunction(
      "AttachIframeGuest",
      base::BindRepeating(&GuestViewInternalCustomBindings::AttachIframeGuest,
                          base::Unretained(this)));
  RouteHandlerFunction(
      "GetFrameToken",
      base::BindRepeating(&GuestViewInternalCustomBindings::GetFrameToken,
                          base::Unretained(this)));
  RouteHandlerFunction(
      "DestroyContainer",
      base::BindRepeating(&GuestViewInternalCustomBindings::DestroyContainer,
                          base::Unretained(this)));
  RouteHandlerFunction(
      "GetViewFromID",
      base::BindRepeating(&GuestViewInternalCustomBindings::GetViewFromID,
                          base::Unretained(this)));
  RouteHandlerFunction(
      "RegisterDestructionCallback",
      base::BindRepeating(
          &GuestViewInternalCustomBindings::RegisterDestructionCallback,
          base::Unretained(this)));
  RouteHandlerFunction(
      "RegisterView",
      base::BindRepeating(&GuestViewInternalCustomBindings::RegisterView,
                          base::Unretained(this)));
  RouteHandlerFunction(
      "RunWithGesture",
      base::BindRepeating(&GuestViewInternalCustomBindings::RunWithGesture,
                          base::Unretained(this)));
  RouteHandlerFunction(
      "AllowGuestViewElementDefinition",
      base::BindRepeating(
          &GuestViewInternalCustomBindings::AllowGuestViewElementDefinition,
          base::Unretained(this)));
}

// static
void GuestViewInternalCustomBindings::ResetMapEntry(
    const v8::WeakCallbackInfo<ViewHolder>& data) {
  ViewHolder* param = data.GetParameter();
  int view_instance_id = param->view_id;
  delete param;
  ViewMap& view_map = weak_view_map.Get();
  auto entry = view_map.find(view_instance_id);
  if (entry == view_map.end())
    return;

  // V8 says we need to explicitly reset weak handles from their callbacks.
  // It is not implicit as one might expect.
  entry->second->Reset();
  delete entry->second;
  view_map.erase(entry);
}

void GuestViewInternalCustomBindings::AttachIframeGuest(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Allow for an optional callback parameter.
  const int num_required_params = 4;
  CHECK(args.Length() >= num_required_params &&
        args.Length() <= (num_required_params + 1));
  // Element Instance ID.
  CHECK(args[0]->IsInt32());
  // Guest Instance ID.
  CHECK(args[1]->IsInt32());
  // Attach Parameters.
  CHECK(args[2]->IsObject());
  // <iframe>.contentWindow.
  CHECK(args[3]->IsObject());
  // Optional Callback Function.
  CHECK(args.Length() <= num_required_params ||
        args[num_required_params]->IsFunction());

  int element_instance_id = args[0].As<v8::Int32>()->Value();
  int guest_instance_id = args[1].As<v8::Int32>()->Value();

  // Get the WebLocalFrame before (possibly) executing any user-space JS while
  // getting the |params|. We track the status of the RenderFrame via an
  // observer in case it is deleted during user code execution.
  content::RenderFrame* render_frame =
      GetRenderFrame(args.GetIsolate(), args[3]);
  RenderFrameStatus render_frame_status(render_frame);

  std::unique_ptr<base::Value> params =
      content::V8ValueConverter::Create()->FromV8Value(args[2],
                                                       context()->v8_context());
  CHECK(params);
  CHECK(params->is_dict());

  if (!render_frame_status.is_ok())
    return;

  blink::WebLocalFrame* frame = render_frame->GetWebFrame();
  // Parent must exist.
  blink::WebFrame* parent_frame = frame->Parent();
  DCHECK(parent_frame);
  DCHECK(parent_frame->IsWebLocalFrame());

  // Add flag to |params| to indicate that the element size is specified in
  // logical units.
  params->GetDict().Set(guest_view::kElementSizeIsLogical, true);

  content::RenderFrame* embedder_parent_frame =
      content::RenderFrame::FromWebFrame(parent_frame->ToWebLocalFrame());

  // Create a GuestViewContainer if it does not exist.
  // An element instance ID uniquely identifies a GuestViewContainer
  // within a RenderView.
  auto* guest_view_container =
      guest_view::GuestViewContainer::FromID(element_instance_id);
  // This is the first time we hear about the |element_instance_id|.
  DCHECK(!guest_view_container);
  // The <webview> element's GC takes ownership of |guest_view_container|.
  guest_view_container = new guest_view::GuestViewContainer(
      embedder_parent_frame, element_instance_id);

  std::unique_ptr<guest_view::GuestViewAttachRequest> request =
      std::make_unique<guest_view::GuestViewAttachRequest>(
          guest_view_container, render_frame, guest_instance_id,
          std::move(*params).TakeDict(),
          args.Length() == (num_required_params + 1)
              ? args[num_required_params].As<v8::Function>()
              : v8::Local<v8::Function>(),
          args.GetIsolate());
  guest_view_container->IssueRequest(std::move(request));

  args.GetReturnValue().Set(true);
}

void GuestViewInternalCustomBindings::GetFrameToken(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(args.Length(), 1);
  if (!args[0]->IsObject()) {
    args.GetReturnValue().SetEmptyString();
    return;
  }

  content::RenderFrame* render_frame =
      GetRenderFrame(args.GetIsolate(), args[0]);
  if (!render_frame) {
    args.GetReturnValue().SetEmptyString();
    return;
  }
  auto frame_token = render_frame->GetWebFrame()->GetLocalFrameToken();
  std::string frame_token_string = frame_token.ToString();
  auto return_object = v8::String::NewFromUtf8(
      args.GetIsolate(), frame_token_string.data(), v8::NewStringType::kNormal,
      frame_token_string.size());
  args.GetReturnValue().Set(return_object.ToLocalChecked());
}

void GuestViewInternalCustomBindings::DestroyContainer(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().SetNull();

  if (args.Length() != 1)
    return;

  // Element Instance ID.
  if (!args[0]->IsInt32())
    return;

  int element_instance_id = args[0].As<v8::Int32>()->Value();
  auto* guest_view_container =
      guest_view::GuestViewContainer::FromID(element_instance_id);
  if (!guest_view_container)
    return;

  // Note: |guest_view_container| is deleted.
  // GuestViewContainer::DidDestroyElement() currently also destroys
  // a GuestViewContainer. That won't be necessary once GuestViewContainer
  // always runs w/o plugin.
  guest_view_container->Destroy(false /* embedder_frame_destroyed */);
}

void GuestViewInternalCustomBindings::GetViewFromID(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Default to returning null.
  args.GetReturnValue().SetNull();
  // There is one argument.
  CHECK(args.Length() == 1);
  // The view ID.
  CHECK(args[0]->IsInt32());
  int view_id = args[0].As<v8::Int32>()->Value();

  ViewMap& view_map = weak_view_map.Get();
  auto map_entry = view_map.find(view_id);
  if (map_entry == view_map.end())
    return;

  auto return_object =
      v8::Local<v8::Object>::New(args.GetIsolate(), *map_entry->second);
  args.GetReturnValue().Set(return_object);
}

void GuestViewInternalCustomBindings::RegisterDestructionCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // There are two parameters.
  CHECK(args.Length() == 2);
  // Element Instance ID.
  CHECK(args[0]->IsInt32());
  // Callback function.
  CHECK(args[1]->IsFunction());

  int element_instance_id = args[0].As<v8::Int32>()->Value();
  // An element instance ID uniquely identifies a GuestViewContainer within a
  // RenderView.
  auto* guest_view_container =
      guest_view::GuestViewContainer::FromID(element_instance_id);
  if (!guest_view_container)
    return;

  guest_view_container->RegisterDestructionCallback(args[1].As<v8::Function>(),
                                                    args.GetIsolate());

  args.GetReturnValue().Set(true);
}

void GuestViewInternalCustomBindings::RegisterView(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // There are three parameters.
  CHECK(args.Length() == 3);
  // View Instance ID.
  CHECK(args[0]->IsInt32());
  // View element.
  CHECK(args[1]->IsObject());
  // View type (e.g. "webview").
  CHECK(args[2]->IsString());

  // A reference to the view object is stored in |weak_view_map| using its view
  // ID as the key. The reference is made weak so that it will not extend the
  // lifetime of the object.
  int view_instance_id = args[0].As<v8::Int32>()->Value();
  auto* object =
      new v8::Global<v8::Object>(args.GetIsolate(), args[1].As<v8::Object>());
  weak_view_map.Get().insert(std::make_pair(view_instance_id, object));

  ViewHolder* view_holder = new ViewHolder();
  view_holder->view_id = view_instance_id;
  auto receiver =
      view_holder->keep_alive_handle_remote.BindNewPipeAndPassReceiver();

  // The `view_holder` is given to the SetWeak callback so that that view's
  // entry in `weak_view_map` can be cleared when the view object is garbage
  // collected. This will then also close the `keep_alive_handle_remote`
  // indicating to the browser the object has been collected.
  object->SetWeak(view_holder, &GuestViewInternalCustomBindings::ResetMapEntry,
                  v8::WeakCallbackType::kParameter);

  // Let the GuestViewManager know that a GuestView has been created.
  const std::string& view_type =
      *v8::String::Utf8Value(args.GetIsolate(), args[2]);
  GetGuestViewHost()->ViewCreated(view_instance_id, view_type,
                                  std::move(receiver));
}

void GuestViewInternalCustomBindings::RunWithGesture(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Gesture is required to request fullscreen.
  // TODO(devlin): All this needs to do is enter fullscreen. We should make this
  // EnterFullscreen() and do it directly rather than having a generic "run with
  // user gesture" function.
  if (context()->web_frame()) {
    context()->web_frame()->NotifyUserActivation(
        blink::mojom::UserActivationNotificationType::kExtensionGuestView);
  }
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsFunction());
  context()->SafeCallFunction(
      v8::Local<v8::Function>::Cast(args[0]), 0, nullptr);
}

void GuestViewInternalCustomBindings::AllowGuestViewElementDefinition(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  blink::WebCustomElement::EmbedderNamesAllowedScope embedder_names_scope;
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsFunction());
  context()->SafeCallFunction(v8::Local<v8::Function>::Cast(args[0]), 0,
                              nullptr);
}

guest_view::mojom::GuestViewHost*
GuestViewInternalCustomBindings::GetGuestViewHost() {
  if (!remote_.is_bound()) {
    content::RenderFrame* render_frame = context()->GetRenderFrame();
    CHECK(render_frame);
    render_frame->GetRemoteAssociatedInterfaces()->GetInterface(&remote_);
  }
  return remote_.get();
}

}  // namespace extensions
