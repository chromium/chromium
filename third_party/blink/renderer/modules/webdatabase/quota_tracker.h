/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_QUOTA_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_QUOTA_TRACKER_H_

#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class SecurityOrigin;

class MODULES_EXPORT QuotaTracker {
  USING_FAST_MALLOC(QuotaTracker);

 public:
  static QuotaTracker& Instance();

  QuotaTracker(const QuotaTracker&) = delete;
  QuotaTracker& operator=(const QuotaTracker&) = delete;

  void GetDatabaseSizeAndSpaceAvailableToOrigin(const SecurityOrigin*,
                                                const String& database_name,
                                                uint64_t* database_size,
                                                uint64_t* space_available);
  void UpdateDatabaseSize(const SecurityOrigin*,
                          const String& database_name,
                          uint64_t database_size);

 private:
  QuotaTracker() = default;

  typedef HashMap<String, uint64_t> SizeMap;
  HashMap<String, SizeMap> database_sizes_ GUARDED_BY(data_guard_);
  base::Lock data_guard_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_QUOTA_TRACKER_H_
