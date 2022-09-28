// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_SQL_FEATURES_H_
#define SQL_SQL_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace sql {

namespace features {

COMPONENT_EXPORT(SQL) BASE_DECLARE_FEATURE(kEnableWALModeByDefault);

}  // namespace features

}  // namespace sql

#endif  // SQL_SQL_FEATURES_H_
