// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/components/javascript_dialog_extensions_client/javascript_dialog_extension_client_impl.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "components/javascript_dialogs/extensions_client.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "ui/gfx/native_widget_types.h"
#include "url/origin.h"

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
  bool GetExtensionName(content::WebContents* web_contents,
                        const GURL& alerting_frame_url,
                        std::string* name_out) override {
    const Extension* extension = GetExtensionForWebContents(web_contents);
    if (extension &&
        url::IsSameOriginWith(alerting_frame_url,
                              web_contents->GetLastCommittedURL())) {
      *name_out = extension->name();
      return true;
    }
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(JavaScriptDialogExtensionsClientImpl);
};

}  // namespace

void InstallClient() {
  javascript_dialogs::AppModalDialogManager::GetInstance()->SetExtensionsClient(
      base::WrapUnique(new JavaScriptDialogExtensionsClientImpl));
}

}  // namespace javascript_dialog_extensions_client
