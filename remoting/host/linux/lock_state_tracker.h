// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_LOCK_STATE_TRACKER_H_
#define REMOTING_HOST_LINUX_LOCK_STATE_TRACKER_H_

namespace remoting {

class LockStateTracker {
 public:
  virtual ~LockStateTracker() = default;

  virtual void Start() = 0;

  virtual bool GetCapsLockState() const = 0;
  virtual bool GetNumLockState() const = 0;

  // Setters for the lock states. These are used after modifying the lock state
  // to "preload" the state, since the underlying mechanism is typically
  // asynchronous.
  virtual void SetExpectedCapsLockState(bool state) = 0;
  virtual void SetExpectedNumLockState(bool state) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_LOCK_STATE_TRACKER_H_
