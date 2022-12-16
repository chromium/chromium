// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "print_settings_conversion_chromeos.h"

#include "printing/print_job_constants.h"

namespace printing {

namespace {

// Assumes that `dict` contains valid data for client-info.
mojom::IppClientInfo GetClientInfoFromDict(const base::Value::Dict& dict) {
  mojom::IppClientInfo client_info;
  client_info.client_type = static_cast<mojom::IppClientInfo::ClientType>(
      dict.FindInt(kSettingIppClientType).value());
  client_info.client_name = *dict.FindString(kSettingIppClientName);
  client_info.client_string_version =
      *dict.FindString(kSettingIppClientStringVersion);

  const std::string* client_patches = dict.FindString(kSettingIppClientPatches);
  if (client_patches) {
    client_info.client_patches = *client_patches;
  }
  const std::string* client_version = dict.FindString(kSettingIppClientVersion);
  if (client_version) {
    client_info.client_version = *client_version;
  }
  return client_info;
}
}  // namespace

base::Value::List ConvertClientInfoToJobSetting(
    const std::vector<mojom::IppClientInfo>& client_infos) {
  base::Value::List client_info_list;
  client_info_list.reserve(client_infos.size());
  for (const auto& client_info : client_infos) {
    base::Value::Dict dict;
    dict.Set(kSettingIppClientType, static_cast<int>(client_info.client_type));
    dict.Set(kSettingIppClientName, client_info.client_name);
    dict.Set(kSettingIppClientStringVersion, client_info.client_string_version);
    if (client_info.client_patches.has_value()) {
      dict.Set(kSettingIppClientPatches, client_info.client_patches.value());
    }
    if (client_info.client_version.has_value()) {
      dict.Set(kSettingIppClientVersion, client_info.client_version.value());
    }
    client_info_list.Append(std::move(dict));
  }
  return client_info_list;
}

std::vector<mojom::IppClientInfo> ConvertJobSettingToClientInfo(
    const base::Value::List& client_info) {
  std::vector<mojom::IppClientInfo> result;
  for (const base::Value& client_info_value : client_info) {
    result.push_back(GetClientInfoFromDict(client_info_value.GetDict()));
  }
  return result;
}

}  // namespace printing
