// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/gservices_switches.h"

namespace switches {

// Sets the checkin service endpoint that will be used for performing Google
// Cloud Messaging checkins.
const char kGCMCheckinURL[] = "gcm-checkin-url";

// Sets the Mobile Connection Server endpoint that will be used for Google
// Cloud Messaging.
const char kGCMMCSEndpoint[] = "gcm-mcs-endpoint";

// Sets the registration endpoint that will be used for creating new Google
// Cloud Messaging registrations.
const char kGCMRegistrationURL[] = "gcm-registration-url";

}  // namespace switches
