// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_NET_LOG_PARAMETERS_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_NET_LOG_PARAMETERS_H_

#include "net/log/net_log_with_source.h"

// This file augments the functions in net/disk_cache/net_log_parameters.h to
// include ones that deal with specifics of the Simple Cache backend.
namespace disk_cache {

class SimpleEntryImpl;

// Logs the construction of a SimpleEntryImpl. Contains the entry's hash.
// |entry| can't be nullptr.
void NetLogSimpleEntryConstruction(const net::NetLogWithSource& net_log,
                                   net::NetLogEventType type,
                                   net::NetLogEventPhase phase,
                                   const SimpleEntryImpl* entry);

// Logs a call to |CreateEntry| or |OpenEntry| on a SimpleEntryImpl. Contains
// the |net_error| and, if successful, the entry's key. |entry| can't be
// nullptr.
void NetLogSimpleEntryCreation(const net::NetLogWithSource& net_log,
                               net::NetLogEventType type,
                               net::NetLogEventPhase phase,
                               const SimpleEntryImpl* entry,
                               int net_error);

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_NET_LOG_PARAMETERS_H_
