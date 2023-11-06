// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_net_log_parameters.h"

#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/simple/simple_entry_impl.h"
#include "net/log/net_log_capture_mode.h"

namespace {

base::Value::Dict NetLogSimpleEntryConstructionParams(
    const disk_cache::SimpleEntryImpl* entry) {
  base::Value::Dict dict;
  dict.Set("entry_hash",
           base::StringPrintf("0x%016" PRIx64, entry->entry_hash()));
  return dict;
}

base::Value::Dict NetLogSimpleEntryCreationParams(
    const disk_cache::SimpleEntryImpl* entry,
    int net_error) {
  base::Value::Dict dict;
  dict.Set("net_error", net_error);
  if (net_error == net::OK)
    dict.Set("key", entry->key().value_or("(nullopt)"));
  return dict;
}

}  // namespace

namespace disk_cache {

void NetLogSimpleEntryConstruction(const net::NetLogWithSource& net_log,
                                   net::NetLogEventType type,
                                   net::NetLogEventPhase phase,
                                   const SimpleEntryImpl* entry) {
  DCHECK(entry);
  net_log.AddEntry(type, phase,
                   [&] { return NetLogSimpleEntryConstructionParams(entry); });
}

void NetLogSimpleEntryCreation(const net::NetLogWithSource& net_log,
                               net::NetLogEventType type,
                               net::NetLogEventPhase phase,
                               const SimpleEntryImpl* entry,
                               int net_error) {
  DCHECK(entry);
  net_log.AddEntry(type, phase, [&] {
    return NetLogSimpleEntryCreationParams(entry, net_error);
  });
}

}  // namespace disk_cache
