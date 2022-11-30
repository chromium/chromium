// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_HOST_EVENT_DISPATCHER_H_
#define REMOTING_PROTOCOL_HOST_EVENT_DISPATCHER_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/input_event_timestamps.h"

namespace remoting::protocol {

class InputStub;

// HostEventDispatcher dispatches incoming messages on the event channel to
// InputStub.
class HostEventDispatcher : public ChannelDispatcherBase {
 public:
  HostEventDispatcher();

  HostEventDispatcher(const HostEventDispatcher&) = delete;
  HostEventDispatcher& operator=(const HostEventDispatcher&) = delete;

  ~HostEventDispatcher() override;

  // Set InputStub that will be called for each incoming input
  // message. Doesn't take ownership of |input_stub|. It must outlive
  // the dispatcher.
  void set_input_stub(InputStub* input_stub) { input_stub_ = input_stub; }

  // Returns InputEventTimestampsSource for events received on input channel.
  //
  // TODO(sergeyu): This timestamps source generates timestamps for input events
  // as they are received. This means that any potential delay in the input
  // injector is not accounted for. Consider moving this to InputInjector to
  // ensure that the timestamps source emits timestamps for each input event
  // only after it's injected. This would require updating InputStub to get
  // timestamps for each event.
  scoped_refptr<InputEventTimestampsSource> event_timestamps_source() {
    return event_timestamps_source_;
  }

 private:
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> buffer) override;

  scoped_refptr<InputEventTimestampsSourceImpl> event_timestamps_source_;

  raw_ptr<InputStub> input_stub_ = nullptr;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_HOST_EVENT_DISPATCHER_H_
