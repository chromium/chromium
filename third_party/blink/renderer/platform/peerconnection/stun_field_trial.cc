// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/stun_field_trial.h"

#include <math.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "third_party/webrtc/api/packet_socket_factory.h"
#include "third_party/webrtc/rtc_base/async_packet_socket.h"
#include "third_party/webrtc/rtc_base/async_resolver_interface.h"
#include "third_party/webrtc/rtc_base/ip_address.h"
#include "third_party/webrtc/rtc_base/network.h"
#include "third_party/webrtc/rtc_base/socket_address.h"
#include "third_party/webrtc/rtc_base/thread.h"

using stunprober::StunProber;

namespace blink {

namespace {

// This needs to be the same as NatTypeCounters in histograms.xml.
enum NatType {
  NAT_TYPE_NONE,
  NAT_TYPE_UNKNOWN,
  NAT_TYPE_SYMMETRIC,
  NAT_TYPE_NON_SYMMETRIC,
  NAT_TYPE_MAX
};

// This needs to match "NatType" in histograms.xml.
const char* const NatTypeNames[] = {"NoNAT", "UnknownNAT", "SymNAT",
                                    "NonSymNAT"};
static_assert(base::size(NatTypeNames) == NAT_TYPE_MAX,
              "NatType enums must match names");

NatType GetNatType(stunprober::NatType nat_type) {
  switch (nat_type) {
    case stunprober::NATTYPE_NONE:
      return NAT_TYPE_NONE;
    case stunprober::NATTYPE_UNKNOWN:
      return NAT_TYPE_UNKNOWN;
    case stunprober::NATTYPE_SYMMETRIC:
      return NAT_TYPE_SYMMETRIC;
    case stunprober::NATTYPE_NON_SYMMETRIC:
      return NAT_TYPE_NON_SYMMETRIC;
    default:
      return NAT_TYPE_MAX;
  }
}

std::string HistogramName(const std::string& prefix,
                          NatType nat_type,
                          int interval_ms,
                          int batch_index) {
  return base::StringPrintf("WebRTC.Stun.%s.%s.%dms.%d", prefix.c_str(),
                            NatTypeNames[nat_type], interval_ms, batch_index);
}

}  // namespace

StunProberTrial::Param::Param() {}

StunProberTrial::Param::~Param() {}

StunProberTrial::StunProberTrial(rtc::NetworkManager* network_manager,
                                 const std::string& params,
                                 rtc::PacketSocketFactory* factory)
    : network_manager_(network_manager),
      param_line_(params),
      factory_(factory) {
  // We have to connect to the signal to avoid a race condition if network
  // manager hasn't received the network update when we start, the StunProber
  // will just fail.
  network_manager_->SignalNetworksChanged.connect(
      this, &StunProberTrial::OnNetworksChanged);
  network_manager_->StartUpdating();
}

StunProberTrial::~StunProberTrial() {}

void StunProberTrial::SaveHistogramData() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NatType nat_type = NAT_TYPE_UNKNOWN;
  int interval_ms = 0;
  int count = 0;
  int total_requests_sent = 0;
  int total_responses_received = 0;
  for (auto* prober : probers_) {
    ++count;

    // Get the stats.
    StunProber::Stats stats;
    if (!prober->GetStats(&stats))
      return;

    // Check if the NAT type is consistent.
    if (nat_type == NAT_TYPE_UNKNOWN) {
      nat_type = GetNatType(stats.nat_type);
    } else {
      NatType type = GetNatType(stats.nat_type);
      // For subsequent probers, we might get unknown as nattype if all the
      // bindings fail, but it's ok.
      if (nat_type != type && type != NAT_TYPE_UNKNOWN)
        return;
    }

    // Check that the interval is consistent. Use the real probe interval for
    // reporting, converting from nanosecond to millisecond.
    int new_interval_ms =
        round(static_cast<float>(stats.actual_request_interval_ns) / 1000);
    if (interval_ms == 0) {
      interval_ms = new_interval_ms;
    } else if (abs(interval_ms - new_interval_ms) > 3) {
      DVLOG(1) << "current interval: " << new_interval_ms
               << " is too far off from previous one: " << interval_ms;
      continue;
    }

    // Sum up the total sent and recv packets.
    total_requests_sent += stats.raw_num_request_sent;
    total_responses_received += stats.num_response_received;

    if (count % batch_size_ > 0)
      continue;

    // If 50% of probers are not counted, ignore this batch.
    // |raw_num_request_sent| should be the same for each prober.
    if (total_requests_sent < (stats.raw_num_request_sent * batch_size_ / 2)) {
      total_responses_received = 0;
      total_requests_sent = 0;
      continue;
    }

    int success_rate = total_responses_received * 100 / total_requests_sent;
    // Use target_request_interval_ns for naming of UMA to avoid inconsistency.
    std::string histogram_name = HistogramName(
        "BatchSuccessPercent", nat_type,
        stats.target_request_interval_ns / 1000, count / batch_size_);

    // Mimic the same behavior as UMA_HISTOGRAM_PERCENTAGE. We can't use
    // that macro as the histogram name is determined dynamically.
    base::HistogramBase* histogram =
        base::Histogram::FactoryGet(histogram_name, 1, 101, 102,
                                    base::Histogram::kUmaTargetedHistogramFlag);
    histogram->Add(success_rate);

    DVLOG(1) << "Histogram '" << histogram_name.c_str()
             << "' = " << success_rate;

    DVLOG(1) << "Shared Socket Mode: " << stats.shared_socket_mode;
    DVLOG(1) << "Requests sent: " << total_requests_sent;
    DVLOG(1) << "Responses received: " << total_responses_received;
    DVLOG(1) << "Target interval (ns): " << stats.target_request_interval_ns;
    DVLOG(1) << "Actual interval (ns): " << stats.actual_request_interval_ns;
    DVLOG(1) << "NAT Type: " << NatTypeNames[nat_type];
    DVLOG(1) << "Host IP: " << stats.host_ip;

    total_requests_sent = 0;
    total_responses_received = 0;
  }
}

bool StunProberTrial::ParseParameters(const std::string& param_line,
                                      StunProberTrial::Param* params) {
  std::vector<std::string> stun_params = base::SplitString(
      param_line, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (stun_params.size() < 5) {
    DLOG(ERROR) << "Not enough parameters specified in StartStunProbeTrial";
    return false;
  }
  auto param = stun_params.begin();

  if (param->empty()) {
    params->requests_per_ip = 10;
  } else if (!base::StringToInt(*param, &params->requests_per_ip)) {
    DLOG(ERROR) << "Failed to parse request_per_ip in StartStunProbeTrial";
    return false;
  }
  param++;

  // Set inter-probe interval randomly from 0, 5, 10, ... 50, 100 ms.
  if ((*param).empty()) {
    params->interval_ms = base::RandInt(0, 11) * 5;
  } else if (!base::StringToInt(*param, &params->interval_ms)) {
    DLOG(ERROR) << "Failed to parse interval in StartStunProbeTrial";
    return false;
  }
  param++;

  if ((*param).empty()) {
    params->shared_socket_mode = base::RandInt(0, 1);
  } else if (!base::StringToInt(*param, &params->shared_socket_mode)) {
    DLOG(ERROR) << "Failed to parse shared_socket_mode in StartStunProbeTrial";
    return false;
  }
  param++;

  if (param->empty()) {
    params->batch_size = 5;
  } else if (!base::StringToInt(*param, &params->batch_size)) {
    DLOG(ERROR) << "Failed to parse batch_size in StartStunProbeTrial";
    return false;
  }
  param++;

  if (param->empty()) {
    params->total_batches = 5;
  } else if (!base::StringToInt(*param, &params->total_batches)) {
    DLOG(ERROR) << "Failed to parse total_batches in StartStunProbeTrial";
    return false;
  }
  param++;

  while (param != stun_params.end() && !param->empty()) {
    rtc::SocketAddress server;
    if (!server.FromString(*param)) {
      DLOG(ERROR) << "Failed to parse address in StartStunProbeTrial";
      return false;
    }
    params->servers.push_back(server);
    param++;
  }

  return !params->servers.empty();
}

void StunProberTrial::OnNetworksChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << "Starting stun trial with params: " << param_line_;
  rtc::NetworkManager::NetworkList networks;
  network_manager_->GetNetworks(&networks);

  // If we don't have local addresses, we won't be able to determine whether
  // we're behind NAT or not.
  if (networks.empty()) {
    DLOG(ERROR) << "No networks specified in StartStunProbeTrial";
    return;
  }

  network_manager_->StopUpdating();
  network_manager_->SignalNetworksChanged.disconnect(this);

  StunProberTrial::Param params;
  if (!ParseParameters(param_line_, &params)) {
    return;
  }

  batch_size_ = params.batch_size;
  total_probers_ = params.total_batches * batch_size_;

  for (int i = 0; i < total_probers_; i++) {
    std::unique_ptr<StunProber> prober(
        new StunProber(factory_, rtc::Thread::Current(), networks));
    if (!prober->Prepare(params.servers, (params.shared_socket_mode != 0),
                         params.interval_ms, params.requests_per_ip, 1000,
                         this)) {
      DLOG(ERROR) << "Failed to Prepare in StartStunProbeTrial";
      return;
    }

    probers_.push_back(prober.release());
  }
}

void StunProberTrial::OnFinished(StunProber* prober,
                                 StunProber::Status result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (result == StunProber::SUCCESS)
    ++finished_probers_;

  if (finished_probers_ == total_probers_)
    SaveHistogramData();
}

void StunProberTrial::OnPrepared(StunProber* prober,
                                 StunProber::Status result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (result == StunProber::SUCCESS)
    ++ready_probers_;

  if (ready_probers_ == total_probers_) {
    // TODO(guoweis) estimated_execution_time() is the same for all probers. It
    // could be moved up to the StunProberTrial class once the DNS resolution
    // part is moved up too.
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMilliseconds(
                     probers_.front()->estimated_execution_time()),
                 this, &StunProberTrial::OnTimer);
  }
}

void StunProberTrial::OnTimer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  probers_[started_probers_]->Start(this);
  started_probers_++;

  if (started_probers_ == total_probers_)
    timer_.Stop();
}

}  // namespace blink
