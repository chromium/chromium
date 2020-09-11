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
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "content/browser/code_cache/generated_code_cache_context.h"  // [nogncheck]
#include "content/browser/renderer_host/code_cache_host_impl.h"  // [nogncheck]
#include "content/browser/storage_partition_impl_map.h"          // [nogncheck]
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/test/fuzzer/code_cache_host_mojolpm_fuzzer.pb.h"
#include "mojo/core/embedder/embedder.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-mojolpm.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"
#include "url/origin.h"

using url::Origin;

const char* cmdline[] = {"code_cache_host_mojolpm_fuzzer", nullptr};

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
            (base::CommandLine::Init(1, cmdline),
             TestTimeouts::Initialize(),
             base::test::TaskEnvironment::MainThreadType::DEFAULT),
            base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC,
            base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
            content::BrowserTaskEnvironment::REAL_IO_THREAD) {
    logging::SetMinLogLevel(logging::LOG_FATAL);
    mojo::core::Init();
    base::i18n::InitializeICU();
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

  // Returns true once either all of the actions in the testcase have been
  // performed, or the per-testcase action limit has been exceeded.
  //
  // This should only be called from the fuzzer sequence.
  bool IsFinished();

  // If there are still actions remaining in the testcase, this will perform the
  // next sequence of actions before returning.
  //
  // If IsFinished() would return true, then calling this function is a no-op.
  //
  // This should only be called from the fuzzer sequence.
  void NextAction();

  void SetUp();
  void TearDown();

 private:
  using Action = content::fuzzing::code_cache_host::proto::Action;

  void SetUpOnUIThread();
  void TearDownOnUIThread();

  // Used by AddCodeCacheHost to create and bind CodeCacheHostImpl on the UI
  // thread.
  void AddCodeCacheHostImpl(
      uint32_t id,
      int renderer_id,
      const Origin& origin,
      mojo::PendingReceiver<::blink::mojom::CodeCacheHost>&& receiver);

  // Create and bind a new instance for fuzzing. This needs to  make sure that
  // the new instance has been created and bound on the correct sequence before
  // returning.
  void AddCodeCacheHost(
      uint32_t id,
      int renderer_id,
      content::fuzzing::code_cache_host::proto::NewCodeCacheHostAction::OriginId
          origin_id);

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

  // Prerequisite state.
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  scoped_refptr<content::CacheStorageContextImpl> cache_storage_context_;
  scoped_refptr<content::GeneratedCodeCacheContext>
      generated_code_cache_context_;

  // Mapping from renderer id to CodeCacheHostImpl instances being fuzzed.
  // Access only from UI thread.
  std::map<int, std::unique_ptr<content::CodeCacheHostImpl>> code_cache_hosts_;

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

void CodeCacheHostTestcase::SetUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::PostTaskAndReply(FROM_HERE, {content::BrowserThread::UI},
                         base::BindOnce(&CodeCacheHostTestcase::SetUpOnUIThread,
                                        base::Unretained(this)),
                         run_loop.QuitClosure());
  run_loop.Run();
}

void CodeCacheHostTestcase::SetUpOnUIThread() {
  browser_context_ = std::make_unique<content::TestBrowserContext>();

  cache_storage_context_ =
      base::MakeRefCounted<content::CacheStorageContextImpl>();
  cache_storage_context_->Init(browser_context_->GetPath(),
                               browser_context_->GetSpecialStoragePolicy(),
                               nullptr);

  generated_code_cache_context_ =
      base::MakeRefCounted<content::GeneratedCodeCacheContext>();
  generated_code_cache_context_->Initialize(browser_context_->GetPath(), 65536);
}

void CodeCacheHostTestcase::TearDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::PostTaskAndReply(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&CodeCacheHostTestcase::TearDownOnUIThread,
                     base::Unretained(this)),
      run_loop.QuitClosure());
  run_loop.Run();
}

void CodeCacheHostTestcase::TearDownOnUIThread() {
  code_cache_hosts_.clear();
  generated_code_cache_context_.reset();
  cache_storage_context_.reset();
  browser_context_.reset();
}

bool CodeCacheHostTestcase::IsFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return next_sequence_idx_ >= testcase_.sequence_indexes_size();
}

void CodeCacheHostTestcase::NextAction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (next_sequence_idx_ < testcase_.sequence_indexes_size()) {
    auto sequence_idx = testcase_.sequence_indexes(next_sequence_idx_++);
    const auto& sequence =
        testcase_.sequences(sequence_idx % testcase_.sequences_size());
    for (auto action_idx : sequence.action_indexes()) {
      if (!testcase_.actions_size() || ++action_count_ > max_action_count_) {
        return;
      }
      const auto& action =
          testcase_.actions(action_idx % testcase_.actions_size());
      if (action.ByteSizeLong() > max_action_size_) {
        return;
      }
      switch (action.action_case()) {
        case Action::kNewCodeCacheHost: {
          AddCodeCacheHost(action.new_code_cache_host().id(),
                           action.new_code_cache_host().render_process_id(),
                           action.new_code_cache_host().origin_id());
        } break;

        case Action::kRunThread: {
          if (action.run_thread().id()) {
            base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
            base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                           run_loop.QuitClosure());
            run_loop.Run();
          } else {
            base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
            base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                           run_loop.QuitClosure());
            run_loop.Run();
          }
        } break;

        case Action::kCodeCacheHostRemoteAction: {
          mojolpm::HandleRemoteAction(action.code_cache_host_remote_action());
        } break;

        case content::fuzzing::code_cache_host::proto::Action::ACTION_NOT_SET:
          break;
      }
    }
  }
}

void CodeCacheHostTestcase::AddCodeCacheHostImpl(
    uint32_t id,
    int renderer_id,
    const Origin& origin,
    mojo::PendingReceiver<::blink::mojom::CodeCacheHost>&& receiver) {
  code_cache_hosts_[renderer_id] = std::make_unique<content::CodeCacheHostImpl>(
      renderer_id, cache_storage_context_, generated_code_cache_context_,
      std::move(receiver));
}

void CodeCacheHostTestcase::AddCodeCacheHost(
    uint32_t id,
    int renderer_id,
    content::fuzzing::code_cache_host::proto::NewCodeCacheHostAction::OriginId
        origin_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::Remote<::blink::mojom::CodeCacheHost> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();

  const Origin* origin = &origin_a_;
  if (origin_id == 1) {
    origin = &origin_b_;
  } else if (origin_id == 2) {
    origin = &origin_opaque_;
  } else if (origin_id == 3) {
    origin = &origin_empty_;
  }

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::PostTaskAndReply(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&CodeCacheHostTestcase::AddCodeCacheHostImpl,
                     base::Unretained(this), id, renderer_id, *origin,
                     std::move(receiver)),
      run_loop.QuitClosure());
  run_loop.Run();

  mojolpm::GetContext()->AddInstance(id, std::move(remote));
}

// Helper function to keep scheduling fuzzer actions on the current runloop
// until the testcase has completed, and then quit the runloop.
void NextAction(CodeCacheHostTestcase* testcase,
                base::RepeatingClosure quit_closure) {
  if (!testcase->IsFinished()) {
    testcase->NextAction();
    GetFuzzerTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(NextAction, base::Unretained(testcase),
                                  std::move(quit_closure)));
  } else {
    GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(quit_closure));
  }
}

// Helper function to setup and run the testcase, since we need to do that from
// the fuzzer sequence rather than the main thread.
void RunTestcase(CodeCacheHostTestcase* testcase) {
  mojo::Message message;
  auto dispatch_context =
      std::make_unique<mojo::internal::MessageDispatchContext>(&message);

  testcase->SetUp();

  mojolpm::GetContext()->StartTestcase();

  base::RunLoop fuzzer_run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(NextAction, base::Unretained(testcase),
                                fuzzer_run_loop.QuitClosure()));
  fuzzer_run_loop.Run();

  mojolpm::GetContext()->EndTestcase();

  testcase->TearDown();
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

  base::RunLoop ui_run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  // Unretained is safe here, because ui_run_loop has to finish before testcase
  // goes out of scope.
  GetFuzzerTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(RunTestcase, base::Unretained(&testcase)),
      ui_run_loop.QuitClosure());

  ui_run_loop.Run();
}
