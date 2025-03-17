// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/protocol/page_handler.h"

#include <variant>

#include "base/functional/bind.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include <optional>

#include "components/printing/browser/print_to_pdf/pdf_print_utils.h"
#endif

namespace headless {
namespace protocol {

PageHandler::PageHandler(scoped_refptr<content::DevToolsAgentHost> agent_host,
                         content::WebContents* web_contents)
    : agent_host_(agent_host), web_contents_(web_contents->GetWeakPtr()) {
  DCHECK(agent_host_);
}

PageHandler::~PageHandler() = default;

void PageHandler::Wire(UberDispatcher* dispatcher) {
  Page::Dispatcher::wire(dispatcher, this);
}

Response PageHandler::Disable() {
  return Response::Success();
}

void PageHandler::PrintToPDF(std::optional<bool> landscape,
                             std::optional<bool> display_header_footer,
                             std::optional<bool> print_background,
                             std::optional<double> scale,
                             std::optional<double> paper_width,
                             std::optional<double> paper_height,
                             std::optional<double> margin_top,
                             std::optional<double> margin_bottom,
                             std::optional<double> margin_left,
                             std::optional<double> margin_right,
                             std::optional<String> page_ranges,
                             std::optional<String> header_template,
                             std::optional<String> footer_template,
                             std::optional<bool> prefer_css_page_size,
                             std::optional<String> transfer_mode,
                             std::optional<bool> generate_tagged_pdf,
                             std::optional<bool> generate_document_outline,
                             std::unique_ptr<PrintToPDFCallback> callback) {
  DCHECK(callback);

#if BUILDFLAG(ENABLE_PRINTING)
  if (!web_contents_) {
    callback->sendFailure(Response::ServerError("No web contents to print"));
    return;
  }

  std::variant<printing::mojom::PrintPagesParamsPtr, std::string>
      print_pages_params = print_to_pdf::GetPrintPagesParams(
          web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(),
          landscape, display_header_footer, print_background, scale,
          paper_width, paper_height, margin_top, margin_bottom, margin_left,
          margin_right, header_template, footer_template, prefer_css_page_size,
          generate_tagged_pdf, generate_document_outline);
  if (std::holds_alternative<std::string>(print_pages_params)) {
    callback->sendFailure(
        Response::InvalidParams(std::get<std::string>(print_pages_params)));
    return;
  }

  DCHECK(std::holds_alternative<printing::mojom::PrintPagesParamsPtr>(
      print_pages_params));

  bool return_as_stream = transfer_mode.value_or("") ==
                          Page::PrintToPDF::TransferModeEnum::ReturnAsStream;
  HeadlessPrintManager::FromWebContents(web_contents_.get())
      ->PrintToPdf(
          web_contents_->GetPrimaryMainFrame(), page_ranges.value_or(""),
          std::move(std::get<printing::mojom::PrintPagesParamsPtr>(
              print_pages_params)),
          base::BindOnce(&PageHandler::PDFCreated, weak_factory_.GetWeakPtr(),
                         return_as_stream, std::move(callback)));
#else
  callback->sendFailure(Response::ServerError("Printing is not enabled"));
  return;
#endif  // BUILDFLAG(ENABLE_PRINTING)
}

#if BUILDFLAG(ENABLE_PRINTING)
void PageHandler::PDFCreated(bool return_as_stream,
                             std::unique_ptr<PrintToPDFCallback> callback,
                             print_to_pdf::PdfPrintResult print_result,
                             scoped_refptr<base::RefCountedMemory> data) {
  if (print_result != print_to_pdf::PdfPrintResult::kPrintSuccess) {
    callback->sendFailure(Response::ServerError(
        print_to_pdf::PdfPrintResultToString(print_result)));
    return;
  }

  if (return_as_stream) {
    std::string handle = agent_host_->CreateIOStreamFromData(data);
    callback->sendSuccess(protocol::Binary(), handle);
  } else {
    callback->sendSuccess(protocol::Binary::fromRefCounted(data), std::nullopt);
  }
}
#endif  // BUILDFLAG(ENABLE_PRINTING)

}  // namespace protocol
}  // namespace headless
