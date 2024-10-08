// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_data_service_base.h"

#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "components/webdata/common/web_database_backend.h"
#include "components/webdata/common/web_database_service.h"

WebDataServiceBase::WebDataServiceBase(
    scoped_refptr<WebDatabaseService> wdbs,
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner)
    : base::RefCountedDeleteOnSequence<WebDataServiceBase>(ui_task_runner),
      wdbs_(wdbs) {}

void WebDataServiceBase::ShutdownOnUISequence() {}

void WebDataServiceBase::Init(ProfileErrorCallback callback) {
  DCHECK(wdbs_);
  wdbs_->RegisterDBErrorCallback(std::move(callback));
  // This schedules an InitDatabase to run on the WebDatabaseBackend DB
  // sequence, to obtain any errors encountered during initialization.
  //
  // Note: Actual database initialization will not occur until
  // `LoadDatabase()` on the `WebDatabaseService` has been called, which has
  // typically already happened by this point, but need not have been.
  wdbs_->GetDbSequence()->PostTask(
      FROM_HERE,
      BindOnce(&WebDatabaseBackend::InitDatabase, wdbs_->GetBackend()));
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

bool WebDataServiceBase::UsesInMemoryDatabaseForTest() const {
  CHECK(wdbs_);
  return wdbs_->UsesInMemoryDatabaseForTest();  // IN-TEST
}

WebDataServiceBase::~WebDataServiceBase() = default;
