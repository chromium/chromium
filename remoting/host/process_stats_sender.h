// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PROCESS_STATS_SENDER_H_
#define REMOTING_HOST_PROCESS_STATS_SENDER_H_

#include <initializer_list>
#include <vector>

#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/proto/process_stats.pb.h"
#include "remoting/protocol/process_stats_stub.h"

namespace remoting {

class ProcessStatsAgent;

// A component to report process statistic data regularly, it starts immediately
// after construction, and stops after destruction.
// All public functions, including constructor and destructor of the object of
// ProcessStatsSender need to be executed in a same thread.
class ProcessStatsSender final {
 public:
  // ProcessStatsSender reports statistic data to |host_stats_stub| once per
  // |interval|.
  // ProcessStatsSender does not take the ownership of both |host_stats_stub|
  // and |agents|. They must outlive the ProcessStatsSender object.
  ProcessStatsSender(protocol::ProcessStatsStub* host_stats_stub,
                     base::TimeDelta interval,
                     std::initializer_list<ProcessStatsAgent*> agents);

  ~ProcessStatsSender();

  base::TimeDelta interval() const;

 private:
  void ReportUsage();

  protocol::ProcessStatsStub* const host_stats_stub_;
  std::vector<ProcessStatsAgent*> agents_;
  const base::TimeDelta interval_;
  base::RepeatingTimer timer_;
  const base::ThreadChecker thread_checker_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_PROCESS_STATS_SENDER_H_
