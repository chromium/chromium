// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_HOST_H_

#include <stdint.h>

#include <map>
#include <set>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/browser/android/java/gin_java_bound_object.h"
#include "content/browser/android/java/gin_java_method_invocation_helper.h"
#include "content/public/browser/web_contents_observer.h"

namespace base {
class ListValue;
}

namespace content {

// This class handles injecting Java objects into a single WebContents /
// WebView. The Java object itself lives in the browser process on a background
// thread, while multiple JavaScript wrapper objects (one per frame) are created
// on the renderer side.  The injected Java objects are identified by ObjectID,
// while wrappers are identified by a pair of (ObjectID, frame_routing_id).
class GinJavaBridgeDispatcherHost
    : public base::RefCountedThreadSafe<GinJavaBridgeDispatcherHost>,
      public WebContentsObserver,
      public GinJavaMethodInvocationHelper::DispatcherDelegate {
 public:
  GinJavaBridgeDispatcherHost(
      WebContents* web_contents,
      const base::android::JavaRef<jobject>& retained_object_set);

  void AddNamedObject(
      const std::string& name,
      const base::android::JavaRef<jobject>& object,
      const base::android::JavaRef<jclass>& safe_annotation_clazz);
  void RemoveNamedObject(const std::string& name);
  void SetAllowObjectContentsInspection(bool allow);

  // WebContentsObserver
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void DocumentAvailableInMainFrame() override;
  void WebContentsDestroyed() override;
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override;

  // GinJavaMethodInvocationHelper::DispatcherDelegate
  JavaObjectWeakGlobalRef GetObjectWeakRef(
      GinJavaBoundObject::ObjectID object_id) override;

  // Run on the background thread.
  void OnGetMethods(GinJavaBoundObject::ObjectID object_id,
                    std::set<std::string>* returned_method_names);
  void OnHasMethod(GinJavaBoundObject::ObjectID object_id,
                   const std::string& method_name,
                   bool* result);
  void OnInvokeMethod(int routing_id,
                      GinJavaBoundObject::ObjectID object_id,
                      const std::string& method_name,
                      const base::ListValue& arguments,
                      base::ListValue* result,
                      content::GinJavaBridgeError* error_code);
  void OnObjectWrapperDeleted(int routing_id,
                              GinJavaBoundObject::ObjectID object_id);

 private:
  friend class base::RefCountedThreadSafe<GinJavaBridgeDispatcherHost>;

  typedef std::map<GinJavaBoundObject::ObjectID,
                   scoped_refptr<GinJavaBoundObject>> ObjectMap;

  ~GinJavaBridgeDispatcherHost() override;

  // Run on the UI thread.
  void InstallFilterAndRegisterAllRoutingIds();

  // Run on any thread.
  GinJavaBoundObject::ObjectID AddObject(
      const base::android::JavaRef<jobject>& object,
      const base::android::JavaRef<jclass>& safe_annotation_clazz,
      bool is_named,
      int32_t holder);
  scoped_refptr<GinJavaBoundObject> FindObject(
      GinJavaBoundObject::ObjectID object_id);
  bool FindObjectId(const base::android::JavaRef<jobject>& object,
                    GinJavaBoundObject::ObjectID* object_id);
  void RemoveFromRetainedObjectSetLocked(const JavaObjectWeakGlobalRef& ref);
  JavaObjectWeakGlobalRef RemoveHolderLocked(int32_t holder,
                                             ObjectMap::iterator* iter_ptr)
      EXCLUSIVE_LOCKS_REQUIRED(objects_lock_);

  // The following objects are used only on the UI thread.

  typedef std::map<std::string, GinJavaBoundObject::ObjectID> NamedObjectMap;
  NamedObjectMap named_objects_;

  // The following objects are used on both threads, so locking must be used.

  GinJavaBoundObject::ObjectID next_object_id_;
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
  bool allow_object_contents_inspection_;

  DISALLOW_COPY_AND_ASSIGN(GinJavaBridgeDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_HOST_H_
