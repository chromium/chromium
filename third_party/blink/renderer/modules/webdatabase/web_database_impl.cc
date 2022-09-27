// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webdatabase/web_database_impl.h"

#include "third_party/blink/renderer/modules/webdatabase/database_tracker.h"
#include "third_party/blink/renderer/modules/webdatabase/quota_tracker.h"

namespace blink {

namespace {

WebDatabaseImpl& GetWebDatabase() {
  DEFINE_STATIC_LOCAL(WebDatabaseImpl, web_database, ());
  return web_database;
}

}  // namespace

WebDatabaseImpl::WebDatabaseImpl() = default;

WebDatabaseImpl::~WebDatabaseImpl() = default;

void WebDatabaseImpl::Bind(
    mojo::PendingReceiver<mojom::blink::WebDatabase> receiver) {
  // This should be called only once per process on RenderProcessWillLaunch.
  DCHECK(!GetWebDatabase().receiver_.is_bound());
  GetWebDatabase().receiver_.Bind(std::move(receiver));
}

void WebDatabaseImpl::UpdateSize(
    const scoped_refptr<const SecurityOrigin>& origin,
    const String& name,
    int64_t size) {
  DCHECK(origin->CanAccessDatabase());
  QuotaTracker::Instance().UpdateDatabaseSize(origin.get(), name, size);
}

void WebDatabaseImpl::CloseImmediately(
    const scoped_refptr<const SecurityOrigin>& origin,
    const String& name) {
  DCHECK(origin->CanAccessDatabase());
  DatabaseTracker::Tracker().CloseDatabasesImmediately(origin.get(), name);
}

}  // namespace blink
