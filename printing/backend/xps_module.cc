// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/xps_module.h"

#include "base/threading/scoped_blocking_call.h"

namespace printing {
namespace xps_module {

HRESULT OpenProvider(const std::wstring& printer_name,
                     DWORD version,
                     HPTPROVIDER* provider) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return PTOpenProvider(printer_name.c_str(), version, provider);
}

HRESULT GetPrintCapabilities(HPTPROVIDER provider,
                             IStream* print_ticket,
                             IStream* capabilities,
                             BSTR* error_message) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return PTGetPrintCapabilities(provider, print_ticket, capabilities,
                                error_message);
}

HRESULT ConvertDevModeToPrintTicket(HPTPROVIDER provider,
                                    ULONG devmode_size_in_bytes,
                                    PDEVMODE devmode,
                                    EPrintTicketScope scope,
                                    IStream* print_ticket) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return PTConvertDevModeToPrintTicket(provider, devmode_size_in_bytes, devmode,
                                       scope, print_ticket);
}

HRESULT ConvertPrintTicketToDevMode(HPTPROVIDER provider,
                                    IStream* print_ticket,
                                    EDefaultDevmodeType base_devmode_type,
                                    EPrintTicketScope scope,
                                    ULONG* devmode_byte_count,
                                    PDEVMODE* devmode,
                                    BSTR* error_message) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return PTConvertPrintTicketToDevMode(
      provider, print_ticket, base_devmode_type, scope, devmode_byte_count,
      devmode, error_message);
}

HRESULT MergeAndValidatePrintTicket(HPTPROVIDER provider,
                                    IStream* base_ticket,
                                    IStream* delta_ticket,
                                    EPrintTicketScope scope,
                                    IStream* result_ticket,
                                    BSTR* error_message) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return PTMergeAndValidatePrintTicket(provider, base_ticket, delta_ticket,
                                       scope, result_ticket, error_message);
}

HRESULT ReleaseMemory(PVOID buffer) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return PTReleaseMemory(buffer);
}

HRESULT CloseProvider(HPTPROVIDER provider) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return PTCloseProvider(provider);
}

}  // namespace xps_module
}  // namespace printing
