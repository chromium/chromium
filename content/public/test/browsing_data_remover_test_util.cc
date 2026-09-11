// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browsing_data_remover_test_util.h"

#include "base/functional/bind.h"

namespace content {

BrowsingDataRemoverCompletionObserver::BrowsingDataRemoverCompletionObserver(
    BrowsingDataRemover* remover)
    : observation_(this) {
  observation_.Observe(remover);
}

BrowsingDataRemoverCompletionObserver::
    ~BrowsingDataRemoverCompletionObserver() = default;

void BrowsingDataRemoverCompletionObserver::BlockUntilCompletion() {
  if (!browsing_data_remover_done_) {
    run_loop_.Run();
  }
}

void BrowsingDataRemoverCompletionObserver::OnBrowsingDataRemoverDone(
    uint64_t failed_data_types) {
  browsing_data_remover_done_ = true;
  failed_data_types_ = failed_data_types;
  DCHECK(observation_.IsObserving());
  observation_.Reset();
  run_loop_.QuitWhenIdle();
}

BrowsingDataRemoverCompletionInhibitor::BrowsingDataRemoverCompletionInhibitor(
    BrowsingDataRemover* remover)
    : remover_(remover), run_loop_(std::make_unique<base::RunLoop>()) {
  DCHECK(remover);
  remover_->SetWouldCompleteCallbackForTesting(
      base::BindRepeating(&BrowsingDataRemoverCompletionInhibitor::
                              OnBrowsingDataRemoverWouldComplete,
                          base::Unretained(this)));
}

BrowsingDataRemoverCompletionInhibitor::
    ~BrowsingDataRemoverCompletionInhibitor() {
  Reset();
}

void BrowsingDataRemoverCompletionInhibitor::Reset() {
  if (!remover_)
    return;
  remover_->SetWouldCompleteCallbackForTesting(
      base::RepeatingCallback<void(base::OnceClosure)>());
  remover_ = nullptr;
}

void BrowsingDataRemoverCompletionInhibitor::BlockUntilNearCompletion() {
  if (!browsing_data_remover_would_complete_done_) {
    run_loop_->Run();
  }
  run_loop_ = std::make_unique<base::RunLoop>();
  browsing_data_remover_would_complete_done_ = false;
}

void BrowsingDataRemoverCompletionInhibitor::ContinueToCompletion() {
  DCHECK(!continue_to_completion_callback_.is_null());
  std::move(continue_to_completion_callback_).Run();
}

void BrowsingDataRemoverCompletionInhibitor::OnBrowsingDataRemoverWouldComplete(
    base::OnceClosure continue_to_completion) {
  DCHECK(continue_to_completion_callback_.is_null());
  continue_to_completion_callback_ = std::move(continue_to_completion);
  browsing_data_remover_would_complete_done_ = true;
  run_loop_->QuitWhenIdle();
}

}  // namespace content
