// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/process_stats_sender.h"

#include <stdint.h>

#include <functional>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "remoting/host/process_stats_agent.h"
#include "remoting/proto/process_stats.pb.h"
#include "remoting/protocol/process_stats_stub.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

class FakeProcessStatsStub : public protocol::ProcessStatsStub {
 public:
  FakeProcessStatsStub() = default;
  ~FakeProcessStatsStub() override = default;

  void OnProcessStats(
      const protocol::AggregatedProcessResourceUsage& usage) override {
    received_.push_back(usage);
    DCHECK_LE(received_.size(), expected_usage_count_);
    DCHECK(!quit_closure_.is_null());
    if (received_.size() == expected_usage_count_) {
      quit_closure_.Run();
    }
  }

  const std::vector<protocol::AggregatedProcessResourceUsage>& received()
      const {
    return received_;
  }

  void set_quit_closure(base::Closure quit_closure) {
    quit_closure_ = quit_closure;
  }

  void set_expected_usage_count(size_t expected_usage_count) {
    expected_usage_count_ = expected_usage_count;
  }

 private:
  std::vector<protocol::AggregatedProcessResourceUsage> received_;
  size_t expected_usage_count_ = 0;
  base::Closure quit_closure_;
};

class FakeProcessStatsAgent : public ProcessStatsAgent {
 public:
  FakeProcessStatsAgent() = default;
  ~FakeProcessStatsAgent() override = default;

  protocol::ProcessResourceUsage GetResourceUsage() override {
    protocol::ProcessResourceUsage usage;
    usage.set_process_name("FakeProcessStatsAgent");
    usage.set_processor_usage(index_);
    usage.set_working_set_size(index_);
    usage.set_pagefile_size(index_);
    index_++;
    return usage;
  }

  // Checks the expected usage based on index.
  static void AssertExpected(const protocol::ProcessResourceUsage& usage,
                             size_t index) {
    ASSERT_EQ(usage.process_name(), "FakeProcessStatsAgent");
    ASSERT_EQ(usage.processor_usage(), index);
    ASSERT_EQ(usage.working_set_size(), index);
    ASSERT_EQ(usage.pagefile_size(), index);
  }

  static void AssertExpected(
      const protocol::AggregatedProcessResourceUsage& usage,
      size_t index) {
    ASSERT_EQ(usage.usages_size(), 1);
    AssertExpected(usage.usages().Get(0), index);
  }

  size_t issued_times() const { return index_; }

 private:
  size_t index_ = 0;
};

}  // namespace

TEST(ProcessStatsSenderTest, ReportUsage) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;
  FakeProcessStatsStub stub;
  std::unique_ptr<ProcessStatsSender> stats;
  FakeProcessStatsAgent agent;

  stub.set_expected_usage_count(10);
  stub.set_quit_closure(base::Bind(
      [](std::unique_ptr<ProcessStatsSender>* stats,
         const FakeProcessStatsStub& stub, const FakeProcessStatsAgent& agent,
         base::RunLoop* run_loop) -> void {
        ASSERT_EQ(stub.received().size(), agent.issued_times());
        stats->reset();
        run_loop->Quit();
      },
      base::Unretained(&stats), std::cref(stub), std::cref(agent),
      base::Unretained(&run_loop)));
  task_environment.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<ProcessStatsSender>* stats,
             FakeProcessStatsStub* stub, FakeProcessStatsAgent* agent) -> void {
            stats->reset(new ProcessStatsSender(
                stub, base::TimeDelta::FromMilliseconds(1), { agent }));
          },
          base::Unretained(&stats), base::Unretained(&stub),
          base::Unretained(&agent)));
  run_loop.Run();

  ASSERT_EQ(stub.received().size(), 10U);
  for (size_t i = 0; i < stub.received().size(); i++) {
    FakeProcessStatsAgent::AssertExpected(stub.received()[i], i);
  }
}

TEST(ProcessStatsSenderTest, MergeUsage) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;
  FakeProcessStatsStub stub;
  std::unique_ptr<ProcessStatsSender> stats;
  // Owned by |stats|.
  FakeProcessStatsAgent agent1;
  FakeProcessStatsAgent agent2;

  stub.set_expected_usage_count(10);
  stub.set_quit_closure(base::Bind(
      [](std::unique_ptr<ProcessStatsSender>* stats,
         const FakeProcessStatsStub& stub, const FakeProcessStatsAgent& agent1,
         const FakeProcessStatsAgent& agent2, base::RunLoop* run_loop) -> void {
        ASSERT_EQ(stub.received().size(), agent1.issued_times());
        ASSERT_EQ(stub.received().size(), agent2.issued_times());
        stats->reset();
        run_loop->Quit();
      },
      base::Unretained(&stats), std::cref(stub), std::cref(agent1),
      std::cref(agent2), base::Unretained(&run_loop)));
  task_environment.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<ProcessStatsSender>* stats,
             FakeProcessStatsStub* stub, FakeProcessStatsAgent* agent1,
             FakeProcessStatsAgent* agent2) -> void {
            stats->reset(new ProcessStatsSender(
                stub, base::TimeDelta::FromMilliseconds(1),
                { agent1, agent2 } ));
          },
          base::Unretained(&stats), base::Unretained(&stub),
          base::Unretained(&agent1), base::Unretained(&agent2)));
  run_loop.Run();

  ASSERT_EQ(stub.received().size(), 10U);
  for (size_t i = 0; i < stub.received().size(); i++) {
    ASSERT_EQ(stub.received()[i].usages_size(), 2);
    for (int j = 0; j < stub.received()[i].usages_size(); j++) {
      FakeProcessStatsAgent::AssertExpected(
          stub.received()[i].usages().Get(j), i);
    }
  }
}

}  // namespace remoting
