// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/common/parser_util.h"

namespace google_apis {

bool IsResourceKindExpected(const base::Value& value,
                            const std::string& expected_kind) {
  const auto* dict = value.GetIfDict();
  if (!dict) {
    return false;
  }
  const std::string* kind = dict->FindString(kApiResponseKindKey);
  return kind && *kind == expected_kind;
}

}  // namespace google_apis
