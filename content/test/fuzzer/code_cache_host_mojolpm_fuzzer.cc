// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/icu_util.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"  // [nogncheck]
#include "content/browser/cache_storage/cache_storage_control_wrapper.h"  // [nogncheck]
#include "content/browser/code_cache/generated_code_cache_context.h"  // [nogncheck]
#include "content/browser/network_service_instance_impl.h"       // [nogncheck]
#include "content/browser/renderer_host/code_cache_host_impl.h"  // [nogncheck]
#include "content/browser/storage_partition_impl.h"              // [nogncheck]
#include "content/browser/storage_partition_impl_map.h"          // [nogncheck]
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/test/fuzzer/code_cache_host_mojolpm_fuzzer.pb.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-mojolpm.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"
#include "url/origin.h"

using url::Origin;

const char* const kCmdline[] = {"code_cache_host_mojolpm_fuzzer", nullptr};

// Global environment needed to run the interface being tested.
//
// This will be created once, before fuzzing starts, and will be shared between
// all testcases. It is created on the main thread.
//
// At a minimum, we should always be able to set up the command line, i18n and
// mojo, and create the thread on which the fuzzer will be run. We want to avoid
// (as much as is reasonable) any state being preserved between testcases.
//
// For CodeCacheHost, we can also safely re-use a single BrowserTaskEnvironment
// and the TestContentClientInitializer between testcases. We try to create an
// environment that matches the real browser process as much as possible, so we
// use real platform threads in the task environment.
class ContentFuzzerEnvironment {
 public:
  ContentFuzzerEnvironment()
      : fuzzer_thread_("fuzzer_thread"),
        task_environment_(
            (base::CommandLine::Init(1, kCmdline),
             TestTimeouts::Initialize(),
             base::test::TaskEnvironment::MainThreadType::DEFAULT),
            base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC,
            base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
            content::BrowserTaskEnvironment::REAL_IO_THREAD) {
    logging::SetMinLogLevel(logging::LOG_FATAL);
    mojo::core::Init();
    base::i18n::InitializeICU();

    content::ForceCreateNetworkServiceDirectlyForTesting();
    content::StoragePartitionImpl::ForceInProcessStorageServiceForTesting();

    fuzzer_thread_.StartAndWaitForTesting();
  }

  scoped_refptr<base::SequencedTaskRunner> fuzzer_task_runner() {
    return fuzzer_thread_.task_runner();
  }

 private:
  base::AtExitManager at_exit_manager_;
  base::Thread fuzzer_thread_;
  content::BrowserTaskEnvironment task_environment_;
  content::TestContentClientInitializer content_client_initializer_;
  mojo::internal::ScopedSuppressValidationErrorLoggingForTests
      validation_error_suppressor_;
  mojo::internal::SerializationWarningObserverForTesting
      serialization_error_suppressor_;
};

ContentFuzzerEnvironment& GetEnvironment() {
  static base::NoDestructor<ContentFuzzerEnvironment> environment;
  return *environment;
}

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() {
  return GetEnvironment().fuzzer_task_runner();
}

// Per-testcase state needed to run the interface being tested.
//
// The lifetime of this is scoped to a single testcase, and it is created and
// destroyed from the fuzzer sequence.
//
// For CodeCacheHost, this needs the basic common Browser process state provided
// by TestBrowserContext, and to set up the cache storage that will provide the
// storage backing for the code cache.
//
// Since the Browser process will host one CodeCacheHostImpl per
// RenderProcessHost, we emulate this by allowing the fuzzer to create (and
// destroy) multiple CodeCacheHostImpl instances.
class CodeCacheHostTestcase {
 public:
  explicit CodeCacheHostTestcase(
      const content::fuzzing::code_cache_host::proto::Testcase& testcase);

  // The three functions below are the public api for the testcase.
  //
  // Each function takes a single argument, which is a closure to be invoked
  // once the operation is complete.

  // SetUp will be invoked prior to the first fuzzer actions running; and once
  // it has completed, all per-testcase fuzzer state should be ready.
  //
  // It can be assumed that the normal Browser process task environment is
  // running, so tasks can be posted to the UI/IO thread.
  //
  // Once setup is complete, `done_closure` should be invoked on the fuzzer
  // sequence.
  //
  // This should only be called from the fuzzer sequence.
  void SetUp(base::OnceClosure done_closure);

  // While there are still actions remaining in the testcase, this will perform
  // the next action, and then queue itself to run again. When the testcase is
  // finished, this will invoke `done_closure` on the fuzzer sequence.
  //
  // This should only be called from the fuzzer sequence.
  void Run(base::OnceClosure done_closure);

  // TearDown will be invoked after the last fuzzer action has run; and once it
  // has completed all per-testcase fuzzer state should have been destroyed (and
  // the process in a state that would permit a new testcase to start).
  //
  // Once teardown is complete, `done_closure` should be invoked on the fuzzer
  // sequence.
  //
  // This should only be called from the fuzzer sequence.
  void TearDown(base::OnceClosure done_closure);

 private:
  using Action = content::fuzzing::code_cache_host::proto::Action;

  void SetUpOnUIThread(base::OnceClosure done_closure);
  void SetUpOnFuzzerThread(base::OnceClosure done_closure);

  bool IsFinished();
  void RunAction(const content::fuzzing::code_cache_host::proto::Action& action,
                 base::OnceClosure done_closure);

  void TearDownOnUIThread(base::OnceClosure done_closure);
  void TearDownOnFuzzerThread(base::OnceClosure done_closure);

  // Used by AddCodeCacheHost to create and bind CodeCacheHostImpl on the code
  // cache thread.
  void AddCodeCacheHostImpl(
      uint32_t id,
      int renderer_id,
      const Origin& origin,
      const net::NetworkIsolationKey& key,
      mojo::PendingReceiver<::blink::mojom::CodeCacheHost>&& receiver);

  // Create and bind a new instance for fuzzing. This ensures that the new
  // instance has been created and bound on the correct sequence before invoking
  // `done_closure`.
  void AddCodeCacheHost(
      uint32_t id,
      int renderer_id,
      content::fuzzing::code_cache_host::proto::NewCodeCacheHostAction::OriginId
          origin_id,
      base::OnceClosure done_closure);

  // The proto message describing the test actions to perform.
  const content::fuzzing::code_cache_host::proto::Testcase& testcase_;

  // This set of origins should cover all of the origin types which have special
  // handling in CodeCacheHostImpl, and give us two distinct "normal" origins,
  // which should be enough to exercise all of the code.
  const Origin origin_a_;
  const Origin origin_b_;
  const Origin origin_opaque_;
  const Origin origin_empty_;

  // Apply a reasonable upper-bound on testcase complexity to avoid timeouts.
  const int max_action_count_ = 512;

  // Apply a reasonable upper-bound on maximum size of action that we will
  // deserialize. (This is deliberately slightly larger than max mojo message
  // size)
  const size_t max_action_size_ = 300 * 1024 * 1024;

  // Count of total actions performed in this testcase.
  int action_count_ = 0;

  // The index of the next sequence of actions to execute.
  int next_sequence_idx_ = 0;
  int next_action_idx_ = 0;

  // Prerequisite state.
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  std::unique_ptr<content::CacheStorageControlWrapper>
      cache_storage_control_wrapper_;
  scoped_refptr<content::GeneratedCodeCacheContext>
      generated_code_cache_context_;

  // Mapping from renderer id to CodeCacheHostImpl instances being fuzzed.
  // Access only from UI thread.
  using UniqueCodeCacheReceiverSet =
      std::unique_ptr<mojo::UniqueReceiverSet<blink::mojom::CodeCacheHost>,
                      base::OnTaskRunnerDeleter>;
  std::map<int, UniqueCodeCacheReceiverSet> code_cache_host_receivers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

CodeCacheHostTestcase::CodeCacheHostTestcase(
    const content::fuzzing::code_cache_host::proto::Testcase& testcase)
    : testcase_(testcase),
      origin_a_(url::Origin::Create(GURL("http://aaa.com/"))),
      origin_b_(url::Origin::Create(GURL("http://bbb.com/"))),
      origin_opaque_(url::Origin::Create(GURL("opaque"))),
      origin_empty_(url::Origin::Create(GURL("file://this_becomes_empty"))) {
  // CodeCacheHostTestcase is created on the main thread, but the actions that
  // we want to validate the sequencing of take place on the fuzzer sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void CodeCacheHostTestcase::SetUp(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CodeCacheHostTestcase::SetUpOnUIThread,
                     base::Unretained(this), std::move(done_closure)));
}

void CodeCacheHostTestcase::SetUpOnUIThread(base::OnceClosure done_closure) {
  browser_context_ = std::make_unique<content::TestBrowserContext>();

  cache_storage_control_wrapper_ =
      std::make_unique<content::CacheStorageControlWrapper>(
          content::GetIOThreadTaskRunner({}), browser_context_->GetPath(),
          browser_context_->GetSpecialStoragePolicy(),
          browser_context_->GetDefaultStoragePartition()
              ->GetQuotaManager()
              ->proxy(),
          content::ChromeBlobStorageContext::GetRemoteFor(
              browser_context_.get()));

  generated_code_cache_context_ =
      base::MakeRefCounted<content::GeneratedCodeCacheContext>();
  generated_code_cache_context_->Initialize(browser_context_->GetPath(), 65536);

  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CodeCacheHostTestcase::SetUpOnFuzzerThread,
                     base::Unretained(this), std::move(done_closure)));
}

void CodeCacheHostTestcase::SetUpOnFuzzerThread(
    base::OnceClosure done_closure) {
  mojolpm::GetContext()->StartTestcase();

  std::move(done_closure).Run();
}

void CodeCacheHostTestcase::TearDown(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CodeCacheHostTestcase::TearDownOnUIThread,
                     base::Unretained(this), std::move(done_closure)));
}

void CodeCacheHostTestcase::TearDownOnUIThread(base::OnceClosure done_closure) {
  code_cache_host_receivers_.clear();
  generated_code_cache_context_.reset();
  cache_storage_control_wrapper_.reset();
  browser_context_.reset();

  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CodeCacheHostTestcase::TearDownOnFuzzerThread,
                     base::Unretained(this), std::move(done_closure)));
}

void CodeCacheHostTestcase::TearDownOnFuzzerThread(
    base::OnceClosure done_closure) {
  mojolpm::GetContext()->EndTestcase();

  std::move(done_closure).Run();
}

bool CodeCacheHostTestcase::IsFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!testcase_.actions_size()) {
    return true;
  }

  if (next_sequence_idx_ >= testcase_.sequence_indexes_size()) {
    return true;
  }

  if (action_count_ >= max_action_count_) {
    return true;
  }

  return false;
}

void CodeCacheHostTestcase::RunAction(
    const content::fuzzing::code_cache_host::proto::Action& action,
    base::OnceClosure run_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto ThreadId_UI =
      content::fuzzing::code_cache_host::proto::RunThreadAction_ThreadId_UI;
  const auto ThreadId_IO =
      content::fuzzing::code_cache_host::proto::RunThreadAction_ThreadId_IO;

  if (action.ByteSizeLong() <= max_action_size_) {
    switch (action.action_case()) {
      case Action::kNewCodeCacheHost:
        AddCodeCacheHost(action.new_code_cache_host().id(),
                         action.new_code_cache_host().render_process_id(),
                         action.new_code_cache_host().origin_id(),
                         std::move(run_closure));
        return;

      case Action::kRunThread:
        // These actions ensure that any tasks currently queued on the named
        // thread have chance to run before the fuzzer continues.
        //
        // We don't provide any particular guarantees here; this does not mean
        // that the named thread is idle, nor does it prevent any other threads
        // from running (or the consequences of any resulting callbacks, for
        // example).
        if (action.run_thread().id() == ThreadId_UI) {
          content::GetUIThreadTaskRunner({})->PostTaskAndReply(
              FROM_HERE, base::DoNothing(), std::move(run_closure));
        } else if (action.run_thread().id() == ThreadId_IO) {
          content::GetIOThreadTaskRunner({})->PostTaskAndReply(
              FROM_HERE, base::DoNothing(), std::move(run_closure));
        }
        return;

      case Action::kCodeCacheHostRemoteAction:
        mojolpm::HandleRemoteAction(action.code_cache_host_remote_action());
        break;

      case content::fuzzing::code_cache_host::proto::Action::ACTION_NOT_SET:
        break;
    }
  }

  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(run_closure));
}

void CodeCacheHostTestcase::Run(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsFinished()) {
    std::move(done_closure).Run();

    // Explicit return early here, since `this` will be invalidated as soon as
    // `done_closure` is invoked.
    return;
  } else {
    // Bind a closure to continue the fuzzing. This must be called in every path
    // through this block, otherwise fuzzing will hang. Unretained is safe since
    // `this` will only be destroyed after `done_closure` is called.
    auto run_closure =
        base::BindOnce(&CodeCacheHostTestcase::Run, base::Unretained(this),
                       std::move(done_closure));

    auto sequence_idx = testcase_.sequence_indexes(next_sequence_idx_);
    const auto& sequence =
        testcase_.sequences(sequence_idx % testcase_.sequences_size());
    if (next_action_idx_ < sequence.action_indexes_size()) {
      auto action_idx = sequence.action_indexes(next_action_idx_++);
      const auto& action =
          testcase_.actions(action_idx % testcase_.actions_size());
      RunAction(action, std::move(run_closure));
    } else {
      next_sequence_idx_++;
      GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(run_closure));
    }
  }
}

void CodeCacheHostTestcase::AddCodeCacheHostImpl(
    uint32_t id,
    int renderer_id,
    const Origin& origin,
    const net::NetworkIsolationKey& nik,
    mojo::PendingReceiver<::blink::mojom::CodeCacheHost>&& receiver) {
  auto code_cache_host = std::make_unique<content::CodeCacheHostImpl>(
      renderer_id, generated_code_cache_context_, nik);
  code_cache_host->SetCacheStorageControlForTesting(
      cache_storage_control_wrapper_.get());
  UniqueCodeCacheReceiverSet receivers(
      new mojo::UniqueReceiverSet<blink::mojom::CodeCacheHost>(),
      base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
  receivers->Add(std::move(code_cache_host), std::move(receiver));
  code_cache_host_receivers_.insert({renderer_id, std::move(receivers)});
}

static void AddCodeCacheHostInstance(
    uint32_t id,
    mojo::Remote<::blink::mojom::CodeCacheHost> remote,
    base::OnceClosure run_closure) {
  mojolpm::GetContext()->AddInstance(id, std::move(remote));

  std::move(run_closure).Run();
}

void CodeCacheHostTestcase::AddCodeCacheHost(
    uint32_t id,
    int renderer_id,
    content::fuzzing::code_cache_host::proto::NewCodeCacheHostAction::OriginId
        origin_id,
    base::OnceClosure run_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::Remote<::blink::mojom::CodeCacheHost> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();

  const auto OriginId_B = content::fuzzing::code_cache_host::proto::
      NewCodeCacheHostAction_OriginId_ORIGIN_B;
  const auto OriginId_OPAQUE = content::fuzzing::code_cache_host::proto::
      NewCodeCacheHostAction_OriginId_ORIGIN_OPAQUE;
  const auto OriginId_EMPTY = content::fuzzing::code_cache_host::proto::
      NewCodeCacheHostAction_OriginId_ORIGIN_EMPTY;

  const Origin* origin = &origin_a_;
  if (origin_id == OriginId_B) {
    origin = &origin_b_;
  } else if (origin_id == OriginId_OPAQUE) {
    origin = &origin_opaque_;
  } else if (origin_id == OriginId_EMPTY) {
    origin = &origin_empty_;
  }

  // Use of Unretained is safe since `this` is guaranteed to live at least until
  // `run_closure` is invoked.
  content::GeneratedCodeCacheContext::GetTaskRunner(
      generated_code_cache_context_)
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&CodeCacheHostTestcase::AddCodeCacheHostImpl,
                         base::Unretained(this), id, renderer_id, *origin,
                         net::NetworkIsolationKey(), std::move(receiver)),
          base::BindOnce(AddCodeCacheHostInstance, id, std::move(remote),
                         std::move(run_closure)));
}

// Helper function to setup and run the testcase, since we need to do that from
// the fuzzer sequence rather than the main thread.
void RunTestcase(CodeCacheHostTestcase* testcase,
                 base::OnceClosure done_closure) {
  auto teardown =
      base::BindOnce(&CodeCacheHostTestcase::TearDown,
                     base::Unretained(testcase), std::move(done_closure));

  auto start_fuzzing =
      base::BindOnce(&CodeCacheHostTestcase::Run, base::Unretained(testcase),
                     std::move(teardown));

  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CodeCacheHostTestcase::SetUp, base::Unretained(testcase),
                     std::move(start_fuzzing)));
}

DEFINE_BINARY_PROTO_FUZZER(
    const content::fuzzing::code_cache_host::proto::Testcase& proto_testcase) {
  if (!proto_testcase.actions_size() || !proto_testcase.sequences_size() ||
      !proto_testcase.sequence_indexes_size()) {
    return;
  }

  // Make sure that the environment is initialized before we do anything else.
  GetEnvironment();

  CodeCacheHostTestcase testcase(proto_testcase);

  base::RunLoop main_run_loop;

  // Unretained is safe here, because `main_run_loop` has to finish before
  // testcase goes out of scope.
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(RunTestcase, base::Unretained(&testcase),
                                main_run_loop.QuitClosure()));

  main_run_loop.Run();
}
