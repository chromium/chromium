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
#include "content/browser/android/java/gin_java_bridge_message_filter.h"
#include "content/browser/android/java/gin_java_bridge_object_deletion_message_filter.h"
#include "content/browser/android/java/java_bridge_thread.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/android/gin_java_bridge_value.h"
#include "content/common/android/hash_set.h"
#include "content/common/gin_java_bridge_messages.h"
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
    : WebContentsObserver(web_contents),
      next_object_id_(1),
      retained_object_set_(base::android::AttachCurrentThread(),
                           retained_object_set),
      allow_object_contents_inspection_(true) {
  DCHECK(!retained_object_set.is_null());
}

GinJavaBridgeDispatcherHost::~GinJavaBridgeDispatcherHost() {
}

// GinJavaBridgeDispatcherHost gets created earlier than RenderProcessHost
// is initialized. So we postpone installing the message filter until we know
// that the RPH is in a good shape. Also, message filter installation is
// postponed until the first named object is created.
void GinJavaBridgeDispatcherHost::InstallFilterAndRegisterAllRoutingIds() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (named_objects_.empty() ||
      !web_contents()->GetPrimaryMainFrame()->GetProcess()->GetChannel()) {
    return;
  }

  web_contents()
      ->GetPrimaryMainFrame()
      ->ForEachRenderFrameHost(
          [this](RenderFrameHostImpl* frame) {
            AgentSchedulingGroupHost& agent_scheduling_group =
                frame->GetAgentSchedulingGroup();

            scoped_refptr<GinJavaBridgeMessageFilter> per_asg_filter =
                GinJavaBridgeMessageFilter::FromHost(
                    agent_scheduling_group,
                    /*create_if_not_exists=*/true);
            if (base::FeatureList::IsEnabled(features::kMBIMode)) {
              scoped_refptr<GinJavaBridgeObjectDeletionMessageFilter>
                  process_global_filter =
                      GinJavaBridgeObjectDeletionMessageFilter::FromHost(
                          agent_scheduling_group.GetProcess(),
                          /*create_if_not_exists=*/true);
              process_global_filter->AddRoutingIdForHost(this, frame);
            }

            per_asg_filter->AddRoutingIdForHost(this, frame);
          });
}

WebContentsImpl* GinJavaBridgeDispatcherHost::web_contents() const {
  return static_cast<WebContentsImpl*>(WebContentsObserver::web_contents());
}

void GinJavaBridgeDispatcherHost::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AgentSchedulingGroupHost& agent_scheduling_group =
      static_cast<RenderFrameHostImpl*>(render_frame_host)
          ->GetAgentSchedulingGroup();
  if (scoped_refptr<GinJavaBridgeMessageFilter> filter =
          GinJavaBridgeMessageFilter::FromHost(
              agent_scheduling_group, /*create_if_not_exists=*/false)) {
    filter->AddRoutingIdForHost(this, render_frame_host);
  } else {
    InstallFilterAndRegisterAllRoutingIds();
  }
  for (NamedObjectMap::const_iterator iter = named_objects_.begin();
       iter != named_objects_.end();
       ++iter) {
    render_frame_host->Send(new GinJavaBridgeMsg_AddNamedObject(
        render_frame_host->GetRoutingID(), iter->first, iter->second));
  }
}

void GinJavaBridgeDispatcherHost::WebContentsDestroyed() {
  // Unretained() is safe because ForEachRenderFrameHost() is synchronous.
  web_contents()->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [this](RenderFrameHostImpl* frame) {
        AgentSchedulingGroupHost& agent_scheduling_group =
            frame->GetAgentSchedulingGroup();
        scoped_refptr<GinJavaBridgeMessageFilter> filter =
            GinJavaBridgeMessageFilter::FromHost(
                agent_scheduling_group, /*create_if_not_exists=*/false);

        if (filter)
          filter->RemoveHost(this);
      });
}

void GinJavaBridgeDispatcherHost::PrimaryPageChanged(Page& page) {
  AgentSchedulingGroupHost& agent_scheduling_group =
      static_cast<PageImpl&>(page).GetMainDocument().GetAgentSchedulingGroup();
  scoped_refptr<GinJavaBridgeMessageFilter> filter =
      GinJavaBridgeMessageFilter::FromHost(agent_scheduling_group,
                                           /*create_if_not_exists=*/false);
  if (!filter)
    InstallFilterAndRegisterAllRoutingIds();
}

GinJavaBoundObject::ObjectID GinJavaBridgeDispatcherHost::AddObject(
    const base::android::JavaRef<jobject>& object,
    const base::android::JavaRef<jclass>& safe_annotation_clazz,
    bool is_named,
    int32_t holder) {
  // Can be called on any thread. Calls come from the UI thread via
  // AddNamedObject, and from the background thread, when injected Java
  // object's method returns a Java object.
  DCHECK(is_named || holder);
  JNIEnv* env = base::android::AttachCurrentThread();
  JavaObjectWeakGlobalRef ref(env, object.obj());
  scoped_refptr<GinJavaBoundObject> new_object =
      is_named ? GinJavaBoundObject::CreateNamed(ref, safe_annotation_clazz)
               : GinJavaBoundObject::CreateTransient(ref, safe_annotation_clazz,
                                                     holder);
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

JavaObjectWeakGlobalRef
GinJavaBridgeDispatcherHost::RemoveHolderLocked(
    int32_t holder,
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
    object_id = AddObject(object, safe_annotation_clazz, true, 0);
  }
  named_objects_[name] = object_id;

  // As GinJavaBridgeDispatcherHost can be created later than WebContents has
  // notified the observers about new RenderFrame, it is necessary to ensure
  // here that all render frame IDs are registered with the filter.
  InstallFilterAndRegisterAllRoutingIds();
  // We should include pending RenderFrameHosts, otherwise they will miss the
  // chance when calling add or remove methods when they are created but not
  // committed. See: http://crbug.com/1087806
  web_contents()
      ->GetPrimaryMainFrame()
      ->ForEachRenderFrameHostIncludingSpeculative(
          [&name, object_id](RenderFrameHostImpl* render_frame_host) {
            render_frame_host->Send(new GinJavaBridgeMsg_AddNamedObject(
                render_frame_host->GetRoutingID(), name, object_id));
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

  // As the object isn't going to be removed from the JavaScript side until the
  // next page reload, calls to it must still work, thus we should continue to
  // hold it. All the transient objects and removed named objects will be purged
  // during the cleansing caused by PrimaryMainDocumentElementAvailable event.

  // We should include pending RenderFrameHosts, otherwise they will miss the
  // chance when calling add or remove methods when they are created but not
  // committed. See: http://crbug.com/1087806
  web_contents()
      ->GetPrimaryMainFrame()
      ->ForEachRenderFrameHostIncludingSpeculative(
          [&copied_name](RenderFrameHostImpl* render_frame_host) {
            render_frame_host->Send(new GinJavaBridgeMsg_RemoveNamedObject(
                render_frame_host->GetRoutingID(), copied_name));
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
      objects_.erase(iter++);
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
    std::set<std::string>* returned_method_names) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  if (!allow_object_contents_inspection_)
    return;
  scoped_refptr<GinJavaBoundObject> object = FindObject(object_id);
  if (object.get())
    *returned_method_names = object->GetMethodNames();
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
    int routing_id,
    GinJavaBoundObject::ObjectID object_id,
    const std::string& method_name,
    const base::Value::List& arguments,
    base::Value::List* wrapped_result,
    content::GinJavaBridgeError* error_code) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  DCHECK(routing_id != MSG_ROUTING_NONE);
  scoped_refptr<GinJavaBoundObject> object = FindObject(object_id);
  if (!object.get()) {
    wrapped_result->Append(base::Value());
    *error_code = kGinJavaBridgeUnknownObjectId;
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
                                     false,
                                     routing_id);
    }
    wrapped_result->Append(base::Value::FromUniquePtrValue(
        GinJavaBridgeValue::CreateObjectIDValue(returned_object_id)));
  } else {
    wrapped_result->Append(base::Value());
  }
}

void GinJavaBridgeDispatcherHost::OnObjectWrapperDeleted(
    int routing_id,
    GinJavaBoundObject::ObjectID object_id) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  DCHECK(routing_id != MSG_ROUTING_NONE);
  base::AutoLock locker(objects_lock_);
  auto iter = objects_.find(object_id);
  if (iter == objects_.end())
    return;
  JavaObjectWeakGlobalRef ref = RemoveHolderLocked(routing_id, &iter);
  if (!ref.is_uninitialized()) {
    RemoveFromRetainedObjectSetLocked(ref);
  }
}

}  // namespace content
