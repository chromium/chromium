// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper class used to find the RenderFrameObservers for a given RenderFrame.
//
// Example usage:
//
//   class MyRFO : public RenderFrameObserver,
//                 public RenderFrameObserverTracker<MyRFO> {
//     ...
//   };
//
//   MyRFO::MyRFO(RenderFrame* render_frame)
//       : RenderFrameObserver(render_frame),
//         RenderFrameObserverTracker<MyRFO>(render_frame) {
//     ...
//   }
//
//  void SomeFunction(RenderFrame* rf) {
//    MyRFO* my_rfo = new MyRFO(rf);
//    MyRFO* my_rfo_tracked = MyRFO::Get(rf);
//    // my_rfo == my_rfo_tracked
//  }

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_TRACKER_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_TRACKER_H_

#include <map>

#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"

namespace content {

class RenderFrame;

template <class T>
class RenderFrameObserverTracker {
 public:
  static T* Get(const RenderFrame* render_frame) {
    return static_cast<T*>(render_frame_map_.Get()[render_frame]);
  }

  explicit RenderFrameObserverTracker(const RenderFrame* render_frame)
      : render_frame_(render_frame) {
    render_frame_map_.Get()[render_frame] = this;
  }

  RenderFrameObserverTracker(const RenderFrameObserverTracker<T>&) = delete;
  RenderFrameObserverTracker<T>& operator=(
      const RenderFrameObserverTracker<T>&) = delete;

  ~RenderFrameObserverTracker() {
    render_frame_map_.Get().erase(render_frame_);
  }

 private:
  raw_ptr<const RenderFrame, DanglingUntriaged> render_frame_;

  static typename base::LazyInstance<
      std::map<const RenderFrame*, RenderFrameObserverTracker<T>*>>::
      DestructorAtExit render_frame_map_;
};

template <class T>
typename base::LazyInstance<
    std::map<const RenderFrame*, RenderFrameObserverTracker<T>*>>::
    DestructorAtExit RenderFrameObserverTracker<T>::render_frame_map_ =
        LAZY_INSTANCE_INITIALIZER;

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_TRACKER_H_
