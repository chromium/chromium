// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_PDF_API_H_
#define PPAPI_THUNK_PPB_PDF_API_H_

#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/shared_impl/singleton_resource_id.h"

namespace ppapi {
namespace thunk {

class PPB_PDF_API {
 public:
  virtual void SearchString(const unsigned short* input_string,
                            const unsigned short* input_term,
                            bool case_sensitive,
                            PP_PrivateFindResult** results,
                            uint32_t* count) = 0;
  virtual void DidStartLoading() = 0;
  virtual void DidStopLoading() = 0;
  virtual void SetContentRestriction(int restrictions) = 0;
  virtual void UserMetricsRecordAction(const PP_Var& action) = 0;
  virtual void HasUnsupportedFeature() = 0;
  virtual void ShowAlertDialog(const char* message) = 0;
  virtual bool ShowConfirmDialog(const char* message) = 0;
  virtual PP_Var ShowPromptDialog(const char* message,
                                  const char* default_answer) = 0;
  virtual void SaveAs() = 0;
  virtual PP_Bool IsFeatureEnabled(PP_PDFFeature feature) = 0;
  virtual void Print() = 0;
  virtual void SetSelectedText(const char* selected_text) = 0;
  virtual void SetLinkUnderCursor(const char* url) = 0;
  virtual void GetV8ExternalSnapshotData(const char** snapshot_data_out,
                                         int* snapshot_size_out) = 0;
  virtual void SetAccessibilityViewportInfo(
      const PP_PrivateAccessibilityViewportInfo* viewport_info) = 0;
  virtual void SetAccessibilityDocInfo(
      const PP_PrivateAccessibilityDocInfo* doc_info) = 0;
  virtual void SetAccessibilityPageInfo(
      const PP_PrivateAccessibilityPageInfo* page_info,
      const PP_PrivateAccessibilityTextRunInfo text_runs[],
      const PP_PrivateAccessibilityCharInfo chars[],
      const PP_PrivateAccessibilityPageObjects* page_objects) = 0;
  virtual void SetCrashData(const char* pdf_url, const char* top_level_url) = 0;
  virtual void SelectionChanged(const PP_FloatPoint& left,
                                int32_t left_height,
                                const PP_FloatPoint& right,
                                int32_t right_height) = 0;
  virtual void SetPluginCanSave(bool can_save) = 0;

  static const SingletonResourceID kSingletonResourceID = PDF_SINGLETON_ID;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_PDF_API_H_
