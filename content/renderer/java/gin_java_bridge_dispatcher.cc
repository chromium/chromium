// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/java/gin_java_bridge_dispatcher.h"

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/gin_java_bridge_messages.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/java/gin_java_bridge_object.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

GinJavaBridgeDispatcher::GinJavaBridgeDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      inside_did_clear_window_object_(false) {
}

GinJavaBridgeDispatcher::~GinJavaBridgeDispatcher() {
}

bool GinJavaBridgeDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GinJavaBridgeDispatcher, msg)
    IPC_MESSAGE_HANDLER(GinJavaBridgeMsg_AddNamedObject, OnAddNamedObject)
    IPC_MESSAGE_HANDLER(GinJavaBridgeMsg_RemoveNamedObject, OnRemoveNamedObject)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GinJavaBridgeDispatcher::DidClearWindowObject() {
  // Accessing window object when adding properties to it may trigger
  // a nested call to DidClearWindowObject.
  if (inside_did_clear_window_object_)
    return;
  base::AutoReset<bool> flag_entry(&inside_did_clear_window_object_, true);
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
        render_frame()->GetWebFrame(), AsWeakPtr(), iter->first, iter->second);
    if (object) {
      objects_.AddWithID(object, iter->second);
    } else {
      // Inform the host about wrapper creation failure.
      render_frame()->Send(new GinJavaBridgeHostMsg_ObjectWrapperDeleted(
          routing_id(), iter->second));
    }
  }
}

void GinJavaBridgeDispatcher::OnAddNamedObject(
    const std::string& name,
    ObjectID object_id) {
  // Added objects only become available after page reload, so here they
  // are only added into the internal map.
  named_objects_.insert(std::make_pair(name, object_id));
}

void GinJavaBridgeDispatcher::OnRemoveNamedObject(const std::string& name) {
  // Removal becomes in effect on next reload. We simply removing the entry
  // from the map here.
  DCHECK(base::Contains(named_objects_, name));
  named_objects_.erase(name);
}

void GinJavaBridgeDispatcher::GetJavaMethods(
    ObjectID object_id,
    std::set<std::string>* methods) {
  render_frame()->Send(new GinJavaBridgeHostMsg_GetMethods(
      routing_id(), object_id, methods));
}

bool GinJavaBridgeDispatcher::HasJavaMethod(ObjectID object_id,
                                            const std::string& method_name) {
  bool result;
  render_frame()->Send(new GinJavaBridgeHostMsg_HasMethod(
      routing_id(), object_id, method_name, &result));
  return result;
}

std::unique_ptr<base::Value> GinJavaBridgeDispatcher::InvokeJavaMethod(
    ObjectID object_id,
    const std::string& method_name,
    const base::Value::List& arguments,
    GinJavaBridgeError* error) {
  base::Value::List result_wrapper;
  render_frame()->Send(
      new GinJavaBridgeHostMsg_InvokeMethod(routing_id(),
                                            object_id,
                                            method_name,
                                            arguments,
                                            &result_wrapper,
                                            error));
  if (result_wrapper.empty())
    return nullptr;
  return base::Value::ToUniquePtrValue(result_wrapper[0].Clone());
}

GinJavaBridgeObject* GinJavaBridgeDispatcher::GetObject(ObjectID object_id) {
  GinJavaBridgeObject* result = objects_.Lookup(object_id);
  if (!result) {
    result = GinJavaBridgeObject::InjectAnonymous(AsWeakPtr(), object_id);
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
  render_frame()->Send(
      new GinJavaBridgeHostMsg_ObjectWrapperDeleted(routing_id(), object_id));
}

void GinJavaBridgeDispatcher::OnDestruct() {
  delete this;
}

}  // namespace content
