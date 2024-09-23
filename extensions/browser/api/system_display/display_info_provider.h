// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "extensions/common/api/system_display.h"
#include "ui/display/display_observer.h"

namespace display {
class Display;
class Screen;
}

namespace extensions {

// Implementation class for chrome.system.display extension API
// (system_display_api.cc). Callbacks that provide an error string use an
// empty string for success.
class DisplayInfoProvider : public display::DisplayObserver {
 public:
  using DisplayUnitInfoList = std::vector<api::system_display::DisplayUnitInfo>;
  using DisplayLayoutList = std::vector<api::system_display::DisplayLayout>;
  using ErrorCallback = base::OnceCallback<void(std::optional<std::string>)>;

  DisplayInfoProvider(const DisplayInfoProvider&) = delete;
  DisplayInfoProvider& operator=(const DisplayInfoProvider&) = delete;

  ~DisplayInfoProvider() override;

  // Returns a pointer to DisplayInfoProvider or null if Create() or
  // InitializeForTesting() have not been called yet.
  static DisplayInfoProvider* Get();

  // Called by tests to provide a test implementation for the extension API.
  static void InitializeForTesting(DisplayInfoProvider* display_info_provider);

  // Called by tests to reset the global instance.
  static void ResetForTesting();

  // Updates display |display_id| with |properties|. If the operation fails,
  // |callback| will be called with a non empty error string and no display
  // properties will be changed.
  virtual void SetDisplayProperties(
      const std::string& display_id,
      const api::system_display::DisplayProperties& properties,
      ErrorCallback callback);

  // Updates the display layout with |layouts|. If the operation fails,
  // |callback| will be called with a non empty error string and the layout will
  // not be changed.
  virtual void SetDisplayLayout(const DisplayLayoutList& layouts,
                                ErrorCallback callback);

  // Enables the unified desktop feature.
  virtual void EnableUnifiedDesktop(bool enable);

  // Requests a list of information for all displays. If |single_unified| is
  // true, when in unified mode a single display will be returned representing
  // the single unified desktop.
  virtual void GetAllDisplaysInfo(
      bool single_unified,
      base::OnceCallback<void(DisplayUnitInfoList result)> callback);

  // Gets display layout information.
  virtual void GetDisplayLayout(
      base::OnceCallback<void(DisplayLayoutList result)> callback);

  // Start/Stop observing display state change
  virtual void StartObserving();
  virtual void StopObserving();

  // Implements overscan calibration methods. See system_display.idl. These
  // return false if |id| is invalid.
  virtual bool OverscanCalibrationStart(const std::string& id);
  virtual bool OverscanCalibrationAdjust(
      const std::string& id,
      const api::system_display::Insets& delta);
  virtual bool OverscanCalibrationReset(const std::string& id);
  virtual bool OverscanCalibrationComplete(const std::string& id);

  // Shows the native touch calibration UI. Returns false if native touch
  // calibration cannot be started. Otherwise |callback| will be run when the
  // calibration has completed.
  virtual void ShowNativeTouchCalibration(const std::string& id,
                                          ErrorCallback callback);

  // These methods implement custom touch calibration. They will return false
  // if |id| is invalid or if the operation is invalid.
  virtual bool StartCustomTouchCalibration(const std::string& id);
  virtual bool CompleteCustomTouchCalibration(
      const api::system_display::TouchCalibrationPairQuad& pairs,
      const api::system_display::Bounds& bounds);
  virtual bool ClearTouchCalibration(const std::string& id);

  // Sets the display mode to the specified mirror mode. See system_display.idl.
  // |info|: Mirror mode properties to apply.
  virtual void SetMirrorMode(const api::system_display::MirrorModeInfo& info,
                             ErrorCallback callback);

 protected:
  explicit DisplayInfoProvider(display::Screen* screen = nullptr);

  // Trigger OnDisplayChangedEvent
  void DispatchOnDisplayChangedEvent();

  // Convert a vector of Displays into a DisplayUnitInfoList. This function
  // needs to be thread-safe since it is called via PostTask.
  DisplayUnitInfoList GetAllDisplaysInfoList(
      const std::vector<display::Display>& displays,
      int64_t primary_id) const;

  // Create a DisplayUnitInfo from a display::Display for implementations of
  // GetAllDisplaysInfo()
  static api::system_display::DisplayUnitInfo CreateDisplayUnitInfo(
      const display::Display& display,
      int64_t primary_display_id);

 private:
  // Update the content of each unit in `units` obtained from the corresponding
  // display in `displays` using a platform specific method.
  // This must be safe to call off the ui thread.
  virtual void UpdateDisplayUnitInfoForPlatform(
      const std::vector<display::Display>& displays,
      DisplayUnitInfoList& units) const;

  // DisplayObserver
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  raw_ptr<display::Screen> provided_screen_ = nullptr;

  std::optional<display::ScopedDisplayObserver> display_observer_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_
