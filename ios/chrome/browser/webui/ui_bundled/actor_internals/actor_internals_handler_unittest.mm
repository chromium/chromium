// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/actor_internals/actor_internals_handler.h"

#import <memory>

#import "base/base_paths.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/location.h"
#import "base/path_service.h"
#import "base/run_loop.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/scoped_path_override.h"
#import "base/test/task_environment.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/public/mojom/actor_internals.mojom.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "mojo/public/cpp/bindings/pending_remote.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Worst-case async chain depth across the thread pool and main sequence:
// failed `Init` -> `TraceFileInitDone` -> `Shutdown` -> `PostDeleteTraceFile`.
constexpr int kMaxAsyncTaskWaves = 3;

constexpr base::FilePath::CharType kMissingDirName[] =
    FILE_PATH_LITERAL("Missing");

}  // namespace

class ActorInternalsHandlerTest : public PlatformTest {
 protected:
  // Creates a handler for `journal_` without a web state. The page remote is
  // connected to `page_receiver_` so that journal entries forwarded to the
  // page are queued instead of hitting an unbound remote.
  std::unique_ptr<ActorInternalsHandler> CreateHandler() {
    mojo::PendingRemote<actor_internals::mojom::Page> page;
    page_receiver_ = page.InitWithNewPipeAndPassReceiver();
    return std::make_unique<ActorInternalsHandler>(
        /*web_state=*/nullptr, journal_.get(), std::move(page),
        mojo::PendingReceiver<actor_internals::mojom::PageHandler>());
  }

  // Drains queued tasks across the thread pool and the main sequence by
  // alternating `FlushForTesting()` with an explicit `QuitClosure()` barrier.
  void FlushAsyncTasks() {
    for (int wave = 0; wave < kMaxAsyncTaskWaves; ++wave) {
      base::ThreadPoolInstance::Get()->FlushForTesting();
      base::RunLoop run_loop;
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, run_loop.QuitClosure());
      run_loop.Run();
    }
  }

  // Starts logging and returns whether a trace file was created.
  [[nodiscard]] bool StartLoggingAndWaitForTraceFile(
      ActorInternalsHandler& handler) {
    handler.StartLogging();
    FlushAsyncTasks();
    return !TempDirIsEmpty();
  }

  base::FilePath TempDir() const {
    return base::PathService::CheckedGet(base::DIR_TEMP);
  }

  bool TempDirIsEmpty() const { return base::IsDirectoryEmpty(TempDir()); }

  base::test::TaskEnvironment task_environment_;
  // Redirects `DIR_TEMP`, where trace files are created, to a scoped temp dir.
  base::ScopedPathOverride temp_dir_override_{base::DIR_TEMP};
  mojo::PendingReceiver<actor_internals::mojom::Page> page_receiver_;
  std::unique_ptr<actor::AggregatedJournal> journal_ =
      std::make_unique<actor::AggregatedJournal>();
};

// Tests that destroying the handler while logging, followed by the journal
// before any posted task runs (as happens at app termination), does not crash
// and deletes the trace file.
TEST_F(ActorInternalsHandlerTest, DestroyedWhileLoggingBeforeJournal) {
  std::unique_ptr<ActorInternalsHandler> handler = CreateHandler();
  ASSERT_TRUE(StartLoggingAndWaitForTraceFile(*handler));

  handler.reset();
  journal_.reset();
  FlushAsyncTasks();

  EXPECT_TRUE(TempDirIsEmpty());
}

// Tests that destroying the handler and the journal before the trace file has
// been created on the writer sequence does not crash and deletes the file once
// created.
TEST_F(ActorInternalsHandlerTest, DestroyedBeforeTraceFileCreated) {
  std::unique_ptr<ActorInternalsHandler> handler = CreateHandler();
  handler->StartLogging();

  handler.reset();
  journal_.reset();
  // Flush the writer sequence first: `Init()` creates the file and `Shutdown()`
  // closes it, while the deletion reply is still queued on the main sequence.
  base::ThreadPoolInstance::Get()->FlushForTesting();
  ASSERT_FALSE(TempDirIsEmpty());

  FlushAsyncTasks();
  EXPECT_TRUE(TempDirIsEmpty());
}

// Tests that stopping logging and then destroying the handler and the journal
// before any posted task runs does not crash and deletes the trace file.
TEST_F(ActorInternalsHandlerTest, StopLoggingThenDestroyedBeforeJournal) {
  std::unique_ptr<ActorInternalsHandler> handler = CreateHandler();
  ASSERT_TRUE(StartLoggingAndWaitForTraceFile(*handler));

  handler->StopLogging();
  handler.reset();
  journal_.reset();
  FlushAsyncTasks();

  EXPECT_TRUE(TempDirIsEmpty());
}

// Tests that stopping logging when the share sheet cannot be presented (no
// window) deletes the trace file.
TEST_F(ActorInternalsHandlerTest, StopLoggingWithoutWindowDeletesTraceFile) {
  std::unique_ptr<ActorInternalsHandler> handler = CreateHandler();
  ASSERT_TRUE(StartLoggingAndWaitForTraceFile(*handler));

  handler->StopLogging();
  FlushAsyncTasks();

  EXPECT_TRUE(TempDirIsEmpty());
}

// Tests that a failure to create the trace file releases the serializer so
// that logging can be started again.
TEST_F(ActorInternalsHandlerTest, TraceFileCreationFailureAllowsRestart) {
  std::unique_ptr<ActorInternalsHandler> handler = CreateHandler();
  {
    // Creating a file in a missing directory fails.
    base::ScopedPathOverride missing_dir_override(
        base::DIR_TEMP, TempDir().Append(kMissingDirName),
        /*is_absolute=*/true, /*create=*/false);
    handler->StartLogging();
    FlushAsyncTasks();
  }

  EXPECT_TRUE(StartLoggingAndWaitForTraceFile(*handler));
}
