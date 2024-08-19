// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/indexed_rule.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "url/gurl.h"

using extensions::api::declarative_net_request::Rule;
using extensions::declarative_net_request::IndexedRule;
using extensions::declarative_net_request::RulesetID;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  // Make a random `Rule`.
  std::optional<base::Value> value =
      base::JSONReader::Read(provider.ConsumeRandomLengthString());
  if (!value || !value->is_dict()) {
    return 0;
  }
  base::expected<Rule, std::u16string> rule = Rule::FromValue(value->GetDict());
  if (!rule.has_value()) {
    return 0;
  }

  // Make a random `GURL`.
  const GURL url(provider.ConsumeRandomLengthString());
  if (!url.is_valid()) {
    return 0;
  }

  // Make a random `RulesetID`.
  const RulesetID ruleset_id(provider.ConsumeIntegralInRange<int>(-1, 100));

  // Run code-under-test.
  IndexedRule indexed_rule;
  indexed_rule.CreateIndexedRule(std::move(*rule), url, ruleset_id,
                                 &indexed_rule);

  return 0;
}
