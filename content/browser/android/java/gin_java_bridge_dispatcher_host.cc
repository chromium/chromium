// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/gin_java_bridge_dispatcher_host.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "content/browser/android/java/gin_java_bound_object_delegate.h"
#include "content/browser/android/java/java_bridge_thread.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/android/gin_java_bridge_value.h"
#include "content/common/android/hash_set.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"

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
  DCHECK(!retained_object_set.is_null());
}

GinJavaBridgeDispatcherHost::~GinJavaBridgeDispatcherHost() {
}

void GinJavaBridgeDispatcherHost::BindNewHostOnBackgroundThread(
    GlobalRenderFrameHostId routing_id,
    mojo::PendingReceiver<mojom::GinJavaBridgeHost> host) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  receivers_.Add(this, std::move(host), routing_id);
}

void GinJavaBridgeDispatcherHost::ClearAllReceivers() {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  receivers_.set_disconnect_handler({});
  receivers_.Clear();
  object_receivers_.set_disconnect_handler({});
  object_receivers_.Clear();
}

mojom::GinJavaBridge* GinJavaBridgeDispatcherHost::GetJavaBridge(
    RenderFrameHost* frame_host,
    bool should_create) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
      bound_remote->AddNamedObject(object.first, object.second);
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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

GinJavaBoundObject::ObjectID GinJavaBridgeDispatcherHost::AddObject(
    const base::android::JavaRef<jobject>& object,
    const base::android::JavaRef<jclass>& safe_annotation_clazz,
    std::optional<GlobalRenderFrameHostId> holder) {
  // Can be called on any thread. Calls come from the UI thread via
  // AddNamedObject, and from the background thread, when injected Java
  // object's method returns a Java object.
  JNIEnv* env = base::android::AttachCurrentThread();
  JavaObjectWeakGlobalRef ref(env, object);
  scoped_refptr<GinJavaBoundObject> new_object =
      !holder ? GinJavaBoundObject::CreateNamed(ref, safe_annotation_clazz)
              : GinJavaBoundObject::CreateTransient(ref, safe_annotation_clazz,
                                                    holder.value());
  GinJavaBoundObject::ObjectID object_id;
  {
    base::AutoLock locker(objects_lock_);
    object_id = next_object_id_++;
    objects_[object_id] = new_object;
  }
#if DCHECK_IS_ON()
  {
    GinJavaBoundObject::ObjectID added_object_id;
    DCHECK(FindObjectId(object, &added_object_id));
    DCHECK_EQ(object_id, added_object_id);
  }
#endif  // DCHECK_IS_ON()
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
        retained_object_set_.get(env);
  if (!retained_object_set.is_null()) {
    base::AutoLock locker(objects_lock_);
    JNI_Java_HashSet_add(env, retained_object_set, object);
  }
  return object_id;
}

bool GinJavaBridgeDispatcherHost::FindObjectId(
    const base::android::JavaRef<jobject>& object,
    GinJavaBoundObject::ObjectID* object_id) {
  // Can be called on any thread.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::AutoLock locker(objects_lock_);
  for (const auto& pair : objects_) {
    if (env->IsSameObject(
            object.obj(),
            pair.second->GetLocalRef(env).obj())) {
      *object_id = pair.first;
      return true;
    }
  }
  return false;
}

JavaObjectWeakGlobalRef GinJavaBridgeDispatcherHost::GetObjectWeakRef(
    GinJavaBoundObject::ObjectID object_id) {
  scoped_refptr<GinJavaBoundObject> object = FindObject(object_id);
  if (object.get())
    return object->GetWeakRef();
  else
    return JavaObjectWeakGlobalRef();
}

JavaObjectWeakGlobalRef GinJavaBridgeDispatcherHost::RemoveHolderLocked(
    const GlobalRenderFrameHostId& holder,
    ObjectMap::iterator* iter_ptr) {
  objects_lock_.AssertAcquired();
  JavaObjectWeakGlobalRef result;
  scoped_refptr<GinJavaBoundObject> object((*iter_ptr)->second);
  if (!object->IsNamed()) {
    object->RemoveHolder(holder);
    if (!object->HasHolders()) {
      result = object->GetWeakRef();
      objects_.erase(*iter_ptr);
    }
  }
  return result;
}

void GinJavaBridgeDispatcherHost::RemoveFromRetainedObjectSetLocked(
    const JavaObjectWeakGlobalRef& ref) {
  objects_lock_.AssertAcquired();
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
      retained_object_set_.get(env);
  if (!retained_object_set.is_null()) {
    JNI_Java_HashSet_remove(env, retained_object_set, ref.get(env));
  }
}

void GinJavaBridgeDispatcherHost::AddNamedObject(
    const std::string& name,
    const base::android::JavaRef<jobject>& object,
    const base::android::JavaRef<jclass>& safe_annotation_clazz) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GinJavaBoundObject::ObjectID object_id;
  NamedObjectMap::iterator iter = named_objects_.find(name);
  bool existing_object = FindObjectId(object, &object_id);
  if (existing_object && iter != named_objects_.end() &&
      iter->second == object_id) {
    // Nothing to do.
    return;
  }
  if (iter != named_objects_.end()) {
    RemoveNamedObject(iter->first);
  }
  if (existing_object) {
    base::AutoLock locker(objects_lock_);
    objects_[object_id]->AddName();
  } else {
    object_id = AddObject(object, safe_annotation_clazz, std::nullopt);
  }
  named_objects_[name] = object_id;

  web_contents()
      ->GetPrimaryMainFrame()
      ->ForEachRenderFrameHostIncludingSpeculative(
          [&name, object_id, this](RenderFrameHostImpl* render_frame_host) {
            if (!render_frame_host->IsRenderFrameLive()) {
              return;
            }
            GetJavaBridge(render_frame_host, /*should_create=*/true)
                ->AddNamedObject(name, object_id);
          });
}

void GinJavaBridgeDispatcherHost::RemoveNamedObject(
    const std::string& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NamedObjectMap::iterator iter = named_objects_.find(name);
  if (iter == named_objects_.end())
    return;

  // |name| may come from |named_objects_|. Make a copy of name so that if
  // |name| is from |named_objects_| it'll be valid after the remove below.
  const std::string copied_name(name);

  {
    base::AutoLock locker(objects_lock_);
    objects_[iter->second]->RemoveName();
  }
  named_objects_.erase(iter);

  web_contents()
      ->GetPrimaryMainFrame()
      ->ForEachRenderFrameHostIncludingSpeculative(
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If we are skipping clearing on main document return early.
  if (mojo_skip_clear_on_main_document_) {
    return;
  }
  // Called when the window object has been cleared in the main frame.
  // That means, all sub-frames have also been cleared, so only named
  // objects survived.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
      retained_object_set_.get(env);
  base::AutoLock locker(objects_lock_);
  if (!retained_object_set.is_null()) {
    JNI_Java_HashSet_clear(env, retained_object_set);
  }
  auto iter = objects_.begin();
  while (iter != objects_.end()) {
    if (iter->second->IsNamed()) {
      if (!retained_object_set.is_null()) {
        JNI_Java_HashSet_add(
            env, retained_object_set, iter->second->GetLocalRef(env));
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
  auto iter = objects_.find(object_id);
  if (iter != objects_.end())
    return iter->second;
  LOG(ERROR) << "WebView: Unknown object: " << object_id;
  return nullptr;
}

void GinJavaBridgeDispatcherHost::OnGetMethods(
    GinJavaBoundObject::ObjectID object_id,
    std::vector<std::string>* returned_method_names) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  if (!allow_object_contents_inspection_)
    return;
  scoped_refptr<GinJavaBoundObject> object = FindObject(object_id);
  if (object.get()) {
    std::set<std::string> result = object->GetMethodNames();
    *returned_method_names = {result.begin(), result.end()};
  }
}

void GinJavaBridgeDispatcherHost::OnHasMethod(
    GinJavaBoundObject::ObjectID object_id,
    const std::string& method_name,
    bool* result) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  scoped_refptr<GinJavaBoundObject> object = FindObject(object_id);
  if (object.get())
    *result = object->HasMethod(method_name);
}

void GinJavaBridgeDispatcherHost::OnInvokeMethod(
    const GlobalRenderFrameHostId& routing_id,
    GinJavaBoundObject::ObjectID object_id,
    const std::string& method_name,
    const base::Value::List& arguments,
    base::Value::List* wrapped_result,
    content::mojom::GinJavaBridgeError* error_code) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  DCHECK(routing_id);
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
    GinJavaBoundObject::ObjectID returned_object_id;
    if (FindObjectId(result->GetObjectResult(), &returned_object_id)) {
      base::AutoLock locker(objects_lock_);
      objects_[returned_object_id]->AddHolder(routing_id);
    } else {
      returned_object_id = AddObject(result->GetObjectResult(),
                                     result->GetSafeAnnotationClass(),
                                     routing_id);
    }
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
  JavaObjectWeakGlobalRef ref = RemoveHolderLocked(routing_id, &iter);
  if (!ref.is_uninitialized()) {
    RemoveFromRetainedObjectSetLocked(ref);
  }
}

void GinJavaBridgeDispatcherHost::OnObjectWrapperDeleted(
    const GlobalRenderFrameHostId& routing_id,
    GinJavaBoundObject::ObjectID object_id) {
  CHECK(JavaBridgeThread::CurrentlyOn());
  DCHECK(routing_id);
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
  object_receivers_.Add(
      this, std::move(receiver),
      std::make_pair(receivers_.current_context(), object_id));

  // Add a holder reference because the object may become unnamed
  // yet the renderer can still hold a reference to it. See
  // crbug.com/333171288.
  scoped_refptr<GinJavaBoundObject> object = FindObject(object_id);
  if (object.get()) {
    object->AddHolder(receivers_.current_context());
  }
}

void GinJavaBridgeDispatcherHost::ObjectWrapperDeleted(int32_t object_id) {
  CHECK(JavaBridgeThread::CurrentlyOn());
  base::AutoLock locker(objects_lock_);
  DeleteObjectForRouteLocked(receivers_.current_context(), object_id);
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
                                               base::Value::List arguments,
                                               InvokeMethodCallback callback) {
  CHECK(JavaBridgeThread::CurrentlyOn());
  base::Value::List wrapped_result;
  content::mojom::GinJavaBridgeError error_code;
  OnInvokeMethod(object_receivers_.current_context().first,
                 object_receivers_.current_context().second, method_name,
                 arguments, &wrapped_result, &error_code);
  std::move(callback).Run(error_code, std::move(wrapped_result));
}

void GinJavaBridgeDispatcherHost::ObjectDisconnected() {
  CHECK(JavaBridgeThread::CurrentlyOn());
  OnObjectWrapperDeleted(object_receivers_.current_context().first,
                         object_receivers_.current_context().second);
}

}  // namespace content
