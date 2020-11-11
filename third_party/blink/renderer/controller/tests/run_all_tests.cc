/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "v8/include/v8.h"

namespace {

class BlinkUnitTestSuite : public base::TestSuite {
 public:
  BlinkUnitTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 private:
  void Initialize() override {
    base::TestSuite::Initialize();

    content::SetUpBlinkTestEnvironment();
  }
  void Shutdown() override {
    // Tickle EndOfTaskRunner which among other things will flush the queue
    // of error messages via V8Initializer::reportRejectedPromisesOnMainThread.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::DoNothing());
    base::RunLoop().RunUntilIdle();

    // Collect garbage (including threadspecific persistent handles) in order
    // to release mock objects referred from v8 or Oilpan heap. Otherwise false
    // mock leaks will be reported.
    blink::ThreadState::Current()->CollectAllGarbageForTesting();

    content::TearDownBlinkTestEnvironment();

    base::TestSuite::Shutdown();
  }

  DISALLOW_COPY_AND_ASSIGN(BlinkUnitTestSuite);
};

}  // namespace

int main(int argc, char** argv) {
  BlinkUnitTestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&BlinkUnitTestSuite::Run, base::Unretained(&test_suite)));
}
