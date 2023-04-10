// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/components/javascript_dialog_extensions_client/javascript_dialog_extension_client_impl.h"

#include <memory>

#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "components/javascript_dialogs/extensions_client.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "ui/gfx/native_widget_types.h"

namespace javascript_dialog_extensions_client {
namespace {

using extensions::Extension;

// Returns the ProcessManager for the browser context from |web_contents|.
extensions::ProcessManager* GetProcessManager(
    content::WebContents* web_contents) {
  return extensions::ProcessManager::Get(web_contents->GetBrowserContext());
}

// Returns the extension associated with |web_contents| or NULL if there is no
// associated extension (or extensions are not supported).
const Extension* GetExtensionForWebContents(
    content::WebContents* web_contents) {
  return GetProcessManager(web_contents)->GetExtensionForWebContents(
      web_contents);
}

class JavaScriptDialogExtensionsClientImpl
    : public javascript_dialogs::ExtensionsClient {
 public:
  JavaScriptDialogExtensionsClientImpl() = default;
  ~JavaScriptDialogExtensionsClientImpl() override = default;
  JavaScriptDialogExtensionsClientImpl(
      const JavaScriptDialogExtensionsClientImpl&) = delete;
  JavaScriptDialogExtensionsClientImpl& operator=(
      const JavaScriptDialogExtensionsClientImpl&) = delete;

  // JavaScriptDialogExtensionsClient:
  void OnDialogOpened(content::WebContents* web_contents) override {
    const Extension* extension = GetExtensionForWebContents(web_contents);
    if (extension == nullptr)
      return;

    DCHECK(web_contents);
    extensions::ProcessManager* pm = GetProcessManager(web_contents);
    if (pm)
      pm->IncrementLazyKeepaliveCount(
          extension, extensions::Activity::MODAL_DIALOG,
          web_contents->GetLastCommittedURL().spec());
  }
  void OnDialogClosed(content::WebContents* web_contents) override {
    const Extension* extension = GetExtensionForWebContents(web_contents);
    if (extension == nullptr)
      return;

    DCHECK(web_contents);
    extensions::ProcessManager* pm = GetProcessManager(web_contents);
    if (pm)
      pm->DecrementLazyKeepaliveCount(
          extension, extensions::Activity::MODAL_DIALOG,
          web_contents->GetLastCommittedURL().spec());
  }
};

}  // namespace

void InstallClient() {
  javascript_dialogs::AppModalDialogManager::GetInstance()->SetExtensionsClient(
      std::make_unique<JavaScriptDialogExtensionsClientImpl>());
}

}  // namespace javascript_dialog_extensions_client
