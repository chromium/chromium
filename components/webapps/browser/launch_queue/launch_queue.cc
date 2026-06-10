// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/launch_queue/launch_queue.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/launch_queue/launch_queue_delegate.h"
#include "content/public/browser/file_system_access_entry_factory.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom.h"
#include "third_party/blink/public/mojom/web_launch/web_launch.mojom.h"
#include "url/origin.h"

namespace webapps {

namespace {

class EntriesBuilder {
 public:
  EntriesBuilder(content::WebContents* web_contents,
                 const GURL& launch_url,
                 size_t expected_number_of_entries)
      : entry_factory_(web_contents->GetPrimaryMainFrame()
                           ->GetProcess()
                           ->GetStoragePartition()
                           ->GetFileSystemAccessEntryFactory()),
        context_(blink::StorageKey::CreateFirstParty(
                     url::Origin::Create(launch_url)),
                 launch_url,
                 content::GlobalRenderFrameHostId(
                     web_contents->GetPrimaryMainFrame()
                         ->GetProcess()
                         ->GetDeprecatedID(),
                     web_contents->GetPrimaryMainFrame()->GetRoutingID())) {
    entries_.reserve(expected_number_of_entries);
  }

  void AddFileEntry(const content::PathInfo& path_info) {
    entries_.push_back(entry_factory_->CreateFileEntryFromPath(
        context_, path_info,
        content::FileSystemAccessEntryFactory::UserAction::kSave));
  }

  void AddDirectoryEntry(const content::PathInfo& path_info) {
    entries_.push_back(entry_factory_->CreateDirectoryEntryFromPath(
        context_, path_info,
        content::FileSystemAccessEntryFactory::UserAction::kOpen));
  }

  std::vector<blink::mojom::FileSystemAccessEntryPtr> Build() {
    return std::move(entries_);
  }

 private:
  std::vector<blink::mojom::FileSystemAccessEntryPtr> entries_;
  scoped_refptr<content::FileSystemAccessEntryFactory> entry_factory_;
  content::FileSystemAccessEntryFactory::BindingContext context_;
};

}  // namespace

LaunchQueue::LaunchQueue(content::WebContents* web_contents,
                         std::unique_ptr<LaunchQueueDelegate> delegate)
    : web_contents_(web_contents), delegate_(std::move(delegate)) {}

LaunchQueue::~LaunchQueue() = default;

void LaunchQueue::Enqueue(LaunchParams launch_params) {
  DCHECK(delegate_->IsInScope(launch_params, launch_params.target_url()))
      << launch_params.target_url().spec();

  if (!delegate_->IsValidLaunchParams(launch_params)) {
    launch_params.clear_paths();
    launch_params.clear_dir();
  }

  SendLaunchParams(std::move(launch_params),
                   web_contents_->GetLastCommittedURL());
}

bool LaunchQueue::IsInScope(const LaunchParams& launch_params,
                            const GURL& url) const {
  return delegate_->IsInScope(launch_params, url);
}

void LaunchQueue::FlushForTesting() const {
  CHECK_IS_TEST();
  mojo::AssociatedRemote<blink::mojom::WebLaunchService> launch_service;
  web_contents_->GetPrimaryMainFrame()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&launch_service);
  launch_service.FlushForTesting();  // IN-TEST
}

void LaunchQueue::SendLaunchParams(LaunchParams launch_params,
                                   const GURL& current_url) {
  // TODO(dmurph): Figure out why this is failing.
  // https://crbug.com/2546057
  DCHECK(delegate_->IsInScope(launch_params, current_url))
      << current_url.spec();
  CHECK(launch_params.target_url().is_valid());
  mojo::AssociatedRemote<blink::mojom::WebLaunchService> launch_service;
  web_contents_->GetPrimaryMainFrame()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&launch_service);
  DCHECK(launch_service);

  std::vector<blink::mojom::FileSystemAccessEntryPtr> files;

  if (!launch_params.paths().empty() || !launch_params.dir().empty()) {
    EntriesBuilder entries_builder(web_contents_, launch_params.target_url(),
                                   launch_params.paths().size() + 1);
    if (!launch_params.dir().empty()) {
      entries_builder.AddDirectoryEntry(
          delegate_->GetPathInfo(launch_params.dir()));
    }

    for (const auto& path : launch_params.paths()) {
      entries_builder.AddFileEntry(delegate_->GetPathInfo(path));
    }

    files = entries_builder.Build();
  }
  launch_service->EnqueueLaunchParams(
      launch_params.target_url(),
      launch_params.time_navigation_started_for_enqueue(),
      launch_params.started_new_navigation(), std::move(files));
}

}  // namespace webapps
