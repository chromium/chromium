// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_JOB_MANAGER_H_
#define NET_URL_REQUEST_URL_REQUEST_JOB_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "net/base/net_export.h"
#include "net/url_request/url_request.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}  // namespace base

namespace net {

// This class is responsible for managing the set of protocol factories and
// request interceptors that determine how an URLRequestJob gets created to
// handle an URLRequest.
//
// MULTI-THREADING NOTICE:
//   URLRequest is designed to have all consumers on a single thread, and
//   so no attempt is made to support Interceptor instances being
//   registered/unregistered or in any way poked on multiple threads.
class NET_EXPORT URLRequestJobManager {
 public:
  // Returns the singleton instance.
  static URLRequestJobManager* GetInstance();

  // Instantiate an URLRequestJob implementation based on the registered
  // interceptors and protocol factories.  This will always succeed in
  // returning a job unless we are--in the extreme case--out of memory.
  URLRequestJob* CreateJob(URLRequest* request,
                           NetworkDelegate* network_delegate) const;

  // Returns true if the manager has a built-in handler for |scheme|.
  static bool SupportsScheme(const std::string& scheme);

 private:
  friend struct base::DefaultSingletonTraits<URLRequestJobManager>;

  URLRequestJobManager();
  ~URLRequestJobManager();

  // The first call to this function sets the allowed thread.  This way we avoid
  // needing to define that thread externally.  Since we expect all callers to
  // be on the same thread, we don't worry about threads racing to set the
  // allowed thread.
  bool IsAllowedThread() const {
#if 0
    return thread_checker_.CalledOnValidThread();
  }

  // We use this to assert that CreateJob and the registration functions all
  // run on the same thread.
  base::ThreadChecker thread_checker_;
#else
    // The previous version of this check used GetCurrentThread on Windows to
    // get thread handles to compare. Unfortunately, GetCurrentThread returns
    // a constant pseudo-handle (0xFFFFFFFE), and therefore IsAllowedThread
    // always returned true. The above code that's turned off is the correct
    // code, but causes the tree to turn red because some caller isn't
    // respecting our thread requirements. We're turning off the check for now;
    // bug http://b/issue?id=1338969 has been filed to fix things and turn the
    // check back on.
    return true;
  }
#endif

  DISALLOW_COPY_AND_ASSIGN(URLRequestJobManager);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_JOB_MANAGER_H_
