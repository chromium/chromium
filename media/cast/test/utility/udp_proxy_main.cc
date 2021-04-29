// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/circular_deque.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/cast/test/utility/udp_proxy.h"
#include "net/base/ip_address.h"

class ByteCounter {
 public:
  ByteCounter() : bytes_(0), packets_(0) {
    push(base::TimeTicks::Now());
  }

  base::TimeDelta time_range() {
    return time_data_.back() - time_data_.front();
  }

  void push(base::TimeTicks now) {
    byte_data_.push_back(bytes_);
    packet_data_.push_back(packets_);
    time_data_.push_back(now);
    while (time_range().InSeconds() > 10) {
      byte_data_.pop_front();
      packet_data_.pop_front();
      time_data_.pop_front();
    }
  }

  double megabits_per_second() {
    double megabits = (byte_data_.back() - byte_data_.front()) * 8 / 1E6;
    return megabits / time_range().InSecondsF();
  }

  double packets_per_second() {
    double packets = packet_data_.back()- packet_data_.front();
    return packets / time_range().InSecondsF();
  }

  void Increment(uint64_t x) {
    bytes_ += x;
    packets_ ++;
  }

 private:
  uint64_t bytes_;
  uint64_t packets_;
  base::circular_deque<uint64_t> byte_data_;
  base::circular_deque<uint64_t> packet_data_;
  base::circular_deque<base::TimeTicks> time_data_;
};

namespace {

struct GlobalCounter {
  base::TimeTicks last_printout;
  ByteCounter in_pipe_input_counter;
  ByteCounter in_pipe_output_counter;
  ByteCounter out_pipe_input_counter;
  ByteCounter out_pipe_output_counter;
};

GlobalCounter* GetGlobalCounter() {
  static GlobalCounter* counter = new GlobalCounter();
  return counter;
}

}  // namespace

class ByteCounterPipe : public media::cast::test::PacketPipe {
 public:
  ByteCounterPipe(ByteCounter* counter) : counter_(counter) {}
  void Send(std::unique_ptr<media::cast::Packet> packet) final {
    counter_->Increment(packet->size());
    pipe_->Send(std::move(packet));
  }
 private:
  ByteCounter* counter_;
};

void SetupByteCounters(std::unique_ptr<media::cast::test::PacketPipe>* pipe,
                       ByteCounter* pipe_input_counter,
                       ByteCounter* pipe_output_counter) {
  media::cast::test::PacketPipe* new_pipe =
      new ByteCounterPipe(pipe_input_counter);
  new_pipe->AppendToPipe(std::move(*pipe));
  new_pipe->AppendToPipe(std::unique_ptr<media::cast::test::PacketPipe>(
      new ByteCounterPipe(pipe_output_counter)));
  pipe->reset(new_pipe);
}

void CheckByteCounters() {
  GlobalCounter* counter = GetGlobalCounter();

  base::TimeTicks now = base::TimeTicks::Now();
  counter->in_pipe_input_counter.push(now);
  counter->in_pipe_output_counter.push(now);
  counter->out_pipe_input_counter.push(now);
  counter->out_pipe_output_counter.push(now);
  if ((now - counter->last_printout).InSeconds() >= 5) {
    fprintf(stderr, "Sending  : %5.2f / %5.2f mbps %6.2f / %6.2f packets / s\n",
            counter->in_pipe_output_counter.megabits_per_second(),
            counter->in_pipe_input_counter.megabits_per_second(),
            counter->in_pipe_output_counter.packets_per_second(),
            counter->in_pipe_input_counter.packets_per_second());
    fprintf(stderr, "Receiving: %5.2f / %5.2f mbps %6.2f / %6.2f packets / s\n",
            counter->out_pipe_output_counter.megabits_per_second(),
            counter->out_pipe_input_counter.megabits_per_second(),
            counter->out_pipe_output_counter.packets_per_second(),
            counter->out_pipe_input_counter.packets_per_second());

    counter->last_printout = now;
  }
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&CheckByteCounters),
      base::TimeDelta::FromMilliseconds(100));
}

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  InitLogging(settings);

  if (argc != 5 && argc != 3) {
    fprintf(stderr,
            "Usage: udp_proxy <localport> <remotehost> <remoteport> <type>\n"
            "or:\n"
            "       udp_proxy <localport> <type>\n"
            "Where type is one of:\n"
            "    perfect, wifi, slow, bad, evil, poisson-wifi\n");
    exit(1);
  }

  net::IPAddress remote_ip_address;
  std::string network_type;
  int local_port = atoi(argv[1]);
  int remote_port = 0;

  if (argc == 5) {
    // V2 proxy
    CHECK(remote_ip_address.AssignFromIPLiteral(argv[2]));
    remote_port = atoi(argv[3]);
    network_type = argv[4];
  } else {
    // V1 proxy
    network_type = argv[2];
  }
  if (local_port < 0 || local_port > 65535 || remote_port < 0 ||
      remote_port > 65535) {
    fprintf(stderr, "Port numbers must be between 0 and 65535\n");
    exit(1);
  }
  net::IPEndPoint remote_endpoint(remote_ip_address,
                                  static_cast<uint16_t>(remote_port));
  net::IPEndPoint local_endpoint(net::IPAddress::IPv4AllZeros(),
                                 static_cast<uint16_t>(local_port));
  std::unique_ptr<media::cast::test::PacketPipe> in_pipe, out_pipe;
  std::unique_ptr<media::cast::test::InterruptedPoissonProcess> ipp(
      media::cast::test::DefaultInterruptedPoissonProcess());

  if (network_type == "perfect") {
    // No action needed.
  } else if (network_type == "wifi") {
    in_pipe = media::cast::test::WifiNetwork();
    out_pipe = media::cast::test::WifiNetwork();
  } else if (network_type == "slow") {
    in_pipe = media::cast::test::SlowNetwork();
    out_pipe = media::cast::test::SlowNetwork();
  } else if (network_type == "bad") {
    in_pipe = media::cast::test::BadNetwork();
    out_pipe = media::cast::test::BadNetwork();
  } else if (network_type == "evil") {
    in_pipe = media::cast::test::EvilNetwork();
    out_pipe = media::cast::test::EvilNetwork();
  } else if (network_type == "poisson-wifi") {
    in_pipe = ipp->NewBuffer(128 * 1024);
    out_pipe = ipp->NewBuffer(128 * 1024);
  } else {
    fprintf(stderr, "Unknown network type.\n");
    exit(1);
  }

  GlobalCounter* counter = GetGlobalCounter();

  SetupByteCounters(&in_pipe, &(counter->in_pipe_input_counter),
                    &(counter->in_pipe_output_counter));
  SetupByteCounters(&out_pipe, &(counter->out_pipe_input_counter),
                    &(counter->out_pipe_output_counter));

  printf("Press Ctrl-C when done.\n");
  std::unique_ptr<media::cast::test::UDPProxy> proxy(
      media::cast::test::UDPProxy::Create(local_endpoint, remote_endpoint,
                                          std::move(in_pipe),
                                          std::move(out_pipe), NULL));
  base::SingleThreadTaskExecutor main_task_executor;
  counter->last_printout = base::TimeTicks::Now();
  CheckByteCounters();
  base::RunLoop().Run();
  return 1;
}
