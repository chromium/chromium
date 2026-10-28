// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/gin_java_bridge_dispatcher_host.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "components/origin_matcher/origin_matcher.h"
#include "content/browser/android/java/gin_java_bound_object_delegate.h"
#include "content/browser/android/java/java_bridge_thread.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/android/gin_java_bridge_value.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "third_party/jni_zero/jni_zero.h"

#if !BUILDFLAG(IS_ANDROID)
#error "JavaBridge only supports OS_ANDROID"
#endif

namespace content {

GinJavaBridgeDispatcherHost::GinJavaBridgeDispatcherHost(
    WebContents* web_contents,
    const base::android::JavaRef<jobject>& retained_object_set)
    : RefCountedDeleteOnSequence<GinJavaBridgeDispatcherHost>(
          base::SequencedTaskRunner::GetCurrentDefault()),
      WebContentsObserver(web_contents),
      retained_object_set_(base::android::AttachCurrentThread(),
                           retained_object_set),
      mojo_skip_clear_on_main_document_(
          base::FeatureList::IsEnabled(
              features::
                  kGinJavaBridgeMojoSkipClearObjectsOnMainDocumentReady)) {
  CHECK(!retained_object_set.is_null(), base::NotFatalUntil::M159);
}

GinJavaBridgeDispatcherHost::~GinJavaBridgeDispatcherHost() {
}

void GinJavaBridgeDispatcherHost::BindNewHostOnBackgroundThread(
    GlobalRenderFrameHostId routing_id,
    mojo::PendingReceiver<mojom::GinJavaBridgeHost> host) {
  CHECK(JavaBridgeThread::CurrentlyOn(), base::NotFatalUntil::M159);
  receivers_.Add(this, std::move(host), routing_id);
}

void GinJavaBridgeDispatcherHost::ClearAllReceivers() {
  CHECK(JavaBridgeThread::CurrentlyOn(), base::NotFatalUntil::M159);
  receivers_.set_disconnect_handler({});
  receivers_.Clear();
  object_receivers_.set_disconnect_handler({});
  object_receivers_.Clear();
}

mojom::GinJavaBridge* GinJavaBridgeDispatcherHost::GetJavaBridge(
    RenderFrameHost* frame_host,
    bool should_create) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  auto routing_id = frame_host->GetGlobalId();
  auto it = remotes_.find(routing_id);
  if (it == remotes_.end()) {
    if (!should_create) {
      return nullptr;
    }
    CHECK(frame_host->IsRenderFrameLive());
    auto& bound_remote = remotes_[routing_id];
    frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
        bound_remote.BindNewEndpointAndPassReceiver());
    bound_remote.set_disconnect_handler(
        base::BindOnce(&GinJavaBridgeDispatcherHost::RemoteDisconnected,
                       base::Unretained(this), routing_id));

    mojo::PendingReceiver<mojom::GinJavaBridgeHost> host_receiver;
    bound_remote->SetHost(host_receiver.InitWithNewPipeAndPassRemote());
    JavaBridgeThread::GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &GinJavaBridgeDispatcherHost::BindNewHostOnBackgroundThread, this,
            routing_id, std::move(host_receiver)));

    // Initialize with all the current named objects.
    for (auto& object : named_objects_) {
      bound_remote->AddNamedObject(object.first, object.second.object_id,
                                   object.second.matcher);
    }

    return bound_remote.get();
  }
  return it->second.get();
}

void GinJavaBridgeDispatcherHost::RemoteDisconnected(
    const content::GlobalRenderFrameHostId& routing_id) {
  remotes_.erase(routing_id);

  auto* frame_host = RenderFrameHost::FromID(routing_id);
  // If the RenderHost is still alive try to reconnect.
  if (frame_host->IsRenderFrameLive()) {
    LOG(ERROR) << "Reconnecting to RenderFrame";
    GetJavaBridge(frame_host, true);
  }
}

WebContentsImpl* GinJavaBridgeDispatcherHost::web_contents() const {
  return static_cast<WebContentsImpl*>(WebContentsObserver::web_contents());
}

void GinJavaBridgeDispatcherHost::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (named_objects_.empty()) {
    return;
  }

  GetJavaBridge(render_frame_host, /*should_create=*/true);
  // Named objects will be sent in GetJavaBridge when it is first connected.
}

void GinJavaBridgeDispatcherHost::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  remotes_.erase(render_frame_host->GetGlobalId());
}

void GinJavaBridgeDispatcherHost::WebContentsDestroyed() {
  JavaBridgeThread::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GinJavaBridgeDispatcherHost::ClearAllReceivers, this));
}

GinJavaBoundObject::ObjectID GinJavaBridgeDispatcherHost::FindOrAddObject(
    const base::android::JavaRef<jobject>& object,
    const base::android::JavaRef<jclass>& safe_annotation_clazz,
    std::optional<GlobalRenderFrameHostId> holder) {
  // Can be called on any thread. Calls come from the UI thread via
  // AddNamedObject, and from the background thread, when injected Java
  // object's method returns a Java object.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::AutoLock locker(objects_lock_);
  auto iter = FindObjectLocked(env, object);
  if (iter != objects_.end()) {
    if (!holder.has_value()) {
      iter->second->AddName();
    } else {
      iter->second->AddHolder(holder.value());
    }
    return iter->first;
  }

  scoped_refptr<GinJavaBoundObject> new_object =
      !holder.has_value()
          ? GinJavaBoundObject::CreateNamed(
                JavaObjectWeakGlobalRef(env, object), safe_annotation_clazz)
          : GinJavaBoundObject::CreateTransient(
                JavaObjectWeakGlobalRef(env, object), safe_annotation_clazz,
                holder.value());
  GinJavaBoundObject::ObjectID object_id = next_object_id_++;
  objects_.emplace(object_id, std::move(new_object));
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
      retained_object_set_.get(env);
  if (!retained_object_set.is_null()) {
    jni_zero::CollectionAdd(env, retained_object_set, object);
  }
  return object_id;
}

GinJavaBridgeDispatcherHost::ObjectMap::iterator
GinJavaBridgeDispatcherHost::FindObjectLocked(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& object) {
  objects_lock_.AssertAcquired();
  for (auto iter = objects_.begin(); iter != objects_.end(); ++iter) {
    if (object.IsSameObject(env, iter->second->GetLocalRef(env))) {
      return iter;
    }
  }
  return objects_.end();
}

JavaObjectWeakGlobalRef GinJavaBridgeDispatcherHost::GetObjectWeakRef(
    GinJavaBoundObject::ObjectID object_id) {
  scoped_refptr<GinJavaBoundObject> object = FindObject(object_id);
  if (object.get())
    return object->GetWeakRef();
  else
    return JavaObjectWeakGlobalRef();
}

void GinJavaBridgeDispatcherHost::RemoveFromRetainedObjectSetLocked(
    const JavaObjectWeakGlobalRef& ref) {
  objects_lock_.AssertAcquired();
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
      retained_object_set_.get(env);
  if (!retained_object_set.is_null()) {
    jni_zero::CollectionRemove(env, retained_object_set, ref.get(env));
  }
}

void GinJavaBridgeDispatcherHost::AddNamedObject(
    const std::string& name,
    const base::android::JavaRef<jobject>& object,
    const base::android::JavaRef<jclass>& safe_annotation_clazz,
    origin_matcher::OriginMatcher matcher) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  NamedObjectMap::iterator iter = named_objects_.find(name);
  if (iter != named_objects_.end()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::AutoLock locker(objects_lock_);
    auto obj_iter = objects_.find(iter->second.object_id);
    if (obj_iter != objects_.end() &&
        object.IsSameObject(env, obj_iter->second->GetLocalRef(env))) {
      // Nothing to do.
      return;
    }
  }

  if (iter != named_objects_.end()) {
    RemoveNamedObject(iter->first);
  }
  GinJavaBoundObject::ObjectID object_id =
      FindOrAddObject(object, safe_annotation_clazz, std::nullopt);

  // We use the serialized string of the matcher and reconstruct it
  // in the render process. We pass this around like this because we can
  // then trust that all the rules being fed to the render process are well
  // formed rules.
  named_objects_[name] = {object_id, matcher};

  web_contents()
      ->GetPrimaryMainFrame()
      ->ForEachRenderFrameHostImplIncludingSpeculative(
          [&name, object_id, &matcher,
           this](RenderFrameHostImpl* render_frame_host) {
            if (!render_frame_host->IsRenderFrameLive()) {
              return;
            }

            GetJavaBridge(render_frame_host, /*should_create=*/true)
                ->AddNamedObject(name, object_id, matcher);
          });
}

void GinJavaBridgeDispatcherHost::RemoveNamedObject(
    const std::string& name) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  NamedObjectMap::iterator iter = named_objects_.find(name);
  if (iter == named_objects_.end())
    return;

  // |name| may come from |named_objects_|. Make a copy of name so that if
  // |name| is from |named_objects_| it'll be valid after the remove below.
  const std::string copied_name(name);

  {
    base::AutoLock locker(objects_lock_);
    auto obj_iter = objects_.find(iter->second.object_id);
    if (obj_iter != objects_.end()) {
      obj_iter->second->RemoveName();
    }
  }
  named_objects_.erase(iter);

  web_contents()
      ->GetPrimaryMainFrame()
      ->ForEachRenderFrameHostImplIncludingSpeculative(
          [&copied_name, this](RenderFrameHostImpl* render_frame_host) {
            if (!render_frame_host->IsRenderFrameLive()) {
              return;
            }

            auto* bridge =
                GetJavaBridge(render_frame_host, /*should_create=*/false);
            if (!bridge) {
              return;
            }
            bridge->RemoveNamedObject(copied_name);
          });
}

void GinJavaBridgeDispatcherHost::SetAllowObjectContentsInspection(bool allow) {
  if (!JavaBridgeThread::CurrentlyOn()) {
    JavaBridgeThread::GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &GinJavaBridgeDispatcherHost::SetAllowObjectContentsInspection,
            this, allow));
    return;
  }
  allow_object_contents_inspection_ = allow;
}

void GinJavaBridgeDispatcherHost::PrimaryMainDocumentElementAvailable() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  // If we are skipping clearing on main document return early.
  if (mojo_skip_clear_on_main_document_) {
    return;
  }
  // Called when the window object has been cleared in the main frame.
  // That means, all sub-frames have also been cleared, so only named
  // objects survived.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::AutoLock locker(objects_lock_);
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
      retained_object_set_.get(env);
  if (!retained_object_set.is_null()) {
    jni_zero::CollectionClear(env, retained_object_set);
  }
  auto iter = objects_.begin();
  while (iter != objects_.end()) {
    if (iter->second->IsNamed()) {
      if (!retained_object_set.is_null()) {
        jni_zero::CollectionAdd(env, retained_object_set,
                                iter->second->GetLocalRef(env));
      }
      ++iter;
    } else {
      iter = objects_.erase(iter);
    }
  }
}

scoped_refptr<GinJavaBoundObject> GinJavaBridgeDispatcherHost::FindObject(
    GinJavaBoundObject::ObjectID object_id) {
  // Can be called on any thread.
  base::AutoLock locker(objects_lock_);
  return FindObjectLocked(object_id);
}

scoped_refptr<GinJavaBoundObject> GinJavaBridgeDispatcherHost::FindObjectLocked(
    GinJavaBoundObject::ObjectID object_id) {
  objects_lock_.AssertAcquired();
  auto iter = objects_.find(object_id);
  if (iter != objects_.end()) {
    return iter->second;
  }
  LOG(ERROR) << "WebView: Unknown object: " << object_id;
  return nullptr;
}

void GinJavaBridgeDispatcherHost::OnInvokeMethod(
    const GlobalRenderFrameHostId& routing_id,
    GinJavaBoundObject::ObjectID object_id,
    const std::string& method_name,
    const base::ListValue& arguments,
    base::ListValue* wrapped_result,
    content::mojom::GinJavaBridgeError* error_code) {
  CHECK(JavaBridgeThread::CurrentlyOn(), base::NotFatalUntil::M159);
  CHECK(routing_id, base::NotFatalUntil::M159);
  scoped_refptr<GinJavaBoundObject> object = FindObject(object_id);
  if (!object.get()) {
    wrapped_result->Append(base::Value());
    *error_code = mojom::GinJavaBridgeError::kGinJavaBridgeUnknownObjectId;
    return;
  }

  auto result = base::MakeRefCounted<GinJavaMethodInvocationHelper>(
      std::make_unique<GinJavaBoundObjectDelegate>(object), method_name,
      arguments);
  result->Init(this);
  result->Invoke();
  *error_code = result->GetInvocationError();
  if (result->HoldsPrimitiveResult()) {
    *wrapped_result = result->GetPrimitiveResult().Clone();
  } else if (!result->GetObjectResult().is_null()) {
    GinJavaBoundObject::ObjectID returned_object_id =
        FindOrAddObject(result->GetObjectResult(),
                        result->GetSafeAnnotationClass(), routing_id);
    wrapped_result->Append(base::Value::FromUniquePtrValue(
        GinJavaBridgeValue::CreateObjectIDValue(returned_object_id)));
  } else {
    wrapped_result->Append(base::Value());
  }
}

void GinJavaBridgeDispatcherHost::DeleteObjectForRouteLocked(
    const GlobalRenderFrameHostId& routing_id,
    GinJavaBoundObject::ObjectID object_id) {
  objects_lock_.AssertAcquired();
  auto iter = objects_.find(object_id);
  if (iter == objects_.end())
    return;
  scoped_refptr<GinJavaBoundObject> object = iter->second;
  if (!object->IsNamed()) {
    object->RemoveHolder(routing_id);
    if (!object->HasHolders()) {
      RemoveFromRetainedObjectSetLocked(object->GetWeakRef());
      objects_.erase(iter);
    }
  }
}

void GinJavaBridgeDispatcherHost::OnObjectWrapperDeleted(
    const GlobalRenderFrameHostId& routing_id,
    GinJavaBoundObject::ObjectID object_id) {
  CHECK(JavaBridgeThread::CurrentlyOn());
  CHECK(routing_id, base::NotFatalUntil::M159);
  base::AutoLock locker(objects_lock_);
  DeleteObjectForRouteLocked(routing_id, object_id);
}

void GinJavaBridgeDispatcherHost::GetObject(
    int32_t object_id,
    mojo::PendingReceiver<mojom::GinJavaBridgeRemoteObject> receiver) {
  CHECK(JavaBridgeThread::CurrentlyOn());
  if (object_receivers_.empty()) {
    object_receivers_.set_disconnect_handler(base::BindRepeating(
        &GinJavaBridgeDispatcherHost::ObjectDisconnected, this));
  }
  GlobalRenderFrameHostId routing_id = receivers_.current_context();
  object_receivers_.Add(this, std::move(receiver),
                        std::make_pair(routing_id, object_id));

  // Add a holder reference because the object may become unnamed
  // yet the renderer can still hold a reference to it. See
  // crbug.com/333171288.
  base::AutoLock locker(objects_lock_);
  scoped_refptr<GinJavaBoundObject> object = FindObjectLocked(object_id);
  if (object) {
    object->AddHolder(routing_id);
  }
}

void GinJavaBridgeDispatcherHost::ObjectWrapperDeleted(int32_t object_id) {
  CHECK(JavaBridgeThread::CurrentlyOn());
  GlobalRenderFrameHostId routing_id = receivers_.current_context();
  OnObjectWrapperDeleted(routing_id, object_id);
}

void GinJavaBridgeDispatcherHost::GetMethods(GetMethodsCallback callback) {
  CHECK(JavaBridgeThread::CurrentlyOn());
  if (!allow_object_contents_inspection_) {
    std::move(callback).Run({});
    return;
  }
  scoped_refptr<GinJavaBoundObject> object =
      FindObject(object_receivers_.current_context().second);
  if (object.get()) {
    std::set<std::string> result = object->GetMethodNames();
    std::move(callback).Run({result.begin(), result.end()});
  } else {
    std::move(callback).Run({});
  }
}

void GinJavaBridgeDispatcherHost::HasMethod(const std::string& method_name,
                                            HasMethodCallback callback) {
  CHECK(JavaBridgeThread::CurrentlyOn());
  scoped_refptr<GinJavaBoundObject> object =
      FindObject(object_receivers_.current_context().second);
  bool result = false;
  if (object.get()) {
    result = object->HasMethod(method_name);
  }
  std::move(callback).Run(result);
}

void GinJavaBridgeDispatcherHost::InvokeMethod(const std::string& method_name,
                                               base::ListValue arguments,
                                               InvokeMethodCallback callback) {
  CHECK(JavaBridgeThread::CurrentlyOn());
  base::ListValue wrapped_result;
  content::mojom::GinJavaBridgeError error_code;
  OnInvokeMethod(object_receivers_.current_context().first,
                 object_receivers_.current_context().second, method_name,
                 arguments, &wrapped_result, &error_code);
  std::move(callback).Run(error_code, std::move(wrapped_result));
}

void GinJavaBridgeDispatcherHost::ObjectDisconnected() {
  CHECK(JavaBridgeThread::CurrentlyOn());
  GlobalRenderFrameHostId routing_id =
      object_receivers_.current_context().first;
  OnObjectWrapperDeleted(routing_id,
                         object_receivers_.current_context().second);
}

}  // namespace content
