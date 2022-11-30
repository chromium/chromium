// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quiche/common/platform/default/quiche_platform_impl/quiche_url_utils_impl.h"

#include <cstdint>
#include <limits>
#include <string>

#include "net/third_party/uri_template/uri_template.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/abseil-cpp/absl/strings/str_cat.h"
#include "third_party/abseil-cpp/absl/strings/str_replace.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace quiche {

bool ExpandURITemplateImpl(
    const std::string& uri_template,
    const absl::flat_hash_map<std::string, std::string>& parameters,
    std::string* target,
    absl::flat_hash_set<std::string>* vars_found) {
  std::unordered_map<std::string, std::string> std_parameters;
  for (const auto& pair : parameters) {
    std_parameters[pair.first] = pair.second;
  }
  std::set<std::string> std_vars_found;
  const bool result =
      uri_template::Expand(uri_template, std_parameters, target,
                           vars_found != nullptr ? &std_vars_found : nullptr);
  if (vars_found != nullptr) {
    for (const std::string& var_found : std_vars_found) {
      vars_found->insert(var_found);
    }
  }
  return result;
}

absl::optional<std::string> AsciiUrlDecodeImpl(absl::string_view input) {
  std::string input_encoded = std::string(input);
  url::RawCanonOutputW<1024> canon_output;
  url::DecodeURLEscapeSequences(input_encoded.c_str(), input_encoded.length(),
                                url::DecodeURLMode::kUTF8, &canon_output);
  std::string output;
  output.reserve(canon_output.length());
  for (int i = 0; i < canon_output.length(); i++) {
    const uint16_t c = reinterpret_cast<uint16_t*>(canon_output.data())[i];
    if (c > std::numeric_limits<signed char>::max()) {
      return absl::nullopt;
    }
    output += static_cast<char>(c);
  }
  return output;
}

}  // namespace quiche
