// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"

#include <optional>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

bool IsValidURL(const KURL& url) {
  return !url.IsEmpty() && url.IsValid();
}

}  // namespace

void WorkletModuleResponsesMap::Entry::AddClient(
    ModuleScriptFetcher::Client* client,
    scoped_refptr<base::SingleThreadTaskRunner> client_task_runner) {
  // Clients can be added only while a module script is being fetched.
  DCHECK_EQ(state_, State::kFetching);
  clients_.insert(client, client_task_runner);
}

// Implementation of the second half of the custom fetch defined in the
// "fetch a worklet script" algorithm:
// https://drafts.css-houdini.org/worklets/#fetch-a-worklet-script
void WorkletModuleResponsesMap::Entry::SetParams(
    const std::optional<ModuleScriptCreationParams>& params) {
  DCHECK_EQ(state_, State::kFetching);

  if (params) {
    state_ = State::kFetched;

    // Step 7: "Let response be the result of fetch when it asynchronously
    // completes."
    // Step 8: "Set the value of the entry in cache whose key is url to
    // response, and asynchronously complete this algorithm with response."
    params_.emplace(params->IsolatedCopy());
    DCHECK(params_->IsSafeToSendToAnotherThread());
    for (auto& it : clients_) {
      PostCrossThreadTask(
          *it.value, FROM_HERE,
          CrossThreadBindOnce(&ModuleScriptFetcher::Client::OnFetched, it.key,
                              *params));
    }
  } else {
    state_ = State::kFailed;
    // TODO(nhiroki): Add |error_messages| to the context's message storage.
    for (auto& it : clients_) {
      PostCrossThreadTask(
          *it.value, FROM_HERE,
          CrossThreadBindOnce(&ModuleScriptFetcher::Client::OnFailed, it.key));
    }
  }

  clients_.clear();
}

// Implementation of the first half of the custom fetch defined in the
// "fetch a worklet script" algorithm:
// https://drafts.css-houdini.org/worklets/#fetch-a-worklet-script
//
// "To perform the fetch given request, perform the following steps:"
// Step 1: "Let cache be the moduleResponsesMap."
// Step 2: "Let url be request's url."
bool WorkletModuleResponsesMap::GetEntry(
    const KURL& url,
    ModuleType module_type,
    ModuleScriptFetcher::Client* client,
    scoped_refptr<base::SingleThreadTaskRunner> client_task_runner) {
  base::AutoLock locker(lock_);
  DCHECK_NE(module_type, ModuleType::kInvalid);
  if (!is_available_ || !IsValidURL(url)) {
    client_task_runner->PostTask(
        FROM_HERE, WTF::BindOnce(&ModuleScriptFetcher::Client::OnFailed,
                                 WrapPersistent(client)));
    return true;
  }

  auto it = entries_.find(std::make_pair(url, module_type));
  if (it != entries_.end()) {
    Entry* entry = it->value.get();
    switch (entry->GetState()) {
      case Entry::State::kFetching:
        // Step 3: "If cache contains an entry with key url whose value is
        // "fetching", wait until that entry's value changes, then proceed to
        // the next step."
        entry->AddClient(client, client_task_runner);
        return true;
      case Entry::State::kFetched:
        // Step 4: "If cache contains an entry with key url, asynchronously
        // complete this algorithm with that entry's value, and abort these
        // steps."
        client_task_runner->PostTask(
            FROM_HERE,
            WTF::BindOnce(&ModuleScriptFetcher::Client::OnFetched,
                          WrapPersistent(client), entry->GetParams()));
        return true;
      case Entry::State::kFailed:
        // Module fetching failed before. Abort following steps.
        client_task_runner->PostTask(
            FROM_HERE, WTF::BindOnce(&ModuleScriptFetcher::Client::OnFailed,
                                     WrapPersistent(client)));
        return true;
    }
    NOTREACHED_IN_MIGRATION();
  }

  // Step 5: "Create an entry in cache with key url and value "fetching"."
  std::unique_ptr<Entry> entry = std::make_unique<Entry>();
  entry->AddClient(client, client_task_runner);
  entries_.insert(std::make_pair(url, module_type), std::move(entry));

  // Step 6: "Fetch request."
  // Running the callback with an empty params will make the fetcher to fallback
  // to regular module loading and Write() will be called once the fetch is
  // complete.
  return false;
}

void WorkletModuleResponsesMap::SetEntryParams(
    const KURL& url,
    ModuleType module_type,
    const std::optional<ModuleScriptCreationParams>& params) {
  base::AutoLock locker(lock_);
  if (!is_available_)
    return;

  DCHECK(entries_.Contains(std::make_pair(url, module_type)));
  Entry* entry = entries_.find(std::make_pair(url, module_type))->value.get();
  entry->SetParams(params);
}

void WorkletModuleResponsesMap::Dispose() {
  DCHECK(IsMainThread());
  base::AutoLock locker(lock_);
  is_available_ = false;
  for (auto& it : entries_) {
    switch (it.value->GetState()) {
      case Entry::State::kFetching:
        it.value->SetParams(std::nullopt);
        break;
      case Entry::State::kFetched:
      case Entry::State::kFailed:
        break;
    }
  }
  entries_.clear();
}

}  // namespace blink
