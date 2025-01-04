// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_FEATURES_H_
#define GOOGLE_APIS_GAIA_GAIA_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace gaia::features {

COMPONENT_EXPORT(GOOGLE_APIS)
BASE_DECLARE_FEATURE(kListAccountsUsesBinaryFormat);

}  // namespace gaia::features

#endif  // GOOGLE_APIS_GAIA_GAIA_FEATURES_H_
