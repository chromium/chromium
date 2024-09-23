// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_DURATIONS_SERVICE_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_DURATIONS_SERVICE_H_

#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace password_manager {
class PasswordSessionDurationsMetricsRecorder;
}

namespace signin {
class IdentityManager;
}

namespace signin_metrics {
enum class SingleProfileSigninStatus;
}

namespace syncer {
class SyncService;
class SyncSessionDurationsMetricsRecorder;
}

namespace unified_consent {
class MsbbSessionDurationsMetricsRecorder;
}

// Tracks the active browsing time that the user spends signed in and/or syncing
// as fraction of their total browsing time.
class IOSProfileSessionDurationsService : public KeyedService {
 public:
  // Callers must ensure that the parameters outlive this object.
  // `sync_service`, `pref_service` and `identity_manager` must be non-null.
  IOSProfileSessionDurationsService(syncer::SyncService* sync_service,
                                    PrefService* pref_service,
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

  signin_metrics::SingleProfileSigninStatus GetSigninStatus() const;
  bool IsSyncing() const;

 protected:
  // Test-only constructor that doesn't do anything and fake implementations
  // can invoke.
  IOSProfileSessionDurationsService();

 private:
  std::unique_ptr<syncer::SyncSessionDurationsMetricsRecorder>
      sync_metrics_recorder_;
  std::unique_ptr<unified_consent::MsbbSessionDurationsMetricsRecorder>
      msbb_metrics_recorder_;
  std::unique_ptr<password_manager::PasswordSessionDurationsMetricsRecorder>
      password_metrics_recorder_;

  bool is_session_active_ = false;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_DURATIONS_SERVICE_H_
