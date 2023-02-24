// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/net_log_parameters.h"

#include <utility>

#include "base/check_op.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_values.h"

namespace {

base::Value::Dict NetLogReadWriteDataParams(int index,
                                            int offset,
                                            int buf_len,
                                            bool truncate) {
  base::Value::Dict dict;
  dict.Set("index", index);
  dict.Set("offset", offset);
  dict.Set("buf_len", buf_len);
  if (truncate)
    dict.Set("truncate", truncate);
  return dict;
}

base::Value::Dict NetLogReadWriteCompleteParams(int bytes_copied) {
  DCHECK_NE(bytes_copied, net::ERR_IO_PENDING);
  base::Value::Dict dict;
  if (bytes_copied < 0) {
    dict.Set("net_error", bytes_copied);
  } else {
    dict.Set("bytes_copied", bytes_copied);
  }
  return dict;
}

base::Value::Dict NetLogSparseOperationParams(int64_t offset, int buf_len) {
  base::Value::Dict dict;
  dict.Set("offset", net::NetLogNumberValue(offset));
  dict.Set("buf_len", buf_len);
  return dict;
}

base::Value::Dict NetLogSparseReadWriteParams(const net::NetLogSource& source,
                                              int child_len) {
  base::Value::Dict dict;
  source.AddToEventParameters(dict);
  dict.Set("child_len", child_len);
  return dict;
}

}  // namespace

namespace disk_cache {

base::Value::Dict CreateNetLogParametersEntryCreationParams(const Entry* entry,
                                                            bool created) {
  DCHECK(entry);
  base::Value::Dict dict;
  dict.Set("key", entry->GetKey());
  dict.Set("created", created);
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

base::Value::Dict CreateNetLogGetAvailableRangeResultParams(
    disk_cache::RangeResult result) {
  base::Value::Dict dict;
  if (result.net_error == net::OK) {
    dict.Set("length", result.available_len);
    dict.Set("start", net::NetLogNumberValue(result.start));
  } else {
    dict.Set("net_error", result.net_error);
  }
  return dict;
}

}  // namespace disk_cache
