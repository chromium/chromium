// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_HOST_H_

#include <stdint.h>

#include <map>
#include <set>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "content/browser/android/java/gin_java_bound_object.h"
#include "content/browser/android/java/gin_java_method_invocation_helper.h"
#include "content/common/buildflags.h"
#include "content/common/gin_java_bridge.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {

class WebContentsImpl;

// This class handles injecting Java objects into a single WebContents /
// WebView. The Java object itself lives in the browser process on a background
// thread, while multiple JavaScript wrapper objects (one per frame) are created
// on the renderer side.  The injected Java objects are identified by ObjectID,
// while wrappers are identified by a pair of (ObjectID, frame_routing_id).
class GinJavaBridgeDispatcherHost
    : public base::RefCountedDeleteOnSequence<GinJavaBridgeDispatcherHost>,
      public WebContentsObserver,
      public mojom::GinJavaBridgeHost,
      public mojom::GinJavaBridgeRemoteObject,
      public GinJavaMethodInvocationHelper::DispatcherDelegate {
 public:
  GinJavaBridgeDispatcherHost(
      WebContents* web_contents,
      const base::android::JavaRef<jobject>& retained_object_set);

  GinJavaBridgeDispatcherHost(const GinJavaBridgeDispatcherHost&) = delete;
  GinJavaBridgeDispatcherHost& operator=(const GinJavaBridgeDispatcherHost&) =
      delete;

  void AddNamedObject(
      const std::string& name,
      const base::android::JavaRef<jobject>& object,
      const base::android::JavaRef<jclass>& safe_annotation_clazz);
  void RemoveNamedObject(const std::string& name);
  void SetAllowObjectContentsInspection(bool allow);

  // WebContentsObserver
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;
  void PrimaryMainDocumentElementAvailable() override;

  // GinJavaMethodInvocationHelper::DispatcherDelegate
  JavaObjectWeakGlobalRef GetObjectWeakRef(
      GinJavaBoundObject::ObjectID object_id) override;

  // Run on the background thread.
  void OnGetMethods(GinJavaBoundObject::ObjectID object_id,
                    std::vector<std::string>* returned_method_names);
  void OnHasMethod(GinJavaBoundObject::ObjectID object_id,
                   const std::string& method_name,
                   bool* result);
  void OnInvokeMethod(const GlobalRenderFrameHostId& routing_id,
                      GinJavaBoundObject::ObjectID object_id,
                      const std::string& method_name,
                      const base::Value::List& arguments,
                      base::Value::List* result,
                      mojom::GinJavaBridgeError* error_code);
  void OnObjectWrapperDeleted(const GlobalRenderFrameHostId& routing_id,
                              GinJavaBoundObject::ObjectID object_id);

  // mojom::GinJavaBridgeHost overrides:
  void GetObject(int32_t object_id,
                 mojo::PendingReceiver<mojom::GinJavaBridgeRemoteObject>
                     receiver) override;
  void ObjectWrapperDeleted(int32_t object_id) override;

  // mojom::GinJavaBridgeRemoteObject overrides:
  void GetMethods(GetMethodsCallback callback) override;
  void HasMethod(const std::string& method_name,
                 HasMethodCallback callback) override;
  void InvokeMethod(const std::string& method_name,
                    base::Value::List arguments,
                    InvokeMethodCallback callback) override;

 private:
  friend class base::RefCountedDeleteOnSequence<GinJavaBridgeDispatcherHost>;
  friend class base::DeleteHelper<GinJavaBridgeDispatcherHost>;

  typedef std::map<GinJavaBoundObject::ObjectID,
                   scoped_refptr<GinJavaBoundObject>> ObjectMap;

  ~GinJavaBridgeDispatcherHost() override;

  // Run on background thread.
  void BindNewHostOnBackgroundThread(
      GlobalRenderFrameHostId routing_id,
      mojo::PendingReceiver<mojom::GinJavaBridgeHost> host);
  void ClearAllReceivers();
  void ObjectDisconnected();

  // Run on the UI thread.
  mojom::GinJavaBridge* GetJavaBridge(RenderFrameHost* frame_host,
                                      bool should_create);

  // Run on the UI thread.
  WebContentsImpl* web_contents() const;
  void RemoteDisconnected(const content::GlobalRenderFrameHostId& routing_id);

  // Run on any thread.
  GinJavaBoundObject::ObjectID AddObject(
      const base::android::JavaRef<jobject>& object,
      const base::android::JavaRef<jclass>& safe_annotation_clazz,
      std::optional<GlobalRenderFrameHostId> holder);
  scoped_refptr<GinJavaBoundObject> FindObject(
      GinJavaBoundObject::ObjectID object_id);
  bool FindObjectId(const base::android::JavaRef<jobject>& object,
                    GinJavaBoundObject::ObjectID* object_id);
  void RemoveFromRetainedObjectSetLocked(const JavaObjectWeakGlobalRef& ref);
  JavaObjectWeakGlobalRef RemoveHolderLocked(
      const GlobalRenderFrameHostId& holder,
      ObjectMap::iterator* iter_ptr) EXCLUSIVE_LOCKS_REQUIRED(objects_lock_);
  void DeleteObjectForRouteLocked(const GlobalRenderFrameHostId& routing_id,
                                  GinJavaBoundObject::ObjectID object_id);

  // The following objects are used only on the UI thread.

  typedef std::map<std::string, GinJavaBoundObject::ObjectID> NamedObjectMap;
  NamedObjectMap named_objects_;

  // The following objects are used on both threads, so locking must be used.

  GinJavaBoundObject::ObjectID next_object_id_ = 1;
  // Every time a GinJavaBoundObject backed by a real Java object is
  // created/destroyed, we insert/remove a strong ref to that Java object into
  // this set so that it doesn't get garbage collected while it's still
  // potentially in use. Although the set is managed native side, it's owned
  // and defined in Java so that pushing refs into it does not create new GC
  // roots that would prevent WebContents from being garbage collected.
  JavaObjectWeakGlobalRef retained_object_set_;
  // Note that retained_object_set_ does not need to be consistent
  // with objects_.
  ObjectMap objects_ GUARDED_BY(objects_lock_);
  base::Lock objects_lock_;

  // The following objects are only used on the background thread.
  bool allow_object_contents_inspection_ = true;

  mojo::ReceiverSet<mojom::GinJavaBridgeHost, GlobalRenderFrameHostId>
      receivers_;
  mojo::ReceiverSet<
      mojom::GinJavaBridgeRemoteObject,
      std::pair<GlobalRenderFrameHostId, GinJavaBoundObject::ObjectID>>
      object_receivers_;
  std::map<GlobalRenderFrameHostId,
           mojo::AssociatedRemote<mojom::GinJavaBridge>>
      remotes_;

  const bool mojo_skip_clear_on_main_document_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_HOST_H_
