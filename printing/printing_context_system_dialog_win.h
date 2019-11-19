// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_SYSTEM_DIALOG_WIN_H_
#define PRINTING_PRINTING_CONTEXT_SYSTEM_DIALOG_WIN_H_

#include <ocidl.h>  // NOLINT(build/include_order)

#include <commdlg.h>  // Must come after ocidl.h.

#include <string>

#include "base/macros.h"
#include "printing/printing_context_win.h"
#include "ui/gfx/native_widget_types.h"

namespace printing {

class PRINTING_EXPORT PrintingContextSystemDialogWin
    : public PrintingContextWin {
 public:
  explicit PrintingContextSystemDialogWin(Delegate* delegate);
  ~PrintingContextSystemDialogWin() override;

  // PrintingContext implementation.
  void AskUserForSettings(int max_pages,
                          bool has_selection,
                          bool is_scripted,
                          PrintSettingsCallback callback) override;

 private:
  friend class MockPrintingContextWin;

  virtual HRESULT ShowPrintDialog(PRINTDLGEX* options);

  // Reads the settings from the selected device context. Updates settings_ and
  // its margins.
  bool InitializeSettingsWithRanges(const DEVMODE& dev_mode,
                                    const std::wstring& new_device_name,
                                    const PRINTPAGERANGE* ranges,
                                    int number_ranges,
                                    bool selection_only);

  // Parses the result of a PRINTDLGEX result.
  Result ParseDialogResultEx(const PRINTDLGEX& dialog_options);

  DISALLOW_COPY_AND_ASSIGN(PrintingContextSystemDialogWin);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_SYSTEM_DIALOG_WIN_H_
