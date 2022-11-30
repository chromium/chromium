// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/common/parser_util.h"

namespace google_apis {

bool IsResourceKindExpected(const base::Value& value,
                            const std::string& expected_kind) {
  if (!value.is_dict())
    return false;
  const std::string* kind = value.FindStringKey(kApiResponseKindKey);
  return kind && *kind == expected_kind;
}

}  // namespace google_apis
