// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/prefetch_test_util.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_url_loader_interceptor.h"

namespace content::test {

class TestPrefetchWatcherImpl {
 public:
  TestPrefetchWatcherImpl();
  ~TestPrefetchWatcherImpl();

  TestPrefetchWatcherImpl(const TestPrefetchWatcherImpl&) = delete;
  TestPrefetchWatcherImpl& operator=(const TestPrefetchWatcherImpl&) = delete;

  PrefetchContainerIdForTesting WaitUntilPrefetchResponseCompleted(
      const std::optional<blink::DocumentToken>& document_token,
      const GURL& url);
  std::optional<bool> PrefetchUsedInLastNavigation();
  std::optional<PrefetchContainerIdForTesting>
  GetPrefetchContainerIdForTestingInLastNavigation();

 private:
  void OnPrefetchResponseCompleted(
      base::WeakPtr<PrefetchContainer> prefetch_container);
  void OnPrefetchInterceptionCompleted(PrefetchContainer* prefetch_container);

  PrefetchContainerIdForTesting WaitUntilPrefetchResponseCompleted(
      const PrefetchContainer::Key& key);

  PrefetchContainerIdForTesting GetContainerIdForTesting(
      PrefetchContainer* prefetch_container);

  std::map<PrefetchContainer::Key, base::WeakPtr<PrefetchContainer>>
      response_completed_prefetches_;
  std::map<PrefetchContainer::Key, base::OnceClosure>
      response_completed_quit_closures_;

  std::optional<raw_ptr<PrefetchContainer>>
      prefetch_container_used_in_last_navigation_ = std::nullopt;
};

TestPrefetchWatcherImpl::TestPrefetchWatcherImpl() {
  PrefetchService::SetPrefetchResponseCompletedCallbackForTesting(
      base::BindRepeating(&TestPrefetchWatcherImpl::OnPrefetchResponseCompleted,
                          base::Unretained(this)));
  PrefetchURLLoaderInterceptor::SetPrefetchCompleteCallbackForTesting(
      base::BindRepeating(
          &TestPrefetchWatcherImpl::OnPrefetchInterceptionCompleted,
          base::Unretained(this)));
}

TestPrefetchWatcherImpl::~TestPrefetchWatcherImpl() {
  PrefetchURLLoaderInterceptor::SetPrefetchCompleteCallbackForTesting(
      base::DoNothing());
  PrefetchService::SetPrefetchResponseCompletedCallbackForTesting(
      base::DoNothing());
}

void TestPrefetchWatcherImpl::OnPrefetchResponseCompleted(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  const PrefetchContainer::Key& key = prefetch_container->key();
  response_completed_prefetches_.emplace(key, prefetch_container);
  if (response_completed_quit_closures_.contains(key)) {
    auto quit_closure = std::move(response_completed_quit_closures_[key]);
    response_completed_quit_closures_.erase(key);
    std::move(quit_closure).Run();
  }
}

void TestPrefetchWatcherImpl::OnPrefetchInterceptionCompleted(
    PrefetchContainer* prefetch_container) {
  prefetch_container_used_in_last_navigation_ = prefetch_container;
}

PrefetchContainerIdForTesting
TestPrefetchWatcherImpl::WaitUntilPrefetchResponseCompleted(
    const std::optional<blink::DocumentToken>& document_token,
    const GURL& url) {
  return WaitUntilPrefetchResponseCompleted(
      PrefetchContainer::Key(document_token, url));
}

PrefetchContainerIdForTesting
TestPrefetchWatcherImpl::WaitUntilPrefetchResponseCompleted(
    const PrefetchContainer::Key& key) {
  if (response_completed_prefetches_.contains(key)) {
    return GetContainerIdForTesting(response_completed_prefetches_[key].get());
  }
  CHECK(!response_completed_quit_closures_.contains(key));
  base::RunLoop loop;
  response_completed_quit_closures_.emplace(key, loop.QuitClosure());
  loop.Run();
  return GetContainerIdForTesting(response_completed_prefetches_[key].get());
}

std::optional<bool> TestPrefetchWatcherImpl::PrefetchUsedInLastNavigation() {
  if (prefetch_container_used_in_last_navigation_.has_value()) {
    return !!prefetch_container_used_in_last_navigation_.value();
  } else {
    return std::nullopt;
  }
}

std::optional<PrefetchContainerIdForTesting>
TestPrefetchWatcherImpl::GetPrefetchContainerIdForTestingInLastNavigation() {
  if (!PrefetchUsedInLastNavigation().has_value()) {
    return std::nullopt;
  }
  if (!PrefetchUsedInLastNavigation().value()) {
    return InvalidPrefetchContainerIdForTesting;
  }
  return GetContainerIdForTesting(
      prefetch_container_used_in_last_navigation_.value());
}

PrefetchContainerIdForTesting TestPrefetchWatcherImpl::GetContainerIdForTesting(
    PrefetchContainer* prefetch_container) {
  return prefetch_container
             ? PrefetchContainerIdForTesting(prefetch_container->RequestId())
             : InvalidPrefetchContainerIdForTesting;
}

TestPrefetchWatcher::TestPrefetchWatcher()
    : impl_(std::make_unique<TestPrefetchWatcherImpl>()) {}

TestPrefetchWatcher::~TestPrefetchWatcher() = default;

PrefetchContainerIdForTesting
TestPrefetchWatcher::WaitUntilPrefetchResponseCompleted(
    const std::optional<blink::DocumentToken>& document_token,
    const GURL& url) {
  return impl_->WaitUntilPrefetchResponseCompleted(document_token, url);
}

std::optional<bool> TestPrefetchWatcher::PrefetchUsedInLastNavigation() {
  return impl_->PrefetchUsedInLastNavigation();
}

std::optional<PrefetchContainerIdForTesting>
TestPrefetchWatcher::GetPrefetchContainerIdForTestingInLastNavigation() {
  return impl_->GetPrefetchContainerIdForTestingInLastNavigation();
}

}  // namespace content::test
