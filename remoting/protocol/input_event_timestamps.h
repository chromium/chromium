// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_INPUT_EVENT_TIMESTAMPS_H_
#define REMOTING_PROTOCOL_INPUT_EVENT_TIMESTAMPS_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace remoting::protocol {

// Used on the host side to track timestamps for input events.
struct InputEventTimestamps {
  // Client-side timestamps. This value comes from the client clock, so it
  // should not be used for any calculations on the host side (except in tests).
  base::TimeTicks client_timestamp;

  // Time when the event was processed by the host.
  base::TimeTicks host_timestamp;

  bool is_null() const { return client_timestamp.is_null(); }
};

// InputEventTimestampsSource is used by VideoStream implementations to get
// event timestamps that are sent back to the client as part of VideoStats
// message.
class InputEventTimestampsSource
    : public base::RefCountedThreadSafe<InputEventTimestampsSource> {
 public:
  InputEventTimestampsSource() = default;

  // Returns event timestamps for the input event that was received since the
  // previous call. Null InputEventTimestamps value is returned if no input
  // events were received. If multiple input events were received, then
  // timestamps for the last one should be returned
  virtual InputEventTimestamps TakeLastEventTimestamps() = 0;

 protected:
  friend base::RefCountedThreadSafe<InputEventTimestampsSource>;
  virtual ~InputEventTimestampsSource() = default;
};

// Simple implementations of InputEventTimestampsSource that just stores the
// value provided to OnEventReceived().
class InputEventTimestampsSourceImpl : public InputEventTimestampsSource {
 public:
  InputEventTimestampsSourceImpl();

  void OnEventReceived(InputEventTimestamps timestamps);

  // InputEventTimestampsSource implementation.
  InputEventTimestamps TakeLastEventTimestamps() override;

 protected:
  ~InputEventTimestampsSourceImpl() override;

 private:
  InputEventTimestamps last_timestamps_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_INPUT_EVENT_TIMESTAMPS_H_
