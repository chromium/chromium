// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/mf_feature_checks.h"

#include "base/win/windows_version.h"
#include "media/base/media_switches.h"

namespace media {

bool SupportMediaFoundationClearPlayback() {
  return base::win::GetVersion() >= base::win::Version::WIN10_RS3 &&
         base::FeatureList::IsEnabled(media::kMediaFoundationClearPlayback);
}

}  // namespace media