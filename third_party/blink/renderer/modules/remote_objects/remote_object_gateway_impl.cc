// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/remote_objects/remote_object_gateway_impl.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/remote_objects/remote_object.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"

#undef GetObject

namespace blink {

// static
const char RemoteObjectGatewayImpl::kSupplementName[] = "RemoteObjectGateway";

// static
RemoteObjectGatewayImpl* RemoteObjectGatewayImpl::From(LocalFrame& frame) {
  return Supplement<LocalFrame>::From<RemoteObjectGatewayImpl>(frame);
}

void RemoteObjectGatewayImpl::InjectNamed(const WTF::String& object_name,
                                          int32_t object_id) {
  ScriptState* script_state = ToScriptStateForMainWorld(GetSupplementable());
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Context> context = script_state->GetContext();
  if (context.IsEmpty())
    return;

  RemoteObject* object = new RemoteObject(isolate, this, object_id);

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> global = context->Global();
  gin::Handle<RemoteObject> controller = gin::CreateHandle(isolate, object);

  // WrappableBase instance deletes itself in case of a wrapper
  // creation failure, thus there is no need to delete |object|.
  if (controller.IsEmpty())
    return;

  global->Set(context, V8AtomicString(isolate, object_name), controller.ToV8())
      .Check();
  object_host_->AcquireObject(object_id);
}

// static
void RemoteObjectGatewayImpl::BindMojoReceiver(
    LocalFrame* frame,
    mojo::PendingRemote<mojom::blink::RemoteObjectHost> host,
    mojo::PendingReceiver<mojom::blink::RemoteObjectGateway> receiver) {
  if (!frame)
    return;

  DCHECK(!RemoteObjectGatewayImpl::From(*frame));

  auto* self = MakeGarbageCollected<RemoteObjectGatewayImpl>(
      base::PassKey<RemoteObjectGatewayImpl>(), *frame, std::move(receiver),
      std::move(host));
  Supplement<LocalFrame>::ProvideTo(*frame, self);
}

RemoteObjectGatewayImpl::RemoteObjectGatewayImpl(
    base::PassKey<RemoteObjectGatewayImpl>,
    LocalFrame& frame,
    mojo::PendingReceiver<mojom::blink::RemoteObjectGateway>
        object_gateway_receiver,
    mojo::PendingRemote<mojom::blink::RemoteObjectHost> object_host_remote)
    : Supplement<LocalFrame>(frame),
      receiver_(this, frame.DomWindow()),
      object_host_(frame.DomWindow()) {
  receiver_.Bind(std::move(object_gateway_receiver),
                 frame.GetTaskRunner(TaskType::kMiscPlatformAPI));
  object_host_.Bind(std::move(object_host_remote),
                    frame.GetTaskRunner(TaskType::kMiscPlatformAPI));
}

void RemoteObjectGatewayImpl::OnClearWindowObjectInMainWorld() {
  for (const auto& pair : named_objects_)
    InjectNamed(pair.key, pair.value);
}

void RemoteObjectGatewayImpl::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  visitor->Trace(object_host_);
  Supplement<LocalFrame>::Trace(visitor);
}

void RemoteObjectGatewayImpl::AddNamedObject(const WTF::String& name,
                                             int32_t id) {
  // Added objects only become available after page reload, so here they
  // are only added into the internal map.
  named_objects_.insert(name, id);
}

void RemoteObjectGatewayImpl::RemoveNamedObject(const WTF::String& name) {
  // Removal becomes in effect on next reload. We simply remove the entry
  // from the map here.
  auto iter = named_objects_.find(name);
  DCHECK(iter != named_objects_.end());
  named_objects_.erase(iter);
}

void RemoteObjectGatewayImpl::BindRemoteObjectReceiver(
    int32_t object_id,
    mojo::PendingReceiver<mojom::blink::RemoteObject> receiver) {
  object_host_->GetObject(object_id, std::move(receiver));
}

void RemoteObjectGatewayImpl::ReleaseObject(int32_t object_id) {
  object_host_->ReleaseObject(object_id);
}

// static
void RemoteObjectGatewayFactoryImpl::Create(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::RemoteObjectGatewayFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(std::unique_ptr<RemoteObjectGatewayFactoryImpl>(
                                  new RemoteObjectGatewayFactoryImpl(*frame)),
                              std::move(receiver));
}

RemoteObjectGatewayFactoryImpl::RemoteObjectGatewayFactoryImpl(
    LocalFrame& frame)
    : frame_(frame) {}

void RemoteObjectGatewayFactoryImpl::CreateRemoteObjectGateway(
    mojo::PendingRemote<mojom::blink::RemoteObjectHost> host,
    mojo::PendingReceiver<mojom::blink::RemoteObjectGateway> receiver) {
  RemoteObjectGatewayImpl::BindMojoReceiver(frame_, std::move(host),
                                            std::move(receiver));
}

}  // namespace blink
