// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_UTIL_H_
#define NET_LOG_NET_LOG_UTIL_H_

#include <memory>
#include <set>

#include "net/base/net_export.h"
#include "net/log/net_log.h"

namespace net {

class URLRequestContext;

// Utility methods for creating NetLog dumps.

// Creates a dictionary containing a legend for net/ constants.
NET_EXPORT base::Value::Dict GetNetConstants();

// Retrieves a dictionary containing information about the current state of
// |context|.
//
// May only be called on |context|'s thread.
NET_EXPORT base::Value::Dict GetNetInfo(URLRequestContext* context);

// Takes in a set of contexts and a NetLog::Observer, and passes in
// NetLog::Entries to the observer for certain NetLogSources with pending
// events.  This allows requests that were ongoing when logging was started to
// have an initial event that has some information.  This is particularly useful
// for hung requests.  Note that these calls are not protected by the NetLog's
// lock, so this should generally be invoked before the observer starts watching
// the NetLog.
//
// All members of |contexts| must be using the same NetLog, and live on the
// current thread.
//
// Currently only creates events for URLRequests.
//
// The reason for not returning a list of NetLog::Entries is that entries don't
// own most of their data, so it's simplest just to pass them in to the observer
// directly while their data is on the stack.
NET_EXPORT void CreateNetLogEntriesForActiveObjects(
    const std::set<URLRequestContext*>& contexts,
    NetLog::ThreadSafeObserver* observer);

}  // namespace net

#endif  // NET_LOG_NET_LOG_UTIL_H_
