// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_XPS_MODULE_H_
#define PRINTING_BACKEND_XPS_MODULE_H_

#include <objidl.h>
#include <prntvpt.h>

#include <string>

namespace printing {
namespace xps_module {

// Wrappers for the XPS APIs (PTxxx APIs) that annotates the XPS APIs with
// `base::ScopedBlockingCall`.
HRESULT OpenProvider(const std::wstring& printer_name,
                     DWORD version,
                     HPTPROVIDER* provider);
HRESULT GetPrintCapabilities(HPTPROVIDER provider,
                             IStream* print_ticket,
                             IStream* capabilities,
                             BSTR* error_message);
HRESULT ConvertDevModeToPrintTicket(HPTPROVIDER provider,
                                    ULONG devmode_size_in_bytes,
                                    PDEVMODE devmode,
                                    EPrintTicketScope scope,
                                    IStream* print_ticket);
HRESULT ConvertPrintTicketToDevMode(HPTPROVIDER provider,
                                    IStream* print_ticket,
                                    EDefaultDevmodeType base_devmode_type,
                                    EPrintTicketScope scope,
                                    ULONG* devmode_byte_count,
                                    PDEVMODE* devmode,
                                    BSTR* error_message);
HRESULT MergeAndValidatePrintTicket(HPTPROVIDER provider,
                                    IStream* base_ticket,
                                    IStream* delta_ticket,
                                    EPrintTicketScope scope,
                                    IStream* result_ticket,
                                    BSTR* error_message);
HRESULT ReleaseMemory(PVOID buffer);
HRESULT CloseProvider(HPTPROVIDER provider);

}  // namespace xps_module
}  // namespace printing

#endif  // PRINTING_BACKEND_XPS_MODULE_H_
