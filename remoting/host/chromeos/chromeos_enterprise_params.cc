// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/chromeos_enterprise_params.h"

#include "base/json/values_util.h"
#include "base/notreached.h"
#include "base/time/time.h"

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
constexpr char kConnectionDialogRequired[] = "connectionDialogRequired";
constexpr char kConnectionAutoAcceptTimeout[] = "connectionAutoAcceptTimeout";
constexpr char kMaximumSessionDuration[] = "maximumSessionDuration";
constexpr char kAllowRemoteInput[] = "allowRemoteInput";
constexpr char kAllowClipboardSync[] = "allowClipboardSync";
constexpr char kChromeOsEnterpriseRequestOrigin[] =
    "chromeOsEnterpriseRequestOrigin";
constexpr char kRequestOriginClassManagement[] = "classManagement";
constexpr char kRequestOriginEnterpriseAdmin[] = "enterpriseAdmin";
constexpr char kRequestOriginUknown[] = "requestOriginUnknown";
constexpr char kChromeOsEnterpriseAudioPlayback[] =
    "chromeOsEnterpriseAudioPlayback";
constexpr char kAudioPlaybackLocalOnly[] = "localOnly";
constexpr char kAudioPlaybackRemoteAndLocal[] = "remoteAndLocal";
constexpr char kAudioPlaybackRemoteOnly[] = "remoteOnly";
constexpr char kAudioPlaybackUnknown[] = "audioPlaybackUnknown";

ChromeOsEnterpriseRequestOrigin ConvertStringToChromeOsEnterpriseRequestOrigin(
    const std::string& request_origin) {
  if (request_origin == kRequestOriginClassManagement) {
    return ChromeOsEnterpriseRequestOrigin::kClassManagement;
  }
  if (request_origin == kRequestOriginEnterpriseAdmin) {
    return ChromeOsEnterpriseRequestOrigin::kEnterpriseAdmin;
  }
  if (request_origin == kRequestOriginUknown) {
    return ChromeOsEnterpriseRequestOrigin::kUnknown;
  }
  NOTREACHED();
}

std::string ConvertChromeOsEnterpriseRequestOriginToString(
    ChromeOsEnterpriseRequestOrigin request_origin) {
  switch (request_origin) {
    case ChromeOsEnterpriseRequestOrigin::kClassManagement:
      return kRequestOriginClassManagement;
    case ChromeOsEnterpriseRequestOrigin::kEnterpriseAdmin:
      return kRequestOriginEnterpriseAdmin;
    case ChromeOsEnterpriseRequestOrigin::kUnknown:
      return kRequestOriginUknown;
  }
}

ChromeOsEnterpriseRequestOrigin GetRequestOriginOrDefault(
    const std::string* input_request_origin,
    ChromeOsEnterpriseRequestOrigin default_request_origin) {
  return input_request_origin ? ConvertStringToChromeOsEnterpriseRequestOrigin(
                                    *input_request_origin)
                              : default_request_origin;
}

ChromeOsEnterpriseAudioPlayback ConvertStringToChromeOsEnterpriseAudioPlayback(
    const std::string& audio_playback) {
  if (audio_playback == kAudioPlaybackLocalOnly) {
    return ChromeOsEnterpriseAudioPlayback::kLocalOnly;
  }
  if (audio_playback == kAudioPlaybackRemoteAndLocal) {
    return ChromeOsEnterpriseAudioPlayback::kRemoteAndLocal;
  }
  if (audio_playback == kAudioPlaybackRemoteOnly) {
    return ChromeOsEnterpriseAudioPlayback::kRemoteOnly;
  }
  if (audio_playback == kAudioPlaybackUnknown) {
    return ChromeOsEnterpriseAudioPlayback::kUnknown;
  }
  NOTREACHED();
}

ChromeOsEnterpriseAudioPlayback GetAudioPlaybackOrDefault(
    const std::string* input_audio_playback,
    ChromeOsEnterpriseAudioPlayback default_audio_playback) {
  return input_audio_playback ? ConvertStringToChromeOsEnterpriseAudioPlayback(
                                    *input_audio_playback)
                              : default_audio_playback;
}
}  // namespace

ChromeOsEnterpriseParams::ChromeOsEnterpriseParams() = default;

ChromeOsEnterpriseParams::ChromeOsEnterpriseParams(
    const ChromeOsEnterpriseParams& other) = default;
ChromeOsEnterpriseParams& ChromeOsEnterpriseParams::operator=(
    const ChromeOsEnterpriseParams& other) = default;

ChromeOsEnterpriseParams::~ChromeOsEnterpriseParams() = default;

bool ChromeOsEnterpriseParams::operator==(
    const ChromeOsEnterpriseParams& other) const = default;

std::string ConvertChromeOsEnterpriseAudioPlaybackToString(
    ChromeOsEnterpriseAudioPlayback audio_playback) {
  switch (audio_playback) {
    case ChromeOsEnterpriseAudioPlayback::kLocalOnly:
      return kAudioPlaybackLocalOnly;
    case ChromeOsEnterpriseAudioPlayback::kRemoteAndLocal:
      return kAudioPlaybackRemoteAndLocal;
    case ChromeOsEnterpriseAudioPlayback::kRemoteOnly:
      return kAudioPlaybackRemoteOnly;
    case ChromeOsEnterpriseAudioPlayback::kUnknown:
      return kAudioPlaybackUnknown;
  }
}

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
  params.maximum_session_duration =
      base::ValueToTimeDelta(dict.Find(kMaximumSessionDuration))
          .value_or(base::TimeDelta());
  params.allow_remote_input = dict.FindBool(kAllowRemoteInput).value_or(true);
  params.allow_clipboard_sync =
      dict.FindBool(kAllowClipboardSync).value_or(true);
  params.show_troubleshooting_tools =
      dict.FindBool(kShowTroubleshootingTools).value_or(false);
  params.allow_troubleshooting_tools =
      dict.FindBool(kAllowTroubleshootingTools).value_or(false);
  params.allow_reconnections =
      dict.FindBool(kAllowReconnections).value_or(false);
  params.allow_file_transfer =
      dict.FindBool(kAllowFileTransfer).value_or(false);
  params.connection_dialog_required =
      dict.FindBool(kConnectionDialogRequired).value_or(false);
  params.request_origin = GetRequestOriginOrDefault(
      dict.FindString(kChromeOsEnterpriseRequestOrigin),
      /* default_request_origin= */ ChromeOsEnterpriseRequestOrigin::
          kEnterpriseAdmin);
  params.audio_playback = GetAudioPlaybackOrDefault(
      dict.FindString(kChromeOsEnterpriseAudioPlayback),
      /* default_audio_playback= */ ChromeOsEnterpriseAudioPlayback::kLocalOnly);
  params.connection_auto_accept_timeout =
      base::ValueToTimeDelta(dict.Find(kConnectionAutoAcceptTimeout))
          .value_or(base::TimeDelta());
  return params;
}

base::Value::Dict ChromeOsEnterpriseParams::ToDict() const {
  return base::Value::Dict()
      .Set(kSuppressUserDialogs, suppress_user_dialogs)
      .Set(kSuppressNotifications, suppress_notifications)
      .Set(kTerminateUponInput, terminate_upon_input)
      .Set(kCurtainLocalUserSession, curtain_local_user_session)
      .Set(kMaximumSessionDuration,
           base::TimeDeltaToValue(maximum_session_duration))
      .Set(kAllowRemoteInput, allow_remote_input)
      .Set(kAllowClipboardSync, allow_clipboard_sync)
      .Set(kShowTroubleshootingTools, show_troubleshooting_tools)
      .Set(kAllowTroubleshootingTools, allow_troubleshooting_tools)
      .Set(kAllowReconnections, allow_reconnections)
      .Set(kAllowFileTransfer, allow_file_transfer)
      .Set(kConnectionDialogRequired, connection_dialog_required)
      .Set(kChromeOsEnterpriseRequestOrigin,
           ConvertChromeOsEnterpriseRequestOriginToString(request_origin))
      .Set(kChromeOsEnterpriseAudioPlayback,
           ConvertChromeOsEnterpriseAudioPlaybackToString(audio_playback))
      .Set(kConnectionAutoAcceptTimeout,
           base::TimeDeltaToValue(connection_auto_accept_timeout));
}

}  // namespace remoting
