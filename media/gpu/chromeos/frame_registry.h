// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_FRAME_REGISTRY_H_
#define MEDIA_GPU_CHROMEOS_FRAME_REGISTRY_H_

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/unguessable_token.h"
#include "media/gpu/chromeos/frame_resource.h"

namespace media {

// This class is used for storing and accessing a frame using a
// base::UnguessableToken as a key. An instance retains a reference to any
// frame that is currently registered and releases the frame when
// UnregisterFrame() is called, or when the FrameRegistry is destroyed.
//
// This class is reference counted because it needs to be used by the
// VideoDecoderPipeline to register output frames, by the
// StableVideoDecoderService to access them, and by the individual frames'
// release callbacks to unregister themselves. VideoDecoderPipeline is
// asynchronously destroyed. Frames may be unregistered after its destruction.
// Use of reference counting allows for safe unregistration of frames.
//
// All public methods of this class are thread-safe. A FrameRegistry can
// be constructed and destroyed on any sequence.
class FrameRegistry final : public base::RefCountedThreadSafe<FrameRegistry> {
 public:
  FrameRegistry();

  // FrameRegistry is not copyable or movable.
  FrameRegistry(const FrameRegistry&) = delete;
  FrameRegistry& operator=(const FrameRegistry&) = delete;

  // Registers |frame| in the registry, using |frame->tracking_token()| as key.
  // A reference to |frame| is taken and will be held until the frame is
  // unregistered or the registry is deleted.
  void RegisterFrame(scoped_refptr<const FrameResource> frame);

  // UnregisterFrame() removes a frame that is associated with |token| from
  // the registry.
  void UnregisterFrame(const base::UnguessableToken& token);

  // AccessFrame() can be called to access a frame in the registry. The method
  // crashes if |token| is not associated with a frame in the registry, so
  // this method always returns a non-null pointer.
  scoped_refptr<const FrameResource> AccessFrame(
      const base::UnguessableToken& token) const;

 private:
  friend class base::RefCountedThreadSafe<FrameRegistry>;
  ~FrameRegistry();

  // Each method that accesses |map_| acquires this lock. A lock is required
  // because frames are registered from a different thread than where they are
  // accessed.
  mutable base::Lock lock_;

  // Frame registry map, indexed by an unguessable token. A reference to the
  // frame is taken until the frame is unregistered with UnregisterFrame(). Use
  // of flat_map is acceptable, because the map should only contain a few
  // elements at a time.
  base::flat_map<base::UnguessableToken, scoped_refptr<const FrameResource>>
      map_ GUARDED_BY(lock_);
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_FRAME_REGISTRY_H_
