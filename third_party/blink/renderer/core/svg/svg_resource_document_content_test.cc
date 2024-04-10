// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_resource_document_content.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"

namespace blink {

class SVGResourceDocumentContentTest : public SimTest {};

TEST_F(SVGResourceDocumentContentTest, GetDocumentBeforeLoadComplete) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete("<html><body></body></html>");

  const char kSVGUrl[] = "https://example.com/svg.svg";
  SimSubresourceRequest svg_resource(kSVGUrl, "application/xml");

  // Request a resource from the cache.
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  ResourceLoaderOptions options(execution_context->GetCurrentWorld());
  options.initiator_info.name = fetch_initiator_type_names::kCSS;
  FetchParameters params(ResourceRequest(kSVGUrl), options);
  auto* entry =
      SVGResourceDocumentContent::Fetch(params, GetDocument(), nullptr);

  // Write part of the response. The document should not be initialized yet,
  // because the response is not complete. The document would be invalid at this
  // point.
  svg_resource.Start();
  svg_resource.Write("<sv");
  EXPECT_EQ(nullptr, entry->GetDocument());

  // Finish the response, the Document should now be accessible.
  svg_resource.Complete("g></svg>");
  EXPECT_NE(nullptr, entry->GetDocument());
}

}  // namespace blink
