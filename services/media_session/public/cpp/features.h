// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_FEATURES_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace media_session {
namespace features {

COMPONENT_EXPORT(MEDIA_SESSION_CPP) BASE_DECLARE_FEATURE(kMediaSessionService);
COMPONENT_EXPORT(MEDIA_SESSION_CPP)
BASE_DECLARE_FEATURE(kAudioFocusEnforcement);
COMPONENT_EXPORT(MEDIA_SESSION_CPP)
BASE_DECLARE_FEATURE(kAudioFocusSessionGrouping);

}  // namespace features
}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_FEATURES_H_
