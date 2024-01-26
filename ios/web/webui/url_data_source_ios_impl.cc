// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/webui/url_data_source_ios_impl.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web/public/webui/url_data_source_ios.h"
#include "ios/web/webui/url_data_manager_ios_backend.h"

namespace web {

URLDataSourceIOSImpl::URLDataSourceIOSImpl(const std::string& source_name,
                                           URLDataSourceIOS* source)
    : source_name_(source_name), backend_(nullptr), source_(source) {}

URLDataSourceIOSImpl::~URLDataSourceIOSImpl() {
}

void URLDataSourceIOSImpl::SendResponse(
    int request_id,
    scoped_refptr<base::RefCountedMemory> bytes) {
  // Take a ref-pointer on entry so byte->Release() will always get called.
  if (URLDataManagerIOS::IsScheduledForDeletion(this)) {
    // We're scheduled for deletion. Servicing the request would result in
    // this->AddRef being invoked, even though the ref count is 0 and 'this' is
    // about to be deleted. If the AddRef were allowed through, when 'this' is
    // released it would be deleted again.
    //
    // This scenario occurs with DataSources that make history requests. Such
    // DataSources do a history query in `StartDataRequest` and the request is
    // live until the object is deleted (history requests don't up the ref
    // count). This means it's entirely possible for the DataSource to invoke
    // `SendResponse` between the time when there are no more refs and the time
    // when the object is deleted.
    return;
  }
  web::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&URLDataSourceIOSImpl::SendResponseOnIOThread,
                                this, request_id, std::move(bytes)));
}

void URLDataSourceIOSImpl::SendResponseOnIOThread(
    int request_id,
    scoped_refptr<base::RefCountedMemory> bytes) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  if (backend_)
    backend_->DataAvailable(request_id, bytes.get());
}

const ui::TemplateReplacements* URLDataSourceIOSImpl::GetReplacements() const {
  return nullptr;
}

bool URLDataSourceIOSImpl::ShouldReplaceI18nInJS() const {
  return false;
}

}  // namespace web
