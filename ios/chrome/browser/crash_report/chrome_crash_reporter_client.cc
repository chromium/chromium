// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/chrome_crash_reporter_client.h"

#include "base/path_service.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"

void ChromeCrashReporterClient::Create() {
  static base::NoDestructor<ChromeCrashReporterClient> crash_client;
  crash_reporter::SetCrashReporterClient(crash_client.get());
}

ChromeCrashReporterClient::ChromeCrashReporterClient() {}

ChromeCrashReporterClient::~ChromeCrashReporterClient() {}

bool ChromeCrashReporterClient::GetCrashDumpLocation(
    base::FilePath* crash_dir) {
  return base::PathService::Get(ios::DIR_CRASH_DUMPS, crash_dir);
}

bool ChromeCrashReporterClient::GetCollectStatsConsent() {
  return breakpad_helper::UserEnabledUploading();
}

bool ChromeCrashReporterClient::IsRunningUnattended() {
  return false;
}
