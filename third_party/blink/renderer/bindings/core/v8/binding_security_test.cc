// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {
const char kMainFrame[] = "https://example.com/main.html";
const char kSameOriginTarget[] = "https://example.com/target.html";
const char kSameOriginDomainTarget[] = "https://sub.example.com/target.html";
const char kCrossOriginTarget[] = "https://not-example.com/target.html";

const char kTargetHTML[] =
    "<!DOCTYPE html>"
    "<script>"
    "  (window.opener || window.top).postMessage('yay', '*');"
    "</script>";
const char kSameOriginDomainTargetHTML[] =
    "<!DOCTYPE html>"
    "<script>"
    "  document.domain = 'example.com';"
    "  (window.opener || window.top).postMessage('yay', '*');"
    "</script>";
}

class BindingSecurityCounterTest
    : public SimTest,
      public testing::WithParamInterface<const char*> {
 public:
  enum class OriginDisposition { CrossOrigin, SameOrigin, SameOriginDomain };

  BindingSecurityCounterTest() = default;

  void LoadWindowAndAccessProperty(OriginDisposition which_origin,
                                   const String& property) {
    const char* target_url;
    const char* target_html;
    switch (which_origin) {
      case OriginDisposition::CrossOrigin:
        target_url = kCrossOriginTarget;
        target_html = kTargetHTML;
        break;
      case OriginDisposition::SameOrigin:
        target_url = kSameOriginTarget;
        target_html = kTargetHTML;
        break;
      case OriginDisposition::SameOriginDomain:
        target_url = kSameOriginDomainTarget;
        target_html = kSameOriginDomainTargetHTML;
        break;
    }

    SimRequest main(kMainFrame, "text/html");
    SimRequest target(target_url, "text/html");
    const String& document = String::Format(
        "<!DOCTYPE html>"
        "<script>"
        "  %s"
        "  window.addEventListener('message', e => {"
        "    window.other = e.source.%s;"
        "    console.log('yay');"
        "  });"
        "  var w = window.open('%s');"
        "</script>",
        which_origin == OriginDisposition::SameOriginDomain
            ? "document.domain = 'example.com';"
            : "",
        property.Utf8().c_str(), target_url);

    LoadURL(kMainFrame);
    main.Complete(document);
    target.Complete(target_html);
    test::RunPendingTasks();
  }

  void LoadFrameAndAccessProperty(OriginDisposition which_origin,
                                  const String& property) {
    const char* target_url;
    const char* target_html;
    switch (which_origin) {
      case OriginDisposition::CrossOrigin:
        target_url = kCrossOriginTarget;
        target_html = kTargetHTML;
        break;
      case OriginDisposition::SameOrigin:
        target_url = kSameOriginTarget;
        target_html = kTargetHTML;
        break;
      case OriginDisposition::SameOriginDomain:
        target_url = kSameOriginDomainTarget;
        target_html = kSameOriginDomainTargetHTML;
        break;
    }
    SimRequest main(kMainFrame, "text/html");
    SimRequest target(target_url, "text/html");
    const String& document = String::Format(
        "<!DOCTYPE html>"
        "<body>"
        "<script>"
        "  %s"
        "  var i = document.createElement('iframe');"
        "  window.addEventListener('message', e => {"
        "    window.other = e.source.%s;"
        "    console.log('yay');"
        "  });"
        "  i.src = '%s';"
        "  document.body.appendChild(i);"
        "</script>",
        which_origin == OriginDisposition::SameOriginDomain
            ? "document.domain = 'example.com';"
            : "",
        property.Utf8().c_str(), target_url);

    LoadURL(kMainFrame);
    main.Complete(document);
    target.Complete(target_html);
    test::RunPendingTasks();
  }
};

INSTANTIATE_TEST_SUITE_P(WindowProperties,
                         BindingSecurityCounterTest,
                         testing::Values("window",
                                         "self",
                                         "location",
                                         "close",
                                         "closed",
                                         "focus",
                                         "blur",
                                         "frames",
                                         "length",
                                         "top",
                                         "opener",
                                         "parent",
                                         "postMessage"));

TEST_P(BindingSecurityCounterTest, CrossOriginWindow) {
  LoadWindowAndAccessProperty(OriginDisposition::CrossOrigin, GetParam());
  EXPECT_TRUE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kCrossOriginPropertyAccess));
  EXPECT_TRUE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kCrossOriginPropertyAccessFromOpener));
  EXPECT_FALSE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kDocumentDomainEnabledCrossOriginAccess));
}

TEST_P(BindingSecurityCounterTest, SameOriginWindow) {
  LoadWindowAndAccessProperty(OriginDisposition::SameOrigin, GetParam());
  EXPECT_FALSE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kCrossOriginPropertyAccess));
  EXPECT_FALSE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kCrossOriginPropertyAccessFromOpener));
  EXPECT_FALSE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kDocumentDomainEnabledCrossOriginAccess));
}

TEST_P(BindingSecurityCounterTest, SameOriginDomainWindow) {
  LoadWindowAndAccessProperty(OriginDisposition::SameOriginDomain, GetParam());
  EXPECT_FALSE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kCrossOriginPropertyAccess));
  EXPECT_FALSE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kCrossOriginPropertyAccessFromOpener));
  EXPECT_TRUE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kDocumentDomainEnabledCrossOriginAccess));
}

TEST_P(BindingSecurityCounterTest, CrossOriginFrame) {
  LoadFrameAndAccessProperty(OriginDisposition::CrossOrigin, GetParam());
  EXPECT_TRUE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kCrossOriginPropertyAccess));
  EXPECT_FALSE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kCrossOriginPropertyAccessFromOpener));
  EXPECT_FALSE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kDocumentDomainEnabledCrossOriginAccess));
}

TEST_P(BindingSecurityCounterTest, SameOriginFrame) {
  LoadFrameAndAccessProperty(OriginDisposition::SameOrigin, GetParam());
  EXPECT_FALSE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kCrossOriginPropertyAccess));
  EXPECT_FALSE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kCrossOriginPropertyAccessFromOpener));
  EXPECT_FALSE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kDocumentDomainEnabledCrossOriginAccess));
}

TEST_P(BindingSecurityCounterTest, SameOriginDomainFrame) {
  LoadFrameAndAccessProperty(OriginDisposition::SameOriginDomain, GetParam());
  EXPECT_FALSE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kCrossOriginPropertyAccess));
  EXPECT_FALSE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kCrossOriginPropertyAccessFromOpener));
  EXPECT_TRUE(
      GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
          WebFeature::kDocumentDomainEnabledCrossOriginAccess));
}

}  // namespace
