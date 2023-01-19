// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/client_info_helpers.h"

#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/types/optional_util.h"
#include "printing/mojom/print.mojom.h"
#include "third_party/re2/src/re2/re2.h"

namespace printing {

namespace {

bool ValidateClientType(mojom::IppClientInfo::ClientType type) {
  return type >= mojom::IppClientInfo::ClientType::kMinValue &&
         type <= mojom::IppClientInfo::ClientType::kMaxValue;
}

bool ValidateStringMember(const std::string* value, size_t max_length) {
  static const base::NoDestructor<RE2> kStringRegex("[a-zA-Z0-9_.-]*");
  return value == nullptr ||
         (value->size() <= max_length && RE2::FullMatch(*value, *kStringRegex));
}

// Returns true if all members of `client_info` are valid.
// String members are considered valid if they match the regex [a-zA-Z0-9_.-]*
// and do not exceed the maximum length specified for the respective IPP member
// attribute. The `client_type` member is valid if it is equal to one of the
// enum values defined for the `client-type` IPP attribute.
bool ValidateClientInfoItem(const mojom::IppClientInfo& client_info) {
  return ValidateClientType(client_info.client_type) &&
         ValidateStringMember(&client_info.client_name,
                              kClientInfoMaxNameLength) &&
         ValidateStringMember(&client_info.client_string_version,
                              kClientInfoMaxStringVersionLength) &&
         ValidateStringMember(base::OptionalToPtr(client_info.client_patches),
                              kClientInfoMaxPatchesLength) &&
         ValidateStringMember(base::OptionalToPtr(client_info.client_version),
                              kClientInfoMaxVersionLength);
}

}  // namespace

absl::optional<std::string> ClientInfoCollectionToCupsOptionValue(
    const mojom::IppClientInfo& client_info) {
  if (!ValidateClientInfoItem(client_info)) {
    return absl::nullopt;
  }
  std::string name = base::StrCat({"client-name=", client_info.client_name});
  std::string type = base::StringPrintf(
      "client-type=%d", static_cast<int>(client_info.client_type));
  std::string string_version = base::StrCat(
      {"client-string-version=", client_info.client_string_version});

  // Missing values for 'client-version' and 'client-patches' correspond to
  // 'no-value' out-of-band IPP values. We omit them because there is no
  // string encoding as a cups_option_t for them that CUPS understands.
  std::string version;
  if (client_info.client_version.has_value()) {
    version =
        base::StrCat({"client-version=", client_info.client_version.value()});
  }
  std::string patches;
  if (client_info.client_patches.has_value()) {
    patches =
        base::StrCat({"client-patches=", client_info.client_patches.value()});
  }

  // The resulting string may have extra spaces between attributes because
  // of missing member attributes which is okay because they are ignored
  // by cupsParseOptions.
  return base::StringPrintf("{%s %s %s %s %s}", name.c_str(), type.c_str(),
                            version.c_str(), string_version.c_str(),
                            patches.c_str());
}

}  // namespace printing
