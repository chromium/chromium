// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_IOS_PROFILE_SESSION_DURATIONS_SERVICE_H_
#define IOS_CHROME_BROWSER_METRICS_IOS_PROFILE_SESSION_DURATIONS_SERVICE_H_

#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/driver/sync_session_durations_metrics_recorder.h"

namespace signin {
class IdentityManager;
}
namespace syncer {
class SyncService;
}

// Tracks the active browsing time that the user spends signed in and/or syncing
// as fraction of their total browsing time.
class IOSProfileSessionDurationsService : public KeyedService {
 public:
  // Callers must ensure that the parameters outlive this object.
  // If `sync_service` and `identity_manager` are null, then this object does
  // not monitor profile session durations.
  IOSProfileSessionDurationsService(syncer::SyncService* sync_service,
                                    signin::IdentityManager* identity_manager);

  IOSProfileSessionDurationsService(const IOSProfileSessionDurationsService&) =
      delete;
  IOSProfileSessionDurationsService& operator=(
      const IOSProfileSessionDurationsService&) = delete;

  ~IOSProfileSessionDurationsService() override;

  // KeyedService:
  void Shutdown() override;

  // A session is defined as the time spent with the application in foreground
  // (the time duration between the application enters foreground until the
  // application enters background).
  virtual void OnSessionStarted(base::TimeTicks session_start);
  virtual void OnSessionEnded(base::TimeDelta session_length);
  virtual bool IsSessionActive();

  bool IsSignedIn() const;
  bool IsSyncing() const;

 private:
  std::unique_ptr<syncer::SyncSessionDurationsMetricsRecorder>
      metrics_recorder_;

  bool is_session_active_ = false;
};

#endif  // IOS_CHROME_BROWSER_METRICS_IOS_PROFILE_SESSION_DURATIONS_SERVICE_H_
