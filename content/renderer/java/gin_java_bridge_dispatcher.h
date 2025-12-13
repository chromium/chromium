// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_H_
#define CONTENT_RENDERER_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/origin_matcher/origin_matcher.h"
#include "content/common/android/gin_java_bridge_errors.h"
#include "content/common/gin_java_bridge.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "v8/include/cppgc/persistent.h"

namespace content {

class GinJavaBridgeObject;

struct NamedObject {
  using ObjectID = int32_t;

  ObjectID object_id;
  origin_matcher::OriginMatcher matcher;
};

// This class handles injecting Java objects into the main frame of a
// RenderView. The 'add' and 'remove' messages received from the browser
// process modify the entries in a map of 'pending' objects. These objects are
// bound to the window object of the main frame when that window object is next
// cleared. These objects remain bound until the window object is cleared
// again.
class GinJavaBridgeDispatcher final : public mojom::GinJavaBridge,
                                      public RenderFrameObserver {
 public:
  // GinJavaBridgeObjects are managed by gin. An object gets destroyed
  // when it is no more referenced from JS. As GinJavaBridgeObject reports
  // deletion of self to GinJavaBridgeDispatcher, we would not have stale
  // pointers here.
  using ObjectID = int32_t;
  using ObjectMap =
      base::flat_map<ObjectID, cppgc::WeakPersistent<GinJavaBridgeObject>>;

  explicit GinJavaBridgeDispatcher(RenderFrame* render_frame);

  GinJavaBridgeDispatcher(const GinJavaBridgeDispatcher&) = delete;
  GinJavaBridgeDispatcher& operator=(const GinJavaBridgeDispatcher&) = delete;

  ~GinJavaBridgeDispatcher() override;

  // RenderFrameObserver override:
  void DidClearWindowObject() override;

  GinJavaBridgeObject* GetObject(ObjectID object_id);
  void OnGinJavaBridgeObjectDeleted(GinJavaBridgeObject* object);

  void AddNamedObject(const std::string& name,
                      ObjectID object_id,
                      const origin_matcher::OriginMatcher& matcher) override;
  void RemoveNamedObject(const std::string& name) override;
  void SetHost(mojo::PendingRemote<mojom::GinJavaBridgeHost> host) override;

  mojom::GinJavaBridgeHost* GetRemoteObjectHost();

 private:
  // RenderFrameObserver implementation.
  void OnDestruct() override;

  typedef std::map<std::string, NamedObject> NamedObjectMap;
  NamedObjectMap named_objects_;
  ObjectMap objects_;
  bool inside_did_clear_window_object_ = false;

  mojo::Remote<mojom::GinJavaBridgeHost> remote_;

  base::WeakPtrFactory<GinJavaBridgeDispatcher> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_H_
