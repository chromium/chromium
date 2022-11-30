// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_SESSION_TRACKER_H_
#define MEDIA_BASE_CDM_SESSION_TRACKER_H_

#include <stdint.h>

#include <string>
#include <unordered_set>

#include "media/base/content_decryption_module.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT CdmSessionTracker {
 public:
  CdmSessionTracker();

  CdmSessionTracker(const CdmSessionTracker&) = delete;
  CdmSessionTracker& operator=(const CdmSessionTracker&) = delete;

  ~CdmSessionTracker();

  // Adds `session_id` to the list of sessions being tracked.
  void AddSession(const std::string& session_id);

  // Removes `session_id` from the list of sessions being tracked.
  void RemoveSession(const std::string& session_id);

  // Calls `session_closed_cb` with `reason` on any remaining sessions in the
  // list and then clear the list.
  void CloseRemainingSessions(const SessionClosedCB& session_closed_cb,
                              CdmSessionClosedReason reason);

  // Returns whether there are any remaining sessions being tracked.
  bool HasRemainingSessions() const;

 private:
  std::unordered_set<std::string> session_ids_;
};

}  // namespace media

#endif  // MEDIA_BASE_CDM_SESSION_TRACKER_H_
