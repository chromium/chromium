// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_MAILBOX_FRAME_REGISTRY_H_
#define MEDIA_GPU_CHROMEOS_MAILBOX_FRAME_REGISTRY_H_

#include "base/containers/small_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/gpu/chromeos/frame_resource.h"

namespace media {

// This class is used for storing and accessing a frame using a gpu::Mailbox as
// a key. An instance retains a reference to any frame that is currently
// registered and releases the frame when UnregisterFrame() is called, or when
// the MailboxFrameRegistry is destroyed.
//
// This class is reference counted because it needs to be used by the
// VideoDecoderPipeline to register output frames, by the
// StableVideoDecoderService to access them, and by the individual frames'
// release callbacks to unregister themselves. VideoDecoderPipeline is
// asynchronously destroyed. Frames may be unregistered after its destruction.
// Use of reference counting allows for safe unregistration of frames.
//
// All public methods of this class are thread-safe. A MailboxFrameRegistry can
// be constructed and destroyed on any sequence.
class MailboxFrameRegistry final
    : public base::RefCountedThreadSafe<MailboxFrameRegistry> {
 public:
  MailboxFrameRegistry();

  // MailboxFrameRegistry is not copyable or movable.
  MailboxFrameRegistry(const MailboxFrameRegistry&) = delete;
  MailboxFrameRegistry& operator=(const MailboxFrameRegistry&) = delete;

  // Generates an unused gpu::Mailbox and associates it with |frame| in the
  // registry. A reference to |frame| is taken and will be held until the frame
  // is Unregistered or the registry is deleted. The returned gpu::Mailbox acts
  // as a token for accessing and unregistering the frame. This method never
  // fails and always returns a non-zero gpu::Mailbox.
  gpu::Mailbox RegisterFrame(scoped_refptr<const FrameResource> frame);

  // UnregisterFrame() removes a frame that is associated with |mailbox| from
  // the registry.
  void UnregisterFrame(const gpu::Mailbox& mailbox);

  // AccessFrame() can be called to access a frame in the registry. The method
  // crashes if |mailbox| is not associated with a frame in the registry, so
  // this method always returns a non-null pointer.
  scoped_refptr<const FrameResource> AccessFrame(
      const gpu::Mailbox& mailbox) const;

 private:
  friend class base::RefCountedThreadSafe<MailboxFrameRegistry>;
  ~MailboxFrameRegistry();

  // Each method that accesses |map_| acquires this lock. A lock is required
  // because frames are registered from a different thread than where they are
  // accessed.
  mutable base::Lock lock_;

  // Frame registry map, indexed by Mailbox. A reference to the frame is taken
  // until the frame is unregistered with UnregisterFrame().
  base::small_map<std::map<gpu::Mailbox, scoped_refptr<const FrameResource>>>
      map_ GUARDED_BY(lock_);

  // |mailbox_id_counter_| is used to name generated mailboxes. Using
  // Mailbox::Generate() creates a crytographically secure ID, but
  // MailboxFrameRegistry just uses the mailbox as an identifier. It is cheaper
  // to use a simple counter, especially since RegisteredMailboxFrameConverter
  // generates a gpu::Mailbox for each frame that is output.
  uint64_t mailbox_id_counter_ GUARDED_BY(lock_) = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_MAILBOX_FRAME_REGISTRY_H_
