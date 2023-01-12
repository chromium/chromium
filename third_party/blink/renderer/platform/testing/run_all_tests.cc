/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include <memory>
#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/task_environment.h"
#include "base/test/test_io_thread.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "gin/array_buffer.h"
#include "gin/public/isolate_holder.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "skia/ext/test_fonts.h"
#endif

namespace {

int runTestSuite(base::TestSuite* testSuite) {
  int result = testSuite->Run();
  {
    base::test::TaskEnvironment task_environment_;
    blink::ThreadState::Current()->CollectAllGarbageForTesting();
  }
  return result;
}

}  // namespace

int main(int argc, char** argv) {
  blink::ScopedUnittestsEnvironmentSetup testEnvironmentSetup(argc, argv);
  int result = 0;

#if BUILDFLAG(IS_FUCHSIA)
  // Some unittests depend on specific fonts provided by the system (e.g. some
  // tests load Arial). On Fuchsia the default font set contains only Roboto.
  // Load //third_party/test_fonts to make these tests pass on Fuchsia.
  skia::InitializeSkFontMgrForTest();
#endif

  {
    base::TestSuite testSuite(argc, argv);
    mojo::core::Init();
    base::TestIOThread testIoThread(base::TestIOThread::kAutoStart);
    mojo::core::ScopedIPCSupport ipcSupport(
        testIoThread.task_runner(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);
    gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                   gin::ArrayBufferAllocator::SharedInstance());
    result = base::LaunchUnitTests(
        argc, argv, base::BindOnce(runTestSuite, base::Unretained(&testSuite)));
  }
  return result;
}
