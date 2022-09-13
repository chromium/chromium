// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/gcm_store.h"

namespace gcm {

GCMStore::LoadResult::LoadResult()
    : success(false),
      store_does_not_exist(false),
      device_android_id(0),
      device_security_token(0) {
}

GCMStore::LoadResult::~LoadResult() {}

void GCMStore::LoadResult::Reset() {
  device_android_id = 0;
  device_security_token = 0;
  registrations.clear();
  incoming_messages.clear();
  outgoing_messages.clear();
  gservices_settings.clear();
  gservices_digest.clear();
  last_checkin_time = base::Time::FromInternalValue(0LL);
  last_token_fetch_time = base::Time::FromInternalValue(0LL);
  last_checkin_accounts.clear();
  account_mappings.clear();
  heartbeat_intervals.clear();
  success = false;
  store_does_not_exist = false;
  instance_id_data.clear();
}

GCMStore::GCMStore() {}

GCMStore::~GCMStore() {}

}  // namespace gcm
