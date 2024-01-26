// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_H_
#define PRINTING_PRINTING_CONTEXT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/native_drawing_context.h"
#include "printing/print_settings.h"
#include "ui/gfx/native_widget_types.h"

namespace printing {

class MetafilePlayer;
class PrintingContextFactoryForTest;

#if BUILDFLAG(IS_WIN)
class PageSetup;
class PrintedPage;
#endif

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

  struct PrinterSettings {
#if BUILDFLAG(IS_MAC)
    // True if the to-be-printed PDF is going to be opened in external
    // preview. Used by macOS only to open Preview.app.
    bool external_preview;
#endif

    // Whether to show the system dialog.
    bool show_system_dialog;

#if BUILDFLAG(IS_WIN)
    // If showing the system dialog, the number of pages in the to-be-printed
    // PDF. Only used on Windows.
    int page_count;
#endif
  };

  enum class ProcessBehavior {
    // Out-of-process support is disabled.  All platform printing calls are
    // performed in the browser process.
    kOopDisabled,
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    // Out-of-process support is enabled.  This is for `PrintingContext`
    // objects which exist in the PrintBackend service.  These objects make
    // platform printing calls.
    kOopEnabledPerformSystemCalls,
    // Out-of-process support is enabled.  This is for `PrintingContext`
    // objects which exist in the browser process.  These objects normally skip
    // doing platform printing calls, deferring to an associated
    // `PrintingContext` that is running in a PrintBackend service (i.e.,
    // deferring to a `PrintingContext` with `kOopEnabledPerformSystemCalls`).
    // An exception to deferring platform calls in this case is for platforms
    // that cannot display a system print dialog from a PrintBackend service.
    // On such platforms the relevant calls to invoke a system print dialog
    // are still made from the browser process.
    kOopEnabledSkipSystemCalls,
#endif
  };

  // Value returned by `job_id()` when there is no active print job or the
  // platform/test does not expose an underlying job ID for extra management.
  static constexpr int kNoPrintJobId = 0;

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

  // Updates the context with PDF printer settings. The PDF settings are
  // guaranteed to be valid.
  void UsePdfSettings();

  // Returns paper size to be used for PDF or Cloud Print in device units.
  virtual gfx::Size GetPdfPaperSizeDeviceUnits() = 0;

  // Updates printer settings.
  virtual mojom::ResultCode UpdatePrinterSettings(
      const PrinterSettings& printer_settings) = 0;

  // Updates Print Settings. `job_settings` contains all print job settings
  // information.
  mojom::ResultCode UpdatePrintSettings(base::Value::Dict job_settings);

#if BUILDFLAG(IS_CHROMEOS)
  // Updates Print Settings.
  mojom::ResultCode UpdatePrintSettingsFromPOD(
      std::unique_ptr<PrintSettings> job_settings);
#endif

  // Sets the print settings to `settings`.
  void SetPrintSettings(const PrintSettings& settings);

  // Set the printable area in print settings to be the default printable area.
  // Intended to be used only for virtual printers.
  void SetDefaultPrintableAreaForVirtualPrinters();

  // Does platform specific setup of the printer before the printing. Signal the
  // printer that a document is about to be spooled.
  // Warning: This function enters a message loop. That may cause side effects
  // like IPC message processing! Some printers have side-effects on this call
  // like virtual printers that ask the user for the path of the saved document;
  // for example a PDF printer.
  virtual mojom::ResultCode NewDocument(
      const std::u16string& document_name) = 0;

#if BUILDFLAG(IS_WIN)
  // Renders a page.
  virtual mojom::ResultCode RenderPage(const PrintedPage& page,
                                       const PageSetup& page_setup) = 0;
#endif

  // Prints the document contained in `metafile`.
  virtual mojom::ResultCode PrintDocument(const MetafilePlayer& metafile,
                                          const PrintSettings& settings,
                                          uint32_t num_pages) = 0;

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

#if BUILDFLAG(IS_WIN)
  // Initializes with predefined settings.
  virtual mojom::ResultCode InitWithSettingsForTest(
      std::unique_ptr<PrintSettings> settings) = 0;
#endif

  // Creates an instance of this object.
  static std::unique_ptr<PrintingContext> Create(
      Delegate* delegate,
      ProcessBehavior process_behavior);

  // Test method for generating printing contexts for testing.  This overrides
  // the platform-specific implementations of CreateImpl().
  static void SetPrintingContextFactoryForTest(
      PrintingContextFactoryForTest* factory);

  // Determine process behavior, which can determine if system calls should be
  // made and if certain extra code paths should be followed to support
  // out-of-process printing.
  ProcessBehavior process_behavior() const { return process_behavior_; }

  void set_margin_type(mojom::MarginType type);
  void set_is_modifiable(bool is_modifiable);

  const PrintSettings& settings() const;

  std::unique_ptr<PrintSettings> TakeAndResetSettings();

  bool PrintingAborted() const { return abort_printing_; }

  int job_id() const { return job_id_; }

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  // Override the job ID for this context.  Can only be called to update the
  // value for a `PrintingContext` in the browser process with a value that was
  // determined by a PrintBackend service.
  void SetJobId(int job_id);
#endif

 protected:
  PrintingContext(Delegate* delegate, ProcessBehavior process_behavior);

  // Creates an instance of this object. Implementers of this interface should
  // implement this method to create an object of their implementation.
  static std::unique_ptr<PrintingContext> CreateImpl(
      Delegate* delegate,
      ProcessBehavior process_behavior);

  // Reinitializes the settings for object reuse.
  void ResetSettings();

  // Does bookkeeping when an error occurs.
  virtual mojom::ResultCode OnError();

  // Complete print context settings.
  std::unique_ptr<PrintSettings> settings_;

  // Printing context delegate.
  const raw_ptr<Delegate> delegate_;

  // Is a print job being done.
  volatile bool in_print_job_;

  // Did the user cancel the print job.
  volatile bool abort_printing_;

  // The job id for the current job used by the underlying platform.
  // The value is `kNoPrintJobId` if no jobs are active or if the platform
  // or test does not require passing such an ID for extra print job
  // management.
  int job_id_ = kNoPrintJobId;

 private:
  const ProcessBehavior process_behavior_;
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_H_
