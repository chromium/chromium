// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_SQL_FEATURES_H_
#define SQL_SQL_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace sql {

namespace features {

COMPONENT_EXPORT(SQL) extern const base::Feature kSqlSkipPreload;

}  // namespace features

}  // namespace sql

#endif  // SQL_SQL_FEATURES_H_
