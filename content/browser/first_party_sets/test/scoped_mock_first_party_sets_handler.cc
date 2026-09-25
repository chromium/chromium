// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/test/scoped_mock_first_party_sets_handler.h"

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/optional_ref.h"
#include "content/browser/first_party_sets/first_party_sets_handler_impl.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"

namespace content {

ScopedMockFirstPartySetsHandler::ScopedMockFirstPartySetsHandler()
    : previous_(FirstPartySetsHandlerImpl::GetInstance()) {
  FirstPartySetsHandlerImpl::SetInstanceForTesting(this);
}

ScopedMockFirstPartySetsHandler::~ScopedMockFirstPartySetsHandler() {
  FirstPartySetsHandlerImpl::SetInstanceForTesting(previous_);
}

bool ScopedMockFirstPartySetsHandler::IsEnabled() const {
  return true;
}

void ScopedMockFirstPartySetsHandler::SetPublicFirstPartySets(
    const base::Version& version,
    base::File sets_file) {}

std::optional<net::FirstPartySetEntry>
ScopedMockFirstPartySetsHandler::FindEntry(
    const net::SchemefulSite& site) const {
  return global_sets_.FindEntry(site);
}

void ScopedMockFirstPartySetsHandler::Init(
    const base::FilePath& user_data_dir) {}

[[nodiscard]] std::optional<net::GlobalFirstPartySets>
ScopedMockFirstPartySetsHandler::GetSets(
    base::OnceCallback<void(net::GlobalFirstPartySets)> callback) {
  if (should_deadlock_) {
    return std::nullopt;
  }
  if (invoke_callbacks_asynchronously_ && !callback.is_null()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), global_sets_.Clone()));
    return std::nullopt;
  }

  return global_sets_.Clone();
}

bool ScopedMockFirstPartySetsHandler::WhenInitComplete(
    base::OnceClosure callback) {
  if (should_deadlock_) {
    return false;
  }
  if (invoke_callbacks_asynchronously_ && !callback.is_null()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return false;
  }

  return true;
}

void ScopedMockFirstPartySetsHandler::ComputeFirstPartySetMetadata(
    const net::SchemefulSite& site,
    base::optional_ref<const net::SchemefulSite> top_frame_site,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) {
  if (should_deadlock_) {
    return;
  }
  net::FirstPartySetMetadata metadata =
      global_sets_.ComputeMetadata(site, top_frame_site);
  if (invoke_callbacks_asynchronously_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(metadata)));
    return;
  }
  return std::move(callback).Run(std::move(metadata));
}

bool ScopedMockFirstPartySetsHandler::ForEachEffectiveSetEntry(
    base::FunctionRef<bool(const net::SchemefulSite&,
                           const net::FirstPartySetEntry&)> f) const {
  if (invoke_callbacks_asynchronously_) {
    return false;
  }
  return global_sets_.ForEachEffectiveSetEntry(f);
}

void ScopedMockFirstPartySetsHandler::SetGlobalSets(
    net::GlobalFirstPartySets global_sets) {
  global_sets_ = std::move(global_sets);
}

}  // namespace content
