// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_PROTOCOL_PAGE_HANDLER_H_
#define HEADLESS_LIB_BROWSER_PROTOCOL_PAGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/devtools_agent_host.h"
#include "headless/lib/browser/protocol/domain_handler.h"
#include "headless/lib/browser/protocol/dp_page.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "headless/lib/browser/headless_print_manager.h"
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
                  Maybe<bool> ignore_invalid_page_ranges,
                  Maybe<String> header_template,
                  Maybe<String> footer_template,
                  Maybe<bool> prefer_css_page_size,
                  Maybe<String> transfer_mode,
                  std::unique_ptr<PrintToPDFCallback> callback) override;

 private:
#if BUILDFLAG(ENABLE_PRINTING)
  void PDFCreated(bool returnAsStream,
                  std::unique_ptr<PageHandler::PrintToPDFCallback> callback,
                  HeadlessPrintManager::PrintResult print_result,
                  scoped_refptr<base::RefCountedMemory> data);
#endif
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  content::WebContents* web_contents_;
  base::WeakPtrFactory<PageHandler> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(PageHandler);
};

}  // namespace protocol
}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_PROTOCOL_PAGE_HANDLER_H_
