// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_impl_instance.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/first_party_sets_handler_impl.h"
#include "content/browser/first_party_sets/first_party_sets_loader.h"
#include "content/browser/first_party_sets/first_party_sets_overrides_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_client.h"
#include "net/base/features.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/first_party_sets/sets_mutation.h"
#include "sql/database.h"

namespace net {
class SchemefulSite;
}  // namespace net

namespace content {

namespace {

constexpr base::FilePath::CharType kFirstPartySetsDatabase[] =
    FILE_PATH_LITERAL("first_party_sets.db");

// Global FirstPartySetsHandler instance for testing. This should be preferred
// by tests when possible.
FirstPartySetsHandler* g_test_instance = nullptr;

// Global FirstPartySetsHandlerImpl instance for testing. This is mainly useful
// for tests that need to know about content-internal details.
FirstPartySetsHandlerImpl* g_impl_test_instance = nullptr;

}  // namespace

// static
void FirstPartySetsHandler::SetInstanceForTesting(
    FirstPartySetsHandler* test_instance) {
  g_test_instance = test_instance;
}

// static
void FirstPartySetsHandlerImpl::SetInstanceForTesting(
    FirstPartySetsHandlerImpl* test_instance) {
  g_impl_test_instance = test_instance;
}

// static
FirstPartySetsHandler* FirstPartySetsHandler::GetInstance() {
  if (g_test_instance) {
    return g_test_instance;
  }

  return FirstPartySetsHandlerImpl::GetInstance();
}

// static
FirstPartySetsHandlerImpl* FirstPartySetsHandlerImpl::GetInstance() {
  static base::NoDestructor<FirstPartySetsHandlerImplInstance> instance(
      GetContentClient()->browser()->IsFirstPartySetsEnabled(),
      GetContentClient()->browser()->WillProvidePublicFirstPartySets());
  if (g_impl_test_instance) {
    return g_impl_test_instance;
  }
  return instance.get();
}

// static
FirstPartySetsHandlerImplInstance
FirstPartySetsHandlerImplInstance::CreateForTesting(
    bool enabled,
    bool embedder_will_provide_public_sets) {
  return FirstPartySetsHandlerImplInstance(enabled,
                                           embedder_will_provide_public_sets);
}

FirstPartySetsHandlerImplInstance::FirstPartySetsHandlerImplInstance(
    bool enabled,
    bool embedder_will_provide_public_sets)
    : enabled_(enabled) {
  if (enabled) {
    on_sets_ready_callbacks_ =
        std::make_unique<base::circular_deque<base::OnceClosure>>();
    sets_loader_ = std::make_unique<FirstPartySetsLoader>(
        base::BindOnce(&FirstPartySetsHandlerImplInstance::SetCompleteSets,
                       // base::Unretained(this) is safe here because
                       // this is a static singleton.
                       base::Unretained(this)));
    if (!embedder_will_provide_public_sets) {
      sets_loader_->SetComponentSets(base::Version(), base::File());
    }
  } else {
    SetCompleteSets(net::GlobalFirstPartySets());
    CHECK(global_sets_.has_value());
  }
}

FirstPartySetsHandlerImplInstance::~FirstPartySetsHandlerImplInstance() =
    default;

std::optional<net::GlobalFirstPartySets>
FirstPartySetsHandlerImplInstance::GetSets(
    base::OnceCallback<void(net::GlobalFirstPartySets)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (global_sets_.has_value()) {
    return global_sets_->Clone();
  }

  if (!callback.is_null()) {
    // base::Unretained(this) is safe here because this is a static singleton.
    EnqueuePendingTask(
        base::BindOnce(&FirstPartySetsHandlerImplInstance::GetGlobalSetsSync,
                       base::Unretained(this))
            .Then(std::move(callback)));
  }

  return std::nullopt;
}

void FirstPartySetsHandlerImplInstance::Init(
    const base::FilePath& user_data_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (initialized_) {
    return;
  }

  initialized_ = true;
  if (!user_data_dir.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(
            [](const base::FilePath& db_path) {
              if (base::PathExists(db_path)) {
                sql::Database::Delete(db_path);
              }
            },
            user_data_dir.Append(kFirstPartySetsDatabase)));
  }
}

bool FirstPartySetsHandlerImplInstance::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return enabled_;
}

void FirstPartySetsHandlerImplInstance::SetPublicFirstPartySets(
    const base::Version& version,
    base::File sets_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!sets_loader_) {
    FirstPartySetsLoader::DisposeFile(std::move(sets_file));
    return;
  }

  sets_loader_->SetComponentSets(version, std::move(sets_file));
}

void FirstPartySetsHandlerImplInstance::SetCompleteSets(
    net::GlobalFirstPartySets sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!global_sets_.has_value());
  global_sets_ = std::move(sets);
  sets_loader_.reset();

  InvokePendingQueries();
}

void FirstPartySetsHandlerImplInstance::EnqueuePendingTask(
    base::OnceClosure run_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!global_sets_.has_value());
  CHECK(on_sets_ready_callbacks_);

  if (!first_async_task_timer_.has_value()) {
    first_async_task_timer_ = base::ElapsedTimer();
  }

  on_sets_ready_callbacks_->push_back(std::move(run_task));
}

void FirstPartySetsHandlerImplInstance::InvokePendingQueries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::circular_deque<base::OnceClosure> queue;
  if (on_sets_ready_callbacks_) {
    queue.swap(*on_sets_ready_callbacks_);
  }

  base::UmaHistogramCounts10000(
      "Cookie.FirstPartySets.Browser.DelayedQueriesCount", queue.size());
  base::UmaHistogramTimes("Cookie.FirstPartySets.Browser.MostDelayedQueryDelta",
                          first_async_task_timer_.has_value()
                              ? first_async_task_timer_->Elapsed()
                              : base::TimeDelta());

  while (!queue.empty()) {
    base::OnceCallback callback = std::move(queue.front());
    queue.pop_front();
    std::move(callback).Run();
  }
  on_sets_ready_callbacks_.reset();
}

std::optional<net::FirstPartySetEntry>
FirstPartySetsHandlerImplInstance::FindEntry(
    const net::SchemefulSite& site) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!global_sets_.has_value()) {
    return std::nullopt;
  }
  return global_sets_->FindEntry(site);
}

net::GlobalFirstPartySets FirstPartySetsHandlerImplInstance::GetGlobalSetsSync()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(global_sets_.has_value());
  return global_sets_->Clone();
}

bool FirstPartySetsHandlerImplInstance::WhenInitComplete(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (global_sets_.has_value()) {
    return true;
  }

  if (!callback.is_null()) {
    EnqueuePendingTask(std::move(callback));
  }

  return false;
}

void FirstPartySetsHandlerImplInstance::ComputeFirstPartySetMetadata(
    const net::SchemefulSite& site,
    base::optional_ref<const net::SchemefulSite> top_frame_site,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!global_sets_.has_value()) {
    EnqueuePendingTask(base::BindOnce(&FirstPartySetsHandlerImplInstance::
                                          ComputeFirstPartySetMetadataInternal,
                                      base::Unretained(this), site,
                                      top_frame_site.CopyAsOptional(),
                                      std::move(callback)));
    return;
  }

  std::move(callback).Run(global_sets_->ComputeMetadata(site, top_frame_site));
}

void FirstPartySetsHandlerImplInstance::ComputeFirstPartySetMetadataInternal(
    const net::SchemefulSite& site,
    base::optional_ref<const net::SchemefulSite> top_frame_site,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(global_sets_.has_value());

  std::move(callback).Run(global_sets_->ComputeMetadata(site, top_frame_site));
}

bool FirstPartySetsHandlerImplInstance::ForEachEffectiveSetEntry(
    base::FunctionRef<bool(const net::SchemefulSite&,
                           const net::FirstPartySetEntry&)> f) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!global_sets_.has_value()) {
    return false;
  }
  return global_sets_->ForEachEffectiveSetEntry(f);
}

}  // namespace content
