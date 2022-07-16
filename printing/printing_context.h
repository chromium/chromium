// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_H_
#define PRINTING_PRINTING_CONTEXT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/native_drawing_context.h"
#include "printing/print_settings.h"
#include "ui/gfx/native_widget_types.h"

namespace printing {

class PrintingContextFactoryForTest;

// An abstraction of a printer context, implemented by objects that describe the
// user selected printing context. This includes the OS-dependent UI to ask the
// user about the print settings. Concrete implementations directly talk to the
// printer and manage the document and page breaks.
class COMPONENT_EXPORT(PRINTING) PrintingContext {
 public:
  // Printing context delegate.
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Returns parent view to use for modal dialogs.
    virtual gfx::NativeView GetParentView() = 0;

    // Returns application locale.
    virtual std::string GetAppLocale() = 0;
  };

  PrintingContext(const PrintingContext&) = delete;
  PrintingContext& operator=(const PrintingContext&) = delete;
  virtual ~PrintingContext();

  // Callback of AskUserForSettings, used to notify the PrintJobWorker when
  // print settings are available.
  using PrintSettingsCallback = base::OnceCallback<void(mojom::ResultCode)>;

  // Asks the user what printer and format should be used to print. Updates the
  // context with the select device settings. The result of the call is returned
  // in the callback. This is necessary for Linux, which only has an
  // asynchronous printing API.
  // On Android, when `is_scripted` is true, calling it initiates a full
  // printing flow from the framework's PrintManager.
  // (see https://codereview.chromium.org/740983002/)
  virtual void AskUserForSettings(int max_pages,
                                  bool has_selection,
                                  bool is_scripted,
                                  PrintSettingsCallback callback) = 0;

  // Selects the user's default printer and format. Updates the context with the
  // default device settings.
  virtual mojom::ResultCode UseDefaultSettings() = 0;

  // Updates the context with PDF printer settings.
  mojom::ResultCode UsePdfSettings();

  // Returns paper size to be used for PDF or Cloud Print in device units.
  virtual gfx::Size GetPdfPaperSizeDeviceUnits() = 0;

  // Updates printer settings.
  // `external_preview` is true if pdf is going to be opened in external
  // preview. Used by MacOS only now to open Preview.app.
  virtual mojom::ResultCode UpdatePrinterSettings(bool external_preview,
                                                  bool show_system_dialog,
                                                  int page_count) = 0;

  // Updates Print Settings. `job_settings` contains all print job
  // settings information.
  mojom::ResultCode UpdatePrintSettings(base::Value job_settings);

#if defined(OS_CHROMEOS)
  // Updates Print Settings.
  mojom::ResultCode UpdatePrintSettingsFromPOD(
      std::unique_ptr<PrintSettings> job_settings);
#endif

  // Applies the print settings to this context.  Intended to be used only by
  // the Print Backend service process.
  void ApplyPrintSettings(const PrintSettings& settings);

  // Does platform specific setup of the printer before the printing. Signal the
  // printer that a document is about to be spooled.
  // Warning: This function enters a message loop. That may cause side effects
  // like IPC message processing! Some printers have side-effects on this call
  // like virtual printers that ask the user for the path of the saved document;
  // for example a PDF printer.
  virtual mojom::ResultCode NewDocument(
      const std::u16string& document_name) = 0;

  // Starts a new page.
  virtual mojom::ResultCode NewPage() = 0;

  // Closes the printed page.
  virtual mojom::ResultCode PageDone() = 0;

  // Closes the printing job. After this call the object is ready to start a new
  // document.
  virtual mojom::ResultCode DocumentDone() = 0;

  // Cancels printing. Can be used in a multi-threaded context. Takes effect
  // immediately.
  virtual void Cancel() = 0;

  // Releases the native printing context.
  virtual void ReleaseContext() = 0;

  // Returns the native context used to print.
  virtual printing::NativeDrawingContext context() const = 0;

#if defined(OS_WIN)
  // Initializes with predefined settings.
  virtual mojom::ResultCode InitWithSettingsForTest(
      std::unique_ptr<PrintSettings> settings) = 0;
#endif

  // Creates an instance of this object.
  static std::unique_ptr<PrintingContext> Create(Delegate* delegate);

  // Test method for generating printing contexts for testing.  This overrides
  // the platform-specific implementations of CreateImpl().
  static void SetPrintingContextFactoryForTest(
      PrintingContextFactoryForTest* factory);

  void set_margin_type(mojom::MarginType type);
  void set_is_modifiable(bool is_modifiable);

  const PrintSettings& settings() const;

  std::unique_ptr<PrintSettings> TakeAndResetSettings();

  int job_id() const { return job_id_; }

 protected:
  explicit PrintingContext(Delegate* delegate);

  // Creates an instance of this object. Implementers of this interface should
  // implement this method to create an object of their implementation.
  static std::unique_ptr<PrintingContext> CreateImpl(Delegate* delegate);

  // Reinitializes the settings for object reuse.
  void ResetSettings();

  // Does bookkeeping when an error occurs.
  virtual mojom::ResultCode OnError();

  // Complete print context settings.
  std::unique_ptr<PrintSettings> settings_;

  // Printing context delegate.
  Delegate* const delegate_;

  // Is a print job being done.
  volatile bool in_print_job_;

  // Did the user cancel the print job.
  volatile bool abort_printing_;

  // The job id for the current job. The value is 0 if no jobs are active.
  int job_id_;
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_H_
