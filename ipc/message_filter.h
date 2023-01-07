// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MESSAGE_FILTER_H_
#define IPC_MESSAGE_FILTER_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "ipc/ipc_channel.h"

namespace IPC {

class Message;

// A class that receives messages on the thread where the IPC channel is
// running.  It can choose to prevent the default action for an IPC message.
class COMPONENT_EXPORT(IPC) MessageFilter
    : public base::RefCountedThreadSafe<MessageFilter> {
 public:
  MessageFilter();

  // Called on the background thread to provide the filter with access to the
  // channel.  Called when the IPC channel is initialized or when AddFilter
  // is called if the channel is already initialized.
  virtual void OnFilterAdded(Channel* channel);

  // Called on the background thread when the filter has been removed from
  // the ChannelProxy and when the Channel is closing.  After a filter is
  // removed, it will not be called again.
  virtual void OnFilterRemoved();

  // Called to inform the filter that the IPC channel is connected and we
  // have received the internal Hello message from the peer.
  virtual void OnChannelConnected(int32_t peer_pid);

  // Called when there is an error on the channel, typically that the channel
  // has been closed.
  virtual void OnChannelError();

  // Called to inform the filter that the IPC channel will be destroyed.
  // OnFilterRemoved is called immediately after this.
  virtual void OnChannelClosing();

  // Return true to indicate that the message was handled, or false to let
  // the message be handled in the default way.
  virtual bool OnMessageReceived(const Message& message);

  // Called to query the Message classes supported by the filter.  Return
  // false to indicate that all message types should reach the filter, or true
  // if the resulting contents of |supported_message_classes| may be used to
  // selectively offer messages of a particular class to the filter.
  virtual bool GetSupportedMessageClasses(
      std::vector<uint32_t>* supported_message_classes) const;

 protected:
  virtual ~MessageFilter();

 private:
  friend class base::RefCountedThreadSafe<MessageFilter>;
};

}  // namespace IPC

#endif  // IPC_MESSAGE_FILTER_H_
