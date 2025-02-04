// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/chromeos_enterprise_params.h"

namespace remoting {

namespace {
constexpr char kSuppressUserDialogs[] = "suppressUserDialogs";
constexpr char kSuppressNotifications[] = "suppressNotifications";
constexpr char kCurtainLocalUserSession[] = "curtainLocalUserSession";
constexpr char kTerminateUponInput[] = "terminateUponInput";
constexpr char kAllowTroubleshootingTools[] = "allowTroubleshootingTools";
constexpr char kShowTroubleshootingTools[] = "showTroubleshootingTools";
constexpr char kAllowReconnections[] = "allowReconnections";
constexpr char kAllowFileTransfer[] = "allowFileTransfer";
// TODO: joedow - Add new enterprise values here.
}  // namespace

ChromeOsEnterpriseParams::ChromeOsEnterpriseParams() = default;

ChromeOsEnterpriseParams::ChromeOsEnterpriseParams(
    const ChromeOsEnterpriseParams& other) = default;
ChromeOsEnterpriseParams& ChromeOsEnterpriseParams::operator=(
    const ChromeOsEnterpriseParams& other) = default;

ChromeOsEnterpriseParams::~ChromeOsEnterpriseParams() = default;

// static
ChromeOsEnterpriseParams ChromeOsEnterpriseParams::FromDict(
    const base::Value::Dict& dict) {
  ChromeOsEnterpriseParams params;
  params.suppress_user_dialogs =
      dict.FindBool(kSuppressUserDialogs).value_or(false);
  params.suppress_notifications =
      dict.FindBool(kSuppressNotifications).value_or(false);
  params.terminate_upon_input =
      dict.FindBool(kTerminateUponInput).value_or(false);
  params.curtain_local_user_session =
      dict.FindBool(kCurtainLocalUserSession).value_or(false);
  params.show_troubleshooting_tools =
      dict.FindBool(kShowTroubleshootingTools).value_or(false);
  params.allow_troubleshooting_tools =
      dict.FindBool(kAllowTroubleshootingTools).value_or(false);
  params.allow_reconnections =
      dict.FindBool(kAllowReconnections).value_or(false);
  params.allow_file_transfer =
      dict.FindBool(kAllowFileTransfer).value_or(false);
  // TODO: joedow - Add new enterprise values here.
  return params;
}

base::Value::Dict ChromeOsEnterpriseParams::ToDict() const {
  return base::Value::Dict()
      .Set(kSuppressUserDialogs, suppress_user_dialogs)
      .Set(kSuppressNotifications, suppress_notifications)
      .Set(kTerminateUponInput, terminate_upon_input)
      .Set(kCurtainLocalUserSession, curtain_local_user_session)
      .Set(kShowTroubleshootingTools, show_troubleshooting_tools)
      .Set(kAllowTroubleshootingTools, allow_troubleshooting_tools)
      .Set(kAllowReconnections, allow_reconnections)
      .Set(kAllowFileTransfer, allow_file_transfer);
  // TODO: joedow - Add new enterprise values here.
}

}  // namespace remoting
