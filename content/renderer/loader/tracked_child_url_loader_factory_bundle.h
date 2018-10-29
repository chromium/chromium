// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_TRACKED_CHILD_URL_LOADER_FACTORY_BUNDLE_H_
#define CONTENT_RENDERER_LOADER_TRACKED_CHILD_URL_LOADER_FACTORY_BUNDLE_H_

#include <memory>
#include <unordered_map>
#include <utility>

#include "base/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "content/renderer/loader/child_url_loader_factory_bundle.h"

namespace content {

class HostChildURLLoaderFactoryBundle;

// Holds the internal state of a |TrackedChildURLLoaderFactoryBundle| in a form
// that is safe to pass across sequences.
class CONTENT_EXPORT TrackedChildURLLoaderFactoryBundleInfo
    : public ChildURLLoaderFactoryBundleInfo {
 public:
  using HostPtrAndTaskRunner =
      std::pair<base::WeakPtr<HostChildURLLoaderFactoryBundle>,
                scoped_refptr<base::SequencedTaskRunner>>;

  TrackedChildURLLoaderFactoryBundleInfo();
  TrackedChildURLLoaderFactoryBundleInfo(
      network::mojom::URLLoaderFactoryPtrInfo default_factory_info,
      SchemeMap scheme_specific_factory_infos,
      OriginMap initiator_specific_factory_infos,
      PossiblyAssociatedURLLoaderFactoryPtrInfo direct_network_factory_info,
      network::mojom::URLLoaderFactoryPtrInfo prefetch_loader_factory_info,
      std::unique_ptr<HostPtrAndTaskRunner> main_thread_host_bundle,
      bool bypass_redirect_checks);
  ~TrackedChildURLLoaderFactoryBundleInfo() override;

  std::unique_ptr<HostPtrAndTaskRunner>& main_thread_host_bundle() {
    return main_thread_host_bundle_;
  }

 protected:
  // ChildURLLoaderFactoryBundleInfo overrides.
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override;

  std::unique_ptr<HostPtrAndTaskRunner> main_thread_host_bundle_;

  DISALLOW_COPY_AND_ASSIGN(TrackedChildURLLoaderFactoryBundleInfo);
};

// This class extends |ChildURLLoaderFactoryBundle| to support a host/observer
// tracking logic. There will be a single |HostChildURLLoaderFactoryBundle|
// owned by |RenderFrameImpl| which lives on the main thread, and multiple
// |TrackedChildURLLoaderFactoryBundle| on the worker thread (for Workers) or
// the main thread (for frames from 'window.open()').
// Both |Host/TrackedChildURLLoaderFactoryBundle::Clone()| can be used to create
// a tracked bundle to the original host bundle.
// These two classes are required to bring bundles back online in the event of
// Network Service crash.
class CONTENT_EXPORT TrackedChildURLLoaderFactoryBundle
    : public ChildURLLoaderFactoryBundle,
      public base::SupportsWeakPtr<TrackedChildURLLoaderFactoryBundle> {
 public:
  using HostPtrAndTaskRunner =
      std::pair<base::WeakPtr<HostChildURLLoaderFactoryBundle>,
                scoped_refptr<base::SequencedTaskRunner>>;

  // Posts a task to the host bundle on main thread to start tracking |this|.
  explicit TrackedChildURLLoaderFactoryBundle(
      std::unique_ptr<TrackedChildURLLoaderFactoryBundleInfo> info);

  // ChildURLLoaderFactoryBundle overrides.
  // Returns |std::unique_ptr<TrackedChildURLLoaderFactoryBundleInfo>|.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override;

 private:
  friend class HostChildURLLoaderFactoryBundle;

  // Posts a task to the host bundle on main thread to stop tracking |this|.
  ~TrackedChildURLLoaderFactoryBundle() override;

  // Helper method to post a task to the host bundle on main thread to start
  // tracking |this|.
  void AddObserverOnMainThread();

  // Helper method to post a task to the host bundle on main thread to start
  // tracking |this|.
  void RemoveObserverOnMainThread();

  // Callback method to receive updates from the host bundle.
  void OnUpdate(std::unique_ptr<network::SharedURLLoaderFactoryInfo> info);

  // |WeakPtr| and |TaskRunner| of the host bundle. Can be copied and passed
  // across sequences.
  std::unique_ptr<HostPtrAndTaskRunner> main_thread_host_bundle_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TrackedChildURLLoaderFactoryBundle);
};

// |HostChildURLLoaderFactoryBundle| lives entirely on the main thread, and all
// methods should be invoked on the main thread or through PostTask. See
// comments in |TrackedChildURLLoaderFactoryBundle| for details about the
// tracking logic.
class CONTENT_EXPORT HostChildURLLoaderFactoryBundle
    : public ChildURLLoaderFactoryBundle,
      public base::SupportsWeakPtr<HostChildURLLoaderFactoryBundle> {
 public:
  using ObserverPtrAndTaskRunner =
      std::pair<base::WeakPtr<TrackedChildURLLoaderFactoryBundle>,
                scoped_refptr<base::SequencedTaskRunner>>;
  using ObserverList =
      std::unordered_map<TrackedChildURLLoaderFactoryBundle*,
                         std::unique_ptr<ObserverPtrAndTaskRunner>>;

  explicit HostChildURLLoaderFactoryBundle(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // ChildURLLoaderFactoryBundle overrides.
  // Returns |std::unique_ptr<TrackedChildURLLoaderFactoryBundleInfo>|.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override;
  std::unique_ptr<network::SharedURLLoaderFactoryInfo>
  CloneWithoutDefaultFactory() override;
  bool IsHostChildURLLoaderFactoryBundle() const override;

  // Update this bundle with |info|, and post cloned |info| to tracked bundles.
  // Note: We don't need to worry about |direct_network_factory_| since it's
  // only used by |RendererBlinkPlatformImpl| and doesn't rely on this codepath.
  void UpdateThisAndAllClones(std::unique_ptr<URLLoaderFactoryBundleInfo> info);

 private:
  friend class TrackedChildURLLoaderFactoryBundle;

  ~HostChildURLLoaderFactoryBundle() override;

  // Must be called by the newly created |TrackedChildURLLoaderFactoryBundle|.
  // |TrackedChildURLLoaderFactoryBundle*| serves as the key and doesn't have to
  // remain valid.
  void AddObserver(TrackedChildURLLoaderFactoryBundle* observer,
                   std::unique_ptr<ObserverPtrAndTaskRunner> observer_info);

  // Must be called by the observer before it was destroyed.
  // |TrackedChildURLLoaderFactoryBundle*| serves as the key and doesn't have to
  // remain valid.
  void RemoveObserver(TrackedChildURLLoaderFactoryBundle* observer);

  // Post an update to the tracked bundle on the worker thread (for Workers) or
  // the main thread (for frames from 'window.open()'). Safe to use after the
  // tracked bundle has been destroyed.
  void NotifyUpdateOnMainOrWorkerThread(
      ObserverPtrAndTaskRunner* observer_bundle,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo> update_info);

  // Contains |WeakPtr| and |TaskRunner| to tracked bundles.
  std::unique_ptr<ObserverList> observer_list_ = nullptr;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(HostChildURLLoaderFactoryBundle);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_TRACKED_CHILD_URL_LOADER_FACTORY_BUNDLE_H_
