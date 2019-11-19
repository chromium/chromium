// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHANNEL_MULTIPLEXER_H_
#define REMOTING_PROTOCOL_CHANNEL_MULTIPLEXER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/buffered_socket_writer.h"
#include "remoting/proto/mux.pb.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/stream_channel_factory.h"

namespace remoting {
namespace protocol {

class ChannelMultiplexer : public StreamChannelFactory {
 public:
  static const char kMuxChannelName[];

  // |factory| is used to create the channel upon which to multiplex.
  ChannelMultiplexer(StreamChannelFactory* factory,
                     const std::string& base_channel_name);
  ~ChannelMultiplexer() override;

  // StreamChannelFactory interface.
  void CreateChannel(const std::string& name,
                     const ChannelCreatedCallback& callback) override;
  void CancelChannelCreation(const std::string& name) override;

 private:
  struct PendingChannel;
  class MuxChannel;
  class MuxSocket;
  friend class MuxChannel;

  // Callback for |base_channel_| creation.
  void OnBaseChannelReady(std::unique_ptr<P2PStreamSocket> socket);

  // Helper to create channels asynchronously.
  void DoCreatePendingChannels();

  // Helper method used to create channels.
  MuxChannel* GetOrCreateChannel(const std::string& name);

  // Error handling callback for |reader_| and |writer_|.
  void OnBaseChannelError(int error);

  // Propagates base channel error to channel |name|, queued asynchronously by
  // OnBaseChannelError().
  void NotifyBaseChannelError(const std::string& name, int error);

  // Callback for |reader_;
  void OnIncomingPacket(std::unique_ptr<CompoundBuffer> buffer);

  // Called by MuxChannel.
  void DoWrite(std::unique_ptr<MultiplexPacket> packet,
               base::OnceClosure done_task,
               const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Factory used to create |base_channel_|. Set to nullptr once creation is
  // finished or failed.
  StreamChannelFactory* base_channel_factory_;

  // Name of the underlying channel.
  std::string base_channel_name_;

  // The channel over which to multiplex.
  std::unique_ptr<P2PStreamSocket> base_channel_;

  // List of requested channels while we are waiting for |base_channel_|.
  std::list<PendingChannel> pending_channels_;

  int next_channel_id_;
  std::map<std::string, std::unique_ptr<MuxChannel>> channels_;

  // Channels are added to |channels_by_receive_id_| only after we receive
  // receive_id from the remote peer.
  std::map<int, MuxChannel*> channels_by_receive_id_;

  BufferedSocketWriter writer_;
  MessageReader reader_;

  base::WeakPtrFactory<ChannelMultiplexer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChannelMultiplexer);
};

}  // namespace protocol
}  // namespace remoting


#endif  // REMOTING_PROTOCOL_CHANNEL_MULTIPLEXER_H_
