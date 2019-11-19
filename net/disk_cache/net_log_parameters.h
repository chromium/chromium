// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_NET_LOG_PARAMETERS_H_
#define NET_DISK_CACHE_NET_LOG_PARAMETERS_H_

#include <stdint.h>

#include <string>

#include "net/log/net_log_with_source.h"

namespace net {
struct NetLogSource;
}

namespace base {
class Value;
}

// This file contains a set of functions to create NetLogParametersCallbacks
// shared by EntryImpls and MemEntryImpls.
namespace disk_cache {

class Entry;

// Creates NetLog parameters for the creation of an Entry.  Contains the Entry's
// key and whether it was created or opened. |entry| can't be nullptr, must
// support GetKey().
base::Value CreateNetLogParametersEntryCreationParams(const Entry* entry,
                                                      bool created);

// Logs an event for the start of a non-sparse read or write of an Entry. For
// reads, |truncate| must be false.
void NetLogReadWriteData(const net::NetLogWithSource& net_log,
                         net::NetLogEventType type,
                         net::NetLogEventPhase phase,
                         int index,
                         int offset,
                         int buf_len,
                         bool truncate);

// Logs an event for when a non-sparse read or write completes.  For reads,
// |truncate| must be false. |bytes_copied| is either the number of bytes copied
// or a network error code.  |bytes_copied| must not be ERR_IO_PENDING, as it's
// not a valid result for an operation.
void NetLogReadWriteComplete(const net::NetLogWithSource& net_log,
                             net::NetLogEventType type,
                             net::NetLogEventPhase phase,
                             int bytes_copied);

// Logs an event for when a sparse operation is started.
void NetLogSparseOperation(const net::NetLogWithSource& net_log,
                           net::NetLogEventType type,
                           net::NetLogEventPhase phase,
                           int64_t offset,
                           int buf_len);

// Logs an event for when a read or write for a sparse entry's child is started.
void NetLogSparseReadWrite(const net::NetLogWithSource& net_log,
                           net::NetLogEventType type,
                           net::NetLogEventPhase phase,
                           const net::NetLogSource& source,
                           int child_len);

// Creates NetLog parameters for when a call to GetAvailableRange returns.
base::Value CreateNetLogGetAvailableRangeResultParams(int64_t start,
                                                      int result);

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_NET_LOG_PARAMETERS_H_
