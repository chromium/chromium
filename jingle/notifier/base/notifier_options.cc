// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/notifier_options.h"

#include "jingle/notifier/base/gaia_constants.h"

namespace notifier {

NotifierOptions::NotifierOptions()
    : try_ssltcp_first(false),
      allow_insecure_connection(false),
      invalidate_xmpp_login(false),
      notification_method(kDefaultNotificationMethod),
      auth_mechanism(kDefaultGaiaAuthMechanism),
      network_connection_tracker(nullptr) {}

NotifierOptions::NotifierOptions(const NotifierOptions& other) = default;

NotifierOptions::~NotifierOptions() { }

}  // namespace notifier
