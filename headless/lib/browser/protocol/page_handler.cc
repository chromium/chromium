// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/protocol/page_handler.h"

#include "base/functional/bind.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include <optional>

#include "components/printing/browser/print_to_pdf/pdf_print_utils.h"
#endif

namespace headless {
namespace protocol {

#if BUILDFLAG(ENABLE_PRINTING)
template <typename T>
std::optional<T> OptionalFromMaybe(const Maybe<T>& maybe) {
  return maybe.has_value() ? std::optional<T>(maybe.value()) : std::nullopt;
}
#endif

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

void PageHandler::PrintToPDF(Maybe<bool> landscape,
                             Maybe<bool> display_header_footer,
                             Maybe<bool> print_background,
                             Maybe<double> scale,
                             Maybe<double> paper_width,
                             Maybe<double> paper_height,
                             Maybe<double> margin_top,
                             Maybe<double> margin_bottom,
                             Maybe<double> margin_left,
                             Maybe<double> margin_right,
                             Maybe<String> page_ranges,
                             Maybe<String> header_template,
                             Maybe<String> footer_template,
                             Maybe<bool> prefer_css_page_size,
                             Maybe<String> transfer_mode,
                             Maybe<bool> generate_tagged_pdf,
                             Maybe<bool> generate_document_outline,
                             std::unique_ptr<PrintToPDFCallback> callback) {
  DCHECK(callback);

#if BUILDFLAG(ENABLE_PRINTING)
  if (!web_contents_) {
    callback->sendFailure(Response::ServerError("No web contents to print"));
    return;
  }

  absl::variant<printing::mojom::PrintPagesParamsPtr, std::string>
      print_pages_params = print_to_pdf::GetPrintPagesParams(
          web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(),
          OptionalFromMaybe<bool>(landscape),
          OptionalFromMaybe<bool>(display_header_footer),
          OptionalFromMaybe<bool>(print_background),
          OptionalFromMaybe<double>(scale),
          OptionalFromMaybe<double>(paper_width),
          OptionalFromMaybe<double>(paper_height),
          OptionalFromMaybe<double>(margin_top),
          OptionalFromMaybe<double>(margin_bottom),
          OptionalFromMaybe<double>(margin_left),
          OptionalFromMaybe<double>(margin_right),
          OptionalFromMaybe<std::string>(header_template),
          OptionalFromMaybe<std::string>(footer_template),
          OptionalFromMaybe<bool>(prefer_css_page_size),
          OptionalFromMaybe<bool>(generate_tagged_pdf),
          OptionalFromMaybe<bool>(generate_document_outline));
  if (absl::holds_alternative<std::string>(print_pages_params)) {
    callback->sendFailure(
        Response::InvalidParams(absl::get<std::string>(print_pages_params)));
    return;
  }

  DCHECK(absl::holds_alternative<printing::mojom::PrintPagesParamsPtr>(
      print_pages_params));

  bool return_as_stream = transfer_mode.value_or("") ==
                          Page::PrintToPDF::TransferModeEnum::ReturnAsStream;
  HeadlessPrintManager::FromWebContents(web_contents_.get())
      ->PrintToPdf(
          web_contents_->GetPrimaryMainFrame(), page_ranges.value_or(""),
          std::move(absl::get<printing::mojom::PrintPagesParamsPtr>(
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
    callback->sendSuccess(protocol::Binary::fromRefCounted(data),
                          Maybe<std::string>());
  }
}
#endif  // BUILDFLAG(ENABLE_PRINTING)

}  // namespace protocol
}  // namespace headless
