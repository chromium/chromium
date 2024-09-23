// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/java/gin_java_bridge_dispatcher.h"

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/content_features.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/java/gin_java_bridge_object.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

GinJavaBridgeDispatcher::GinJavaBridgeDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

GinJavaBridgeDispatcher::~GinJavaBridgeDispatcher() = default;

void GinJavaBridgeDispatcher::DidClearWindowObject() {
  // Accessing window object when adding properties to it may trigger
  // a nested call to DidClearWindowObject.
  if (inside_did_clear_window_object_)
    return;
  base::AutoReset<bool> flag_entry(&inside_did_clear_window_object_, true);
  if (!named_objects_.empty()) {
    // Ensure we have a `remote_` if we have named objects.
    CHECK(remote_);
  }
  for (NamedObjectMap::const_iterator iter = named_objects_.begin();
       iter != named_objects_.end(); ++iter) {
    // Always create a new GinJavaBridgeObject, so we don't pull any of the V8
    // wrapper's custom properties into the context of the page we have
    // navigated to. The old GinJavaBridgeObject will be automatically
    // deleted after its wrapper will be collected.
    // On the browser side, we ignore wrapper deletion events for named objects,
    // as they are only removed upon embedder's request (RemoveNamedObject).
    if (objects_.Lookup(iter->second))
      objects_.Remove(iter->second);
    GinJavaBridgeObject* object = GinJavaBridgeObject::InjectNamed(
        render_frame()->GetWebFrame(), weak_ptr_factory_.GetWeakPtr(),
        iter->first, iter->second);
    if (object) {
      objects_.AddWithID(object, iter->second);
    } else {
      GetRemoteObjectHost()->ObjectWrapperDeleted(iter->second);
    }
  }
}

void GinJavaBridgeDispatcher::AddNamedObject(const std::string& name,
                                             ObjectID object_id) {
  // We should already have received the `remote_` via the SetHost method.
  CHECK(remote_);
  // Added objects only become available after page reload, so here they
  // are only added into the internal map.
  named_objects_.insert(std::make_pair(name, object_id));
}

void GinJavaBridgeDispatcher::RemoveNamedObject(const std::string& name) {
  // Removal becomes in effect on next reload. We simply removing the entry
  // from the map here.
  DCHECK(base::Contains(named_objects_, name));
  named_objects_.erase(name);
}

void GinJavaBridgeDispatcher::SetHost(
    mojo::PendingRemote<mojom::GinJavaBridgeHost> host) {
  CHECK(!remote_);
  CHECK(named_objects_.empty());
  remote_.Bind(std::move(host));
}

GinJavaBridgeObject* GinJavaBridgeDispatcher::GetObject(ObjectID object_id) {
  GinJavaBridgeObject* result = objects_.Lookup(object_id);
  if (!result) {
    result = GinJavaBridgeObject::InjectAnonymous(
        render_frame()->GetWebFrame(), weak_ptr_factory_.GetWeakPtr(),
        object_id);
    if (result)
      objects_.AddWithID(result, object_id);
  }
  return result;
}

void GinJavaBridgeDispatcher::OnGinJavaBridgeObjectDeleted(
    GinJavaBridgeObject* object) {
  int object_id = object->object_id();
  // Ignore cleaning up of old object wrappers.
  if (objects_.Lookup(object_id) != object) return;
  objects_.Remove(object_id);

  GetRemoteObjectHost()->ObjectWrapperDeleted(object_id);
}

void GinJavaBridgeDispatcher::OnDestruct() {
  // This is a self owned receiver.
}

mojom::GinJavaBridgeHost* GinJavaBridgeDispatcher::GetRemoteObjectHost() {
  // Remote should always be sent because it is the first method sent to
  // this object.
  CHECK(remote_);
  return remote_.get();
}

}  // namespace content
