// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_SYSTEM_DIALOG_WIN_H_
#define PRINTING_PRINTING_CONTEXT_SYSTEM_DIALOG_WIN_H_

#include <ocidl.h>

#include <commdlg.h>

#include <string>

#include "printing/mojom/print.mojom.h"
#include "printing/printing_context_win.h"
#include "ui/gfx/native_widget_types.h"

namespace printing {

class COMPONENT_EXPORT(PRINTING) PrintingContextSystemDialogWin
    : public PrintingContextWin {
 public:
  PrintingContextSystemDialogWin(Delegate* delegate,
                                 ProcessBehavior process_behavior);
  PrintingContextSystemDialogWin(const PrintingContextSystemDialogWin&) =
      delete;
  PrintingContextSystemDialogWin& operator=(
      const PrintingContextSystemDialogWin&) = delete;
  ~PrintingContextSystemDialogWin() override;

  // PrintingContext implementation.
  void AskUserForSettings(int max_pages,
                          bool has_selection,
                          bool is_scripted,
                          PrintSettingsCallback callback) override;

 private:
  friend class MockPrintingContextWin;

  HWND GetWindow();

  virtual HRESULT ShowPrintDialog(PRINTDLGEX* options);

  // Reads the settings from the selected device context. Updates settings_ and
  // its margins.
  bool InitializeSettingsWithRanges(const DEVMODE& dev_mode,
                                    const std::wstring& new_device_name,
                                    const PRINTPAGERANGE* ranges,
                                    int number_ranges,
                                    bool selection_only);

  // Parses the result of a PRINTDLGEX result.
  mojom::ResultCode ParseDialogResultEx(const PRINTDLGEX& dialog_options);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_SYSTEM_DIALOG_WIN_H_
