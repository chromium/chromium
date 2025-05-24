// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_display/system_display_api.h"

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/permissions/permissions_data.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#endif

namespace extensions {

namespace display = api::system_display;

const char SystemDisplayFunction::kApiNotAvailableError[] =
    "System display API is not available.";

const char SystemDisplayCrOSRestrictedFunction::kCrosOnlyError[] =
    "Function available only on ChromeOS.";
const char SystemDisplayCrOSRestrictedFunction::kKioskOnlyError[] =
    "Only kiosk enabled extensions are allowed to use this function.";

namespace {

class OverscanTracker;

// Singleton class to track overscan calibration overlays. An observer is
// created per WebContents which tracks any calbiration overlays by id.
// If the primary main render frame is deleted (e.g. the tab is closed)
// before the overlay calibraiton is completed, the observer will call the
// overscan complete method to remove the overlay. When all observers are
// removed, the singleton tracker will delete itself.
class OverscanTracker {
 public:
  static void AddDisplay(content::WebContents* web_contents,
                         const std::string& id);
  static void RemoveDisplay(content::WebContents* web_contents,
                            const std::string& id);
  static void RemoveObserver(content::WebContents* web_contents);

  OverscanTracker() = default;

  OverscanTracker(const OverscanTracker&) = delete;
  OverscanTracker& operator=(const OverscanTracker&) = delete;

  ~OverscanTracker() = default;

 private:
  class OverscanWebObserver;

  OverscanWebObserver* GetObserver(content::WebContents* web_contents,
                                   bool create);
  bool RemoveObserverImpl(content::WebContents* web_contents);

  using ObserverMap =
      std::map<content::WebContents*, std::unique_ptr<OverscanWebObserver>>;
  ObserverMap observers_;
};

class OverscanTracker::OverscanWebObserver
    : public content::WebContentsObserver {
 public:
  explicit OverscanWebObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  OverscanWebObserver(const OverscanWebObserver&) = delete;
  OverscanWebObserver& operator=(const OverscanWebObserver&) = delete;

  ~OverscanWebObserver() override = default;

  // WebContentsObserver
  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    DisplayInfoProvider* provider = DisplayInfoProvider::Get();
    DCHECK(provider);
    if (!render_frame_host->IsInPrimaryMainFrame())
      return;
    for (const std::string& id : display_ids_) {
      // Reset any uncomitted calibraiton changes and complete calibration to
      // hide the overlay.
      provider->OverscanCalibrationReset(id);
      provider->OverscanCalibrationComplete(id);
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
             mojom::APIPermissionID::kAutoTestPrivate);
}

#if BUILDFLAG(IS_CHROMEOS)
// |edid| is available only to Chrome OS kiosk mode applications.
bool ShouldRestrictEdidInformation(const ExtensionFunction& function) {
  if (function.extension()) {
    return !(HasAutotestPrivate(function) ||
             KioskModeInfo::IsKioskEnabled(function.extension()));
  }

  return function.source_context_type() != mojom::ContextType::kWebUi;
}
#endif

}  // namespace

bool SystemDisplayFunction::PreRunValidation(std::string* error) {
  if (!ExtensionFunction::PreRunValidation(error))
    return false;

  if (!DisplayInfoProvider::Get()) {
    *error = kApiNotAvailableError;
    return false;
  }

  return true;
}

bool SystemDisplayCrOSRestrictedFunction::PreRunValidation(std::string* error) {
  if (!SystemDisplayFunction::PreRunValidation(error))
    return false;

#if BUILDFLAG(IS_CHROMEOS)
  if (!ShouldRestrictToKioskAndWebUI())
    return true;

  if (source_context_type() == mojom::ContextType::kWebUi) {
    return true;
  }
  if (KioskModeInfo::IsKioskEnabled(extension()))
    return true;
  *error = kKioskOnlyError;
  return false;
#else
  *error = kCrosOnlyError;
  return false;
#endif
}

bool SystemDisplayCrOSRestrictedFunction::ShouldRestrictToKioskAndWebUI() {
  return !HasAutotestPrivate(*this);
}

ExtensionFunction::ResponseAction SystemDisplayGetInfoFunction::Run() {
  std::optional<display::GetInfo::Params> params =
      display::GetInfo::Params::Create(args());
  DisplayInfoProvider* provider = DisplayInfoProvider::Get();
  DCHECK(provider);

  bool single_unified = params->flags && params->flags->single_unified &&
                        *params->flags->single_unified;
  provider->GetAllDisplaysInfo(
      single_unified,
      base::BindOnce(&SystemDisplayGetInfoFunction::Response, this));
  return RespondLater();
}

void SystemDisplayGetInfoFunction::Response(
    std::vector<api::system_display::DisplayUnitInfo> all_displays_info) {
#if BUILDFLAG(IS_CHROMEOS)
  if (ShouldRestrictEdidInformation(*this)) {
    for (auto& display_info : all_displays_info)
      display_info.edid.reset();
  }
#endif
  Respond(ArgumentList(display::GetInfo::Results::Create(all_displays_info)));
}

ExtensionFunction::ResponseAction SystemDisplayGetDisplayLayoutFunction::Run() {
  DisplayInfoProvider* provider = DisplayInfoProvider::Get();
  DCHECK(provider);
  provider->GetDisplayLayout(
      base::BindOnce(&SystemDisplayGetDisplayLayoutFunction::Response, this));
  return RespondLater();
}

void SystemDisplayGetDisplayLayoutFunction::Response(
    std::vector<api::system_display::DisplayLayout> display_layout) {
  return Respond(
      ArgumentList(display::GetDisplayLayout::Results::Create(display_layout)));
}

bool SystemDisplayGetDisplayLayoutFunction::ShouldRestrictToKioskAndWebUI() {
  return false;
}

ExtensionFunction::ResponseAction
SystemDisplaySetDisplayPropertiesFunction::Run() {
  std::optional<display::SetDisplayProperties::Params> params =
      display::SetDisplayProperties::Params::Create(args());
  DisplayInfoProvider* provider = DisplayInfoProvider::Get();
  DCHECK(provider);
  provider->SetDisplayProperties(
      params->id, params->info,
      base::BindOnce(&SystemDisplaySetDisplayPropertiesFunction::Response,
                     this));
  return RespondLater();
}

void SystemDisplaySetDisplayPropertiesFunction::Response(
    std::optional<std::string> error) {
  Respond(error ? Error(*error) : NoArguments());
}

ExtensionFunction::ResponseAction SystemDisplaySetDisplayLayoutFunction::Run() {
  std::optional<display::SetDisplayLayout::Params> params =
      display::SetDisplayLayout::Params::Create(args());
  DisplayInfoProvider* provider = DisplayInfoProvider::Get();
  DCHECK(provider);
  provider->SetDisplayLayout(
      params->layouts,
      base::BindOnce(&SystemDisplaySetDisplayLayoutFunction::Response, this));
  return RespondLater();
}

void SystemDisplaySetDisplayLayoutFunction::Response(
    std::optional<std::string> error) {
  Respond(error ? Error(*error) : NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayEnableUnifiedDesktopFunction::Run() {
  std::optional<display::EnableUnifiedDesktop::Params> params =
      display::EnableUnifiedDesktop::Params::Create(args());
  DisplayInfoProvider* provider = DisplayInfoProvider::Get();
  DCHECK(provider);
  provider->EnableUnifiedDesktop(params->enabled);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayOverscanCalibrationStartFunction::Run() {
  std::optional<display::OverscanCalibrationStart::Params> params =
      display::OverscanCalibrationStart::Params::Create(args());
  DisplayInfoProvider* provider = DisplayInfoProvider::Get();
  DCHECK(provider);
  if (!provider->OverscanCalibrationStart(params->id))
    return RespondNow(Error("Invalid display ID: " + params->id));
  OverscanTracker::AddDisplay(GetSenderWebContents(), params->id);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayOverscanCalibrationAdjustFunction::Run() {
  std::optional<display::OverscanCalibrationAdjust::Params> params =
      display::OverscanCalibrationAdjust::Params::Create(args());
  if (!params)
    return RespondNow(Error("Invalid parameters"));

  DisplayInfoProvider* provider = DisplayInfoProvider::Get();
  DCHECK(provider);
  if (!provider->OverscanCalibrationAdjust(params->id, params->delta)) {
    return RespondNow(
        Error("Calibration not started for display ID: " + params->id));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayOverscanCalibrationResetFunction::Run() {
  std::optional<display::OverscanCalibrationReset::Params> params =
      display::OverscanCalibrationReset::Params::Create(args());
  DisplayInfoProvider* provider = DisplayInfoProvider::Get();
  DCHECK(provider);
  if (!provider->OverscanCalibrationReset(params->id))
    return RespondNow(
        Error("Calibration not started for display ID: " + params->id));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayOverscanCalibrationCompleteFunction::Run() {
  std::optional<display::OverscanCalibrationComplete::Params> params =
      display::OverscanCalibrationComplete::Params::Create(args());
  DisplayInfoProvider* provider = DisplayInfoProvider::Get();
  DCHECK(provider);
  if (!provider->OverscanCalibrationComplete(params->id)) {
    return RespondNow(
        Error("Calibration not started for display ID: " + params->id));
  }
  OverscanTracker::RemoveDisplay(GetSenderWebContents(), params->id);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayShowNativeTouchCalibrationFunction::Run() {
  std::optional<display::ShowNativeTouchCalibration::Params> params =
      display::ShowNativeTouchCalibration::Params::Create(args());
  DisplayInfoProvider* provider = DisplayInfoProvider::Get();
  DCHECK(provider);
  provider->ShowNativeTouchCalibration(
      params->id,
      base::BindOnce(&SystemDisplayShowNativeTouchCalibrationFunction::
                         OnCalibrationComplete,
                     this));
  return RespondLater();
}

void SystemDisplayShowNativeTouchCalibrationFunction::OnCalibrationComplete(
    std::optional<std::string> error) {
  Respond(error ? Error(*error) : WithArguments(true));
}

ExtensionFunction::ResponseAction
SystemDisplayStartCustomTouchCalibrationFunction::Run() {
  std::optional<display::StartCustomTouchCalibration::Params> params =
      display::StartCustomTouchCalibration::Params::Create(args());
  DisplayInfoProvider* provider = DisplayInfoProvider::Get();
  DCHECK(provider);
  if (!provider->StartCustomTouchCalibration(params->id)) {
    return RespondNow(
        Error("Custom touch calibration not available for display."));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayCompleteCustomTouchCalibrationFunction::Run() {
  std::optional<display::CompleteCustomTouchCalibration::Params> params =
      display::CompleteCustomTouchCalibration::Params::Create(args());
  DisplayInfoProvider* provider = DisplayInfoProvider::Get();
  DCHECK(provider);
  if (!provider->CompleteCustomTouchCalibration(params->pairs,
                                                params->bounds)) {
    return RespondNow(Error("Custom touch calibration completion failed."));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayClearTouchCalibrationFunction::Run() {
  std::optional<display::ClearTouchCalibration::Params> params =
      display::ClearTouchCalibration::Params::Create(args());
  DisplayInfoProvider* provider = DisplayInfoProvider::Get();
  DCHECK(provider);
  if (!provider->ClearTouchCalibration(params->id))
    return RespondNow(Error("Failed to clear custom touch calibration data."));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SystemDisplaySetMirrorModeFunction::Run() {
  std::optional<display::SetMirrorMode::Params> params =
      display::SetMirrorMode::Params::Create(args());

  DisplayInfoProvider* provider = DisplayInfoProvider::Get();
  DCHECK(provider);
  provider->SetMirrorMode(
      params->info,
      base::BindOnce(&SystemDisplaySetMirrorModeFunction::Response, this));
  return RespondLater();
}

void SystemDisplaySetMirrorModeFunction::Response(
    std::optional<std::string> error) {
  Respond(error ? Error(*error) : NoArguments());
}

}  // namespace extensions
