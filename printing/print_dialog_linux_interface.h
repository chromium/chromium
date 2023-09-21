// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_DIALOG_LINUX_INTERFACE_H_
#define PRINTING_PRINT_DIALOG_LINUX_INTERFACE_H_

#include <memory>
#include <string>

#include "printing/printing_context_linux.h"
#include "ui/gfx/native_widget_types.h"

namespace printing {

class MetafilePlayer;
class PrintSettings;

// An interface for Linux printing dialogs. Classes that live outside of
// printing/ can implement this interface and get threading requirements
// correct without exposing those requirements to printing/.
class PrintDialogLinuxInterface {
 public:
  // Tell the dialog to use the default print setting.
  virtual void UseDefaultSettings() = 0;

  // Updates the dialog to use `settings`. Only used when printing without the
  // system print dialog. E.g. for Print Preview.
  virtual void UpdateSettings(std::unique_ptr<PrintSettings> settings) = 0;

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
  // Updates the dialog to use system print dialog settings saved in `settings`.
  virtual void LoadPrintSettings(const PrintSettings& settings) = 0;
#endif

  // Shows the dialog and handles the response with `callback`. Only used when
  // printing with the native print dialog.
  virtual void ShowDialog(
      gfx::NativeView parent_view,
      bool has_selection,
      PrintingContextLinux::PrintSettingsCallback callback) = 0;

  // Prints the document named `document_name` contained in `metafile`.
  // Called from the print worker thread. Once called, the
  // PrintDialogLinuxInterface instance should not be reused.
  virtual void PrintDocument(const MetafilePlayer& metafile,
                             const std::u16string& document_name) = 0;

  // Releases the caller's ownership of the PrintDialogLinuxInterface. When
  // called, the caller must not access the PrintDialogLinuxInterface
  // afterwards, and vice versa.
  virtual void ReleaseDialog() = 0;

 protected:
  virtual ~PrintDialogLinuxInterface() = default;
};

}  // namespace printing

#endif  // PRINTING_PRINT_DIALOG_LINUX_INTERFACE_H_
