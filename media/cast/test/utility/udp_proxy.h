// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_UTILITY_UDP_PROXY_H_
#define MEDIA_CAST_TEST_UTILITY_UDP_PROXY_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <random>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "media/cast/net/cast_transport_config.h"
#include "net/base/ip_endpoint.h"

namespace net {
class NetLog;
}

namespace base {
class TickClock;
}

namespace media {
namespace cast {
namespace test {

class PacketPipe {
 public:
  PacketPipe();
  virtual ~PacketPipe();
  virtual void Send(std::unique_ptr<Packet> packet) = 0;
  // Allows injection of fake test runner for testing.
  virtual void InitOnIOThread(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const base::TickClock* clock);
  virtual void AppendToPipe(std::unique_ptr<PacketPipe> pipe);

 protected:
  std::unique_ptr<PacketPipe> pipe_;
  // Allows injection of fake task runner for testing.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const base::TickClock* clock_;
};

// Implements a Interrupted Poisson Process for packet delivery.
// The process has 2 states: ON and OFF, the rate of switching between
// these two states are defined.
// When in ON state packets are sent according to a defined rate.
// When in OFF state packets are not sent.
// The rate above is the average rate of a poisson distribution.
class InterruptedPoissonProcess {
 public:
  InterruptedPoissonProcess(const std::vector<double>& average_rates,
                            double coef_burstiness,
                            double coef_variance,
                            uint32_t rand_seed);
  ~InterruptedPoissonProcess();

  std::unique_ptr<PacketPipe> NewBuffer(size_t size);

 private:
  class InternalBuffer;

  // |task_runner| is the executor of the IO thread.
  // |clock| is the system clock.
  void InitOnIOThread(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const base::TickClock* clock);

  base::TimeDelta NextEvent(double rate);
  double RandDouble();
  void ComputeRates();
  void UpdateRates();
  void SwitchOff();
  void SwitchOn();
  void SendPacket();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const base::TickClock* clock_;
  const std::vector<double> average_rates_;
  const double coef_burstiness_;
  const double coef_variance_;
  int rate_index_;

  // The following rates are per milliseconds.
  double send_rate_;
  double switch_off_rate_;
  double switch_on_rate_;
  bool on_state_;

  std::vector<base::WeakPtr<InternalBuffer> > send_buffers_;

  // Fast pseudo random number generator.
  std::mt19937 mt_rand_;

  base::WeakPtrFactory<InterruptedPoissonProcess> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InterruptedPoissonProcess);
};

// A UDPProxy will set up a UDP socket and bind to |local_port|.
// Packets send to that port will be forwarded to |destination|.
// Packets send from |destination| to |local_port| will be returned
// to whoever sent a packet to |local_port| last. (Not counting packets
// from |destination|.) The UDPProxy will run a separate thread to
// do the forwarding of packets, and will keep doing so until destroyed.
// You can insert delays and packet drops by supplying a PacketPipe.
// The PacketPipes may also be NULL if you just want to forward packets.
class UDPProxy {
 public:
  virtual ~UDPProxy() {}
  static std::unique_ptr<UDPProxy> Create(
      const net::IPEndPoint& local_port,
      const net::IPEndPoint& destination,
      std::unique_ptr<PacketPipe> to_dest_pipe,
      std::unique_ptr<PacketPipe> from_dest_pipe,
      net::NetLog* net_log);
};

// The following functions create PacketPipes which can be linked
// together (with AppendToPipe) and passed into UdpProxy::Create below.

// This PacketPipe emulates a buffer of a given size. Limits our output
// from the buffer at a rate given by |bandwidth| (in megabits per second).
// Packets entering the buffer will be dropped if there is not enough
// room for them.
std::unique_ptr<PacketPipe> NewBuffer(size_t buffer_size, double bandwidth);

// Randomly drops |drop_fraction|*100% of packets.
std::unique_ptr<PacketPipe> NewRandomDrop(double drop_fraction);

// Delays each packet by |delay_seconds|.
std::unique_ptr<PacketPipe> NewConstantDelay(double delay_seconds);

// Delays packets by a random amount between zero and |delay|.
// This PacketPipe can reorder packets.
std::unique_ptr<PacketPipe> NewRandomUnsortedDelay(double delay);

// Duplicates every packet, one is transmitted immediately,
// one is transmitted after a random delay between |delay_min|
// and |delay_min + random_delay|.
// This PacketPipe will reorder packets.
std::unique_ptr<PacketPipe> NewDuplicateAndDelay(double delay_min,
                                                 double random_delay);

// This PacketPipe inserts a random delay between each packet.
// This PacketPipe cannot re-order packets. The delay between each
// packet is asically |min_delay| + random( |random_delay| )
// However, every now and then a delay of |big_delay| will be
// inserted (roughly every |seconds_between_big_delay| seconds).
std::unique_ptr<PacketPipe> NewRandomSortedDelay(
    double random_delay,
    double big_delay,
    double seconds_between_big_delay);

// This PacketPipe emulates network outages. It basically waits
// for 0-2*|average_work_time| seconds, then kills the network for
// 0-|2*average_outage_time| seconds. Then it starts over again.
std::unique_ptr<PacketPipe> NewNetworkGlitchPipe(double average_work_time,
                                                 double average_outage_time);

// This method builds a stack of PacketPipes to emulate a reasonably
// good network. ~50mbit, ~3ms latency, no packet loss unless saturated.
std::unique_ptr<PacketPipe> GoodNetwork();

// This method builds a stack of PacketPipes to emulate a reasonably
// good wifi network. ~20mbit, 1% packet loss, ~3ms latency.
std::unique_ptr<PacketPipe> WifiNetwork();

// This method builds a stack of PacketPipes to emulate a slow, but
// reasonably good "older technology" wifi network. ~2mbit, 1% packet loss,
// ~30ms latency.
std::unique_ptr<PacketPipe> SlowNetwork();

// This method builds a stack of PacketPipes to emulate a
// bad wifi network. ~5mbit, 5% packet loss, ~7ms latency
// 40ms dropouts every ~2 seconds. Can reorder packets.
std::unique_ptr<PacketPipe> BadNetwork();

// This method builds a stack of PacketPipes to emulate a crappy wifi network.
// ~2mbit, 20% packet loss, ~40ms latency and packets can get reordered.
// 300ms drouputs every ~2 seconds.
std::unique_ptr<PacketPipe> EvilNetwork();

// Builds an Interrupted Poisson Process network simulator with default
// settings. It simulates a challenging interference-heavy WiFi environment
// of roughly 2mbits/s.
std::unique_ptr<InterruptedPoissonProcess> DefaultInterruptedPoissonProcess();

}  // namespace test
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_UTILITY_UDP_PROXY_H_
