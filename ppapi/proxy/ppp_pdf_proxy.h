// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_PDF_PROXY_H_
#define PPAPI_PROXY_PPP_PDF_PROXY_H_

#include <string>

#include "base/macros.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "ppapi/proxy/interface_proxy.h"

namespace ppapi {

namespace proxy {

class PPP_Pdf_Proxy : public InterfaceProxy {
 public:
  explicit PPP_Pdf_Proxy(Dispatcher* dispatcher);
  ~PPP_Pdf_Proxy() override;

  static const PPP_Pdf* GetProxyInterface();

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  // Message handlers.
  void OnPluginMsgRotate(PP_Instance instance, bool clockwise);
  void OnPluginMsgPrintPresetOptions(PP_Instance instance,
                                     PP_PdfPrintPresetOptions_Dev* options,
                                     PP_Bool* result);
  void OnPluginMsgEnableAccessibility(PP_Instance instance);
  void OnPluginMsgSetCaretPosition(PP_Instance instance,
                                   const PP_FloatPoint& position);
  void OnPluginMsgMoveRangeSelectionExtent(PP_Instance instance,
                                           const PP_FloatPoint& extent);
  void OnPluginMsgSetSelectionBounds(PP_Instance instance,
                                     const PP_FloatPoint& base,
                                     const PP_FloatPoint& extent);
  void OnPluginMsgCanEditText(PP_Instance instance, PP_Bool* result);
  void OnPluginMsgHasEditableText(PP_Instance instance, PP_Bool* result);
  void OnPluginMsgCanUndo(PP_Instance instance, PP_Bool* result);
  void OnPluginMsgCanRedo(PP_Instance instance, PP_Bool* result);
  void OnPluginMsgUndo(PP_Instance instance);
  void OnPluginMsgRedo(PP_Instance instance);
  void OnPluginMsgHandleAccessibilityAction(
      PP_Instance instance,
      const PP_PdfAccessibilityActionData& action_data);
  void OnPluginMsgReplaceSelection(PP_Instance instance,
                                   const std::string& text);
  void OnPluginMsgPrintBegin(PP_Instance instance,
                             const PP_PrintSettings_Dev& print_settings,
                             const PP_PdfPrintSettings_Dev& pdf_print_settings,
                             int32_t* result);

  // When this proxy is in the plugin side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the host, this value is always NULL.
  const PPP_Pdf* ppp_pdf_;

  DISALLOW_COPY_AND_ASSIGN(PPP_Pdf_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_PDF_PROXY_H_
