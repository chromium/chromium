// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_RPC_BROKER_H_
#define MEDIA_REMOTING_RPC_BROKER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/remoting/media_remoting_rpc.pb.h"

namespace media {
namespace remoting {

// Utility class to process incoming and outgoing RPC message to desired
// components on both end points. On sender side, for outgoing message, sender
// sends RPC message with associated handle value. On receiver side, for
// component which is interested in this RPC message has to register itself to
// RpcBroker. Before the RPC transmission starts, both sender and receiver need
// to negotiate the handle value in the existing RPC communication channel using
// handle kAcquireHandle.
//
// The class doesn't actually send RPC message to remote end point. Actual
// sender needs to set SendMessageCallback to RpcBroker. The class doesn't
// actually receive RPC message from the remote end point, either. Actually
// receiver needs to call ProcessMessageFromRemote() when RPC message is
// received. RpcBroker will distribute each RPC message to the components based
// on the handle value in the RPC message.
//
// Note this is single-threaded class running on main thread. It provides
// WeakPtr() for caller to post tasks to the main thread.
class RpcBroker {
 public:
  using SendMessageCallback =
      base::Callback<void(std::unique_ptr<std::vector<uint8_t>>)>;
  explicit RpcBroker(const SendMessageCallback& send_message_cb);
  ~RpcBroker();

  // Get unique handle value (larger than 0) for RPC message handles.
  int GetUniqueHandle();

  using ReceiveMessageCallback =
      base::RepeatingCallback<void(std::unique_ptr<pb::RpcMessage>)>;
  // Register a component to receive messages via the given
  // ReceiveMessageCallback. |handle| is a unique handle value provided by a
  // prior call to GetUniqueHandle() and is used to reference the component in
  // the RPC messages. The receiver can then use it to direct an RPC message
  // back to a specific component.
  void RegisterMessageReceiverCallback(int handle,
                                       const ReceiveMessageCallback& callback);
  // Allows components to unregister in order to stop receiving message.
  void UnregisterMessageReceiverCallback(int handle);

  // Allows RpcBroker to distribute incoming RPC message to desired components.
  void ProcessMessageFromRemote(std::unique_ptr<pb::RpcMessage> message);
  // Sends RPC message to remote end point. The actually sender which sets
  // SendMessageCallback to RpcBrokwer will receive RPC message to do actual
  // data transmission.
  void SendMessageToRemote(std::unique_ptr<pb::RpcMessage> message);

  // Gets weak pointer of RpcBroker. This allows callers to post tasks to
  // RpcBroker on the main thread.
  base::WeakPtr<RpcBroker> GetWeakPtr();

  // Overwrites |send_message_cb_|. This is used only for test purposes.
  void SetMessageCallbackForTesting(const SendMessageCallback& send_message_cb);

  // Predefined invalid handle value for RPC message.
  static constexpr int kInvalidHandle = -1;

  // Predefined handle value for RPC messages related to initialization (before
  // the receiver handle(s) are known).
  static constexpr int kAcquireHandle = 0;

  // The first handle to return from GetUniqueHandle().
  static constexpr int kFirstHandle = 1;

 private:
  // Checks that all method calls occur on the same thread.
  base::ThreadChecker thread_checker_;

  // Next unique handle to return from GetUniqueHandle().
  int next_handle_;

  // Maps to hold handle value associated to MessageReceiver.
  std::map<int, ReceiveMessageCallback> receive_callbacks_;

  // Callback that is run to send a serialized message.
  SendMessageCallback send_message_cb_;

  base::WeakPtrFactory<RpcBroker> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RpcBroker);
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_RPC_BROKER_H_
