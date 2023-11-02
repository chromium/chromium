// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_native_messaging_host_allowed_origins.h"

#include <iterator>

namespace remoting {

// If you modify the list of allowed_origins, don't forget to update
// remoting/host/it2me/com.google.chrome.remote_assistance.json.jinja2
// to keep the two lists in sync.
const char* const kIt2MeOrigins[] = {
    "chrome-extension://inomeogfingihgjfjlpeplalcfajhgai/",
    "chrome-extension://hpodccmdligbeohchckkeajbfohibipg/"};

const size_t kIt2MeOriginsSize = std::size(kIt2MeOrigins);

const char kIt2MeNativeMessageHostName[] =
    "com.google.chrome.remote_assistance";

}  // namespace remoting
