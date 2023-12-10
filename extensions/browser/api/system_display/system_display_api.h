// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_

#include <string>

#include "extensions/browser/extension_function.h"
#include "extensions/common/api/system_display.h"

namespace extensions {

class SystemDisplayFunction : public ExtensionFunction {
 public:
  static const char kApiNotAvailableError[];

 protected:
  ~SystemDisplayFunction() override = default;
  bool PreRunValidation(std::string* error) override;
};

class SystemDisplayCrOSRestrictedFunction : public SystemDisplayFunction {
 public:
  static const char kCrosOnlyError[];
  static const char kKioskOnlyError[];

 protected:
  ~SystemDisplayCrOSRestrictedFunction() override = default;
  bool PreRunValidation(std::string* error) override;

  // Returns true if this function should be restricted to kiosk-mode apps and
  // webui. The default is true.
  virtual bool ShouldRestrictToKioskAndWebUI();
};

// This function inherits from SystemDisplayFunction because, unlike the
// rest of this API, it's available on all platforms.
class SystemDisplayGetInfoFunction : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.getInfo", SYSTEM_DISPLAY_GETINFO)

 protected:
  ~SystemDisplayGetInfoFunction() override = default;

  ResponseAction Run() override;

  void Response(
      std::vector<api::system_display::DisplayUnitInfo> all_displays_info);
};

class SystemDisplayGetDisplayLayoutFunction
    : public SystemDisplayCrOSRestrictedFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.getDisplayLayout",
                             SYSTEM_DISPLAY_GETDISPLAYLAYOUT)

 protected:
  ~SystemDisplayGetDisplayLayoutFunction() override = default;
  ResponseAction Run() override;
  bool ShouldRestrictToKioskAndWebUI() override;

  void Response(std::vector<api::system_display::DisplayLayout> display_layout);
};

class SystemDisplaySetDisplayPropertiesFunction
    : public SystemDisplayCrOSRestrictedFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.setDisplayProperties",
                             SYSTEM_DISPLAY_SETDISPLAYPROPERTIES)

 protected:
  ~SystemDisplaySetDisplayPropertiesFunction() override = default;
  ResponseAction Run() override;

  void Response(std::optional<std::string> error);
};

class SystemDisplaySetDisplayLayoutFunction
    : public SystemDisplayCrOSRestrictedFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.setDisplayLayout",
                             SYSTEM_DISPLAY_SETDISPLAYLAYOUT)

 protected:
  ~SystemDisplaySetDisplayLayoutFunction() override = default;
  ResponseAction Run() override;

  void Response(std::optional<std::string> error);
};

class SystemDisplayEnableUnifiedDesktopFunction
    : public SystemDisplayCrOSRestrictedFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.enableUnifiedDesktop",
                             SYSTEM_DISPLAY_ENABLEUNIFIEDDESKTOP)

 protected:
  ~SystemDisplayEnableUnifiedDesktopFunction() override = default;
  ResponseAction Run() override;
};

class SystemDisplayOverscanCalibrationStartFunction
    : public SystemDisplayCrOSRestrictedFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.overscanCalibrationStart",
                             SYSTEM_DISPLAY_OVERSCANCALIBRATIONSTART)

 protected:
  ~SystemDisplayOverscanCalibrationStartFunction() override = default;
  ResponseAction Run() override;
};

class SystemDisplayOverscanCalibrationAdjustFunction
    : public SystemDisplayCrOSRestrictedFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.overscanCalibrationAdjust",
                             SYSTEM_DISPLAY_OVERSCANCALIBRATIONADJUST)

 protected:
  ~SystemDisplayOverscanCalibrationAdjustFunction() override = default;
  ResponseAction Run() override;
};

class SystemDisplayOverscanCalibrationResetFunction
    : public SystemDisplayCrOSRestrictedFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.overscanCalibrationReset",
                             SYSTEM_DISPLAY_OVERSCANCALIBRATIONRESET)

 protected:
  ~SystemDisplayOverscanCalibrationResetFunction() override = default;
  ResponseAction Run() override;
};

class SystemDisplayOverscanCalibrationCompleteFunction
    : public SystemDisplayCrOSRestrictedFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.overscanCalibrationComplete",
                             SYSTEM_DISPLAY_OVERSCANCALIBRATIONCOMPLETE)

 protected:
  ~SystemDisplayOverscanCalibrationCompleteFunction() override = default;
  ResponseAction Run() override;
};

class SystemDisplayShowNativeTouchCalibrationFunction
    : public SystemDisplayCrOSRestrictedFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.showNativeTouchCalibration",
                             SYSTEM_DISPLAY_SHOWNATIVETOUCHCALIBRATION)

 protected:
  ~SystemDisplayShowNativeTouchCalibrationFunction() override = default;
  ResponseAction Run() override;

  void OnCalibrationComplete(std::optional<std::string> error);
};

class SystemDisplayStartCustomTouchCalibrationFunction
    : public SystemDisplayCrOSRestrictedFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.startCustomTouchCalibration",
                             SYSTEM_DISPLAY_STARTCUSTOMTOUCHCALIBRATION)

 protected:
  ~SystemDisplayStartCustomTouchCalibrationFunction() override = default;
  ResponseAction Run() override;
};

class SystemDisplayCompleteCustomTouchCalibrationFunction
    : public SystemDisplayCrOSRestrictedFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.completeCustomTouchCalibration",
                             SYSTEM_DISPLAY_COMPLETECUSTOMTOUCHCALIBRATION)

 protected:
  ~SystemDisplayCompleteCustomTouchCalibrationFunction() override = default;
  ResponseAction Run() override;
};

class SystemDisplayClearTouchCalibrationFunction
    : public SystemDisplayCrOSRestrictedFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.clearTouchCalibration",
                             SYSTEM_DISPLAY_CLEARTOUCHCALIBRATION)

 protected:
  ~SystemDisplayClearTouchCalibrationFunction() override = default;
  ResponseAction Run() override;
};

class SystemDisplaySetMirrorModeFunction
    : public SystemDisplayCrOSRestrictedFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.setMirrorMode",
                             SYSTEM_DISPLAY_SETMIRRORMODE)

 protected:
  ~SystemDisplaySetMirrorModeFunction() override = default;
  ResponseAction Run() override;

  void Response(std::optional<std::string> error);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_
