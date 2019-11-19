// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_print_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/render_view_host.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"

namespace headless {

HeadlessPrintSettings::HeadlessPrintSettings()
    : prefer_css_page_size(false),
      landscape(false),
      display_header_footer(false),
      should_print_backgrounds(false),
      scale(1),
      ignore_invalid_page_ranges(false) {}

HeadlessPrintManager::HeadlessPrintManager(content::WebContents* web_contents)
    : PrintManager(web_contents) {
  Reset();
}

HeadlessPrintManager::~HeadlessPrintManager() = default;

// static
std::string HeadlessPrintManager::PrintResultToString(PrintResult result) {
  switch (result) {
    case PRINT_SUCCESS:
      return std::string();  // no error message
    case PRINTING_FAILED:
      return "Printing failed";
    case INVALID_PRINTER_SETTINGS:
      return "Show invalid printer settings error";
    case INVALID_MEMORY_HANDLE:
      return "Invalid memory handle";
    case METAFILE_MAP_ERROR:
      return "Map to shared memory error";
    case METAFILE_INVALID_HEADER:
      return "Invalid metafile header";
    case METAFILE_GET_DATA_ERROR:
      return "Get data from metafile error";
    case SIMULTANEOUS_PRINT_ACTIVE:
      return "The previous printing job hasn't finished";
    case PAGE_RANGE_SYNTAX_ERROR:
      return "Page range syntax error";
    case PAGE_COUNT_EXCEEDED:
      return "Page range exceeds page count";
    default:
      NOTREACHED();
      return "Unknown PrintResult";
  }
}

// static
HeadlessPrintManager::PageRangeStatus
HeadlessPrintManager::PageRangeTextToPages(base::StringPiece page_range_text,
                                           bool ignore_invalid_page_ranges,
                                           int pages_count,
                                           std::vector<int>* pages) {
  printing::PageRanges page_ranges;
  for (const auto& range_string :
       base::SplitStringPiece(page_range_text, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    printing::PageRange range;
    if (range_string.find("-") == base::StringPiece::npos) {
      if (!base::StringToInt(range_string, &range.from))
        return SYNTAX_ERROR;
      range.to = range.from;
    } else if (range_string == "-") {
      range.from = 1;
      range.to = pages_count;
    } else if (range_string.starts_with("-")) {
      range.from = 1;
      if (!base::StringToInt(range_string.substr(1), &range.to))
        return SYNTAX_ERROR;
    } else if (range_string.ends_with("-")) {
      range.to = pages_count;
      if (!base::StringToInt(range_string.substr(0, range_string.length() - 1),
                             &range.from))
        return SYNTAX_ERROR;
    } else {
      auto tokens = base::SplitStringPiece(
          range_string, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (tokens.size() != 2 || !base::StringToInt(tokens[0], &range.from) ||
          !base::StringToInt(tokens[1], &range.to))
        return SYNTAX_ERROR;
    }

    if (range.from < 1 || range.from > range.to) {
      if (!ignore_invalid_page_ranges)
        return SYNTAX_ERROR;
      continue;
    }
    if (range.from > pages_count) {
      if (!ignore_invalid_page_ranges)
        return LIMIT_ERROR;
      continue;
    }

    if (range.to > pages_count)
      range.to = pages_count;

    // Page numbers are 1-based in the dictionary.
    // Page numbers are 0-based for the print settings.
    range.from--;
    range.to--;
    page_ranges.push_back(range);
  }
  *pages = printing::PageRange::GetPages(page_ranges);
  return PRINT_NO_ERROR;
}

void HeadlessPrintManager::GetPDFContents(content::RenderFrameHost* rfh,
                                          const HeadlessPrintSettings& settings,
                                          GetPDFCallback callback) {
  DCHECK(callback);

  if (callback_) {
    std::move(callback).Run(SIMULTANEOUS_PRINT_ACTIVE,
                            base::MakeRefCounted<base::RefCountedString>());
    return;
  }
  printing_rfh_ = rfh;
  callback_ = std::move(callback);
  print_params_ = GetPrintParamsFromSettings(settings);
  page_ranges_text_ = settings.page_ranges;
  ignore_invalid_page_ranges_ = settings.ignore_invalid_page_ranges;
  GetPrintRenderFrame(rfh)->PrintRequestedPages();
}

std::unique_ptr<PrintMsg_PrintPages_Params>
HeadlessPrintManager::GetPrintParamsFromSettings(
    const HeadlessPrintSettings& settings) {
  printing::PrintSettings print_settings;
  print_settings.set_dpi(printing::kPointsPerInch);
  print_settings.set_should_print_backgrounds(
      settings.should_print_backgrounds);
  print_settings.set_scale_factor(settings.scale);
  print_settings.SetOrientation(settings.landscape);

  print_settings.set_display_header_footer(settings.display_header_footer);
  if (print_settings.display_header_footer()) {
    url::Replacements<char> url_sanitizer;
    url_sanitizer.ClearUsername();
    url_sanitizer.ClearPassword();
    std::string url = printing_rfh_->GetLastCommittedURL()
                          .ReplaceComponents(url_sanitizer)
                          .spec();
    print_settings.set_url(base::UTF8ToUTF16(url));
  }

  print_settings.set_margin_type(printing::CUSTOM_MARGINS);
  print_settings.SetCustomMargins(settings.margins_in_points);

  gfx::Rect printable_area_device_units(settings.paper_size_in_points);
  print_settings.SetPrinterPrintableArea(settings.paper_size_in_points,
                                         printable_area_device_units, true);

  auto print_params = std::make_unique<PrintMsg_PrintPages_Params>();
  printing::RenderParamsFromPrintSettings(print_settings,
                                          &print_params->params);
  print_params->params.document_cookie = printing::PrintSettings::NewCookie();
  print_params->params.header_template =
      base::UTF8ToUTF16(settings.header_template);
  print_params->params.footer_template =
      base::UTF8ToUTF16(settings.footer_template);
  print_params->params.prefer_css_page_size = settings.prefer_css_page_size;
  return print_params;
}

bool HeadlessPrintManager::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  if (!printing_rfh_ &&
      (message.type() == PrintHostMsg_GetDefaultPrintSettings::ID ||
       message.type() == PrintHostMsg_ScriptedPrint::ID)) {
    std::string type;
    switch (message.type()) {
      case PrintHostMsg_GetDefaultPrintSettings::ID:
        type = "GetDefaultPrintSettings";
        break;
      case PrintHostMsg_ScriptedPrint::ID:
        type = "ScriptedPrint";
        break;
      default:
        type = "Unknown";
        break;
    }
    DLOG(ERROR)
        << "Unexpected message received before GetPDFContents is called: "
        << type;

    // TODO: consider propagating the error back to the caller, rather than
    // effectively dropping the request.
    render_frame_host->Send(IPC::SyncMessage::GenerateReply(&message));
    return true;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(HeadlessPrintManager, message)
    IPC_MESSAGE_HANDLER(PrintHostMsg_ShowInvalidPrinterSettingsError,
                        OnShowInvalidPrinterSettingsError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled || PrintManager::OnMessageReceived(message, render_frame_host);
}

void HeadlessPrintManager::OnGetDefaultPrintSettings(
    content::RenderFrameHost* render_frame_host,
    IPC::Message* reply_msg) {
  PrintHostMsg_GetDefaultPrintSettings::WriteReplyParams(reply_msg,
                                                         print_params_->params);
  // Intentionally using |printing_rfh_| instead of |render_frame_host|
  // parameter.
  printing_rfh_->Send(reply_msg);
}

void HeadlessPrintManager::OnScriptedPrint(
    content::RenderFrameHost* render_frame_host,
    const PrintHostMsg_ScriptedPrint_Params& params,
    IPC::Message* reply_msg) {
  PageRangeStatus status =
      PageRangeTextToPages(page_ranges_text_, ignore_invalid_page_ranges_,
                           params.expected_pages_count, &print_params_->pages);
  // Intentionally using |printing_rfh_| instead of |render_frame_host|
  // parameter.
  switch (status) {
    case SYNTAX_ERROR:
      printing_rfh_->Send(reply_msg);
      ReleaseJob(PAGE_RANGE_SYNTAX_ERROR);
      return;
    case LIMIT_ERROR:
      printing_rfh_->Send(reply_msg);
      ReleaseJob(PAGE_COUNT_EXCEEDED);
      return;
    case PRINT_NO_ERROR:
      PrintHostMsg_ScriptedPrint::WriteReplyParams(reply_msg, *print_params_);
      printing_rfh_->Send(reply_msg);
      return;
    default:
      NOTREACHED();
      return;
  }
}

void HeadlessPrintManager::OnShowInvalidPrinterSettingsError() {
  ReleaseJob(INVALID_PRINTER_SETTINGS);
}

void HeadlessPrintManager::OnPrintingFailed(int cookie) {
  ReleaseJob(PRINTING_FAILED);
}

void HeadlessPrintManager::OnDidPrintDocument(
    content::RenderFrameHost* render_frame_host,
    const PrintHostMsg_DidPrintDocument_Params& params,
    std::unique_ptr<DelayedFrameDispatchHelper> helper) {
  auto& content = params.content;
  if (!content.metafile_data_region.IsValid()) {
    ReleaseJob(INVALID_MEMORY_HANDLE);
    return;
  }
  base::ReadOnlySharedMemoryMapping map = content.metafile_data_region.Map();
  if (!map.IsValid()) {
    ReleaseJob(METAFILE_MAP_ERROR);
    return;
  }
  data_ = std::string(static_cast<const char*>(map.memory()), map.size());
  helper->SendCompleted();
  ReleaseJob(PRINT_SUCCESS);
}

void HeadlessPrintManager::Reset() {
  printing_rfh_ = nullptr;
  callback_.Reset();
  print_params_.reset();
  page_ranges_text_.clear();
  ignore_invalid_page_ranges_ = false;
  data_.clear();
}

void HeadlessPrintManager::ReleaseJob(PrintResult result) {
  if (!callback_) {
    DLOG(ERROR) << "ReleaseJob is called when callback_ is null. Check whether "
                   "ReleaseJob is called more than once.";
    return;
  }

  if (result == PRINT_SUCCESS) {
    std::move(callback_).Run(result,
                             base::RefCountedString::TakeString(&data_));
  } else {
    std::move(callback_).Run(result,
                             base::MakeRefCounted<base::RefCountedString>());
  }
  GetPrintRenderFrame(printing_rfh_)->PrintingDone(result == PRINT_SUCCESS);
  Reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HeadlessPrintManager)

}  // namespace headless
