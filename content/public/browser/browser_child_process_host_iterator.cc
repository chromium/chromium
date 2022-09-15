// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_child_process_host_iterator.h"

#include "base/check_op.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

BrowserChildProcessHostIterator::BrowserChildProcessHostIterator()
    : all_(true), process_type_(PROCESS_TYPE_UNKNOWN) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
      << "BrowserChildProcessHostIterator must be used on the IO thread.";
  iterator_ = BrowserChildProcessHostImpl::GetIterator()->begin();
}

BrowserChildProcessHostIterator::BrowserChildProcessHostIterator(int type)
    : all_(false), process_type_(type) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
      << "BrowserChildProcessHostIterator must be used on the IO thread.";
  DCHECK_NE(PROCESS_TYPE_RENDERER, type) <<
      "BrowserChildProcessHostIterator doesn't work for renderer processes; "
      "try RenderProcessHost::AllHostsIterator() instead.";
  iterator_ = BrowserChildProcessHostImpl::GetIterator()->begin();
  if (!Done() && (*iterator_)->GetData().process_type != process_type_)
    ++(*this);
}

BrowserChildProcessHostIterator::~BrowserChildProcessHostIterator() {
}

bool BrowserChildProcessHostIterator::operator++() {
  CHECK(!Done());
  do {
    ++iterator_;
    if (Done())
      break;

    if (!all_ && (*iterator_)->GetData().process_type != process_type_)
      continue;

    return true;
  } while (true);

  return false;
}

bool BrowserChildProcessHostIterator::Done() {
  return iterator_ == BrowserChildProcessHostImpl::GetIterator()->end();
}

const ChildProcessData& BrowserChildProcessHostIterator::GetData() {
  CHECK(!Done());
  return (*iterator_)->GetData();
}

bool BrowserChildProcessHostIterator::Send(IPC::Message* message) {
  CHECK(!Done());
  return (*iterator_)->Send(message);
}

BrowserChildProcessHostDelegate*
    BrowserChildProcessHostIterator::GetDelegate() {
  return (*iterator_)->delegate();
}

ChildProcessHost* BrowserChildProcessHostIterator::GetHost() {
  CHECK(!Done());
  return (*iterator_)->GetHost();
}

}  // namespace content
