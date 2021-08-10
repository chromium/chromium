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
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/render_view_host.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "mojo/public/cpp/bindings/message.h"
#endif

namespace headless {

namespace {

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
constexpr char kInvalidUpdatePrintSettingsCall[] =
    "Invalid UpdatePrintSettings Call";
constexpr char kInvalidSetupScriptedPrintPreviewCall[] =
    "Invalid SetupScriptedPrintPreview Call";
constexpr char kInvalidShowScriptedPrintPreviewCall[] =
    "Invalid ShowScriptedPrintPreview Call";
constexpr char kInvalidRequestPrintPreviewCall[] =
    "Invalid RequestPrintPreview Call";
constexpr char kInvalidCheckForCancelCall[] = "Invalid CheckForCancel Call";
#endif

#if BUILDFLAG(ENABLE_TAGGED_PDF)
constexpr char kInvalidSetAccessibilityTreeCall[] =
    "Invalid SetAccessibilityTree Call";
#endif

}  // namespace

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
void HeadlessPrintManager::BindPrintManagerHost(
    mojo::PendingAssociatedReceiver<printing::mojom::PrintManagerHost> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* print_manager = HeadlessPrintManager::FromWebContents(web_contents);
  if (!print_manager)
    return;
  print_manager->BindReceiver(std::move(receiver), rfh);
}

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
                                           uint32_t pages_count,
                                           std::vector<uint32_t>* pages) {
  printing::PageRanges page_ranges;
  for (const auto& range_string :
       base::SplitStringPiece(page_range_text, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    printing::PageRange range;
    if (range_string.find("-") == base::StringPiece::npos) {
      if (!base::StringToUint(range_string, &range.from))
        return SYNTAX_ERROR;
      range.to = range.from;
    } else if (range_string == "-") {
      range.from = 1;
      range.to = pages_count;
    } else if (base::StartsWith(range_string, "-")) {
      range.from = 1;
      if (!base::StringToUint(range_string.substr(1), &range.to))
        return SYNTAX_ERROR;
    } else if (base::EndsWith(range_string, "-")) {
      range.to = pages_count;
      if (!base::StringToUint(range_string.substr(0, range_string.length() - 1),
                              &range.from))
        return SYNTAX_ERROR;
    } else {
      auto tokens = base::SplitStringPiece(
          range_string, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (tokens.size() != 2 || !base::StringToUint(tokens[0], &range.from) ||
          !base::StringToUint(tokens[1], &range.to))
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
  set_cookie(print_params_->params->document_cookie);
  GetPrintRenderFrame(rfh)->PrintRequestedPages();
}

printing::mojom::PrintPagesParamsPtr
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

  print_settings.set_margin_type(printing::mojom::MarginType::kCustomMargins);
  print_settings.SetCustomMargins(settings.margins_in_points);

  gfx::Rect printable_area_device_units(settings.paper_size_in_points);
  print_settings.SetPrinterPrintableArea(settings.paper_size_in_points,
                                         printable_area_device_units, true);

  auto print_params = printing::mojom::PrintPagesParams::New();
  print_params->params = printing::mojom::PrintParams::New();
  printing::RenderParamsFromPrintSettings(print_settings,
                                          print_params->params.get());
  print_params->params->document_cookie = printing::PrintSettings::NewCookie();
  print_params->params->header_template =
      base::UTF8ToUTF16(settings.header_template);
  print_params->params->footer_template =
      base::UTF8ToUTF16(settings.footer_template);
  print_params->params->prefer_css_page_size = settings.prefer_css_page_size;
  return print_params;
}

void HeadlessPrintManager::GetDefaultPrintSettings(
    GetDefaultPrintSettingsCallback callback) {
  if (!printing_rfh_) {
    DLOG(ERROR) << "Unexpected message received before GetPDFContents is "
                   "called: GetDefaultPrintSettings";
    std::move(callback).Run(printing::mojom::PrintParams::New());
    return;
  }
  std::move(callback).Run(print_params_->params->Clone());
}

void HeadlessPrintManager::ScriptedPrint(
    printing::mojom::ScriptedPrintParamsPtr params,
    ScriptedPrintCallback callback) {
  PageRangeStatus status =
      PageRangeTextToPages(page_ranges_text_, ignore_invalid_page_ranges_,
                           params->expected_pages_count, &print_params_->pages);

  auto default_param = printing::mojom::PrintPagesParams::New();
  default_param->params = printing::mojom::PrintParams::New();
  if (!printing_rfh_) {
    DLOG(ERROR) << "Unexpected message received before GetPDFContents is "
                   "called: ScriptedPrint";
    std::move(callback).Run(std::move(default_param));
    return;
  }
  switch (status) {
    case SYNTAX_ERROR:
      ReleaseJob(PAGE_RANGE_SYNTAX_ERROR);
      std::move(callback).Run(std::move(default_param));
      return;
    case LIMIT_ERROR:
      ReleaseJob(PAGE_COUNT_EXCEEDED);
      std::move(callback).Run(std::move(default_param));
      return;
    case PRINT_NO_ERROR:
      std::move(callback).Run(print_params_->Clone());
      return;
    default:
      NOTREACHED();
      return;
  }
}

void HeadlessPrintManager::ShowInvalidPrinterSettingsError() {
  ReleaseJob(INVALID_PRINTER_SETTINGS);
}

void HeadlessPrintManager::PrintingFailed(int32_t cookie) {
  ReleaseJob(PRINTING_FAILED);
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void HeadlessPrintManager::UpdatePrintSettings(
    int32_t cookie,
    base::Value job_settings,
    UpdatePrintSettingsCallback callback) {
  // UpdatePrintSettingsCallback() should never be called on
  // HeadlessPrintManager, since it is only triggered by Print Preview.
  mojo::ReportBadMessage(kInvalidUpdatePrintSettingsCall);
}

void HeadlessPrintManager::SetupScriptedPrintPreview(
    SetupScriptedPrintPreviewCallback callback) {
  // SetupScriptedPrintPreview() should never be called on HeadlessPrintManager,
  // since it is only triggered by Print Preview.
  mojo::ReportBadMessage(kInvalidSetupScriptedPrintPreviewCall);
}

void HeadlessPrintManager::ShowScriptedPrintPreview(bool source_is_modifiable) {
  // ShowScriptedPrintPreview() should never be called on HeadlessPrintManager,
  // since it is only triggered by Print Preview.
  mojo::ReportBadMessage(kInvalidShowScriptedPrintPreviewCall);
}

void HeadlessPrintManager::RequestPrintPreview(
    printing::mojom::RequestPrintPreviewParamsPtr params) {
  // RequestPrintPreview() should never be called on HeadlessPrintManager, since
  // it is only triggered by Print Preview.
  mojo::ReportBadMessage(kInvalidRequestPrintPreviewCall);
}

void HeadlessPrintManager::CheckForCancel(int32_t preview_ui_id,
                                          int32_t request_id,
                                          CheckForCancelCallback callback) {
  // CheckForCancel() should never be called on HeadlessPrintManager, since it
  // is only triggered by Print Preview.
  mojo::ReportBadMessage(kInvalidCheckForCancelCall);
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(ENABLE_TAGGED_PDF)
void HeadlessPrintManager::SetAccessibilityTree(
    int32_t cookie,
    const ui::AXTreeUpdate& accessibility_tree) {
  // SetAccessibilityTree() should never be called on HeadlessPrintManager,
  // since it is only triggered by Print Preview.
  mojo::ReportBadMessage(kInvalidSetAccessibilityTreeCall);
}
#endif

void HeadlessPrintManager::DidPrintDocument(
    printing::mojom::DidPrintDocumentParamsPtr params,
    DidPrintDocumentCallback callback) {
  auto& content = *params->content;
  if (!content.metafile_data_region.IsValid()) {
    ReleaseJob(INVALID_MEMORY_HANDLE);
    std::move(callback).Run(false);
    return;
  }
  base::ReadOnlySharedMemoryMapping map = content.metafile_data_region.Map();
  if (!map.IsValid()) {
    ReleaseJob(METAFILE_MAP_ERROR);
    std::move(callback).Run(false);
    return;
  }
  data_ = std::string(static_cast<const char*>(map.memory()), map.size());
  std::move(callback).Run(true);
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
