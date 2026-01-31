// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/corp_messaging_constants.h"

namespace remoting {

// Corp user JIDs have a format like <username>@corp.google.com. These can't be
// received via FTL messaging as it uses actual email addresses and the
// `corp.google.com` value is something we generate in our backend service.
const char kCorpSignalingDomain[] = "corp.google.com";

}  // namespace remoting
