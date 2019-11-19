// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/net_log_parameters.h"

#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_values.h"

namespace {

base::Value NetLogReadWriteDataParams(int index,
                                      int offset,
                                      int buf_len,
                                      bool truncate) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("index", index);
  dict.SetIntKey("offset", offset);
  dict.SetIntKey("buf_len", buf_len);
  if (truncate)
    dict.SetBoolKey("truncate", truncate);
  return dict;
}

base::Value NetLogReadWriteCompleteParams(int bytes_copied) {
  DCHECK_NE(bytes_copied, net::ERR_IO_PENDING);
  base::Value dict(base::Value::Type::DICTIONARY);
  if (bytes_copied < 0) {
    dict.SetIntKey("net_error", bytes_copied);
  } else {
    dict.SetIntKey("bytes_copied", bytes_copied);
  }
  return dict;
}

base::Value NetLogSparseOperationParams(int64_t offset, int buf_len) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("offset", net::NetLogNumberValue(offset));
  dict.SetIntKey("buf_len", buf_len);
  return dict;
}

base::Value NetLogSparseReadWriteParams(const net::NetLogSource& source,
                                        int child_len) {
  base::Value dict(base::Value::Type::DICTIONARY);
  source.AddToEventParameters(&dict);
  dict.SetIntKey("child_len", child_len);
  return dict;
}

}  // namespace

namespace disk_cache {

base::Value CreateNetLogParametersEntryCreationParams(const Entry* entry,
                                                      bool created) {
  DCHECK(entry);
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("key", entry->GetKey());
  dict.SetBoolKey("created", created);
  return dict;
}

void NetLogReadWriteData(const net::NetLogWithSource& net_log,
                         net::NetLogEventType type,
                         net::NetLogEventPhase phase,
                         int index,
                         int offset,
                         int buf_len,
                         bool truncate) {
  net_log.AddEntry(type, phase, [&] {
    return NetLogReadWriteDataParams(index, offset, buf_len, truncate);
  });
}

void NetLogReadWriteComplete(const net::NetLogWithSource& net_log,
                             net::NetLogEventType type,
                             net::NetLogEventPhase phase,
                             int bytes_copied) {
  net_log.AddEntry(type, phase,
                   [&] { return NetLogReadWriteCompleteParams(bytes_copied); });
}

void NetLogSparseOperation(const net::NetLogWithSource& net_log,
                           net::NetLogEventType type,
                           net::NetLogEventPhase phase,
                           int64_t offset,
                           int buf_len) {
  net_log.AddEntry(type, phase, [&] {
    return NetLogSparseOperationParams(offset, buf_len);
  });
}

void NetLogSparseReadWrite(const net::NetLogWithSource& net_log,
                           net::NetLogEventType type,
                           net::NetLogEventPhase phase,
                           const net::NetLogSource& source,
                           int child_len) {
  net_log.AddEntry(type, phase, [&] {
    return NetLogSparseReadWriteParams(source, child_len);
  });
}

base::Value CreateNetLogGetAvailableRangeResultParams(int64_t start,
                                                      int result) {
  base::Value dict(base::Value::Type::DICTIONARY);
  if (result > 0) {
    dict.SetIntKey("length", result);
    dict.SetKey("start", net::NetLogNumberValue(start));
  } else {
    dict.SetIntKey("net_error", result);
  }
  return dict;
}

}  // namespace disk_cache
