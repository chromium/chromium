// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_REMOTE_OBJECTS_REMOTE_OBJECT_GATEWAY_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_REMOTE_OBJECTS_REMOTE_OBJECT_GATEWAY_IMPL_H_

#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/remote_objects/remote_objects.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class LocalFrame;

class MODULES_EXPORT RemoteObjectGatewayImpl
    : public GarbageCollected<RemoteObjectGatewayImpl>,
      public Supplement<LocalFrame>,
      public mojom::blink::RemoteObjectGateway {
 public:
  static const char kSupplementName[];

  RemoteObjectGatewayImpl(
      base::PassKey<RemoteObjectGatewayImpl>,
      LocalFrame&,
      mojo::PendingReceiver<mojom::blink::RemoteObjectGateway>,
      mojo::PendingRemote<mojom::blink::RemoteObjectHost>);

  // Not copyable or movable
  RemoteObjectGatewayImpl(const RemoteObjectGatewayImpl&) = delete;
  RemoteObjectGatewayImpl& operator=(const RemoteObjectGatewayImpl&) = delete;
  ~RemoteObjectGatewayImpl() override = default;

  static void BindMojoReceiver(
      LocalFrame*,
      mojo::PendingRemote<mojom::blink::RemoteObjectHost>,
      mojo::PendingReceiver<mojom::blink::RemoteObjectGateway>);

  // This supplement is only installed if the RemoteObjectGateway mojom
  // interface is requested to be bound (currently only for Android WebView).
  static RemoteObjectGatewayImpl* From(LocalFrame&);

  void OnClearWindowObjectInMainWorld();

  void Trace(Visitor* visitor) const override;

  void BindRemoteObjectReceiver(
      int32_t object_id,
      mojo::PendingReceiver<mojom::blink::RemoteObject>);
  void ReleaseObject(int32_t object_id);

 private:
  // mojom::blink::RemoteObjectGateway
  void AddNamedObject(const WTF::String& name, int32_t id) override;
  void RemoveNamedObject(const WTF::String& name) override;

  void InjectNamed(const WTF::String& object_name, int32_t object_id);

  HashMap<String, int32_t> named_objects_;

  HeapMojoReceiver<mojom::blink::RemoteObjectGateway,
                   RemoteObjectGatewayImpl,
                   HeapMojoWrapperMode::kForceWithoutContextObserver>
      receiver_;
  HeapMojoRemote<mojom::blink::RemoteObjectHost,
                 HeapMojoWrapperMode::kForceWithoutContextObserver>
      object_host_;
};

class RemoteObjectGatewayFactoryImpl
    : public mojom::blink::RemoteObjectGatewayFactory {
 public:
  static void Create(
      LocalFrame* frame,
      mojo::PendingReceiver<mojom::blink::RemoteObjectGatewayFactory> receiver);

 private:
  explicit RemoteObjectGatewayFactoryImpl(LocalFrame& frame);
  // Not copyable or movable
  RemoteObjectGatewayFactoryImpl(const RemoteObjectGatewayFactoryImpl&) =
      delete;
  RemoteObjectGatewayFactoryImpl& operator=(
      const RemoteObjectGatewayFactoryImpl&) = delete;

  // mojom::blink::RemoteObjectGatewayFactory
  void CreateRemoteObjectGateway(
      mojo::PendingRemote<mojom::blink::RemoteObjectHost> host,
      mojo::PendingReceiver<mojom::blink::RemoteObjectGateway> receiver)
      override;

  WeakPersistent<LocalFrame> frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_REMOTE_OBJECTS_REMOTE_OBJECT_GATEWAY_IMPL_H_
