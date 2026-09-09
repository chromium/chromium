// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/tools/fuzzers/mojolpm.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "mojo/public/tools/fuzzers/mojolpm_unittest.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojolpm {
namespace {

class TestcaseUnderTest : public Testcase<unittest::proto::TestProto,
                                          unittest::proto::TestAction,
                                          /*kMaxActionCount=*/3> {
 public:
  using ProtoTestProto = unittest::proto::TestProto;
  using ProtoAction = unittest::proto::TestAction;
  explicit TestcaseUnderTest(const unittest::proto::TestProto& testcase)
      : Testcase<unittest::proto::TestProto,
                 unittest::proto::TestAction,
                 /*kMaxActionCount=*/3>(testcase) {}

  int actions_run() const { return actions_run_; }

  void SetUp(base::OnceClosure done_closure) override {
    std::move(done_closure).Run();
  }

  void TearDown(base::OnceClosure done_closure) override {
    std::move(done_closure).Run();
  }

  void RunAction(const unittest::proto::TestAction&,
                 base::OnceClosure run_closure) override {
    ++actions_run_;
    std::move(run_closure).Run();
  }

 private:
  int actions_run_ = 0;
};

// Checks that the Testcase correctly stops after the maximum number of
// actions has been run. This test is written similar to a MojoLPM fuzzer,
// with the RunAction, TearDown, and SetUp functions overridden to fit the
// needs of this test.

// The task environment and testcase is first initialized.
// The testcase is then run with a RunLoop that is only quit once the
// testcase has finished. RunTestcase configures the SequencedTaskRunner to run
// the fuzzer's SetUp, RunAction, and TearDown functions. On loop completion,
// the number of actions run is then checked against the expected maximum number
// of actions (established by the template).
TEST(MojoLPMTestcaseTest, StopsAtMaximumActionCount) {
  base::test::TaskEnvironment task_environment;

  unittest::proto::TestProto proto;
  proto.add_actions();

  auto* sequence = proto.add_sequences();
  sequence->add_action_indexes(0);
  sequence->add_action_indexes(0);
  sequence->add_action_indexes(0);
  sequence->add_action_indexes(0);
  sequence->add_action_indexes(0);

  proto.add_sequence_indexes(0);
  proto.add_sequence_indexes(0);

  TestcaseUnderTest testcase(proto);

  base::RunLoop run_loop;
  RunTestcase(&testcase, base::SequencedTaskRunner::GetCurrentDefault(),
              run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(testcase.actions_run(), 3);
}

}  // namespace
}  // namespace mojolpm
