// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/javascript_dialog_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_types.h"

namespace extensions {

namespace {

std::string JavaScriptDialogTypeToString(
    content::JavaScriptDialogType dialog_type) {
  switch (dialog_type) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT:
      return "alert";
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM:
      return "confirm";
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT:
      return "prompt";
    default:
      NOTREACHED() << "Unknown JavaScript Message Type.";
      return "unknown";
  }
}

}  // namespace

JavaScriptDialogHelper::JavaScriptDialogHelper(WebViewGuest* guest)
    : web_view_guest_(guest) {
}

JavaScriptDialogHelper::~JavaScriptDialogHelper() {
}

void JavaScriptDialogHelper::RunJavaScriptDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    content::JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  base::DictionaryValue request_info;
  request_info.SetStringKey(webview::kDefaultPromptText,
                            base::UTF16ToUTF8(default_prompt_text));
  request_info.SetStringKey(webview::kMessageText,
                            base::UTF16ToUTF8(message_text));
  request_info.SetStringKey(webview::kMessageType,
                            JavaScriptDialogTypeToString(dialog_type));
  request_info.SetStringKey(guest_view::kUrl,
                            render_frame_host->GetLastCommittedURL().spec());

  WebViewPermissionHelper* web_view_permission_helper =
      web_view_guest_->web_view_permission_helper();
  web_view_permission_helper->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_JAVASCRIPT_DIALOG, request_info,
      base::BindOnce(&JavaScriptDialogHelper::OnPermissionResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      false /* allowed_by_default */);
}

void JavaScriptDialogHelper::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    bool is_reload,
    DialogClosedCallback callback) {
  // This is called if the guest has a beforeunload event handler.
  // This callback allows navigation to proceed.
  std::move(callback).Run(true, std::u16string());
}

bool JavaScriptDialogHelper::HandleJavaScriptDialog(
    content::WebContents* web_contents,
    bool accept,
    const std::u16string* prompt_override) {
  return false;
}

void JavaScriptDialogHelper::CancelDialogs(content::WebContents* web_contents,
                                           bool reset_state) {}

void JavaScriptDialogHelper::OnPermissionResponse(
    DialogClosedCallback callback,
    bool allow,
    const std::string& user_input) {
  std::move(callback).Run(allow && web_view_guest_->attached(),
                          base::UTF8ToUTF16(user_input));
}

}  // namespace extensions
