// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_display/system_display_api.h"

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/api/system_display.h"
#include "extensions/common/permissions/permissions_data.h"

#if defined(OS_CHROMEOS)
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#endif

namespace extensions {

namespace display = api::system_display;

const char SystemDisplayCrOSRestrictedFunction::kCrosOnlyError[] =
    "Function available only on ChromeOS.";
const char SystemDisplayCrOSRestrictedFunction::kKioskOnlyError[] =
    "Only kiosk enabled extensions are allowed to use this function.";

namespace {

class OverscanTracker;

// Singleton class to track overscan calibration overlays. An observer is
// created per WebContents which tracks any calbiration overlays by id.
// If the render frame is deleted (e.g. the tab is closed) before the overlay
// calibraiton is completed, the observer will call the overscan complete
// method to remove the overlay. When all observers are removed, the singleton
// tracker will delete itself.
class OverscanTracker {
 public:
  static void AddDisplay(content::WebContents* web_contents,
                         const std::string& id);
  static void RemoveDisplay(content::WebContents* web_contents,
                            const std::string& id);
  static void RemoveObserver(content::WebContents* web_contents);

  OverscanTracker() {}
  ~OverscanTracker() {}

 private:
  class OverscanWebObserver;

  OverscanWebObserver* GetObserver(content::WebContents* web_contents,
                                   bool create);
  bool RemoveObserverImpl(content::WebContents* web_contents);

  using ObserverMap =
      std::map<content::WebContents*, std::unique_ptr<OverscanWebObserver>>;
  ObserverMap observers_;

  DISALLOW_COPY_AND_ASSIGN(OverscanTracker);
};

class OverscanTracker::OverscanWebObserver
    : public content::WebContentsObserver {
 public:
  explicit OverscanWebObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~OverscanWebObserver() override {}

  // WebContentsObserver
  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    for (const std::string& id : display_ids_) {
      // Reset any uncomitted calibraiton changes and complete calibration to
      // hide the overlay.
      DisplayInfoProvider::Get()->OverscanCalibrationReset(id);
      DisplayInfoProvider::Get()->OverscanCalibrationComplete(id);
    }
    OverscanTracker::RemoveObserver(web_contents());  // Deletes this.
  }

  void AddDisplay(const std::string& id) { display_ids_.insert(id); }

  void RemoveDisplay(const std::string& id) {
    display_ids_.erase(id);
    if (display_ids_.empty())
      OverscanTracker::RemoveObserver(web_contents());  // Deletes this.
  }

 private:
  std::set<std::string> display_ids_;

  DISALLOW_COPY_AND_ASSIGN(OverscanWebObserver);
};

static OverscanTracker* g_overscan_tracker = nullptr;

// static
void OverscanTracker::AddDisplay(content::WebContents* web_contents,
                                 const std::string& id) {
  if (!g_overscan_tracker)
    g_overscan_tracker = new OverscanTracker;
  g_overscan_tracker->GetObserver(web_contents, true)->AddDisplay(id);
}

// static
void OverscanTracker::RemoveDisplay(content::WebContents* web_contents,
                                    const std::string& id) {
  if (!g_overscan_tracker)
    return;
  OverscanWebObserver* observer =
      g_overscan_tracker->GetObserver(web_contents, false);
  if (observer)
    observer->RemoveDisplay(id);
}

// static
void OverscanTracker::RemoveObserver(content::WebContents* web_contents) {
  if (!g_overscan_tracker)
    return;
  if (g_overscan_tracker->RemoveObserverImpl(web_contents)) {
    delete g_overscan_tracker;
    g_overscan_tracker = nullptr;
  }
}

OverscanTracker::OverscanWebObserver* OverscanTracker::GetObserver(
    content::WebContents* web_contents,
    bool create) {
  auto iter = observers_.find(web_contents);
  if (iter != observers_.end())
    return iter->second.get();
  if (!create)
    return nullptr;
  auto owned_observer = std::make_unique<OverscanWebObserver>(web_contents);
  auto* observer_ptr = owned_observer.get();
  observers_[web_contents] = std::move(owned_observer);
  return observer_ptr;
}

bool OverscanTracker::RemoveObserverImpl(content::WebContents* web_contents) {
  observers_.erase(web_contents);
  return observers_.empty();
}

bool HasAutotestPrivate(const ExtensionFunction& function) {
  return function.extension() &&
         function.extension()->permissions_data()->HasAPIPermission(
             APIPermission::kAutoTestPrivate);
}

#if defined(OS_CHROMEOS)
// |edid| is available only to Chrome OS kiosk mode applications.
bool ShouldRestrictEdidInformation(const ExtensionFunction& function) {
  if (function.extension()) {
    return !(HasAutotestPrivate(function) ||
             KioskModeInfo::IsKioskEnabled(function.extension()));
  }

  return function.source_context_type() != Feature::WEBUI_CONTEXT;
}
#endif

}  // namespace

bool SystemDisplayCrOSRestrictedFunction::PreRunValidation(std::string* error) {
  if (!ExtensionFunction::PreRunValidation(error))
    return false;

#if !defined(OS_CHROMEOS)
  *error = kCrosOnlyError;
  return false;
#else
  if (!ShouldRestrictToKioskAndWebUI())
    return true;

  if (source_context_type() == Feature::WEBUI_CONTEXT)
    return true;
  if (KioskModeInfo::IsKioskEnabled(extension()))
    return true;
  *error = kKioskOnlyError;
  return false;
#endif
}

bool SystemDisplayCrOSRestrictedFunction::ShouldRestrictToKioskAndWebUI() {
  return !HasAutotestPrivate(*this);
}

ExtensionFunction::ResponseAction SystemDisplayGetInfoFunction::Run() {
  std::unique_ptr<display::GetInfo::Params> params(
      display::GetInfo::Params::Create(*args_));
  bool single_unified = params->flags && params->flags->single_unified &&
                        *params->flags->single_unified;
  DisplayInfoProvider::Get()->GetAllDisplaysInfo(
      single_unified,
      base::BindOnce(&SystemDisplayGetInfoFunction::Response, this));
  return RespondLater();
}

void SystemDisplayGetInfoFunction::Response(
    DisplayInfoProvider::DisplayUnitInfoList all_displays_info) {
#if defined(OS_CHROMEOS)
  if (ShouldRestrictEdidInformation(*this)) {
    for (auto& display_info : all_displays_info)
      display_info.edid.release();
  }
#endif
  Respond(ArgumentList(display::GetInfo::Results::Create(all_displays_info)));
}

ExtensionFunction::ResponseAction SystemDisplayGetDisplayLayoutFunction::Run() {
  DisplayInfoProvider::Get()->GetDisplayLayout(
      base::BindOnce(&SystemDisplayGetDisplayLayoutFunction::Response, this));
  return RespondLater();
}

void SystemDisplayGetDisplayLayoutFunction::Response(
    DisplayInfoProvider::DisplayLayoutList display_layout) {
  return Respond(
      ArgumentList(display::GetDisplayLayout::Results::Create(display_layout)));
}

bool SystemDisplayGetDisplayLayoutFunction::ShouldRestrictToKioskAndWebUI() {
  return false;
}

ExtensionFunction::ResponseAction
SystemDisplaySetDisplayPropertiesFunction::Run() {
  std::unique_ptr<display::SetDisplayProperties::Params> params(
      display::SetDisplayProperties::Params::Create(*args_));
  DisplayInfoProvider::Get()->SetDisplayProperties(
      params->id, params->info,
      base::BindOnce(&SystemDisplaySetDisplayPropertiesFunction::Response,
                     this));
  return RespondLater();
}

void SystemDisplaySetDisplayPropertiesFunction::Response(
    base::Optional<std::string> error) {
  Respond(error ? Error(*error) : NoArguments());
}

ExtensionFunction::ResponseAction SystemDisplaySetDisplayLayoutFunction::Run() {
  std::unique_ptr<display::SetDisplayLayout::Params> params(
      display::SetDisplayLayout::Params::Create(*args_));
  DisplayInfoProvider::Get()->SetDisplayLayout(
      params->layouts,
      base::BindOnce(&SystemDisplaySetDisplayLayoutFunction::Response, this));
  return RespondLater();
}

void SystemDisplaySetDisplayLayoutFunction::Response(
    base::Optional<std::string> error) {
  Respond(error ? Error(*error) : NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayEnableUnifiedDesktopFunction::Run() {
  std::unique_ptr<display::EnableUnifiedDesktop::Params> params(
      display::EnableUnifiedDesktop::Params::Create(*args_));
  DisplayInfoProvider::Get()->EnableUnifiedDesktop(params->enabled);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayOverscanCalibrationStartFunction::Run() {
  std::unique_ptr<display::OverscanCalibrationStart::Params> params(
      display::OverscanCalibrationStart::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->OverscanCalibrationStart(params->id))
    return RespondNow(Error("Invalid display ID: " + params->id));
  OverscanTracker::AddDisplay(GetSenderWebContents(), params->id);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayOverscanCalibrationAdjustFunction::Run() {
  std::unique_ptr<display::OverscanCalibrationAdjust::Params> params(
      display::OverscanCalibrationAdjust::Params::Create(*args_));
  if (!params)
    return RespondNow(Error("Invalid parameters"));
  if (!DisplayInfoProvider::Get()->OverscanCalibrationAdjust(params->id,
                                                             params->delta)) {
    return RespondNow(
        Error("Calibration not started for display ID: " + params->id));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayOverscanCalibrationResetFunction::Run() {
  std::unique_ptr<display::OverscanCalibrationReset::Params> params(
      display::OverscanCalibrationReset::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->OverscanCalibrationReset(params->id))
    return RespondNow(
        Error("Calibration not started for display ID: " + params->id));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayOverscanCalibrationCompleteFunction::Run() {
  std::unique_ptr<display::OverscanCalibrationComplete::Params> params(
      display::OverscanCalibrationComplete::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->OverscanCalibrationComplete(params->id)) {
    return RespondNow(
        Error("Calibration not started for display ID: " + params->id));
  }
  OverscanTracker::RemoveDisplay(GetSenderWebContents(), params->id);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayShowNativeTouchCalibrationFunction::Run() {
  std::unique_ptr<display::ShowNativeTouchCalibration::Params> params(
      display::ShowNativeTouchCalibration::Params::Create(*args_));
  DisplayInfoProvider::Get()->ShowNativeTouchCalibration(
      params->id,
      base::BindOnce(&SystemDisplayShowNativeTouchCalibrationFunction::
                         OnCalibrationComplete,
                     this));
  return RespondLater();
}

void SystemDisplayShowNativeTouchCalibrationFunction::OnCalibrationComplete(
    base::Optional<std::string> error) {
  Respond(error ? Error(*error)
                : OneArgument(std::make_unique<base::Value>(true)));
}

ExtensionFunction::ResponseAction
SystemDisplayStartCustomTouchCalibrationFunction::Run() {
  std::unique_ptr<display::StartCustomTouchCalibration::Params> params(
      display::StartCustomTouchCalibration::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->StartCustomTouchCalibration(params->id)) {
    return RespondNow(
        Error("Custom touch calibration not available for display."));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayCompleteCustomTouchCalibrationFunction::Run() {
  std::unique_ptr<display::CompleteCustomTouchCalibration::Params> params(
      display::CompleteCustomTouchCalibration::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->CompleteCustomTouchCalibration(
          params->pairs, params->bounds)) {
    return RespondNow(Error("Custom touch calibration completion failed."));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayClearTouchCalibrationFunction::Run() {
  std::unique_ptr<display::ClearTouchCalibration::Params> params(
      display::ClearTouchCalibration::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->ClearTouchCalibration(params->id))
    return RespondNow(Error("Failed to clear custom touch calibration data."));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SystemDisplaySetMirrorModeFunction::Run() {
  std::unique_ptr<display::SetMirrorMode::Params> params(
      display::SetMirrorMode::Params::Create(*args_));

  DisplayInfoProvider::Get()->SetMirrorMode(
      params->info,
      base::BindOnce(&SystemDisplaySetMirrorModeFunction::Response, this));
  return RespondLater();
}

void SystemDisplaySetMirrorModeFunction::Response(
    base::Optional<std::string> error) {
  Respond(error ? Error(*error) : NoArguments());
}

}  // namespace extensions
