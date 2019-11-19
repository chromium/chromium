// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_DIALOG_GTK_INTERFACE_H_
#define PRINTING_PRINT_DIALOG_GTK_INTERFACE_H_

#include <memory>

#include "base/strings/string16.h"
#include "printing/printing_context_linux.h"
#include "ui/gfx/native_widget_types.h"

namespace printing {

class MetafilePlayer;
class PrintSettings;

// An interface for GTK printing dialogs. Classes that live outside of
// printing/ can implement this interface and get threading requirements
// correct without exposing those requirements to printing/.
class PrintDialogGtkInterface {
 public:
  // Tell the dialog to use the default print setting.
  virtual void UseDefaultSettings() = 0;

  // Updates the dialog to use |settings|. Only used when printing without the
  // system print dialog. E.g. for Print Preview.
  virtual void UpdateSettings(std::unique_ptr<PrintSettings> settings) = 0;

  // Shows the dialog and handles the response with |callback|. Only used when
  // printing with the native print dialog.
  virtual void ShowDialog(
      gfx::NativeView parent_view,
      bool has_selection,
      PrintingContextLinux::PrintSettingsCallback callback) = 0;

  // Prints the document named |document_name| contained in |metafile|.
  // Called from the print worker thread. Once called, the
  // PrintDialogGtkInterface instance should not be reused.
  virtual void PrintDocument(const MetafilePlayer& metafile,
                             const base::string16& document_name) = 0;

  // Same as AddRef/Release, but with different names since
  // PrintDialogGtkInterface does not inherit from RefCounted.
  virtual void AddRefToDialog() = 0;
  virtual void ReleaseDialog() = 0;

 protected:
  virtual ~PrintDialogGtkInterface() {}
};

}  // namespace printing

#endif  // PRINTING_PRINT_DIALOG_GTK_INTERFACE_H_
