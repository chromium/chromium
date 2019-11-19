// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_STUN_FIELD_TRIAL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_STUN_FIELD_TRIAL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "third_party/blink/renderer/platform/p2p/network_list_manager.h"
#include "third_party/blink/renderer/platform/p2p/network_list_observer.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/webrtc/p2p/stunprober/stun_prober.h"
#include "third_party/webrtc/rtc_base/network.h"
#include "third_party/webrtc/rtc_base/third_party/sigslot/sigslot.h"

namespace rtc {
class PacketSocketFactory;
class SocketAddress;
}  // namespace rtc

namespace blink {

// Wait for 30 seconds to avoid high CPU usage during browser start-up which
// might affect the accuracy of the trial. The trial wakes up the browser every
// 1 ms for no more than 3 seconds to see if time has passed for sending next
// stun probe.
static const int kExperimentStartDelayMs = 30000;

// TODO(crbug.com/787254): Migrate away from std::vector and std::string.
class StunProberTrial : public stunprober::StunProber::Observer,
                        public sigslot::has_slots<> {
 public:
  struct PLATFORM_EXPORT Param {
    Param();
    ~Param();
    int requests_per_ip = 0;
    int interval_ms = 0;
    int shared_socket_mode = 0;
    int batch_size = 0;
    int total_batches = 0;
    std::vector<rtc::SocketAddress> servers;
  };

  PLATFORM_EXPORT StunProberTrial(rtc::NetworkManager* network_manager,
                                  const std::string& params,
                                  rtc::PacketSocketFactory* factory);
  ~StunProberTrial() override;

 private:
  // This will use |factory_| to create sockets, send stun binding requests with
  // various intervals as determined by |params|, observed the success rate and
  // latency of the stun responses and report through UMA.
  void OnNetworksChanged();

  // Parsing function to decode the '/' separated parameter string |params|.
  static PLATFORM_EXPORT bool ParseParameters(const std::string& param_line,
                                              Param* params);

  // stunprober::StunProber::Observer:
  void OnPrepared(stunprober::StunProber* prober,
                  stunprober::StunProber::Status status) override;
  // OnFinished is invoked when the StunProber receives all the responses or
  // times out.
  void OnFinished(stunprober::StunProber* prober,
                  stunprober::StunProber::Status status) override;

  // This will be invoked repeatedly for |total_probers_| times with the
  // interval equal to the estimated run time of a prober.
  void OnTimer();

  void SaveHistogramData();

  rtc::NetworkManager* network_manager_;
  std::string param_line_;
  rtc::PacketSocketFactory* factory_ = nullptr;
  int total_probers_ = 0;
  int batch_size_ = 0;
  int ready_probers_ = 0;
  int started_probers_ = 0;
  int finished_probers_ = 0;
  std::vector<stunprober::StunProber*> probers_;
  THREAD_CHECKER(thread_checker_);

  // The reason we use a timer instead of depending on the OnFinished callback
  // of each prober is that the OnFinished is not fired at the last of STUN
  // request of each prober, instead, it includes a timeout period which waits
  // the server response to come back. Having a timer guarantees the
  // inter-prober intervals is the same as the STUN interval inside a prober.
  base::RepeatingTimer timer_;

  FRIEND_TEST_ALL_PREFIXES(StunProbeTrial, VerifyParameterParsing);
  DISALLOW_COPY_AND_ASSIGN(StunProberTrial);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_STUN_FIELD_TRIAL_H_
