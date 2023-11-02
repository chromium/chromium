// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_data_service_base.h"

#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "components/webdata/common/web_database_service.h"

WebDataServiceBase::WebDataServiceBase(
    scoped_refptr<WebDatabaseService> wdbs,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
    : base::RefCountedDeleteOnSequence<WebDataServiceBase>(ui_task_runner),
      wdbs_(wdbs) {}

void WebDataServiceBase::ShutdownOnUISequence() {}

void WebDataServiceBase::Init(ProfileErrorCallback callback) {
  DCHECK(wdbs_);
  wdbs_->RegisterDBErrorCallback(std::move(callback));
  wdbs_->LoadDatabase();
}

void WebDataServiceBase::ShutdownDatabase() {
  if (wdbs_)
    wdbs_->ShutdownDatabase();
}

void WebDataServiceBase::CancelRequest(Handle h) {
  if (wdbs_)
    wdbs_->CancelRequest(h);
}

WebDatabase* WebDataServiceBase::GetDatabase() {
  return wdbs_ ? wdbs_->GetDatabaseOnDB() : nullptr;
}

WebDataServiceBase::~WebDataServiceBase() = default;
