// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CONNECTIVITY_PROBING_MANAGER_H_
#define NET_QUIC_QUIC_CONNECTIVITY_PROBING_MANAGER_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/net_export.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/third_party/quiche/src/quic/core/quic_time.h"

namespace net {

// Responsible for sending and retransmitting connectivity probing packet on a
// designated path to the specified peer, and for notifying associated session
// when connectivity probe fails or succeeds.
class NET_EXPORT_PRIVATE QuicConnectivityProbingManager
    : public QuicChromiumPacketWriter::Delegate {
 public:
  // Delegate interface which receives notifications on probing results.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    virtual ~Delegate() {}

    // Called when probing to |peer_address| on |network| succeeded.
    // Caller hands off the ownership of |socket|, |writer| and |reader| for
    // |peer_address| on |network| to delegate.
    virtual void OnProbeSucceeded(
        NetworkChangeNotifier::NetworkHandle network,
        const quic::QuicSocketAddress& peer_address,
        const quic::QuicSocketAddress& self_address,
        std::unique_ptr<DatagramClientSocket> socket,
        std::unique_ptr<QuicChromiumPacketWriter> writer,
        std::unique_ptr<QuicChromiumPacketReader> reader) = 0;

    // Called when probing to |peer_address| on |network| failed.
    virtual void OnProbeFailed(NetworkChangeNotifier::NetworkHandle network,
                               const quic::QuicSocketAddress& peer_address) = 0;

    // Called when a connectivity probing packet needs to be sent to
    // |peer_address| using |writer|. Returns true if subsequent packets can be
    // written by the |writer|.
    virtual bool OnSendConnectivityProbingPacket(
        QuicChromiumPacketWriter* writer,
        const quic::QuicSocketAddress& peer_address) = 0;
  };

  QuicConnectivityProbingManager(Delegate* delegate,
                                 base::SequencedTaskRunner* task_runner);
  ~QuicConnectivityProbingManager();

  // QuicChromiumPacketWriter::Delegate interface.
  int HandleWriteError(int error_code,
                       scoped_refptr<QuicChromiumPacketWriter::ReusableIOBuffer>
                           last_packet) override;
  void OnWriteError(int error_code) override;
  void OnWriteUnblocked() override;

  // Starts probing |peer_address| on |network|.
  // |this| will take the ownership of |socket|, |writer| and |reader|.
  // |writer| and |reader| should be bound to |socket|. |writer| will be used
  // to send connectivity probes. Connectivity probes will be resent after
  // |initial_timeout|. Mutilple trials will be attempted with exponential
  // backoff until a connectivity probe response is received from by |reader|
  // or the final timeout is reached.
  void StartProbing(NetworkChangeNotifier::NetworkHandle network,
                    const quic::QuicSocketAddress& peer_address,
                    std::unique_ptr<DatagramClientSocket> socket,
                    std::unique_ptr<QuicChromiumPacketWriter> writer,
                    std::unique_ptr<QuicChromiumPacketReader> reader,
                    base::TimeDelta initial_timeout,
                    const NetLogWithSource& net_log);

  // Cancels undergoing probing if |this| is currently probing |peer_address|
  // on |network|.
  void CancelProbing(NetworkChangeNotifier::NetworkHandle network,
                     const quic::QuicSocketAddress& peer_address);

  // Called when a new packet has been received from |peer_address| on a socket
  // with |self_address|. |is_connectivity_probe| is true if the received
  // packet is a connectivity probe.
  void OnPacketReceived(const quic::QuicSocketAddress& self_address,
                        const quic::QuicSocketAddress& peer_address,
                        bool is_connectivity_probe);

  // Returns true if the manager is currently probing |peer_address| on
  // |network|.
  bool IsUnderProbing(NetworkChangeNotifier::NetworkHandle network,
                      const quic::QuicSocketAddress& peer_address) {
    return (is_running_ && network == network_ &&
            peer_address == peer_address_);
  }

 private:
  // Cancels undergoing probing.
  void CancelProbingIfAny();

  // Called when a connectivity probe needs to be sent and set a timer to
  // resend a connectivity probing packet to peer after |timeout|.
  void SendConnectivityProbingPacket(base::TimeDelta timeout);

  // Called when no connectivity probe response has been received on the
  // currrent probing path after some timeout.
  void MaybeResendConnectivityProbingPacket();

  void NotifyDelegateProbeFailed();

  Delegate* delegate_;  // Unowned, must outlive |this|.
  NetLogWithSource net_log_;

  // Current path: |peer_address_| on |network_|, that is under probing
  // if |is_running_| is true.
  bool is_running_;
  NetworkChangeNotifier::NetworkHandle network_;
  quic::QuicSocketAddress peer_address_;

  std::unique_ptr<DatagramClientSocket> socket_;
  std::unique_ptr<QuicChromiumPacketWriter> writer_;
  std::unique_ptr<QuicChromiumPacketReader> reader_;

  int64_t retry_count_;
  base::TimeTicks probe_start_time_;
  base::TimeDelta initial_timeout_;
  base::OneShotTimer retransmit_timer_;

  base::SequencedTaskRunner* task_runner_;

  base::WeakPtrFactory<QuicConnectivityProbingManager> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(QuicConnectivityProbingManager);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONNECTIVITY_PROBING_MANAGER_H_
