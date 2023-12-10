// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_print_settings_manager.h"

#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "ppapi/c/pp_errors.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "base/threading/thread_restrictions.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "printing/printing_context.h"  // nogncheck
#include "printing/units.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

namespace content {

namespace {

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
// Print units conversion functions.
int32_t DeviceUnitsInPoints(int32_t device_units,
                            int32_t device_units_per_inch) {
  return printing::ConvertUnit(
      device_units, device_units_per_inch, printing::kPointsPerInch);
}

PP_Size PrintSizeToPPPrintSize(const gfx::Size& print_size,
                               int32_t device_units_per_inch) {
  PP_Size result;
  result.width = DeviceUnitsInPoints(print_size.width(), device_units_per_inch);
  result.height =
      DeviceUnitsInPoints(print_size.height(), device_units_per_inch);
  return result;
}

PP_Rect PrintAreaToPPPrintArea(const gfx::Rect& print_area,
                               int32_t device_units_per_inch) {
  PP_Rect result;
  result.point.x =
      DeviceUnitsInPoints(print_area.origin().x(), device_units_per_inch);
  result.point.y =
      DeviceUnitsInPoints(print_area.origin().y(), device_units_per_inch);
  result.size =
      PrintSizeToPPPrintSize(print_area.size(), device_units_per_inch);
  return result;
}

class PrintingContextDelegate : public printing::PrintingContext::Delegate {
 public:
  // PrintingContext::Delegate methods.
  gfx::NativeView GetParentView() override { return gfx::NativeView(); }
  std::string GetAppLocale() override {
    return GetContentClient()->browser()->GetApplicationLocale();
  }
};

#endif

}  // namespace

PepperPrintSettingsManager::Result
PepperPrintSettingsManagerImpl::ComputeDefaultPrintSettings() {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // This function should run on the UI thread because |PrintingContext| methods
  // call into platform APIs.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if BUILDFLAG(IS_WIN)
  // Blocking is needed here because Windows printer drivers are oftentimes
  // not thread-safe and have to be accessed on the UI thread.
  base::ScopedAllowBlocking allow_blocking;
#endif

  PrintingContextDelegate delegate;
  std::unique_ptr<printing::PrintingContext> context(
      printing::PrintingContext::Create(
          &delegate, printing::PrintingContext::ProcessBehavior::kOopDisabled));
  if (!context.get() ||
      context->UseDefaultSettings() != printing::mojom::ResultCode::kSuccess) {
    return PepperPrintSettingsManager::Result(PP_PrintSettings_Dev(),
                                              PP_ERROR_FAILED);
  }
  const printing::PrintSettings& print_settings = context->settings();
  const printing::PageSetup& page_setup =
      print_settings.page_setup_device_units();
  int device_units_per_inch = print_settings.device_units_per_inch();
  if (device_units_per_inch <= 0) {
    return PepperPrintSettingsManager::Result(PP_PrintSettings_Dev(),
                                              PP_ERROR_FAILED);
  }
  PP_PrintSettings_Dev settings;
  settings.printable_area = PrintAreaToPPPrintArea(page_setup.printable_area(),
                                                   device_units_per_inch);
  settings.content_area =
      PrintAreaToPPPrintArea(page_setup.content_area(), device_units_per_inch);
  settings.paper_size =
      PrintSizeToPPPrintSize(page_setup.physical_size(), device_units_per_inch);
  settings.dpi = print_settings.dpi();

  // The remainder of the attributes are hard-coded to the defaults as set
  // elsewhere.
  settings.orientation = PP_PRINTORIENTATION_NORMAL;
  settings.grayscale = PP_FALSE;
  settings.print_scaling_option = PP_PRINTSCALINGOPTION_SOURCE_SIZE;

  // TODO(raymes): Should be computed in the same way as
  // |PluginInstance::GetPreferredPrintOutputFormat|.
  // |PP_PRINTOUTPUTFORMAT_PDF| is currently the only supported format though,
  // so just make it the default.
  settings.format = PP_PRINTOUTPUTFORMAT_PDF;
  return PepperPrintSettingsManager::Result(settings, PP_OK);
#else
  return PepperPrintSettingsManager::Result(PP_PrintSettings_Dev(),
                                            PP_ERROR_NOTSUPPORTED);
#endif
}

void PepperPrintSettingsManagerImpl::GetDefaultPrintSettings(
    PepperPrintSettingsManager::Callback callback) {
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(ComputeDefaultPrintSettings),
      std::move(callback));
}

}  // namespace content
