// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/url_data_source_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"

namespace content {

URLDataSourceImpl::URLDataSourceImpl(const std::string& source_name,
                                     std::unique_ptr<URLDataSource> source)
    : source_name_(source_name), source_(std::move(source)) {}

URLDataSourceImpl::~URLDataSourceImpl() {
}

void URLDataSourceImpl::SendResponse(
    int request_id,
    scoped_refptr<base::RefCountedMemory> bytes) {
  if (URLDataManager::IsScheduledForDeletion(this)) {
    // We're scheduled for deletion. Servicing the request would result in
    // this->AddRef being invoked, even though the ref count is 0 and 'this' is
    // about to be deleted. If the AddRef were allowed through, when 'this' is
    // released it would be deleted again.
    //
    // This scenario occurs with DataSources that make history requests. Such
    // DataSources do a history query in |StartDataRequest| and the request is
    // live until the object is deleted (history requests don't up the ref
    // count). This means it's entirely possible for the DataSource to invoke
    // |SendResponse| between the time when there are no more refs and the time
    // when the object is deleted.
    return;
  }
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&URLDataSourceImpl::SendResponseOnIOThread, this,
                     request_id, std::move(bytes)));
}

bool URLDataSourceImpl::IsWebUIDataSourceImpl() const {
  return false;
}

void URLDataSourceImpl::SendResponseOnIOThread(
    int request_id,
    scoped_refptr<base::RefCountedMemory> bytes) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (backend_)
    backend_->DataAvailable(request_id, bytes.get());
}

const ui::TemplateReplacements* URLDataSourceImpl::GetReplacements() const {
  return nullptr;
}

}  // namespace content
