// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_PRINT_MANAGER_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_PRINT_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "components/printing/browser/print_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_user_data.h"
#include "headless/public/headless_export.h"
#include "printing/print_settings.h"

struct PrintHostMsg_DidPrintDocument_Params;
struct PrintHostMsg_ScriptedPrint_Params;
struct PrintMsg_PrintPages_Params;

namespace headless {

// Exported for tests.
struct HEADLESS_EXPORT HeadlessPrintSettings {
  HeadlessPrintSettings();

  gfx::Size paper_size_in_points;
  printing::PageMargins margins_in_points;
  bool prefer_css_page_size;

  bool landscape;
  bool display_header_footer;
  bool should_print_backgrounds;
  // scale = 1 means 100%.
  double scale;

  std::string page_ranges;
  bool ignore_invalid_page_ranges;
  std::string header_template;
  std::string footer_template;
};

class HeadlessPrintManager
    : public printing::PrintManager,
      public content::WebContentsUserData<HeadlessPrintManager> {
 public:
  enum PrintResult {
    PRINT_SUCCESS,
    PRINTING_FAILED,
    INVALID_PRINTER_SETTINGS,
    INVALID_MEMORY_HANDLE,
    METAFILE_MAP_ERROR,
    METAFILE_INVALID_HEADER,
    METAFILE_GET_DATA_ERROR,
    SIMULTANEOUS_PRINT_ACTIVE,
    PAGE_RANGE_SYNTAX_ERROR,
    PAGE_COUNT_EXCEEDED,
  };

  enum PageRangeStatus {
    PRINT_NO_ERROR,
    SYNTAX_ERROR,
    LIMIT_ERROR,
  };

  using GetPDFCallback =
      base::OnceCallback<void(PrintResult,
                              scoped_refptr<base::RefCountedMemory>)>;

  ~HeadlessPrintManager() override;

  static std::string PrintResultToString(PrintResult result);
  // Exported for tests.
  HEADLESS_EXPORT static PageRangeStatus PageRangeTextToPages(
      base::StringPiece page_range_text,
      bool ignore_invalid_page_ranges,
      int pages_count,
      std::vector<int>* pages);

  // Prints the current document immediately. Since the rendering is
  // asynchronous, the actual printing will not be completed on the return of
  // this function, and |callback| will always get called when printing
  // finishes.
  void GetPDFContents(content::RenderFrameHost* rfh,
                      const HeadlessPrintSettings& settings,
                      GetPDFCallback callback);

 private:
  explicit HeadlessPrintManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<HeadlessPrintManager>;

  std::unique_ptr<PrintMsg_PrintPages_Params> GetPrintParamsFromSettings(
      const HeadlessPrintSettings& settings);
  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

  // printing::PrintManager:
  void OnDidPrintDocument(
      content::RenderFrameHost* render_frame_host,
      const PrintHostMsg_DidPrintDocument_Params& params,
      std::unique_ptr<DelayedFrameDispatchHelper> helper) override;
  void OnGetDefaultPrintSettings(content::RenderFrameHost* render_frame_host,
                                 IPC::Message* reply_msg) override;
  void OnPrintingFailed(int cookie) override;
  void OnScriptedPrint(content::RenderFrameHost* render_frame_host,
                       const PrintHostMsg_ScriptedPrint_Params& params,
                       IPC::Message* reply_msg) override;

  void OnShowInvalidPrinterSettingsError();

  void Reset();
  void ReleaseJob(PrintResult result);

  content::RenderFrameHost* printing_rfh_ = nullptr;
  GetPDFCallback callback_;
  std::unique_ptr<PrintMsg_PrintPages_Params> print_params_;
  std::string page_ranges_text_;
  bool ignore_invalid_page_ranges_ = false;
  std::string data_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(HeadlessPrintManager);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_PRINT_MANAGER_H_
