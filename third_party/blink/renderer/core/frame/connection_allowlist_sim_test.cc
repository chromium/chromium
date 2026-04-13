// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {
namespace {

const char kMainFrame[] = "https://example.com/main.html";

class ConnectionAllowlistSimTest : public SimTest {
 public:
  ConnectionAllowlistSimTest() = default;

  void LoadWindowWithConnectionAllowlistHeader(const String& header_name) {
    SimRequest::Params params;
    params.response_http_headers.insert(header_name, "(response_origin)");

    SimRequest main(kMainFrame, "text/html", params);
    const String& document = String("<html><body>Hello</body></html>");

    LoadURL(kMainFrame);
    main.Complete(document);
    test::RunPendingTasks();
  }
};

TEST_F(ConnectionAllowlistSimTest, UseCounterForEnforcedHeader) {
  LoadWindowWithConnectionAllowlistHeader("Connection-Allowlist");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kConnectionAllowlist));
}

TEST_F(ConnectionAllowlistSimTest, UseCounterForReportOnlyHeader) {
  LoadWindowWithConnectionAllowlistHeader("Connection-Allowlist-Report-Only");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kConnectionAllowlist));
}

}  // namespace
}  // namespace blink
