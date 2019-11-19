// Copyright 2012 The Chromium Authors. All rights reserved.
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
#include <string>

#include "base/macros.h"
#include "base/memory/free_deleter.h"
#include "base/strings/string16.h"
#include "base/win/scoped_handle.h"
#include "printing/printing_export.h"

// These are helper functions for dealing with Windows Printing.
namespace printing {

struct PRINTING_EXPORT PrinterBasicInfo;

class PRINTING_EXPORT PrinterHandleTraits {
 public:
  using Handle = HANDLE;

  static bool CloseHandle(HANDLE handle);

  static bool IsHandleValid(HANDLE handle) { return !!handle; }

  static HANDLE NullHandle() { return nullptr; }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PrinterHandleTraits);
};

class PRINTING_EXPORT ScopedPrinterHandle
    : public base::win::GenericScopedHandle<PrinterHandleTraits,
                                            base::win::DummyVerifierTraits> {
 public:
  bool OpenPrinterWithName(const wchar_t* printer);
};

class PRINTING_EXPORT PrinterChangeHandleTraits {
 public:
  using Handle = HANDLE;

  static bool CloseHandle(HANDLE handle);

  static bool IsHandleValid(HANDLE handle) { return !!handle; }

  static HANDLE NullHandle() { return nullptr; }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PrinterChangeHandleTraits);
};

using ScopedPrinterChangeHandle =
    base::win::GenericScopedHandle<PrinterChangeHandleTraits,
                                   base::win::DummyVerifierTraits>;

// Wrapper class to wrap the XPS APIs (PTxxx APIs). This is needed because these
// APIs are not available by default on XP. We could delayload prntvpt.dll but
// this would mean having to add that to every binary that links with
// printing.lib (which is a LOT of binaries). So choosing the GetProcAddress
// route instead).
class PRINTING_EXPORT XPSModule {
 public:
  // Returns true if OpenXPS printing is supported.
  static bool IsOpenXpsCapable();

  // All the other methods can ONLY be called after a successful call to Init.
  // Init can be called many times and by multiple threads.
  static bool Init();
  static HRESULT OpenProvider(const base::string16& printer_name,
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
class PRINTING_EXPORT ScopedXPSInitializer {
 public:
  ScopedXPSInitializer();
  ~ScopedXPSInitializer();

  bool initialized() const { return initialized_; }

 private:
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(ScopedXPSInitializer);
};

// Wrapper class to wrap the XPS Print APIs (these are different from the PTxxx
// which deal with the XML Print Schema). This is needed because these
// APIs are only available on Windows 7 and higher.
class PRINTING_EXPORT XPSPrintModule {
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
PRINTING_EXPORT void SetGetDisplayNameFunction(
    std::string (*get_display_name_func)(const std::string& printer_name));

PRINTING_EXPORT bool InitBasicPrinterInfo(HANDLE printer,
                                          PrinterBasicInfo* printer_info);

PRINTING_EXPORT std::string GetDriverInfo(HANDLE printer);

PRINTING_EXPORT std::unique_ptr<DEVMODE, base::FreeDeleter> XpsTicketToDevMode(
    const base::string16& printer_name,
    const std::string& print_ticket);

PRINTING_EXPORT bool IsDevModeWithColor(const DEVMODE* devmode);

// Creates default DEVMODE and sets color option. Some devices need special
// workaround for color.
PRINTING_EXPORT std::unique_ptr<DEVMODE, base::FreeDeleter>
CreateDevModeWithColor(HANDLE printer,
                       const base::string16& printer_name,
                       bool color);

// Creates new DEVMODE. If |in| is not NULL copy settings from there.
PRINTING_EXPORT std::unique_ptr<DEVMODE, base::FreeDeleter> CreateDevMode(
    HANDLE printer,
    DEVMODE* in);

// Prompts for new DEVMODE. If |in| is not NULL copy settings from there.
PRINTING_EXPORT std::unique_ptr<DEVMODE, base::FreeDeleter> PromptDevMode(
    HANDLE printer,
    const base::string16& printer_name,
    DEVMODE* in,
    HWND window,
    bool* canceled);

}  // namespace printing

#endif  // PRINTING_BACKEND_WIN_HELPER_H_
