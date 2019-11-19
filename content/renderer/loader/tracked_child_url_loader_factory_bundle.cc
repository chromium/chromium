// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/tracked_child_url_loader_factory_bundle.h"

#include <utility>

#include "base/bind.h"
#include "content/public/renderer/render_thread.h"

namespace content {

TrackedChildURLLoaderFactoryBundleInfo::
    TrackedChildURLLoaderFactoryBundleInfo() = default;

TrackedChildURLLoaderFactoryBundleInfo::TrackedChildURLLoaderFactoryBundleInfo(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_default_factory,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_appcache_factory,
    SchemeMap pending_scheme_specific_factories,
    OriginMap pending_isolated_world_factories,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        direct_network_factory_remote,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_prefetch_loader_factory,
    std::unique_ptr<HostPtrAndTaskRunner> main_thread_host_bundle,
    bool bypass_redirect_checks)
    : ChildURLLoaderFactoryBundleInfo(
          std::move(pending_default_factory),
          std::move(pending_appcache_factory),
          std::move(pending_scheme_specific_factories),
          std::move(pending_isolated_world_factories),
          std::move(direct_network_factory_remote),
          std::move(pending_prefetch_loader_factory),
          bypass_redirect_checks),
      main_thread_host_bundle_(std::move(main_thread_host_bundle)) {}

TrackedChildURLLoaderFactoryBundleInfo::
    ~TrackedChildURLLoaderFactoryBundleInfo() = default;

scoped_refptr<network::SharedURLLoaderFactory>
TrackedChildURLLoaderFactoryBundleInfo::CreateFactory() {
  auto other = std::make_unique<TrackedChildURLLoaderFactoryBundleInfo>();
  other->pending_default_factory_ = std::move(pending_default_factory_);
  other->pending_appcache_factory_ = std::move(pending_appcache_factory_);
  other->pending_scheme_specific_factories_ =
      std::move(pending_scheme_specific_factories_);
  other->pending_isolated_world_factories_ =
      std::move(pending_isolated_world_factories_);
  other->direct_network_factory_remote_ =
      std::move(direct_network_factory_remote_);
  other->pending_prefetch_loader_factory_ =
      std::move(pending_prefetch_loader_factory_);
  other->main_thread_host_bundle_ = std::move(main_thread_host_bundle_);
  other->bypass_redirect_checks_ = bypass_redirect_checks_;

  return base::MakeRefCounted<TrackedChildURLLoaderFactoryBundle>(
      std::move(other));
}

// -----------------------------------------------------------------------------

TrackedChildURLLoaderFactoryBundle::TrackedChildURLLoaderFactoryBundle(
    std::unique_ptr<TrackedChildURLLoaderFactoryBundleInfo> pending_factories) {
  DCHECK(pending_factories->main_thread_host_bundle());
  main_thread_host_bundle_ =
      std::move(pending_factories->main_thread_host_bundle());
  Update(std::move(pending_factories));
  AddObserverOnMainThread();
}

TrackedChildURLLoaderFactoryBundle::~TrackedChildURLLoaderFactoryBundle() {
  RemoveObserverOnMainThread();
}

std::unique_ptr<network::SharedURLLoaderFactoryInfo>
TrackedChildURLLoaderFactoryBundle::Clone() {
  auto pending_factories =
      base::WrapUnique(static_cast<ChildURLLoaderFactoryBundleInfo*>(
          ChildURLLoaderFactoryBundle::Clone().release()));

  DCHECK(main_thread_host_bundle_);

  auto main_thread_host_bundle_clone = std::make_unique<HostPtrAndTaskRunner>(
      main_thread_host_bundle_->first, main_thread_host_bundle_->second);

  return std::make_unique<TrackedChildURLLoaderFactoryBundleInfo>(
      std::move(pending_factories->pending_default_factory()),
      std::move(pending_factories->pending_appcache_factory()),
      std::move(pending_factories->pending_scheme_specific_factories()),
      std::move(pending_factories->pending_isolated_world_factories()),
      std::move(pending_factories->direct_network_factory_remote()),
      std::move(pending_factories->pending_prefetch_loader_factory()),
      std::move(main_thread_host_bundle_clone),
      pending_factories->bypass_redirect_checks());
}

void TrackedChildURLLoaderFactoryBundle::AddObserverOnMainThread() {
  DCHECK(main_thread_host_bundle_);

  // Required by |SequencedTaskRunnerHandle::Get()| below.
  if (!base::SequencedTaskRunnerHandle::IsSet())
    return;

  main_thread_host_bundle_->second->PostTask(
      FROM_HERE,
      base::BindOnce(
          &HostChildURLLoaderFactoryBundle::AddObserver,
          main_thread_host_bundle_->first, base::Unretained(this),
          std::make_unique<
              HostChildURLLoaderFactoryBundle::ObserverPtrAndTaskRunner>(
              AsWeakPtr(), base::SequencedTaskRunnerHandle::Get())));
}

void TrackedChildURLLoaderFactoryBundle::RemoveObserverOnMainThread() {
  DCHECK(main_thread_host_bundle_);

  main_thread_host_bundle_->second->PostTask(
      FROM_HERE,
      base::BindOnce(&HostChildURLLoaderFactoryBundle::RemoveObserver,
                     main_thread_host_bundle_->first, base::Unretained(this)));
}

void TrackedChildURLLoaderFactoryBundle::OnUpdate(
    std::unique_ptr<network::SharedURLLoaderFactoryInfo> info) {
  Update(base::WrapUnique(
      static_cast<ChildURLLoaderFactoryBundleInfo*>(info.release())));
}

// -----------------------------------------------------------------------------

HostChildURLLoaderFactoryBundle::HostChildURLLoaderFactoryBundle(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : observer_list_(std::make_unique<ObserverList>()),
      task_runner_(std::move(task_runner)) {
  DCHECK(RenderThread::Get()) << "HostChildURLLoaderFactoryBundle should live "
                                 "on the main renderer thread";
}

HostChildURLLoaderFactoryBundle::~HostChildURLLoaderFactoryBundle() = default;

std::unique_ptr<network::SharedURLLoaderFactoryInfo>
HostChildURLLoaderFactoryBundle::Clone() {
  auto pending_factories =
      base::WrapUnique(static_cast<ChildURLLoaderFactoryBundleInfo*>(
          ChildURLLoaderFactoryBundle::Clone().release()));

  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  auto main_thread_host_bundle_clone = std::make_unique<
      TrackedChildURLLoaderFactoryBundle::HostPtrAndTaskRunner>(AsWeakPtr(),
                                                                task_runner_);

  return std::make_unique<TrackedChildURLLoaderFactoryBundleInfo>(
      std::move(pending_factories->pending_default_factory()),
      std::move(pending_factories->pending_appcache_factory()),
      std::move(pending_factories->pending_scheme_specific_factories()),
      std::move(pending_factories->pending_isolated_world_factories()),
      std::move(pending_factories->direct_network_factory_remote()),
      std::move(pending_factories->pending_prefetch_loader_factory()),
      std::move(main_thread_host_bundle_clone),
      pending_factories->bypass_redirect_checks());
}

std::unique_ptr<network::SharedURLLoaderFactoryInfo>
HostChildURLLoaderFactoryBundle::CloneWithoutAppCacheFactory() {
  auto pending_factories =
      base::WrapUnique(static_cast<ChildURLLoaderFactoryBundleInfo*>(
          ChildURLLoaderFactoryBundle::CloneWithoutAppCacheFactory()
              .release()));

  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  auto main_thread_host_bundle_clone = std::make_unique<
      TrackedChildURLLoaderFactoryBundle::HostPtrAndTaskRunner>(AsWeakPtr(),
                                                                task_runner_);

  return std::make_unique<TrackedChildURLLoaderFactoryBundleInfo>(
      std::move(pending_factories->pending_default_factory()),
      std::move(pending_factories->pending_appcache_factory()),
      std::move(pending_factories->pending_scheme_specific_factories()),
      std::move(pending_factories->pending_isolated_world_factories()),
      std::move(pending_factories->direct_network_factory_remote()),
      std::move(pending_factories->pending_prefetch_loader_factory()),
      std::move(main_thread_host_bundle_clone),
      pending_factories->bypass_redirect_checks());
}

void HostChildURLLoaderFactoryBundle::UpdateThisAndAllClones(
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo> info) {
  DCHECK(RenderThread::Get()) << "Should run on the main renderer thread";
  DCHECK(observer_list_);

  auto partial_bundle = base::MakeRefCounted<ChildURLLoaderFactoryBundle>();
  static_cast<blink::URLLoaderFactoryBundle*>(partial_bundle.get())
      ->Update(std::move(info));

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
    TrackedChildURLLoaderFactoryBundle* observer,
    std::unique_ptr<ObserverPtrAndTaskRunner> observer_info) {
  DCHECK(RenderThread::Get()) << "Should run in the main renderer thread";
  DCHECK(observer_list_);
  (*observer_list_)[observer] = std::move(observer_info);
}

void HostChildURLLoaderFactoryBundle::RemoveObserver(
    TrackedChildURLLoaderFactoryBundle* observer) {
  DCHECK(RenderThread::Get()) << "Should run in the main renderer thread";
  DCHECK(observer_list_);
  observer_list_->erase(observer);
}

void HostChildURLLoaderFactoryBundle::NotifyUpdateOnMainOrWorkerThread(
    ObserverPtrAndTaskRunner* observer_bundle,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo> update_info) {
  observer_bundle->second->PostTask(
      FROM_HERE,
      base::BindOnce(&TrackedChildURLLoaderFactoryBundle::OnUpdate,
                     observer_bundle->first, std::move(update_info)));
}

}  // namespace content
