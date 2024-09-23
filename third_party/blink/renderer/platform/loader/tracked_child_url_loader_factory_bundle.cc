// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/tracked_child_url_loader_factory_bundle.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

TrackedChildPendingURLLoaderFactoryBundle::
    TrackedChildPendingURLLoaderFactoryBundle() = default;

TrackedChildPendingURLLoaderFactoryBundle::
    TrackedChildPendingURLLoaderFactoryBundle(
        mojo::PendingRemote<network::mojom::URLLoaderFactory>
            pending_default_factory,
        SchemeMap pending_scheme_specific_factories,
        OriginMap pending_isolated_world_factories,
        mojo::PendingRemote<network::mojom::URLLoaderFactory>
            pending_subresource_proxying_loader_factory,
        mojo::PendingRemote<network::mojom::URLLoaderFactory>
            pending_keep_alive_loader_factory,
        mojo::PendingAssociatedRemote<blink::mojom::FetchLaterLoaderFactory>
            pending_fetch_later_loader_factory,
        std::unique_ptr<HostPtrAndTaskRunner> main_thread_host_bundle,
        bool bypass_redirect_checks)
    : ChildPendingURLLoaderFactoryBundle(
          std::move(pending_default_factory),
          std::move(pending_scheme_specific_factories),
          std::move(pending_isolated_world_factories),
          std::move(pending_subresource_proxying_loader_factory),
          std::move(pending_keep_alive_loader_factory),
          std::move(pending_fetch_later_loader_factory),
          bypass_redirect_checks),
      main_thread_host_bundle_(std::move(main_thread_host_bundle)) {}

TrackedChildPendingURLLoaderFactoryBundle::
    ~TrackedChildPendingURLLoaderFactoryBundle() = default;

bool TrackedChildPendingURLLoaderFactoryBundle::
    IsTrackedChildPendingURLLoaderFactoryBundle() const {
  return true;
}

scoped_refptr<network::SharedURLLoaderFactory>
TrackedChildPendingURLLoaderFactoryBundle::CreateFactory() {
  auto other = std::make_unique<TrackedChildPendingURLLoaderFactoryBundle>();
  other->pending_default_factory_ = std::move(pending_default_factory_);
  other->pending_scheme_specific_factories_ =
      std::move(pending_scheme_specific_factories_);
  other->pending_isolated_world_factories_ =
      std::move(pending_isolated_world_factories_);
  other->pending_subresource_proxying_loader_factory_ =
      std::move(pending_subresource_proxying_loader_factory_);
  other->pending_keep_alive_loader_factory_ =
      std::move(pending_keep_alive_loader_factory_);
  other->pending_fetch_later_loader_factory_ =
      std::move(pending_fetch_later_loader_factory_);
  other->main_thread_host_bundle_ = std::move(main_thread_host_bundle_);
  other->bypass_redirect_checks_ = bypass_redirect_checks_;

  return base::MakeRefCounted<TrackedChildURLLoaderFactoryBundle>(
      std::move(other));
}

// -----------------------------------------------------------------------------

TrackedChildURLLoaderFactoryBundle::TrackedChildURLLoaderFactoryBundle(
    std::unique_ptr<TrackedChildPendingURLLoaderFactoryBundle>
        pending_factories) {
  DCHECK(pending_factories->main_thread_host_bundle());
  main_thread_host_bundle_ =
      std::move(pending_factories->main_thread_host_bundle());
  Update(std::move(pending_factories));
  AddObserverOnMainThread();
}

TrackedChildURLLoaderFactoryBundle::~TrackedChildURLLoaderFactoryBundle() {
  RemoveObserverOnMainThread();
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
TrackedChildURLLoaderFactoryBundle::Clone() {
  auto pending_factories =
      base::WrapUnique(static_cast<ChildPendingURLLoaderFactoryBundle*>(
          ChildURLLoaderFactoryBundle::Clone().release()));

  DCHECK(main_thread_host_bundle_);

  auto main_thread_host_bundle_clone = std::make_unique<HostPtrAndTaskRunner>(
      main_thread_host_bundle_->first, main_thread_host_bundle_->second);

  return std::make_unique<TrackedChildPendingURLLoaderFactoryBundle>(
      std::move(pending_factories->pending_default_factory()),
      std::move(pending_factories->pending_scheme_specific_factories()),
      std::move(pending_factories->pending_isolated_world_factories()),
      std::move(
          pending_factories->pending_subresource_proxying_loader_factory()),
      std::move(pending_factories->pending_keep_alive_loader_factory()),
      std::move(pending_factories->pending_fetch_later_loader_factory()),
      std::move(main_thread_host_bundle_clone),
      pending_factories->bypass_redirect_checks());
}

void TrackedChildURLLoaderFactoryBundle::AddObserverOnMainThread() {
  DCHECK(main_thread_host_bundle_);

  // Required by |SequencedTaskRunner::GetCurrentDefault()| below.
  if (!base::SequencedTaskRunner::HasCurrentDefault())
    return;

  main_thread_host_bundle_->second->PostTask(
      FROM_HERE,
      base::BindOnce(
          &HostChildURLLoaderFactoryBundle::AddObserver,
          main_thread_host_bundle_->first, reinterpret_cast<ObserverKey>(this),
          std::make_unique<
              HostChildURLLoaderFactoryBundle::ObserverPtrAndTaskRunner>(
              weak_ptr_factory_.GetWeakPtr(),
              base::SequencedTaskRunner::GetCurrentDefault())));
}

void TrackedChildURLLoaderFactoryBundle::RemoveObserverOnMainThread() {
  DCHECK(main_thread_host_bundle_);

  main_thread_host_bundle_->second->PostTask(
      FROM_HERE,
      base::BindOnce(&HostChildURLLoaderFactoryBundle::RemoveObserver,
                     main_thread_host_bundle_->first,
                     reinterpret_cast<ObserverKey>(this)));
}

void TrackedChildURLLoaderFactoryBundle::OnUpdate(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factories) {
  Update(base::WrapUnique(static_cast<ChildPendingURLLoaderFactoryBundle*>(
      pending_factories.release())));
}

// -----------------------------------------------------------------------------

HostChildURLLoaderFactoryBundle::HostChildURLLoaderFactoryBundle(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : observer_list_(std::make_unique<ObserverList>()),
      task_runner_(std::move(task_runner)) {
  DCHECK(IsMainThread()) << "HostChildURLLoaderFactoryBundle should live "
                            "on the main renderer thread";
}

HostChildURLLoaderFactoryBundle::~HostChildURLLoaderFactoryBundle() = default;

std::unique_ptr<network::PendingSharedURLLoaderFactory>
HostChildURLLoaderFactoryBundle::Clone() {
  auto pending_factories =
      base::WrapUnique(static_cast<ChildPendingURLLoaderFactoryBundle*>(
          ChildURLLoaderFactoryBundle::Clone().release()));

  DCHECK(base::SequencedTaskRunner::HasCurrentDefault());
  auto main_thread_host_bundle_clone = std::make_unique<
      TrackedChildURLLoaderFactoryBundle::HostPtrAndTaskRunner>(
      weak_ptr_factory_.GetWeakPtr(), task_runner_);

  return std::make_unique<TrackedChildPendingURLLoaderFactoryBundle>(
      std::move(pending_factories->pending_default_factory()),
      std::move(pending_factories->pending_scheme_specific_factories()),
      std::move(pending_factories->pending_isolated_world_factories()),
      std::move(
          pending_factories->pending_subresource_proxying_loader_factory()),
      std::move(pending_factories->pending_keep_alive_loader_factory()),
      std::move(pending_factories->pending_fetch_later_loader_factory()),
      std::move(main_thread_host_bundle_clone),
      pending_factories->bypass_redirect_checks());
}

void HostChildURLLoaderFactoryBundle::UpdateThisAndAllClones(
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> pending_factories) {
  DCHECK(IsMainThread()) << "Should run on the main renderer thread";
  DCHECK(observer_list_);

  auto partial_bundle = base::MakeRefCounted<ChildURLLoaderFactoryBundle>();
  static_cast<blink::URLLoaderFactoryBundle*>(partial_bundle.get())
      ->Update(std::move(pending_factories));

  for (const auto& iter : *observer_list_) {
    NotifyUpdateOnMainOrWorkerThread(iter.second.get(),
                                     partial_bundle->Clone());
  }

  Update(partial_bundle->PassInterface());
}

bool HostChildURLLoaderFactoryBundle::IsHostChildURLLoaderFactoryBundle()
    const {
  return true;
}

void HostChildURLLoaderFactoryBundle::AddObserver(
    ObserverKey observer,
    std::unique_ptr<ObserverPtrAndTaskRunner> observer_info) {
  DCHECK(IsMainThread()) << "Should run in the main renderer thread";
  DCHECK(observer_list_);
  (*observer_list_)[observer] = std::move(observer_info);
}

void HostChildURLLoaderFactoryBundle::RemoveObserver(ObserverKey observer) {
  DCHECK(IsMainThread()) << "Should run in the main renderer thread";
  DCHECK(observer_list_);
  observer_list_->erase(observer);
}

void HostChildURLLoaderFactoryBundle::NotifyUpdateOnMainOrWorkerThread(
    ObserverPtrAndTaskRunner* observer_bundle,
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factories) {
  observer_bundle->second->PostTask(
      FROM_HERE,
      base::BindOnce(&TrackedChildURLLoaderFactoryBundle::OnUpdate,
                     observer_bundle->first, std::move(pending_factories)));
}

}  // namespace blink
