// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_THREAD_WEB_THREAD_DELEGATE_H_
#define IOS_WEB_PUBLIC_THREAD_WEB_THREAD_DELEGATE_H_

namespace web {

// WebThread::SetDelegate was deprecated, this is now only used by
// WebThread::SetIOThreadDelegate.
//
// If registered as such, it will schedule to run Init() before the
// message loop begins, and receive a CleanUp() call right after the message
// loop ends (and before the WebThread has done its own clean-up).

// A delegate for //web embedders to perform extra initialization/cleanup on
// WebThread::IO.
class WebThreadDelegate {
 public:
  virtual ~WebThreadDelegate() {}

  // Called prior to completing initialization of WebThread::IO.
  virtual void Init() = 0;

  // Called during teardown of WebThread::IO.
  virtual void CleanUp() = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_THREAD_WEB_THREAD_DELEGATE_H_
