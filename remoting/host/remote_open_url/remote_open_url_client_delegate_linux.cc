// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url/remote_open_url_client_delegate_linux.h"

#include <gtk/gtk.h>

#include <string_view>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "remoting/base/host_settings.h"
#include "remoting/base/logging.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/host_setting_keys.h"
#include "remoting/host/resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

namespace {

constexpr char kXdgCurrentDesktopEnvVar[] = "XDG_CURRENT_DESKTOP";

void ShowMessageDialog(const std::string& message) {
  // IDS_URL_FORWARDER_NAME's placeholder is in the printf format (%s) since
  // it's used in Jinja.
  std::string dialog_title = l10n_util::GetStringUTF8(IDS_URL_FORWARDER_NAME);
  base::ReplaceFirstSubstringAfterOffset(
      &dialog_title, /* start_offset= */ 0, "%s",
      l10n_util::GetStringUTF8(IDS_PRODUCT_NAME));
  GtkWidget* dialog =
      gtk_message_dialog_new(nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
                             GTK_BUTTONS_OK, "%s", dialog_title.c_str());
  gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s",
                                           message.c_str());
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

bool IsBrowserValid(const std::string& browser) {
  static constexpr auto invalid_browsers =
      base::MakeFixedFlatSet<std::string_view>({
          // This is the chromoting forwarder itself.
          "crd-url-forwarder.desktop",

          // XFCE's forwarder. May potentially launch the chromoting forwarder
          // recursively.
          "xfce4-web-browser.desktop",
      });
  if (browser.empty()) {
    return false;
  }
  return !invalid_browsers.contains(browser);
}

// Shows a window for the user to choose the fallback browser then sets it on
// the host settings with |host_setting_key|. Returns the chosen browser's
// desktop entry ID, e.g. google-chrome.desktop. Returns empty string if the
// user cancels the dialog.
std::string ChooseFallbackBrowser(const HostSettingKey host_setting_key) {
  std::string browser_chooser_heading =
      l10n_util::GetStringUTF8(IDS_CHOOSE_FALLBACK_BROWSER);
  std::string invalid_browser_message =
      l10n_util::GetStringUTF8(IDS_BROWSER_IS_INVALID);

  while (true) {
    GtkWidget* dialog = gtk_app_chooser_dialog_new_for_content_type(
        nullptr, GTK_DIALOG_MODAL, "x-scheme-handler/http");
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
    gtk_app_chooser_dialog_set_heading(GTK_APP_CHOOSER_DIALOG(dialog),
                                       browser_chooser_heading.c_str());
    int result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result != GTK_RESPONSE_OK) {
      HOST_LOG << "User canceled choosing the fallback browser.";
      gtk_widget_destroy(dialog);
      return std::string();
    }

    GAppInfo* app_info = gtk_app_chooser_get_app_info(GTK_APP_CHOOSER(dialog));
    std::string app_id = g_app_info_get_id(app_info);
    g_object_unref(app_info);
    gtk_widget_destroy(dialog);

    if (IsBrowserValid(app_id)) {
      HOST_LOG << "Setting " << host_setting_key << " to " << app_id;
      HostSettings::GetInstance()->SetString(host_setting_key, app_id);
      return app_id;
    }

    ShowMessageDialog(invalid_browser_message);
  }
}

}  // namespace

RemoteOpenUrlClientDelegateLinux::RemoteOpenUrlClientDelegateLinux()
    : environment_(base::Environment::Create()) {
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_init();
#else
  gtk_init(nullptr, nullptr);
#endif
}

RemoteOpenUrlClientDelegateLinux::~RemoteOpenUrlClientDelegateLinux() = default;

void RemoteOpenUrlClientDelegateLinux::OpenUrlOnFallbackBrowser(
    const GURL& url) {
  std::string current_desktop;
  environment_->GetVar(kXdgCurrentDesktopEnvVar, &current_desktop);

  const char* host_setting_key = kLinuxPreviousDefaultWebBrowserGeneric;
  if (base::Contains(current_desktop, "Cinnamon")) {
    host_setting_key = kLinuxPreviousDefaultWebBrowserCinnamon;
  } else if (base::Contains(current_desktop, "XFCE")) {
    host_setting_key = kLinuxPreviousDefaultWebBrowserXfce;
  } else if (base::Contains(current_desktop, "GNOME")) {
    host_setting_key = kLinuxPreviousDefaultWebBrowserGnome;
  } else {
    LOG(WARNING) << "Unknown desktop environment: " << current_desktop
                 << ", X-Generic will be used.";
  }

  std::string previous_default_browser =
      HostSettings::GetInstance()->GetString(host_setting_key);
  if (!IsBrowserValid(previous_default_browser)) {
    LOG(WARNING)
        << "Fallback browser is invalid. Showing the browser chooser...";
    previous_default_browser = ChooseFallbackBrowser(host_setting_key);
    if (previous_default_browser.empty()) {
      // User canceled the dialog.
      return;
    }
  }
  // gtk-launch DESKTOP_ENTRY [URL...]
  base::CommandLine gtk_launch_command(
      {"gtk-launch", previous_default_browser});
  if (!url.is_empty()) {
    gtk_launch_command.AppendArg(url.spec());
  }
  base::LaunchOptions options;
  // Some browsers require privileges that can't be granted with the
  // PR_SET_NO_NEW_PRIVS bit set.
  options.allow_new_privs = true;
  base::LaunchProcess(gtk_launch_command, options);
}

void RemoteOpenUrlClientDelegateLinux::ShowOpenUrlError(const GURL& url) {
  std::string message = l10n_util::GetStringFUTF8(
      IDS_REMOTE_OPEN_URL_FAILED, base::UTF8ToUTF16(url.spec()));
  LOG(ERROR) << "Failed to open URL remotely: " << url.spec();
  ShowMessageDialog(message);
}

}  // namespace remoting
