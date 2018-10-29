// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/blob_holder.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/bad_message.h"

namespace extensions {

namespace {

// Address to this variable used as the user data key.
const int kBlobHolderUserDataKey = 0;
}

// static
BlobHolder* BlobHolder::FromRenderProcessHost(
    content::RenderProcessHost* render_process_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_process_host);
  BlobHolder* existing = static_cast<BlobHolder*>(
      render_process_host->GetUserData(&kBlobHolderUserDataKey));

  if (existing)
    return existing;

  BlobHolder* new_instance = new BlobHolder(render_process_host);
  render_process_host->SetUserData(&kBlobHolderUserDataKey,
                                   base::WrapUnique(new_instance));
  return new_instance;
}

BlobHolder::~BlobHolder() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void BlobHolder::HoldBlobReference(std::unique_ptr<content::BlobHandle> blob) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!ContainsBlobHandle(blob.get()));

  std::string uuid = blob->GetUUID();
  held_blobs_.insert(make_pair(uuid, std::move(blob)));
}

BlobHolder::BlobHolder(content::RenderProcessHost* render_process_host)
    : render_process_host_(render_process_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

bool BlobHolder::ContainsBlobHandle(content::BlobHandle* handle) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto it = held_blobs_.cbegin(); it != held_blobs_.cend(); ++it) {
    if (it->second.get() == handle)
      return true;
  }

  return false;
}

void BlobHolder::DropBlobs(const std::vector<std::string>& blob_uuids) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto uuid_it = blob_uuids.cbegin(); uuid_it != blob_uuids.cend();
       ++uuid_it) {
    auto it = held_blobs_.find(*uuid_it);

    if (it != held_blobs_.end()) {
      held_blobs_.erase(it);
    } else {
      DLOG(ERROR) << "Tried to release a Blob we don't have ownership to."
                  << "UUID: " << *uuid_it;
      bad_message::ReceivedBadMessage(render_process_host_,
                                      bad_message::BH_BLOB_NOT_OWNED);
    }
  }
}

}  // namespace extensions
