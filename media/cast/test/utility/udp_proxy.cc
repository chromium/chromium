// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/utility/udp_proxy.h"

#include <math.h>
#include <stdlib.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/rand_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/udp_server_socket.h"

namespace media {
namespace cast {
namespace test {

const size_t kMaxPacketSize = 65536;

PacketPipe::PacketPipe() = default;
PacketPipe::~PacketPipe() = default;
void PacketPipe::InitOnIOThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const base::TickClock* clock) {
  task_runner_ = task_runner;
  clock_ = clock;
  if (pipe_) {
    pipe_->InitOnIOThread(task_runner, clock);
  }
}
void PacketPipe::AppendToPipe(std::unique_ptr<PacketPipe> pipe) {
  if (pipe_) {
    pipe_->AppendToPipe(std::move(pipe));
  } else {
    pipe_ = std::move(pipe);
  }
}

// Roughly emulates a buffer inside a device.
// If the buffer is full, packets are dropped.
// Packets are output at a maximum bandwidth.
class Buffer : public PacketPipe {
 public:
  Buffer(size_t buffer_size, double max_megabits_per_second)
      : buffer_size_(0),
        max_buffer_size_(buffer_size),
        max_megabits_per_second_(max_megabits_per_second) {
    CHECK_GT(max_buffer_size_, 0UL);
    CHECK_GT(max_megabits_per_second, 0);
  }

  void Send(std::unique_ptr<Packet> packet) final {
    if (packet->size() + buffer_size_ <= max_buffer_size_) {
      buffer_size_ += packet->size();
      buffer_.push_back(std::move(packet));
      if (buffer_.size() == 1) {
        Schedule();
      }
    }
  }

 private:
  void Schedule() {
    last_schedule_ = clock_->NowTicks();
    double megabits = buffer_.front()->size() * 8 / 1000000.0;
    double seconds = megabits / max_megabits_per_second_;
    int64_t microseconds = static_cast<int64_t>(seconds * 1E6);
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&Buffer::ProcessBuffer, weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMicroseconds(microseconds));
  }

  void ProcessBuffer() {
    int64_t bytes_to_send = static_cast<int64_t>(
        (clock_->NowTicks() - last_schedule_).InSecondsF() *
        max_megabits_per_second_ * 1E6 / 8);
    if (bytes_to_send < static_cast<int64_t>(buffer_.front()->size())) {
      bytes_to_send = buffer_.front()->size();
    }
    while (!buffer_.empty() &&
           static_cast<int64_t>(buffer_.front()->size()) <= bytes_to_send) {
      CHECK(!buffer_.empty());
      std::unique_ptr<Packet> packet = std::move(buffer_.front());
      bytes_to_send -= packet->size();
      buffer_size_ -= packet->size();
      buffer_.pop_front();
      pipe_->Send(std::move(packet));
    }
    if (!buffer_.empty()) {
      Schedule();
    }
  }

  base::circular_deque<std::unique_ptr<Packet>> buffer_;
  base::TimeTicks last_schedule_;
  size_t buffer_size_;
  size_t max_buffer_size_;
  double max_megabits_per_second_;  // megabits per second
  base::WeakPtrFactory<Buffer> weak_factory_{this};
};

std::unique_ptr<PacketPipe> NewBuffer(size_t buffer_size, double bandwidth) {
  return std::unique_ptr<PacketPipe>(new Buffer(buffer_size, bandwidth));
}

class RandomDrop : public PacketPipe {
 public:
  RandomDrop(double drop_fraction)
      : drop_fraction_(static_cast<int>(drop_fraction * RAND_MAX)) {}

  void Send(std::unique_ptr<Packet> packet) final {
    if (rand() > drop_fraction_) {
      pipe_->Send(std::move(packet));
    }
  }

 private:
  int drop_fraction_;
};

std::unique_ptr<PacketPipe> NewRandomDrop(double drop_fraction) {
  return std::unique_ptr<PacketPipe>(new RandomDrop(drop_fraction));
}

class SimpleDelayBase : public PacketPipe {
 public:
  SimpleDelayBase() {}
  ~SimpleDelayBase() override = default;

  void Send(std::unique_ptr<Packet> packet) override {
    double seconds = GetDelay();
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SimpleDelayBase::SendInternal,
                       weak_factory_.GetWeakPtr(), std::move(packet)),
        base::TimeDelta::FromMicroseconds(static_cast<int64_t>(seconds * 1E6)));
  }
 protected:
  virtual double GetDelay() = 0;

 private:
  virtual void SendInternal(std::unique_ptr<Packet> packet) {
    pipe_->Send(std::move(packet));
  }

  base::WeakPtrFactory<SimpleDelayBase> weak_factory_{this};
};

class ConstantDelay : public SimpleDelayBase {
 public:
  ConstantDelay(double delay_seconds) : delay_seconds_(delay_seconds) {}
  double GetDelay() final { return delay_seconds_; }

 private:
  double delay_seconds_;
};

std::unique_ptr<PacketPipe> NewConstantDelay(double delay_seconds) {
  return std::unique_ptr<PacketPipe>(new ConstantDelay(delay_seconds));
}

class RandomUnsortedDelay : public SimpleDelayBase {
 public:
  RandomUnsortedDelay(double random_delay) : random_delay_(random_delay) {}

  double GetDelay() override { return random_delay_ * base::RandDouble(); }

 private:
  double random_delay_;
};

std::unique_ptr<PacketPipe> NewRandomUnsortedDelay(double random_delay) {
  return std::unique_ptr<PacketPipe>(new RandomUnsortedDelay(random_delay));
}

class DuplicateAndDelay : public RandomUnsortedDelay {
 public:
  DuplicateAndDelay(double delay_min,
                    double random_delay) :
      RandomUnsortedDelay(random_delay),
      delay_min_(delay_min) {
  }
  void Send(std::unique_ptr<Packet> packet) final {
    pipe_->Send(std::unique_ptr<Packet>(new Packet(*packet.get())));
    RandomUnsortedDelay::Send(std::move(packet));
  }
  double GetDelay() final {
    return RandomUnsortedDelay::GetDelay() + delay_min_;
  }
 private:
  double delay_min_;
};

std::unique_ptr<PacketPipe> NewDuplicateAndDelay(double delay_min,
                                                 double random_delay) {
  return std::unique_ptr<PacketPipe>(
      new DuplicateAndDelay(delay_min, random_delay));
}

class RandomSortedDelay : public PacketPipe {
 public:
  RandomSortedDelay(double random_delay,
                    double extra_delay,
                    double seconds_between_extra_delay)
      : random_delay_(random_delay),
        extra_delay_(extra_delay),
        seconds_between_extra_delay_(seconds_between_extra_delay) {}

  void Send(std::unique_ptr<Packet> packet) final {
    buffer_.push_back(std::move(packet));
    if (buffer_.size() == 1) {
      next_send_ = std::max(
          clock_->NowTicks() +
          base::TimeDelta::FromSecondsD(base::RandDouble() * random_delay_),
          next_send_);
      ProcessBuffer();
    }
  }
  void InitOnIOThread(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const base::TickClock* clock) final {
    PacketPipe::InitOnIOThread(task_runner, clock);
    // As we start the stream, assume that we are in a random
    // place between two extra delays, thus multiplier = 1.0;
    ScheduleExtraDelay(1.0);
  }

 private:
  void ScheduleExtraDelay(double mult) {
    double seconds = seconds_between_extra_delay_ * mult * base::RandDouble();
    int64_t microseconds = static_cast<int64_t>(seconds * 1E6);
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RandomSortedDelay::CauseExtraDelay,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMicroseconds(microseconds));
  }

  void CauseExtraDelay() {
    next_send_ = std::max<base::TimeTicks>(
        clock_->NowTicks() + base::TimeDelta::FromMicroseconds(
                                 static_cast<int64_t>(extra_delay_ * 1E6)),
        next_send_);
    // An extra delay just happened, wait up to seconds_between_extra_delay_*2
    // before scheduling another one to make the average equal to
    // seconds_between_extra_delay_.
    ScheduleExtraDelay(2.0);
  }

  void ProcessBuffer() {
    base::TimeTicks now = clock_->NowTicks();
    while (!buffer_.empty() && next_send_ <= now) {
      std::unique_ptr<Packet> packet = std::move(buffer_.front());
      pipe_->Send(std::move(packet));
      buffer_.pop_front();

      next_send_ += base::TimeDelta::FromSecondsD(
          base::RandDouble() * random_delay_);
    }

    if (!buffer_.empty()) {
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&RandomSortedDelay::ProcessBuffer,
                         weak_factory_.GetWeakPtr()),
          next_send_ - now);
    }
  }

  base::TimeTicks block_until_;
  base::circular_deque<std::unique_ptr<Packet>> buffer_;
  double random_delay_;
  double extra_delay_;
  double seconds_between_extra_delay_;
  base::TimeTicks next_send_;
  base::WeakPtrFactory<RandomSortedDelay> weak_factory_{this};
};

std::unique_ptr<PacketPipe> NewRandomSortedDelay(
    double random_delay,
    double extra_delay,
    double seconds_between_extra_delay) {
  return std::unique_ptr<PacketPipe>(new RandomSortedDelay(
      random_delay, extra_delay, seconds_between_extra_delay));
}

class NetworkGlitchPipe : public PacketPipe {
 public:
  NetworkGlitchPipe(double average_work_time, double average_outage_time)
      : works_(false),
        max_work_time_(average_work_time * 2),
        max_outage_time_(average_outage_time * 2) {}

  void InitOnIOThread(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const base::TickClock* clock) final {
    PacketPipe::InitOnIOThread(task_runner, clock);
    Flip();
  }

  void Send(std::unique_ptr<Packet> packet) final {
    if (works_) {
      pipe_->Send(std::move(packet));
    }
  }

 private:
  void Flip() {
    works_ = !works_;
    double seconds = base::RandDouble() *
        (works_ ? max_work_time_ : max_outage_time_);
    int64_t microseconds = static_cast<int64_t>(seconds * 1E6);
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&NetworkGlitchPipe::Flip, weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMicroseconds(microseconds));
  }

  bool works_;
  double max_work_time_;
  double max_outage_time_;
  base::WeakPtrFactory<NetworkGlitchPipe> weak_factory_{this};
};

std::unique_ptr<PacketPipe> NewNetworkGlitchPipe(double average_work_time,
                                                 double average_outage_time) {
  return std::unique_ptr<PacketPipe>(
      new NetworkGlitchPipe(average_work_time, average_outage_time));
}


// Internal buffer object for a client of the IPP model.
class InterruptedPoissonProcess::InternalBuffer : public PacketPipe {
 public:
  InternalBuffer(base::WeakPtr<InterruptedPoissonProcess> ipp, size_t size)
      : ipp_(ipp), stored_size_(0), stored_limit_(size), clock_(NULL) {}

  void Send(std::unique_ptr<Packet> packet) final {
    // Drop if buffer is full.
    if (stored_size_ >= stored_limit_)
      return;
    stored_size_ += packet->size();
    buffer_.push_back(std::move(packet));
    buffer_time_.push_back(clock_->NowTicks());
    DCHECK(buffer_.size() == buffer_time_.size());
  }

  void InitOnIOThread(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const base::TickClock* clock) final {
    clock_ = clock;
    if (ipp_)
      ipp_->InitOnIOThread(task_runner, clock);
    PacketPipe::InitOnIOThread(task_runner, clock);
  }

  void SendOnePacket() {
    std::unique_ptr<Packet> packet = std::move(buffer_.front());
    stored_size_ -= packet->size();
    buffer_.pop_front();
    buffer_time_.pop_front();
    pipe_->Send(std::move(packet));
    DCHECK(buffer_.size() == buffer_time_.size());
  }

  bool Empty() const {
    return buffer_.empty();
  }

  base::TimeTicks FirstPacketTime() const {
    DCHECK(!buffer_time_.empty());
    return buffer_time_.front();
  }

  base::WeakPtr<InternalBuffer> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();

  }

 private:
  const base::WeakPtr<InterruptedPoissonProcess> ipp_;
  size_t stored_size_;
  const size_t stored_limit_;
  base::circular_deque<std::unique_ptr<Packet>> buffer_;
  base::circular_deque<base::TimeTicks> buffer_time_;
  const base::TickClock* clock_;
  base::WeakPtrFactory<InternalBuffer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InternalBuffer);
};

InterruptedPoissonProcess::InterruptedPoissonProcess(
    const std::vector<double>& average_rates,
    double coef_burstiness,
    double coef_variance,
    uint32_t rand_seed)
    : clock_(NULL),
      average_rates_(average_rates),
      coef_burstiness_(coef_burstiness),
      coef_variance_(coef_variance),
      rate_index_(0),
      on_state_(true),
      mt_rand_(rand_seed) {
  DCHECK(!average_rates.empty());
  ComputeRates();
}

InterruptedPoissonProcess::~InterruptedPoissonProcess() = default;

void InterruptedPoissonProcess::InitOnIOThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const base::TickClock* clock) {
  // Already initialized and started.
  if (task_runner_.get() && clock_)
    return;
  task_runner_ = task_runner;
  clock_ = clock;
  UpdateRates();
  SwitchOn();
  SendPacket();
}

std::unique_ptr<PacketPipe> InterruptedPoissonProcess::NewBuffer(size_t size) {
  std::unique_ptr<InternalBuffer> buffer(
      new InternalBuffer(weak_factory_.GetWeakPtr(), size));
  send_buffers_.push_back(buffer->GetWeakPtr());
  return std::move(buffer);
}

base::TimeDelta InterruptedPoissonProcess::NextEvent(double rate) {
  // Rate is per milliseconds.
  // The time until next event is exponentially distributed to the
  // inverse of |rate|.
  return base::TimeDelta::FromMillisecondsD(
      fabs(-log(1.0 - RandDouble()) / rate));
}

double InterruptedPoissonProcess::RandDouble() {
  // Generate a 64-bits random number from MT19937 and then convert
  // it to double.
  uint64_t rand = mt_rand_();
  rand <<= 32;
  rand |= mt_rand_();
  return base::BitsToOpenEndedUnitInterval(rand);
}

void InterruptedPoissonProcess::ComputeRates() {
  double avg_rate = average_rates_[rate_index_];

  send_rate_ = avg_rate / coef_burstiness_;
  switch_off_rate_ =
      2 * avg_rate * (1 - coef_burstiness_) * (1 - coef_burstiness_) /
      coef_burstiness_ / (coef_variance_ - 1);
  switch_on_rate_ =
      2 * avg_rate * (1 - coef_burstiness_) / (coef_variance_ - 1);
}

void InterruptedPoissonProcess::UpdateRates() {
  ComputeRates();

  // Rates are updated once per second.
  rate_index_ = (rate_index_ + 1) % average_rates_.size();
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&InterruptedPoissonProcess::UpdateRates,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(1));
}

void InterruptedPoissonProcess::SwitchOff() {
  on_state_ = false;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&InterruptedPoissonProcess::SwitchOn,
                     weak_factory_.GetWeakPtr()),
      NextEvent(switch_on_rate_));
}

void InterruptedPoissonProcess::SwitchOn() {
  on_state_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&InterruptedPoissonProcess::SwitchOff,
                     weak_factory_.GetWeakPtr()),
      NextEvent(switch_off_rate_));
}

void InterruptedPoissonProcess::SendPacket() {
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&InterruptedPoissonProcess::SendPacket,
                     weak_factory_.GetWeakPtr()),
      NextEvent(send_rate_));

  // If OFF then don't send.
  if (!on_state_)
    return;

  // Find the earliest packet to send.
  base::TimeTicks earliest_time;
  for (size_t i = 0; i < send_buffers_.size(); ++i) {
    if (!send_buffers_[i])
      continue;
    if (send_buffers_[i]->Empty())
      continue;
    if (earliest_time.is_null() ||
        send_buffers_[i]->FirstPacketTime() < earliest_time)
      earliest_time = send_buffers_[i]->FirstPacketTime();
  }
  for (size_t i = 0; i < send_buffers_.size(); ++i) {
    if (!send_buffers_[i])
      continue;
    if (send_buffers_[i]->Empty())
      continue;
    if (send_buffers_[i]->FirstPacketTime() != earliest_time)
      continue;
    send_buffers_[i]->SendOnePacket();
    break;
  }
}

class UDPProxyImpl;

class PacketSender : public PacketPipe {
 public:
  PacketSender(UDPProxyImpl* udp_proxy, const net::IPEndPoint* destination)
      : udp_proxy_(udp_proxy), destination_(destination) {}
  void Send(std::unique_ptr<Packet> packet) final;
  void AppendToPipe(std::unique_ptr<PacketPipe> pipe) final { NOTREACHED(); }

 private:
  UDPProxyImpl* udp_proxy_;
  const net::IPEndPoint* destination_;  // not owned
};

namespace {
void BuildPipe(std::unique_ptr<PacketPipe>* pipe, PacketPipe* next) {
  if (*pipe) {
    (*pipe)->AppendToPipe(std::unique_ptr<PacketPipe>(next));
  } else {
    pipe->reset(next);
  }
}
}  // namespace

std::unique_ptr<PacketPipe> GoodNetwork() {
  // This represents the buffer on the sender.
  std::unique_ptr<PacketPipe> pipe;
  BuildPipe(&pipe, new Buffer(2 << 20, 50));
  BuildPipe(&pipe, new ConstantDelay(1E-3));
  BuildPipe(&pipe, new RandomSortedDelay(1E-3, 2E-3, 3));
  // This represents the buffer on the receiving device.
  BuildPipe(&pipe, new Buffer(2 << 20, 50));
  return pipe;
}

std::unique_ptr<PacketPipe> WifiNetwork() {
  // This represents the buffer on the sender.
  std::unique_ptr<PacketPipe> pipe;
  BuildPipe(&pipe, new Buffer(256 << 10, 20));
  BuildPipe(&pipe, new RandomDrop(0.005));
  // This represents the buffer on the router.
  BuildPipe(&pipe, new ConstantDelay(1E-3));
  BuildPipe(&pipe, new RandomSortedDelay(1E-3, 20E-3, 3));
  BuildPipe(&pipe, new Buffer(256 << 10, 20));
  BuildPipe(&pipe, new ConstantDelay(1E-3));
  BuildPipe(&pipe, new RandomSortedDelay(1E-3, 20E-3, 3));
  BuildPipe(&pipe, new RandomDrop(0.005));
  // This represents the buffer on the receiving device.
  BuildPipe(&pipe, new Buffer(256 << 10, 20));
  return pipe;
}

std::unique_ptr<PacketPipe> SlowNetwork() {
  // This represents the buffer on the sender.
  std::unique_ptr<PacketPipe> pipe;
  BuildPipe(&pipe, new Buffer(256 << 10, 1.5));
  BuildPipe(&pipe, new RandomDrop(0.005));
  // This represents the buffer on the router.
  BuildPipe(&pipe, new ConstantDelay(10E-3));
  BuildPipe(&pipe, new RandomSortedDelay(10E-3, 20E-3, 3));
  BuildPipe(&pipe, new Buffer(256 << 10, 20));
  BuildPipe(&pipe, new ConstantDelay(10E-3));
  BuildPipe(&pipe, new RandomSortedDelay(10E-3, 20E-3, 3));
  BuildPipe(&pipe, new RandomDrop(0.005));
  // This represents the buffer on the receiving device.
  BuildPipe(&pipe, new Buffer(256 << 10, 20));
  return pipe;
}

std::unique_ptr<PacketPipe> BadNetwork() {
  std::unique_ptr<PacketPipe> pipe;
  // This represents the buffer on the sender.
  BuildPipe(&pipe, new Buffer(64 << 10, 5)); // 64 kb buf, 5mbit/s
  BuildPipe(&pipe, new RandomDrop(0.05));  // 5% packet drop
  BuildPipe(&pipe, new RandomSortedDelay(2E-3, 20E-3, 1));
  // This represents the buffer on the router.
  BuildPipe(&pipe, new Buffer(64 << 10, 5));  // 64 kb buf, 4mbit/s
  BuildPipe(&pipe, new ConstantDelay(1E-3));
  // Random 40ms every other second
  //  BuildPipe(&pipe, new NetworkGlitchPipe(2, 40E-1));
  BuildPipe(&pipe, new RandomUnsortedDelay(5E-3));
  // This represents the buffer on the receiving device.
  BuildPipe(&pipe, new Buffer(64 << 10, 5));  // 64 kb buf, 5mbit/s
  return pipe;
}

std::unique_ptr<PacketPipe> EvilNetwork() {
  // This represents the buffer on the sender.
  std::unique_ptr<PacketPipe> pipe;
  BuildPipe(&pipe, new Buffer(4 << 10, 5));  // 4 kb buf, 2mbit/s
  // This represents the buffer on the router.
  BuildPipe(&pipe, new RandomDrop(0.1));  // 10% packet drop
  BuildPipe(&pipe, new RandomSortedDelay(20E-3, 60E-3, 1));
  BuildPipe(&pipe, new Buffer(4 << 10, 2));  // 4 kb buf, 2mbit/s
  BuildPipe(&pipe, new RandomDrop(0.1));  // 10% packet drop
  BuildPipe(&pipe, new ConstantDelay(1E-3));
  BuildPipe(&pipe, new NetworkGlitchPipe(2.0, 0.3));
  BuildPipe(&pipe, new RandomUnsortedDelay(20E-3));
  // This represents the buffer on the receiving device.
  BuildPipe(&pipe, new Buffer(4 << 10, 2));  // 4 kb buf, 2mbit/s
  return pipe;
}

std::unique_ptr<InterruptedPoissonProcess> DefaultInterruptedPoissonProcess() {
  // The following values are taken from a session reported from a user.
  // They are experimentally tested to demonstrate challenging network
  // conditions. The average bitrate is about 2mbits/s.

  // Each element in this vector is the average number of packets sent
  // per millisecond. The average changes and rotates every second.
  std::vector<double> average_rates;
  average_rates.push_back(0.609);
  average_rates.push_back(0.495);
  average_rates.push_back(0.561);
  average_rates.push_back(0.458);
  average_rates.push_back(0.538);
  average_rates.push_back(0.513);
  average_rates.push_back(0.585);
  average_rates.push_back(0.592);
  average_rates.push_back(0.658);
  average_rates.push_back(0.556);
  average_rates.push_back(0.371);
  average_rates.push_back(0.595);
  average_rates.push_back(0.490);
  average_rates.push_back(0.980);
  average_rates.push_back(0.781);
  average_rates.push_back(0.463);

  const double burstiness = 0.609;
  const double variance = 4.1;

  std::unique_ptr<InterruptedPoissonProcess> ipp(
      new InterruptedPoissonProcess(average_rates, burstiness, variance, 0));
  return ipp;
}

class UDPProxyImpl : public UDPProxy {
 public:
  UDPProxyImpl(const net::IPEndPoint& local_port,
               const net::IPEndPoint& destination,
               std::unique_ptr<PacketPipe> to_dest_pipe,
               std::unique_ptr<PacketPipe> from_dest_pipe,
               net::NetLog* net_log)
      : local_port_(local_port),
        destination_(destination),
        destination_is_mutable_(destination.address().empty()),
        proxy_thread_("media::cast::test::UdpProxy Thread"),
        to_dest_pipe_(std::move(to_dest_pipe)),
        from_dest_pipe_(std::move(from_dest_pipe)),
        blocked_(false) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    proxy_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    base::WaitableEvent start_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    proxy_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&UDPProxyImpl::Start, base::Unretained(this),
                                  base::Unretained(&start_event), net_log));
    start_event.Wait();
  }

  ~UDPProxyImpl() final {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::WaitableEvent stop_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    proxy_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&UDPProxyImpl::Stop, base::Unretained(this),
                                  base::Unretained(&stop_event)));
    stop_event.Wait();
    proxy_thread_.Stop();
  }

  void Send(std::unique_ptr<Packet> packet,
            const net::IPEndPoint& destination) {
    if (blocked_) {
      LOG(ERROR) << "Cannot write packet right now: blocked";
      return;
    }

    VLOG(1) << "Sending packet, len = " << packet->size();
    // We ignore all problems, callbacks and errors.
    // If it didn't work we just drop the packet at and call it a day.
    auto buf = base::MakeRefCounted<net::WrappedIOBuffer>(
        reinterpret_cast<char*>(&packet->front()));
    size_t buf_size = packet->size();
    int result;
    if (destination.address().empty()) {
      VLOG(1) << "Destination has not been set yet.";
      result = net::ERR_INVALID_ARGUMENT;
    } else {
      VLOG(1) << "Destination:" << destination.ToString();
      result = socket_->SendTo(buf.get(),
                               static_cast<int>(buf_size),
                               destination,
                               base::Bind(&UDPProxyImpl::AllowWrite,
                                          weak_factory_.GetWeakPtr(),
                                          buf,
                                          base::Passed(&packet)));
    }
    if (result == net::ERR_IO_PENDING) {
      blocked_ = true;
    } else if (result < 0) {
      LOG(ERROR) << "Failed to write packet.";
    }
  }

 private:
  void Start(base::WaitableEvent* start_event,
             net::NetLog* net_log) {
    socket_.reset(new net::UDPServerSocket(net_log, net::NetLogSource()));
    BuildPipe(&to_dest_pipe_, new PacketSender(this, &destination_));
    BuildPipe(&from_dest_pipe_, new PacketSender(this, &return_address_));
    to_dest_pipe_->InitOnIOThread(base::ThreadTaskRunnerHandle::Get(),
                                  base::DefaultTickClock::GetInstance());
    from_dest_pipe_->InitOnIOThread(base::ThreadTaskRunnerHandle::Get(),
                                    base::DefaultTickClock::GetInstance());

    VLOG(0) << "From:" << local_port_.ToString();
    if (!destination_is_mutable_)
      VLOG(0) << "To:" << destination_.ToString();

    CHECK_GE(socket_->Listen(local_port_), 0);

    start_event->Signal();
    PollRead();
  }

  void Stop(base::WaitableEvent* stop_event) {
    to_dest_pipe_.reset(NULL);
    from_dest_pipe_.reset(NULL);
    socket_.reset(NULL);
    stop_event->Signal();
  }

  void ProcessPacket(scoped_refptr<net::IOBuffer> recv_buf, int len) {
    DCHECK_NE(len, net::ERR_IO_PENDING);
    VLOG(1) << "Got packet, len = " << len;
    if (len < 0) {
      LOG(WARNING) << "Socket read error: " << len;
      return;
    }
    packet_->resize(len);
    if (destination_is_mutable_ && set_destination_next_ &&
        !(recv_address_ == return_address_) &&
        !(recv_address_ == destination_)) {
      destination_ = recv_address_;
    }
    if (recv_address_ == destination_) {
      set_destination_next_ = false;
      from_dest_pipe_->Send(std::move(packet_));
    } else {
      set_destination_next_ = true;
      VLOG(1) << "Return address = " << recv_address_.ToString();
      return_address_ = recv_address_;
      to_dest_pipe_->Send(std::move(packet_));
    }
  }

  void ReadCallback(scoped_refptr<net::IOBuffer> recv_buf, int len) {
    ProcessPacket(recv_buf, len);
    PollRead();
  }

  void PollRead() {
    while (true) {
      packet_.reset(new Packet(kMaxPacketSize));
      auto recv_buf = base::MakeRefCounted<net::WrappedIOBuffer>(
          reinterpret_cast<char*>(&packet_->front()));
      int len = socket_->RecvFrom(
          recv_buf.get(),
          kMaxPacketSize,
          &recv_address_,
          base::Bind(
              &UDPProxyImpl::ReadCallback, base::Unretained(this), recv_buf));
      if (len == net::ERR_IO_PENDING)
        break;
      ProcessPacket(recv_buf, len);
    }
  }

  void AllowWrite(scoped_refptr<net::IOBuffer> buf,
                  std::unique_ptr<Packet> packet,
                  int unused_len) {
    DCHECK(blocked_);
    blocked_ = false;
  }

  // Input
  net::IPEndPoint local_port_;

  net::IPEndPoint destination_;
  bool destination_is_mutable_;

  net::IPEndPoint return_address_;
  bool set_destination_next_;

  base::Thread proxy_thread_;
  std::unique_ptr<net::UDPServerSocket> socket_;
  std::unique_ptr<PacketPipe> to_dest_pipe_;
  std::unique_ptr<PacketPipe> from_dest_pipe_;

  // For receiving.
  net::IPEndPoint recv_address_;
  std::unique_ptr<Packet> packet_;

  // For sending.
  bool blocked_;

  base::WeakPtrFactory<UDPProxyImpl> weak_factory_{this};
};

void PacketSender::Send(std::unique_ptr<Packet> packet) {
  udp_proxy_->Send(std::move(packet), *destination_);
}

std::unique_ptr<UDPProxy> UDPProxy::Create(
    const net::IPEndPoint& local_port,
    const net::IPEndPoint& destination,
    std::unique_ptr<PacketPipe> to_dest_pipe,
    std::unique_ptr<PacketPipe> from_dest_pipe,
    net::NetLog* net_log) {
  std::unique_ptr<UDPProxy> ret(
      new UDPProxyImpl(local_port, destination, std::move(to_dest_pipe),
                       std::move(from_dest_pipe), net_log));
  return ret;
}

}  // namespace test
}  // namespace cast
}  // namespace media
