// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_PROTOCOL_PAGE_HANDLER_H_
#define HEADLESS_LIB_BROWSER_PROTOCOL_PAGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/devtools_agent_host.h"
#include "headless/lib/browser/protocol/domain_handler.h"
#include "headless/lib/browser/protocol/page.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/printing/browser/headless/headless_print_manager.h"
#include "components/printing/browser/print_to_pdf/pdf_print_result.h"
#include "headless/public/headless_export.h"
#endif

namespace content {
class WebContents;
}

namespace headless {
namespace protocol {

class PageHandler : public DomainHandler, public Page::Backend {
 public:
  PageHandler(scoped_refptr<content::DevToolsAgentHost> agent_host,
              content::WebContents* web_contents);

  PageHandler(const PageHandler&) = delete;
  PageHandler& operator=(const PageHandler&) = delete;

  ~PageHandler() override;

  // DomainHandler implementation
  void Wire(UberDispatcher* dispatcher) override;
  Response Disable() override;

  // Page::Backend implementation
  void PrintToPDF(Maybe<bool> landscape,
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
                  std::unique_ptr<PrintToPDFCallback> callback) override;

 private:
#if BUILDFLAG(ENABLE_PRINTING)
  void PDFCreated(bool return_as_stream,
                  std::unique_ptr<PrintToPDFCallback> callback,
                  print_to_pdf::PdfPrintResult print_result,
                  scoped_refptr<base::RefCountedMemory> data);
#endif

  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  base::WeakPtr<content::WebContents> web_contents_;

  base::WeakPtrFactory<PageHandler> weak_factory_{this};
};

}  // namespace protocol
}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_PROTOCOL_PAGE_HANDLER_H_
