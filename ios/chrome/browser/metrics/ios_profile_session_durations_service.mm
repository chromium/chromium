// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_profile_session_durations_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSProfileSessionDurationsService::IOSProfileSessionDurationsService(
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager)
    : KeyedService() {
  if (!sync_service && !identity_manager) {
    // |sync_service| and |identity_maanger| may be null for testing.
    return;
  }

  metrics_recorder_ =
      std::make_unique<syncer::SyncSessionDurationsMetricsRecorder>(
          sync_service, identity_manager);

  // |IOSProfileSessionDurationsService| is called explicitly each time a
  // session starts or ends. So there is no need to mimic what is done on
  // Android and to start a session in the constuctor of the service.
}

IOSProfileSessionDurationsService::~IOSProfileSessionDurationsService() =
    default;

void IOSProfileSessionDurationsService::Shutdown() {
  metrics_recorder_.reset();
}

void IOSProfileSessionDurationsService::OnSessionStarted(
    base::TimeTicks session_start) {
  metrics_recorder_->OnSessionStarted(session_start);
}

void IOSProfileSessionDurationsService::OnSessionEnded(
    base::TimeDelta session_length) {
  metrics_recorder_->OnSessionEnded(session_length);
}
