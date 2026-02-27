#include <string>

#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_capability_element_base.h"
#include "third_party/blink/renderer/core/html/html_geolocation_element.h"
#include "third_party/blink/renderer/core/html/html_permission_element_test_helper.h"
#include "third_party/blink/renderer/core/testing/fuzztest_utils/css_domains.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/fuzztest/src/fuzztest/googletest_fixture_adapter.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

class BlinkFuzzTestEnvironment : public testing::Environment {
 public:
  void SetUp() override { static BlinkFuzzerTestSupport test_support; }
};

testing::Environment* const g_test_env =
    testing::AddGlobalTestEnvironment(new BlinkFuzzTestEnvironment);

class HTMLPermissionElementFuzzer
    : public fuzztest::PerIterationFixtureAdapter<SimTest> {
 public:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameWidget()->Resize(gfx::Size(800, 600));
    WebRuntimeFeatures::EnableFeatureFromString("GeolocationElement", true);

    // Bind the fake permission service to handle registration.
    MainFrame().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::PermissionService::Name_,
        base::BindRepeating(&PermissionElementTestPermissionService::BindHandle,
                            base::Unretained(&permission_service_)));
  }

  void TearDown() override {
    MainFrame().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::PermissionService::Name_, {});
    SimTest::TearDown();
  }

  void TestPermissionElementBounds(const std::string& container_style,
                                   const std::string& element_style) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");

    // Inject the fuzzed style into the permission element.
    StringBuilder html;
    html.Append(R"(
        <style>
          #container { )");
    html.Append(container_style.c_str());
    html.Append(R"( }
          #target { )");
    html.Append(element_style.c_str());
    html.Append(R"( }
        </style>
        <div id="container">
          <geolocation id="target"></geolocation>
        </div>
    )");
    main_resource.Complete(html.ToString());

    // Run lifecycle to ensure layout and paint are up to date.
    Compositor().BeginFrame();
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();

    auto* permission_element = To<HTMLGeolocationElement>(
        GetDocument().getElementById(AtomicString("target")));

    // If the element doesn't exist (e.g. if style caused it to not render or be
    // removed?), return.
    if (!permission_element || !permission_element->IsRendered()) {
      return;
    }

    // Wait for the element to become valid.
    {
      // By default a ScopedRunLoopTimeout will CHECK() when the timeout is hit.
      // Since in our fuzzer this is an expected case, set the timeout cb to
      // ensure this does not happen.
      base::test::ScopedRunLoopTimeout::SetTimeoutCallbackForTesting(
          std::make_unique<base::test::ScopedRunLoopTimeout::TimeoutCallback>(
              base::DoNothing()));
      base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Seconds(2));

      absl::Cleanup reset_timeout_callback = [] {
        base::test::ScopedRunLoopTimeout::SetTimeoutCallbackForTesting(nullptr);
      };

      // Some styles make the element invalid, if that is the case return early
      // since we're only interested in finding styles which leave the element
      // valid but cause the text to go out of element bounds.
      if (!base::test::RunUntil(
              [&]() { return permission_element->isValid(); })) {
        return;
      }
    }

    // Ensure element is valid.
    if (permission_element->isValid()) {
      // If the element is valid, the text span must be within the element's
      // bounds.
      auto text_span = permission_element->permission_text_span_for_testing();
      EXPECT_TRUE(text_span) << "The text span could not be found";

      EXPECT_TRUE(permission_element->GetLayoutObject() &&
                  text_span->GetLayoutObject())
          << "The element should have appropriate layout objects";

      gfx::Rect element_rect =
          permission_element->GetBoundingClientRect()->ToEnclosingRect();
      gfx::Rect text_rect =
          text_span->GetBoundingClientRect()->ToEnclosingRect();

      // Check that the text is contained by the element.
      EXPECT_TRUE(element_rect.Contains(text_rect))
          << "Style resulted in the text being outside the permission element"
          << "\nContainer Style: " << container_style
          << "\nElement Style: " << element_style
          << "\nElement Rect: " << element_rect.ToString()
          << "\nText Rect: " << text_rect.ToString();
    }
  }

 private:
  PermissionElementTestPermissionService permission_service_;
};

// Register the FuzzTest.
// We use AnyCssDeclaration() to generate valid-ish CSS property-value pairs.
FUZZ_TEST_F(HTMLPermissionElementFuzzer, TestPermissionElementBounds)
    .WithDomains(AnyCssDeclaration(), AnyCssDeclaration());

}  // namespace blink
