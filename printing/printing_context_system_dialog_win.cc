// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_system_dialog_win.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/message_loop/message_loop_current.h"
#include "base/stl_util.h"
#include "printing/backend/win_helper.h"
#include "printing/print_settings_initializer_win.h"
#include "skia/ext/skia_utils_win.h"

namespace printing {

PrintingContextSystemDialogWin::PrintingContextSystemDialogWin(
    Delegate* delegate)
    : PrintingContextWin(delegate) {}

PrintingContextSystemDialogWin::~PrintingContextSystemDialogWin() {}

void PrintingContextSystemDialogWin::AskUserForSettings(
    int max_pages,
    bool has_selection,
    bool is_scripted,
    PrintSettingsCallback callback) {
  DCHECK(!in_print_job_);

  HWND window = GetRootWindow(delegate_->GetParentView());
  DCHECK(window);

  // Show the OS-dependent dialog box.
  // If the user press
  // - OK, the settings are reset and reinitialized with the new settings. OK is
  //   returned.
  // - Apply then Cancel, the settings are reset and reinitialized with the new
  //   settings. CANCEL is returned.
  // - Cancel, the settings are not changed, the previous setting, if it was
  //   initialized before, are kept. CANCEL is returned.
  // On failure, the settings are reset and FAILED is returned.
  PRINTDLGEX dialog_options = {sizeof(PRINTDLGEX)};
  dialog_options.hwndOwner = window;
  // Disable options we don't support currently.
  // TODO(maruel):  Reuse the previously loaded settings!
  dialog_options.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE |
                         PD_NOCURRENTPAGE | PD_HIDEPRINTTOFILE;
  if (!has_selection)
    dialog_options.Flags |= PD_NOSELECTION;

  PRINTPAGERANGE ranges[32];
  dialog_options.nStartPage = START_PAGE_GENERAL;
  if (max_pages) {
    // Default initialize to print all the pages.
    memset(ranges, 0, sizeof(ranges));
    ranges[0].nFromPage = 1;
    ranges[0].nToPage = max_pages;
    dialog_options.nPageRanges = 1;
    dialog_options.nMaxPageRanges = base::size(ranges);
    dialog_options.nMinPage = 1;
    dialog_options.nMaxPage = max_pages;
    dialog_options.lpPageRanges = ranges;
  } else {
    // No need to bother, we don't know how many pages are available.
    dialog_options.Flags |= PD_NOPAGENUMS;
  }

  if (ShowPrintDialog(&dialog_options) != S_OK) {
    ResetSettings();
    std::move(callback).Run(FAILED);
    return;
  }

  // TODO(maruel):  Support PD_PRINTTOFILE.
  std::move(callback).Run(ParseDialogResultEx(dialog_options));
}

HRESULT PrintingContextSystemDialogWin::ShowPrintDialog(PRINTDLGEX* options) {
  // Runs always on the UI thread.
  static bool is_dialog_shown = false;
  if (is_dialog_shown)
    return E_FAIL;
  // Block opening dialog from nested task. It crashes PrintDlgEx.
  base::AutoReset<bool> auto_reset(&is_dialog_shown, true);

  // Note that this cannot use ui::BaseShellDialog as the print dialog is
  // system modal: opening it from a background thread can cause Windows to
  // get the wrong Z-order which will make the print dialog appear behind the
  // browser frame (but still being modal) so neither the browser frame nor
  // the print dialog will get any input. See http://crbug.com/342697
  // http://crbug.com/180997 for details.
  base::MessageLoopCurrent::ScopedNestableTaskAllower allow;

  return PrintDlgEx(options);
}

bool PrintingContextSystemDialogWin::InitializeSettingsWithRanges(
    const DEVMODE& dev_mode,
    const std::wstring& new_device_name,
    const PRINTPAGERANGE* ranges,
    int number_ranges,
    bool selection_only) {
  DCHECK(GetDeviceCaps(context(), CLIPCAPS));
  DCHECK(GetDeviceCaps(context(), RASTERCAPS) & RC_STRETCHDIB);
  DCHECK(GetDeviceCaps(context(), RASTERCAPS) & RC_BITMAP64);
  // Some printers don't advertise these.
  // DCHECK(GetDeviceCaps(context(), RASTERCAPS) & RC_SCALING);
  // DCHECK(GetDeviceCaps(context(), SHADEBLENDCAPS) & SB_CONST_ALPHA);
  // DCHECK(GetDeviceCaps(context(), SHADEBLENDCAPS) & SB_PIXEL_ALPHA);

  // StretchDIBits() support is needed for printing.
  if (!(GetDeviceCaps(context(), RASTERCAPS) & RC_STRETCHDIB) ||
      !(GetDeviceCaps(context(), RASTERCAPS) & RC_BITMAP64)) {
    NOTREACHED();
    ResetSettings();
    return false;
  }

  DCHECK(!in_print_job_);
  DCHECK(context());
  PageRanges ranges_vector;
  if (!selection_only) {
    // Convert the PRINTPAGERANGE array to a PrintSettings::PageRanges vector.
    ranges_vector.reserve(number_ranges);
    for (int i = 0; i < number_ranges; ++i) {
      PageRange range;
      // Transfer from 1-based to 0-based.
      range.from = ranges[i].nFromPage - 1;
      range.to = ranges[i].nToPage - 1;
      ranges_vector.push_back(range);
    }
  }

  settings_->set_ranges(ranges_vector);
  settings_->set_device_name(new_device_name);
  settings_->set_selection_only(selection_only);
  PrintSettingsInitializerWin::InitPrintSettings(context(), dev_mode,
                                                 settings_.get());

  return true;
}

PrintingContext::Result PrintingContextSystemDialogWin::ParseDialogResultEx(
    const PRINTDLGEX& dialog_options) {
  // If the user clicked OK or Apply then Cancel, but not only Cancel.
  if (dialog_options.dwResultAction != PD_RESULT_CANCEL) {
    // Start fresh, but preserve is_modifiable and GDI print setting.
    bool is_modifiable = settings_->is_modifiable();
    bool print_text_with_gdi = settings_->print_text_with_gdi();
    ResetSettings();
    settings_->set_is_modifiable(is_modifiable);
    settings_->set_print_text_with_gdi(print_text_with_gdi);

    DEVMODE* dev_mode = NULL;
    if (dialog_options.hDevMode) {
      dev_mode =
          reinterpret_cast<DEVMODE*>(GlobalLock(dialog_options.hDevMode));
      DCHECK(dev_mode);
    }

    std::wstring device_name;
    if (dialog_options.hDevNames) {
      DEVNAMES* dev_names =
          reinterpret_cast<DEVNAMES*>(GlobalLock(dialog_options.hDevNames));
      DCHECK(dev_names);
      if (dev_names) {
        device_name = reinterpret_cast<const wchar_t*>(dev_names) +
                      dev_names->wDeviceOffset;
        GlobalUnlock(dialog_options.hDevNames);
      }
    }

    bool success = false;
    if (dev_mode && !device_name.empty()) {
      set_context(dialog_options.hDC);
      PRINTPAGERANGE* page_ranges = NULL;
      DWORD num_page_ranges = 0;
      bool print_selection_only = false;
      if (dialog_options.Flags & PD_PAGENUMS) {
        page_ranges = dialog_options.lpPageRanges;
        num_page_ranges = dialog_options.nPageRanges;
      }
      if (dialog_options.Flags & PD_SELECTION) {
        print_selection_only = true;
      }
      success =
          InitializeSettingsWithRanges(*dev_mode, device_name, page_ranges,
                                       num_page_ranges, print_selection_only);
    }

    if (!success && dialog_options.hDC) {
      DeleteDC(dialog_options.hDC);
      set_context(NULL);
    }

    if (dev_mode) {
      GlobalUnlock(dialog_options.hDevMode);
    }
  } else {
    if (dialog_options.hDC) {
      DeleteDC(dialog_options.hDC);
    }
  }

  if (dialog_options.hDevMode != NULL)
    GlobalFree(dialog_options.hDevMode);
  if (dialog_options.hDevNames != NULL)
    GlobalFree(dialog_options.hDevNames);

  switch (dialog_options.dwResultAction) {
    case PD_RESULT_PRINT:
      return context() ? OK : FAILED;
    case PD_RESULT_APPLY:
      return context() ? CANCEL : FAILED;
    case PD_RESULT_CANCEL:
      return CANCEL;
    default:
      return FAILED;
  }
}

}  // namespace printing
