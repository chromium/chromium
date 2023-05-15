// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/i18n/icu_util.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"  // [nogncheck]
#include "content/browser/cache_storage/cache_storage_control_wrapper.h"  // [nogncheck]
#include "content/browser/code_cache/generated_code_cache_context.h"  // [nogncheck]
#include "content/browser/renderer_host/code_cache_host_impl.h"  // [nogncheck]
#include "content/browser/storage_partition_impl.h"              // [nogncheck]
#include "content/browser/storage_partition_impl_map.h"          // [nogncheck]
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/fuzzer/code_cache_host_mojolpm_fuzzer.pb.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-mojolpm.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"
#include "url/origin.h"

using url::Origin;

const char* kCmdline[] = {"code_cache_host_mojolpm_fuzzer", nullptr};

content::mojolpm::FuzzerEnvironment& GetEnvironment() {
  static base::NoDestructor<
      content::mojolpm::FuzzerEnvironmentWithTaskEnvironment>
      environment(1, kCmdline);
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
class CodeCacheHostTestcase
    : public mojolpm::Testcase<
          content::fuzzing::code_cache_host::proto::Testcase,
          content::fuzzing::code_cache_host::proto::Action> {
 public:
  using ProtoTestcase = content::fuzzing::code_cache_host::proto::Testcase;
  using ProtoAction = content::fuzzing::code_cache_host::proto::Action;

  explicit CodeCacheHostTestcase(const ProtoTestcase& testcase);

  void SetUp(base::OnceClosure done_closure) override;
  void TearDown(base::OnceClosure done_closure) override;

  void RunAction(const ProtoAction& action,
                 base::OnceClosure done_closure) override;

 private:
  void SetUpOnUIThread(base::OnceClosure done_closure);
  void SetUpOnFuzzerThread(base::OnceClosure done_closure);

  void TearDownOnUIThread(base::OnceClosure done_closure);
  void TearDownOnFuzzerThread(base::OnceClosure done_closure);

  // Used by AddCodeCacheHost to create and bind CodeCacheHostImpl on the code
  // cache thread.
  void AddCodeCacheHostImpl(
      uint32_t id,
      int renderer_id,
      const net::NetworkIsolationKey& key,
      const blink::StorageKey& storage_key,
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

  // This set of origins should cover all of the origin types which have special
  // handling in CodeCacheHostImpl, and give us two distinct "normal" origins,
  // which should be enough to exercise all of the code.
  const Origin origin_a_;
  const Origin origin_b_;
  const Origin origin_opaque_;
  const Origin origin_empty_;

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
};

CodeCacheHostTestcase::CodeCacheHostTestcase(
    const content::fuzzing::code_cache_host::proto::Testcase& testcase)
    : Testcase<ProtoTestcase, ProtoAction>(testcase),
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

void CodeCacheHostTestcase::RunAction(const ProtoAction& action,
                                      base::OnceClosure run_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto ThreadId_UI =
      content::fuzzing::code_cache_host::proto::RunThreadAction_ThreadId_UI;
  const auto ThreadId_IO =
      content::fuzzing::code_cache_host::proto::RunThreadAction_ThreadId_IO;

  switch (action.action_case()) {
    case ProtoAction::kNewCodeCacheHost:
      AddCodeCacheHost(action.new_code_cache_host().id(),
                       action.new_code_cache_host().render_process_id(),
                       action.new_code_cache_host().origin_id(),
                       std::move(run_closure));
      return;

    case ProtoAction::kRunThread:
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

    case ProtoAction::kCodeCacheHostRemoteAction:
      mojolpm::HandleRemoteAction(action.code_cache_host_remote_action());
      break;

    case ProtoAction::ACTION_NOT_SET:
      break;
  }

  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(run_closure));
}

void CodeCacheHostTestcase::AddCodeCacheHostImpl(
    uint32_t id,
    int renderer_id,
    const net::NetworkIsolationKey& nik,
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<::blink::mojom::CodeCacheHost>&& receiver) {
  auto code_cache_host = std::make_unique<content::CodeCacheHostImpl>(
      renderer_id, generated_code_cache_context_, nik, storage_key);
  code_cache_host->SetCacheStorageControlForTesting(
      cache_storage_control_wrapper_.get());
  UniqueCodeCacheReceiverSet receivers(
      new mojo::UniqueReceiverSet<blink::mojom::CodeCacheHost>(),
      base::OnTaskRunnerDeleter(
          base::SequencedTaskRunner::GetCurrentDefault()));
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
  auto storage_key = blink::StorageKey::CreateFirstParty(*origin);

  // Use of Unretained is safe since `this` is guaranteed to live at least until
  // `run_closure` is invoked.
  content::GeneratedCodeCacheContext::GetTaskRunner(
      generated_code_cache_context_)
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&CodeCacheHostTestcase::AddCodeCacheHostImpl,
                         base::Unretained(this), id, renderer_id,
                         net::NetworkIsolationKey(), storage_key,
                         std::move(receiver)),
          base::BindOnce(AddCodeCacheHostInstance, id, std::move(remote),
                         std::move(run_closure)));
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
      FROM_HERE,
      base::BindOnce(&mojolpm::RunTestcase<CodeCacheHostTestcase>,
                     base::Unretained(&testcase), GetFuzzerTaskRunner(),
                     main_run_loop.QuitClosure()));

  main_run_loop.Run();
}
