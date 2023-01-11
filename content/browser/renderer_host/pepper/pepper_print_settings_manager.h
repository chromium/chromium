// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_PRINT_SETTINGS_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_PRINT_SETTINGS_MANAGER_H_

#include <stdint.h>

#include "base/functional/bind.h"
#include "ppapi/c/dev/pp_print_settings_dev.h"

namespace content {

// A class for getting the default print settings for the default printer.
class PepperPrintSettingsManager {
 public:
  using Result = std::pair<PP_PrintSettings_Dev, int32_t>;
  using Callback = base::OnceCallback<void(Result)>;

  // The default print settings are obtained asynchronously and |callback|
  // is called with the the print settings when they are available. |callback|
  // will always be called on the same thread from which
  // |GetDefaultPrintSettings| was issued.
  virtual void GetDefaultPrintSettings(Callback callback) = 0;

  virtual ~PepperPrintSettingsManager() {}
};

// Real implementation for getting the default print settings.
class PepperPrintSettingsManagerImpl : public PepperPrintSettingsManager {
 public:
  PepperPrintSettingsManagerImpl() {}

  PepperPrintSettingsManagerImpl(const PepperPrintSettingsManagerImpl&) =
      delete;
  PepperPrintSettingsManagerImpl& operator=(
      const PepperPrintSettingsManagerImpl&) = delete;

  ~PepperPrintSettingsManagerImpl() override {}

  // PepperPrintSettingsManager implementation.
  void GetDefaultPrintSettings(
      PepperPrintSettingsManager::Callback callback) override;

 private:
  static PepperPrintSettingsManager::Result ComputeDefaultPrintSettings();
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_PRINT_SETTINGS_MANAGER_H_
