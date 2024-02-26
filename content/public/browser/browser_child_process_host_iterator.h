// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_HOST_ITERATOR_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_HOST_ITERATOR_H_

#include <list>

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"

namespace IPC {
class Message;
}

namespace content {
class BrowserChildProcessHostDelegate;
class BrowserChildProcessHostImpl;
struct ChildProcessData;
class ChildProcessHost;

// This class allows iteration through either all child processes, or ones of a
// specific type, depending on which constructor is used.  Note that this should
// be done from the IO thread and that the iterator should not be kept around as
// it may be invalidated on subsequent event processing in the event loop.
class CONTENT_EXPORT BrowserChildProcessHostIterator {
 public:
  BrowserChildProcessHostIterator();
  explicit BrowserChildProcessHostIterator(int type);
  ~BrowserChildProcessHostIterator();

  // These methods work on the current iterator object. Only call them if
  // Done() returns false.
  bool operator++();
  bool Done();
  const ChildProcessData& GetData();
  bool Send(IPC::Message* message);
  BrowserChildProcessHostDelegate* GetDelegate();
  ChildProcessHost* GetHost();

 private:
  bool all_;
  int process_type_;
  std::list<raw_ptr<BrowserChildProcessHostImpl, CtnExperimental>>::iterator
      iterator_;
};

// Helper class so that subclasses of BrowserChildProcessHostDelegate can be
// iterated with no casting needing. Note that because of the components build,
// this class can only be used by BCPHD implementations that live in content,
// otherwise link errors will result.
template <class T>
class CONTENT_EXPORT BrowserChildProcessHostTypeIterator
    : public BrowserChildProcessHostIterator {
 public:
  explicit BrowserChildProcessHostTypeIterator(int process_type)
      : BrowserChildProcessHostIterator(process_type) {}
  T* operator->() {
    return static_cast<T*>(GetDelegate());
  }
  T* operator*() {
    return static_cast<T*>(GetDelegate());
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_HOST_ITERATOR_H_
