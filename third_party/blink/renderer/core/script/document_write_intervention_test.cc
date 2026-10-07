// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/document_write_intervention.h"

#include "base/test/scoped_feature_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
namespace {

class DocumentWriteInterventionTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    GetDocument()
        .GetSettings()
        ->SetDisallowFetchForDocWrittenScriptsInMainFrame(false);
  }
};

TEST_F(DocumentWriteInterventionTest,
       InterventionHeaderOmittedWhenFeatureEnabled) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest script_resource("https://other.org/script.js",
                                        "application/javascript");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <script>
      document.write('<script src="https://other.org/script.js"><\/script>');
    </script>
  )HTML");

  Resource* resource = GetDocument().Fetcher()->CachedResource(
      KURL("https://other.org/script.js"));
  ASSERT_TRUE(resource);
  EXPECT_EQ(resource->GetResourceRequest().HttpHeaderField(
                AtomicString("Intervention")),
            g_null_atom);
  EXPECT_THAT(ConsoleMessages(),
              testing::Contains(testing::ResultOf(
                  [](const auto& m) { return m.Utf8(); },
                  testing::HasSubstr("A parser-blocking, cross site"))));

  script_resource.Complete("");
}

TEST_F(DocumentWriteInterventionTest,
       InterventionHeaderOmittedForCrossOriginScript) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest script_resource("https://other.org/script.js",
                                        "application/javascript");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <script>
      document.write(
          '<script crossorigin src="https://other.org/script.js"><\/script>');
    </script>
  )HTML");

  Resource* resource = GetDocument().Fetcher()->CachedResource(
      KURL("https://other.org/script.js"));
  ASSERT_TRUE(resource);
  EXPECT_EQ(resource->GetResourceRequest().GetMode(),
            network::mojom::RequestMode::kCors);
  EXPECT_EQ(resource->GetResourceRequest().HttpHeaderField(
                AtomicString("Intervention")),
            g_null_atom);

  script_resource.Complete("");
}

TEST_F(DocumentWriteInterventionTest,
       InterventionHeaderAttachedWhenKillSwitchActive) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      network::features::kDisallowInterventionInCorsSafelistedHeaders);

  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest script_resource("https://other.org/script.js",
                                        "application/javascript");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <script>
      document.write('<script src="https://other.org/script.js"><\/script>');
    </script>
  )HTML");

  Resource* resource = GetDocument().Fetcher()->CachedResource(
      KURL("https://other.org/script.js"));
  ASSERT_TRUE(resource);
  EXPECT_EQ(
      resource->GetResourceRequest().HttpHeaderField(
          AtomicString("Intervention")),
      AtomicString("<https://www.chromestatus.com/feature/5718547946799104>; "
                   "level=\"warning\""));

  script_resource.Complete("");
}

}  // namespace
}  // namespace blink
