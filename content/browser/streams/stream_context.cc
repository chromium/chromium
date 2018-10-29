// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/streams/stream_context.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/streams/stream_registry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using base::UserDataAdapter;

namespace {

const char kStreamContextKeyName[] = "content_stream_context";

}  // namespace

namespace content {

StreamContext::StreamContext() {}

StreamContext* StreamContext::GetFor(BrowserContext* context) {
  if (!context->GetUserData(kStreamContextKeyName)) {
    scoped_refptr<StreamContext> stream = new StreamContext();
    context->SetUserData(
        kStreamContextKeyName,
        std::make_unique<UserDataAdapter<StreamContext>>(stream.get()));
    // Check first to avoid memory leak in unittests.
    if (BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&StreamContext::InitializeOnIOThread, stream));
    }
  }

  return UserDataAdapter<StreamContext>::Get(context, kStreamContextKeyName);
}

void StreamContext::InitializeOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  registry_.reset(new StreamRegistry());
}

StreamContext::~StreamContext() {}

void StreamContext::DeleteOnCorrectThread() const {
  // In many tests, there isn't a valid IO thread.  In that case, just delete on
  // the current thread.
  // TODO(zork): Remove this custom deleter, and fix the leaks in all the
  // tests.
  if (BrowserThread::IsThreadInitialized(BrowserThread::IO) &&
      !BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, this);
    return;
  }
  delete this;
}

}  // namespace content
