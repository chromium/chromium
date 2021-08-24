// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url_client_delegate_win.h"

#include <string>

#include "base/notreached.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "remoting/base/logging.h"
#include "remoting/host/user_setting_keys.h"
#include "remoting/host/user_settings.h"

namespace remoting {

namespace {

// See: https://docs.microsoft.com/en-us/windows/win32/com/-progid--key
static constexpr wchar_t kShellOpenCommandPathPattern[] =
    L"SOFTWARE\\Classes\\%ls\\shell\\open\\command";

std::wstring GetLaunchBrowserCommand(const std::wstring& browser_prog_id,
                                     const GURL& url) {
  std::wstring shell_open_command_path =
      base::StringPrintf(kShellOpenCommandPathPattern, browser_prog_id.c_str());
  base::win::RegKey shell_open_command_key;
  LONG result = shell_open_command_key.Open(
      HKEY_LOCAL_MACHINE, shell_open_command_path.c_str(), KEY_READ);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to open registry key HKLM\\"
               << shell_open_command_path << ", result: " << result;
    return std::wstring();
  }
  std::wstring open_command;
  // Read the default value.
  result = shell_open_command_key.ReadValue(nullptr, &open_command);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to get open command for ProgID " << browser_prog_id
               << ", result: " << result;
    return std::wstring();
  }
  std::wstring wide_url = base::UTF8ToWide(url.spec().c_str());
  base::ReplaceSubstringsAfterOffset(&open_command, 0, L"%1", wide_url);
  return open_command;
}

}  // namespace

RemoteOpenUrlClientDelegateWin::RemoteOpenUrlClientDelegateWin() = default;

RemoteOpenUrlClientDelegateWin::~RemoteOpenUrlClientDelegateWin() = default;

bool RemoteOpenUrlClientDelegateWin::IsInRemoteDesktopSession() {
  NOTIMPLEMENTED();
  return true;
}

void RemoteOpenUrlClientDelegateWin::OpenUrlOnFallbackBrowser(const GURL& url) {
  std::wstring fallback_browser_prog_id =
      base::UTF8ToWide(UserSettings::GetInstance()->GetString(
          kWinPreviousDefaultWebBrowserProgId));
  if (fallback_browser_prog_id.empty()) {
    // TODO(b/183135000): Implement some sort of fallback browser chooser.
    LOG(ERROR) << "Cannot determine the fallback browser.";
    return;
  }

  // ShellExecuteExW works as expected if the ProgID registry key doesn't have
  // an empty "URL Protocol" value. If it has such value, then ShellExecuteExW
  // will mysteriously create an offline page and make the browser load it
  // instead of the actual URL.
  // Instead, we just fetch the open command ourselves from the registry and use
  // it to launch the fallback browser.

  std::wstring launch_command =
      GetLaunchBrowserCommand(fallback_browser_prog_id, url);
  if (launch_command.empty()) {
    return;
  }
  HOST_LOG << "Opening URL with fallback browser: " << fallback_browser_prog_id;
  base::LaunchOptions options;
  options.grant_foreground_privilege = true;
  base::LaunchProcess(launch_command, options);
}

void RemoteOpenUrlClientDelegateWin::ShowOpenUrlError(const GURL& url) {
  NOTIMPLEMENTED();
}

}  // namespace remoting
