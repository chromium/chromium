// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_WIN_HELPER_H_
#define PRINTING_BACKEND_WIN_HELPER_H_

#include <objidl.h>
#include <prntvpt.h>

// Important to include wincrypt_shim.h before xpsprint.h since
// xpsprint.h includes <wincrypt.h> (xpsprint.h -> msopc.h ->
// wincrypt.h) which in its normal state is incompatible with
// OpenSSL/BoringSSL.
#include "base/win/wincrypt_shim.h"

#include <xpsprint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/win/scoped_handle.h"
#include "printing/mojom/print.mojom.h"

// These are helper functions for dealing with Windows Printing.
namespace printing {

struct COMPONENT_EXPORT(PRINT_BACKEND) PrinterBasicInfo;

class COMPONENT_EXPORT(PRINT_BACKEND) PrinterHandleTraits {
 public:
  PrinterHandleTraits() = delete;
  PrinterHandleTraits(const PrinterHandleTraits&) = delete;
  PrinterHandleTraits& operator=(const PrinterHandleTraits&) = delete;

  using Handle = HANDLE;

  static bool CloseHandle(HANDLE handle);

  static bool IsHandleValid(HANDLE handle) { return !!handle; }

  static HANDLE NullHandle() { return nullptr; }
};

class COMPONENT_EXPORT(PRINT_BACKEND) ScopedPrinterHandle
    : public base::win::GenericScopedHandle<PrinterHandleTraits,
                                            base::win::DummyVerifierTraits> {
 public:
  bool OpenPrinterWithName(const wchar_t* printer);
};

class COMPONENT_EXPORT(PRINT_BACKEND) PrinterChangeHandleTraits {
 public:
  PrinterChangeHandleTraits() = delete;
  PrinterChangeHandleTraits(const PrinterChangeHandleTraits&) = delete;
  PrinterChangeHandleTraits& operator=(const PrinterChangeHandleTraits&) =
      delete;

  using Handle = HANDLE;

  static bool CloseHandle(HANDLE handle);

  static bool IsHandleValid(HANDLE handle) { return !!handle; }

  static HANDLE NullHandle() { return nullptr; }
};

using ScopedPrinterChangeHandle =
    base::win::GenericScopedHandle<PrinterChangeHandleTraits,
                                   base::win::DummyVerifierTraits>;

// Wrapper class to wrap the XPS APIs (PTxxx APIs). This is needed because these
// APIs are not available by default on XP. We could delayload prntvpt.dll but
// this would mean having to add that to every binary that links with
// printing.lib (which is a LOT of binaries). So choosing the GetProcAddress
// route instead).
class COMPONENT_EXPORT(PRINT_BACKEND) XPSModule {
 public:
  // All the other methods can ONLY be called after a successful call to Init.
  // Init can be called many times and by multiple threads.
  static bool Init();
  static HRESULT OpenProvider(const std::wstring& printer_name,
                              DWORD version,
                              HPTPROVIDER* provider);
  static HRESULT GetPrintCapabilities(HPTPROVIDER provider,
                                      IStream* print_ticket,
                                      IStream* capabilities,
                                      BSTR* error_message);
  static HRESULT ConvertDevModeToPrintTicket(HPTPROVIDER provider,
                                             ULONG devmode_size_in_bytes,
                                             PDEVMODE devmode,
                                             EPrintTicketScope scope,
                                             IStream* print_ticket);
  static HRESULT ConvertPrintTicketToDevMode(
      HPTPROVIDER provider,
      IStream* print_ticket,
      EDefaultDevmodeType base_devmode_type,
      EPrintTicketScope scope,
      ULONG* devmode_byte_count,
      PDEVMODE* devmode,
      BSTR* error_message);
  static HRESULT MergeAndValidatePrintTicket(HPTPROVIDER provider,
                                             IStream* base_ticket,
                                             IStream* delta_ticket,
                                             EPrintTicketScope scope,
                                             IStream* result_ticket,
                                             BSTR* error_message);
  static HRESULT ReleaseMemory(PVOID buffer);
  static HRESULT CloseProvider(HPTPROVIDER provider);

 private:
  XPSModule() {}
  static bool InitImpl();
};

// See comments in cc file explaining why we need this.
class COMPONENT_EXPORT(PRINT_BACKEND) ScopedXPSInitializer {
 public:
  ScopedXPSInitializer();
  ScopedXPSInitializer(const ScopedXPSInitializer&) = delete;
  ScopedXPSInitializer& operator=(const ScopedXPSInitializer&) = delete;
  ~ScopedXPSInitializer();

  bool initialized() const { return initialized_; }

 private:
  bool initialized_;
};

// Wrapper class to wrap the XPS Print APIs (these are different from the PTxxx
// which deal with the XML Print Schema). This is needed because these
// APIs are only available on Windows 7 and higher.
class COMPONENT_EXPORT(PRINT_BACKEND) XPSPrintModule {
 public:
  // All the other methods can ONLY be called after a successful call to Init.
  // Init can be called many times and by multiple threads.
  static bool Init();
  static HRESULT StartXpsPrintJob(const LPCWSTR printer_name,
                                  const LPCWSTR job_name,
                                  const LPCWSTR output_file_name,
                                  HANDLE progress_event,
                                  HANDLE completion_event,
                                  UINT8* printable_pages_on,
                                  UINT32 printable_pages_on_count,
                                  IXpsPrintJob** xps_print_job,
                                  IXpsPrintJobStream** document_stream,
                                  IXpsPrintJobStream** print_ticket_stream);

 private:
  XPSPrintModule() {}
  static bool InitImpl();
};

// Sets the function that gets friendly names for network printers.
COMPONENT_EXPORT(PRINT_BACKEND)
void SetGetDisplayNameFunction(
    std::string (*get_display_name_func)(const std::string& printer_name));

COMPONENT_EXPORT(PRINT_BACKEND)
std::optional<PrinterBasicInfo> GetBasicPrinterInfo(HANDLE printer);

COMPONENT_EXPORT(PRINT_BACKEND)
std::vector<std::string> GetDriverInfo(HANDLE printer);

// Determines if the specified printer driver is known to cause a file save
// UI dialog to be displayed when printing a document.
COMPONENT_EXPORT(PRINT_BACKEND)
bool DoesDriverDisplayFileDialogForPrinting(const std::string& printer_name);

COMPONENT_EXPORT(PRINT_BACKEND)
std::unique_ptr<DEVMODE, base::FreeDeleter> XpsTicketToDevMode(
    const std::wstring& printer_name,
    const std::string& print_ticket);

COMPONENT_EXPORT(PRINT_BACKEND) bool IsDevModeWithColor(const DEVMODE* devmode);

// Creates default DEVMODE and sets color option. Some devices need special
// workaround for color.
COMPONENT_EXPORT(PRINT_BACKEND)
std::unique_ptr<DEVMODE, base::FreeDeleter> CreateDevModeWithColor(
    HANDLE printer,
    const std::wstring& printer_name,
    bool color);

// Creates new DEVMODE. If `in` is not NULL copy settings from there.
COMPONENT_EXPORT(PRINT_BACKEND)
std::unique_ptr<DEVMODE, base::FreeDeleter> CreateDevMode(HANDLE printer,
                                                          DEVMODE* in);

// Prompts for new DEVMODE. If `in` is not NULL copy settings from there.
COMPONENT_EXPORT(PRINT_BACKEND)
std::unique_ptr<DEVMODE, base::FreeDeleter> PromptDevMode(
    HANDLE printer,
    const std::wstring& printer_name,
    DEVMODE* in,
    HWND window,
    bool* canceled);

// Expose helper to convert a driver version number to human-friendly
// dot-separated format only for testing.
COMPONENT_EXPORT(PRINT_BACKEND)
std::string GetDriverVersionStringForTesting(DWORDLONG version_number);

// `GetResultCodeFromSystemErrorCode()` is only ever invoked when something has
// gone wrong while interacting with the OS printing system.  If the cause of
// the failure was not of the type to register and be and available from
// `GetLastError()` then we should just use the general error result.
COMPONENT_EXPORT(PRINT_BACKEND)
mojom::ResultCode GetResultCodeFromSystemErrorCode(
    logging::SystemErrorCode system_code);

}  // namespace printing

#endif  // PRINTING_BACKEND_WIN_HELPER_H_
