// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_pdf_proxy.h"

#include "build/build_config.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {
namespace proxy {

namespace {

#if !defined(OS_NACL)
PP_Var GetLinkAtPosition(PP_Instance instance, PP_Point point) {
  // This isn't implemented in the out of process case.
  return PP_MakeUndefined();
}

void Transform(PP_Instance instance, PP_PrivatePageTransformType type) {
  bool clockwise = type == PP_PRIVATEPAGETRANSFORMTYPE_ROTATE_90_CW;
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPdf_Rotate(API_ID_PPP_PDF, instance, clockwise));
}

PP_Bool GetPrintPresetOptionsFromDocument(
    PP_Instance instance,
    PP_PdfPrintPresetOptions_Dev* options) {
  PP_Bool ret = PP_FALSE;
  HostDispatcher::GetForInstance(instance)
      ->Send(new PpapiMsg_PPPPdf_PrintPresetOptions(
          API_ID_PPP_PDF, instance, options, &ret));
  return ret;
}

void EnableAccessibility(PP_Instance instance) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPdf_EnableAccessibility(API_ID_PPP_PDF, instance));
}

void SetCaretPosition(PP_Instance instance, const PP_FloatPoint* position) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPdf_SetCaretPosition(API_ID_PPP_PDF, instance,
                                           *position));
}

void MoveRangeSelectionExtent(PP_Instance instance,
                              const PP_FloatPoint* extent) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPdf_MoveRangeSelectionExtent(API_ID_PPP_PDF, instance,
                                                   *extent));
}

void SetSelectionBounds(PP_Instance instance,
                        const PP_FloatPoint* base,
                        const PP_FloatPoint* extent) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPdf_SetSelectionBounds(API_ID_PPP_PDF, instance, *base,
                                             *extent));
}

PP_Bool CanEditText(PP_Instance instance) {
  PP_Bool ret = PP_FALSE;
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPdf_CanEditText(API_ID_PPP_PDF, instance, &ret));
  return ret;
}

PP_Bool HasEditableText(PP_Instance instance) {
  PP_Bool ret = PP_FALSE;
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPdf_HasEditableText(API_ID_PPP_PDF, instance, &ret));
  return ret;
}

void ReplaceSelection(PP_Instance instance, const char* text) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPdf_ReplaceSelection(API_ID_PPP_PDF, instance, text));
}

PP_Bool CanUndo(PP_Instance instance) {
  PP_Bool ret = PP_FALSE;
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPdf_CanUndo(API_ID_PPP_PDF, instance, &ret));
  return ret;
}

PP_Bool CanRedo(PP_Instance instance) {
  PP_Bool ret = PP_FALSE;
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPdf_CanRedo(API_ID_PPP_PDF, instance, &ret));
  return ret;
}

void Undo(PP_Instance instance) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPdf_Undo(API_ID_PPP_PDF, instance));
}

void Redo(PP_Instance instance) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPdf_Redo(API_ID_PPP_PDF, instance));
}

void HandleAccessibilityAction(
    PP_Instance instance,
    const PP_PdfAccessibilityActionData& action_data) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPdf_HandleAccessibilityAction(API_ID_PPP_PDF, instance,
                                                    action_data));
}

int32_t PrintBegin(PP_Instance instance,
                   const PP_PrintSettings_Dev* print_settings,
                   const PP_PdfPrintSettings_Dev* pdf_print_settings) {
  int32_t ret = 0;
  HostDispatcher::GetForInstance(instance)->Send(new PpapiMsg_PPPPdf_PrintBegin(
      API_ID_PPP_PDF, instance, *print_settings, *pdf_print_settings, &ret));
  return ret;
}

const PPP_Pdf ppp_pdf_interface = {
    &GetLinkAtPosition,
    &Transform,
    &GetPrintPresetOptionsFromDocument,
    &EnableAccessibility,
    &SetCaretPosition,
    &MoveRangeSelectionExtent,
    &SetSelectionBounds,
    &CanEditText,
    &HasEditableText,
    &ReplaceSelection,
    &CanUndo,
    &CanRedo,
    &Undo,
    &Redo,
    &HandleAccessibilityAction,
    &PrintBegin,
};
#else
// The NaCl plugin doesn't need the host side interface - stub it out.
const PPP_Pdf ppp_pdf_interface = {};
#endif

}  // namespace

PPP_Pdf_Proxy::PPP_Pdf_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_pdf_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_pdf_ = static_cast<const PPP_Pdf*>(
        dispatcher->local_get_interface()(PPP_PDF_INTERFACE));
  }
}

PPP_Pdf_Proxy::~PPP_Pdf_Proxy() {
}

// static
const PPP_Pdf* PPP_Pdf_Proxy::GetProxyInterface() {
  return &ppp_pdf_interface;
}

bool PPP_Pdf_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->IsPlugin())
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_Pdf_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPdf_Rotate, OnPluginMsgRotate)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPdf_PrintPresetOptions,
                        OnPluginMsgPrintPresetOptions)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPdf_EnableAccessibility,
                        OnPluginMsgEnableAccessibility)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPdf_SetCaretPosition,
                        OnPluginMsgSetCaretPosition)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPdf_MoveRangeSelectionExtent,
                        OnPluginMsgMoveRangeSelectionExtent)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPdf_SetSelectionBounds,
                        OnPluginMsgSetSelectionBounds)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPdf_CanEditText, OnPluginMsgCanEditText)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPdf_HasEditableText,
                        OnPluginMsgHasEditableText)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPdf_ReplaceSelection,
                        OnPluginMsgReplaceSelection)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPdf_CanUndo, OnPluginMsgCanUndo)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPdf_CanRedo, OnPluginMsgCanRedo)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPdf_Undo, OnPluginMsgUndo)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPdf_Redo, OnPluginMsgRedo)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPdf_HandleAccessibilityAction,
                        OnPluginMsgHandleAccessibilityAction)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPdf_PrintBegin, OnPluginMsgPrintBegin)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_Pdf_Proxy::OnPluginMsgRotate(PP_Instance instance, bool clockwise) {
  PP_PrivatePageTransformType type = clockwise ?
      PP_PRIVATEPAGETRANSFORMTYPE_ROTATE_90_CW :
      PP_PRIVATEPAGETRANSFORMTYPE_ROTATE_90_CCW;
  if (ppp_pdf_)
    CallWhileUnlocked(ppp_pdf_->Transform, instance, type);
}

void PPP_Pdf_Proxy::OnPluginMsgPrintPresetOptions(
    PP_Instance instance,
    PP_PdfPrintPresetOptions_Dev* options,
    PP_Bool* result) {
  if (ppp_pdf_) {
    *result = CallWhileUnlocked(
        ppp_pdf_->GetPrintPresetOptionsFromDocument, instance, options);
  }
}

void PPP_Pdf_Proxy::OnPluginMsgEnableAccessibility(PP_Instance instance) {
  if (ppp_pdf_)
    CallWhileUnlocked(ppp_pdf_->EnableAccessibility, instance);
}

void PPP_Pdf_Proxy::OnPluginMsgSetCaretPosition(PP_Instance instance,
                                                const PP_FloatPoint& position) {
  if (ppp_pdf_)
    CallWhileUnlocked(ppp_pdf_->SetCaretPosition, instance, &position);
}

void PPP_Pdf_Proxy::OnPluginMsgMoveRangeSelectionExtent(
    PP_Instance instance,
    const PP_FloatPoint& extent) {
  if (ppp_pdf_)
    CallWhileUnlocked(ppp_pdf_->MoveRangeSelectionExtent, instance, &extent);
}

void PPP_Pdf_Proxy::OnPluginMsgSetSelectionBounds(PP_Instance instance,
                                                  const PP_FloatPoint& base,
                                                  const PP_FloatPoint& extent) {
  if (ppp_pdf_)
    CallWhileUnlocked(ppp_pdf_->SetSelectionBounds, instance, &base, &extent);
}

void PPP_Pdf_Proxy::OnPluginMsgCanEditText(PP_Instance instance,
                                           PP_Bool* result) {
  *result = PP_FromBool(ppp_pdf_ &&
                        CallWhileUnlocked(ppp_pdf_->CanEditText, instance));
}

void PPP_Pdf_Proxy::OnPluginMsgHasEditableText(PP_Instance instance,
                                               PP_Bool* result) {
  *result = PP_FromBool(ppp_pdf_ &&
                        CallWhileUnlocked(ppp_pdf_->HasEditableText, instance));
}

void PPP_Pdf_Proxy::OnPluginMsgReplaceSelection(PP_Instance instance,
                                                const std::string& text) {
  if (ppp_pdf_)
    CallWhileUnlocked(ppp_pdf_->ReplaceSelection, instance, text.c_str());
}

void PPP_Pdf_Proxy::OnPluginMsgCanUndo(PP_Instance instance, PP_Bool* result) {
  *result =
      PP_FromBool(ppp_pdf_ && CallWhileUnlocked(ppp_pdf_->CanUndo, instance));
}

void PPP_Pdf_Proxy::OnPluginMsgCanRedo(PP_Instance instance, PP_Bool* result) {
  *result =
      PP_FromBool(ppp_pdf_ && CallWhileUnlocked(ppp_pdf_->CanRedo, instance));
}

void PPP_Pdf_Proxy::OnPluginMsgUndo(PP_Instance instance) {
  if (ppp_pdf_)
    CallWhileUnlocked(ppp_pdf_->Undo, instance);
}

void PPP_Pdf_Proxy::OnPluginMsgRedo(PP_Instance instance) {
  if (ppp_pdf_)
    CallWhileUnlocked(ppp_pdf_->Redo, instance);
}

void PPP_Pdf_Proxy::OnPluginMsgHandleAccessibilityAction(
    PP_Instance instance,
    const PP_PdfAccessibilityActionData& action_data) {
  if (ppp_pdf_) {
    CallWhileUnlocked(ppp_pdf_->HandleAccessibilityAction, instance,
                      action_data);
  }
}

void PPP_Pdf_Proxy::OnPluginMsgPrintBegin(
    PP_Instance instance,
    const PP_PrintSettings_Dev& print_settings,
    const PP_PdfPrintSettings_Dev& pdf_print_settings,
    int32_t* result) {
  if (!ppp_pdf_) {
    *result = 0;
    return;
  }

  *result = CallWhileUnlocked(ppp_pdf_->PrintBegin, instance, &print_settings,
                              &pdf_print_settings);
}

}  // namespace proxy
}  // namespace ppapi
