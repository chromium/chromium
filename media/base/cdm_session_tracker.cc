// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_session_tracker.h"

#include "base/functional/callback.h"
#include "base/not_fatal_until.h"

namespace media {

CdmSessionTracker::CdmSessionTracker() = default;

CdmSessionTracker::~CdmSessionTracker() {
  DCHECK(!HasRemainingSessions());
}

void CdmSessionTracker::AddSession(const std::string& session_id) {
  DCHECK(session_ids_.find(session_id) == session_ids_.end());
  session_ids_.insert(session_id);
}

void CdmSessionTracker::RemoveSession(const std::string& session_id) {
  auto it = session_ids_.find(session_id);
  CHECK(it != session_ids_.end(), base::NotFatalUntil::M130);
  session_ids_.erase(it);
}

void CdmSessionTracker::CloseRemainingSessions(
    const SessionClosedCB& session_closed_cb,
    CdmSessionClosedReason reason) {
  std::unordered_set<std::string> session_ids;
  session_ids.swap(session_ids_);

  for (const auto& session_id : session_ids)
    session_closed_cb.Run(session_id, reason);
}

bool CdmSessionTracker::HasRemainingSessions() const {
  return !session_ids_.empty();
}

}  // namespace media
