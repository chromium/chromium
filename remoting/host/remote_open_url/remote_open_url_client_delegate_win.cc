// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url/remote_open_url_client_delegate_win.h"

#include <windows.h>

#include <ios>
#include <string>

#include "base/notreached.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/current_module.h"
#include "base/win/default_apps_util.h"
#include "base/win/shlwapi.h"
#include "remoting/base/logging.h"
#include "remoting/base/user_settings.h"
#include "remoting/host/remote_open_url/remote_open_url_constants.h"
#include "remoting/host/user_setting_keys.h"
#include "remoting/host/win/core_resource.h"
#include "remoting/host/win/simple_task_dialog.h"

namespace remoting {

namespace {

std::wstring GetLaunchBrowserCommand(const std::wstring& browser_prog_id,
                                     const GURL& url) {
  wchar_t open_cmd_buf[MAX_PATH];
  DWORD open_cmd_buf_len = std::size(open_cmd_buf);
  HRESULT hr =
      AssocQueryString(ASSOCF_NONE, ASSOCSTR_COMMAND, browser_prog_id.c_str(),
                       L"open", open_cmd_buf, &open_cmd_buf_len);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to look up open command for ProgID: "
               << browser_prog_id << ", result: 0x" << std::hex << hr;
    return std::wstring();
  }

  std::wstring open_command(open_cmd_buf);
  std::wstring wide_url = base::UTF8ToWide(url.spec().c_str());
  base::ReplaceSubstringsAfterOffset(&open_command, 0, L"%1", wide_url);
  return open_command;
}

void ShowIncorrectConfigurationPrompt() {
  SimpleTaskDialog task_dialog(CURRENT_MODULE());
  task_dialog.SetTitleTextWithStringId(IDS_URL_FORWARDER_NAME);
  task_dialog.SetMessageTextWithStringId(
      IDS_URL_FORWARDER_INCORRECTLY_CONFIGURED);
  task_dialog.AppendButtonWithStringId(IDOK,
                                       IDS_OPEN_DEFAULT_APPS_SETTINGS_BUTTON);
  task_dialog.set_default_button(IDOK);
  std::optional<int> result = task_dialog.Show();
  DCHECK_EQ(IDOK, *result);
  base::win::LaunchDefaultAppsSettingsModernDialog(/*protocol=*/std::wstring());
}

}  // namespace

RemoteOpenUrlClientDelegateWin::RemoteOpenUrlClientDelegateWin() = default;

RemoteOpenUrlClientDelegateWin::~RemoteOpenUrlClientDelegateWin() = default;

void RemoteOpenUrlClientDelegateWin::OpenUrlOnFallbackBrowser(const GURL& url) {
  std::wstring fallback_browser_prog_id =
      base::UTF8ToWide(UserSettings::GetInstance()->GetString(
          kWinPreviousDefaultWebBrowserProgId));
  if (fallback_browser_prog_id.empty() ||
      fallback_browser_prog_id == kUrlForwarderProgId ||
      // "Undecided" pops up a browser chooser for opening the URL, but it
      // doesn't remember the user's choice, and we don't know what browser has
      // been chosen either, so it'd be more usable to just treat it as an
      // incorrect configuration and let the user choose a different browser
      // from the Settings app.
      fallback_browser_prog_id == kUndecidedProgId) {
    ShowIncorrectConfigurationPrompt();
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
    ShowIncorrectConfigurationPrompt();
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
