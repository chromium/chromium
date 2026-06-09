// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_INSTALL_PROMPT_CLIENT_H_
#define EXTENSIONS_BROWSER_EXTENSION_INSTALL_PROMPT_CLIENT_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_types.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class SkBitmap;

namespace content {
class BrowserContext;
}

namespace extensions {
class CrxInstallError;
class Extension;

class ExtensionInstallPromptClient {
 public:
  enum class Result {
    ACCEPTED,
    ACCEPTED_WITH_WITHHELD_PERMISSIONS,
    USER_CANCELED,
    ABORTED,
  };

  struct DoneCallbackPayload {
    explicit DoneCallbackPayload(Result result);
    DoneCallbackPayload(Result result, std::string justification);
    ~DoneCallbackPayload() = default;

    const Result result;
    const std::string justification;
  };

  // Interface for observing events on the prompt.
  class Observer : public base::CheckedObserver {
   public:
    // Called right before the dialog is about to show.
    virtual void OnDialogOpened() = 0;

    // Called when the user clicks accept on the dialog.
    virtual void OnDialogAccepted() = 0;

    // Called when the user clicks cancel on the dialog, presses 'x' or escape.
    virtual void OnDialogCanceled() = 0;
  };

  using DoneCallback = base::OnceCallback<void(DoneCallbackPayload payload)>;

  // Installation was successful.
  virtual void OnInstallSuccess(scoped_refptr<const Extension> extension,
                                SkBitmap* icon) = 0;

  // Installation failed.
  virtual void OnInstallFailure(const CrxInstallError& error) = 0;

  // Sets whether to show  a bubble when an app is installed.
  virtual void SetUseAppInstalledBubble(bool use_bubble) = 0;

  // Sets whether to show the default UI after completing the installation.
  virtual void SetSkipPostInstallUI(bool skip_ui) = 0;

  // Starts the process to show the install prompt.
  virtual void ConfirmInstall(DoneCallback install_callback,
                              const Extension* extension) = 0;

  // Starts the process to show the re-enable prompt.
  virtual void ConfirmReEnable(DoneCallback install_callback,
                               const Extension* extension,
                               content::BrowserContext* browser_context) = 0;

  virtual ~ExtensionInstallPromptClient() = default;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_INSTALL_PROMPT_CLIENT_H_
