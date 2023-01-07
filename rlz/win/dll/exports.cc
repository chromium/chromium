// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Functions exported by the RLZ DLL.

#include <stddef.h>

#include "rlz/lib/machine_deal_win.h"
#include "rlz/lib/net_response_check.h"
#include "rlz/lib/rlz_lib.h"
#include "rlz/lib/rlz_lib_clear.h"

#define RLZ_DLL_EXPORT extern "C" __declspec(dllexport)

RLZ_DLL_EXPORT bool RecordProductEvent(rlz_lib::Product product,
                                       rlz_lib::AccessPoint point,
                                       rlz_lib::Event event_id) {
  return rlz_lib::RecordProductEvent(product, point, event_id);
}

RLZ_DLL_EXPORT bool GetProductEventsAsCgi(rlz_lib::Product product,
                                          char* unescaped_cgi,
                                          size_t unescaped_cgi_size) {
  return rlz_lib::GetProductEventsAsCgi(product, unescaped_cgi,
                                        unescaped_cgi_size);
}
RLZ_DLL_EXPORT bool ClearAllProductEvents(rlz_lib::Product product) {
  return rlz_lib::ClearAllProductEvents(product);
}

RLZ_DLL_EXPORT bool ClearProductEvent(rlz_lib::Product product,
                                      rlz_lib::AccessPoint point,
                                      rlz_lib::Event event_id) {
  return rlz_lib::ClearProductEvent(product, point, event_id);
}

RLZ_DLL_EXPORT bool GetAccessPointRlz(rlz_lib::AccessPoint point,
                                      char* rlz,
                                      size_t rlz_size) {
  return rlz_lib::GetAccessPointRlz(point, rlz, rlz_size);
}

RLZ_DLL_EXPORT bool SetAccessPointRlz(rlz_lib::AccessPoint point,
                                      const char* new_rlz) {
  return rlz_lib::SetAccessPointRlz(point, new_rlz);
}

RLZ_DLL_EXPORT bool CreateMachineState() {
  return rlz_lib::CreateMachineState();
}

RLZ_DLL_EXPORT bool SetMachineDealCode(const char* dcc) {
  return rlz_lib::SetMachineDealCode(dcc);
}

RLZ_DLL_EXPORT bool GetMachineDealCodeAsCgi(char* cgi, size_t cgi_size) {
  return rlz_lib::GetMachineDealCodeAsCgi(cgi, cgi_size);
}

RLZ_DLL_EXPORT bool GetMachineDealCode2(char* dcc, size_t dcc_size) {
  return rlz_lib::GetMachineDealCode(dcc, dcc_size);
}

RLZ_DLL_EXPORT bool GetPingParams(rlz_lib::Product product,
                                  const rlz_lib::AccessPoint* access_points,
                                  char* unescaped_cgi,
                                  size_t unescaped_cgi_size) {
  return rlz_lib::GetPingParams(product, access_points, unescaped_cgi,
                                unescaped_cgi_size);
}

RLZ_DLL_EXPORT bool ParsePingResponse(rlz_lib::Product product,
                                      const char* response) {
  return rlz_lib::ParsePingResponse(product, response);
}

RLZ_DLL_EXPORT bool IsPingResponseValid(const char* response,
                                        int* checksum_idx) {
  return rlz_lib::IsPingResponseValid(response, checksum_idx);
}

RLZ_DLL_EXPORT bool SetMachineDealCodeFromPingResponse(const char* response) {
  return rlz_lib::SetMachineDealCodeFromPingResponse(response);
}

RLZ_DLL_EXPORT bool SendFinancialPing(rlz_lib::Product product,
                                      const rlz_lib::AccessPoint* access_points,
                                      const char* product_signature,
                                      const char* product_brand,
                                      const char* product_id,
                                      const char* product_lang,
                                      bool exclude_machine_id) {
  return rlz_lib::SendFinancialPing(product, access_points, product_signature,
      product_brand, product_id, product_lang, exclude_machine_id);
}

RLZ_DLL_EXPORT bool SendFinancialPingNoDelay(
    rlz_lib::Product product,
    const rlz_lib::AccessPoint* access_points,
    const char* product_signature,
    const char* product_brand,
    const char* product_id,
    const char* product_lang,
    bool exclude_machine_id) {
  return rlz_lib::SendFinancialPing(product, access_points, product_signature,
      product_brand, product_id, product_lang, exclude_machine_id, true);
}

RLZ_DLL_EXPORT void ClearProductState(
    rlz_lib::Product product, const rlz_lib::AccessPoint* access_points) {
  return rlz_lib::ClearProductState(product, access_points);
}
