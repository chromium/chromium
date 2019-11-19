// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_manager.h"

#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/manifest/manifest_change_notifier.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

void RegisterMockedURL(const std::string& base_url,
                       const std::string& file_name) {
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(base_url), test::CoreTestDataPath(),
      WebString::FromUTF8(file_name));
}

}  // namespace

class MockManifestChangeNotifier : public ManifestChangeNotifier {
 public:
  MockManifestChangeNotifier(LocalFrame& frame)
      : ManifestChangeNotifier(frame), manifest_change_count_(0) {}
  ~MockManifestChangeNotifier() override = default;

  // ManifestChangeNotifier:
  void DidChangeManifest() override { ++manifest_change_count_; }

  int ManifestChangeCount() { return manifest_change_count_; }

 private:
  int manifest_change_count_;
};

class ManifestManagerTest : public PageTestBase {
 protected:
  ManifestManagerTest() : base_url_("http://internal.test/") {}
  void SetUp() override {
    PageTestBase::SetUp(IntSize());
    ManifestManager::ProvideTo(GetFrame());
  }

  void TearDown() override {
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

  ManifestManager* GetManifestManager(LocalFrame* frame = nullptr) {
    return ManifestManager::From(frame ? *frame : GetFrame());
  }

  void SetMockManifestChangeNotifier(LocalFrame* frame) {
    GetManifestManager(frame)->manifest_change_notifier_ =
        MakeGarbageCollected<MockManifestChangeNotifier>(*frame);
  }

  int ManifestChangeCount(LocalFrame* frame) {
    return static_cast<MockManifestChangeNotifier*>(
               GetManifestManager(frame)->manifest_change_notifier_.Get())
        ->ManifestChangeCount();
  }

  std::string base_url_;
};

TEST_F(ManifestManagerTest, ManifestURL) {
  // Test the default result.
  EXPECT_EQ(nullptr, GetDocument().LinkManifest());

  // Check that we use the first manifest with <link rel=manifest>
  auto* link_manifest = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  link_manifest->setAttribute(blink::html_names::kRelAttr, "manifest");
  GetDocument().head()->AppendChild(link_manifest);
  EXPECT_EQ(link_manifest, GetDocument().LinkManifest());

  // No href attribute was set.
  EXPECT_EQ(KURL(), GetManifestManager()->ManifestURL());

  // Set to some absolute url.
  link_manifest->setAttribute(html_names::kHrefAttr,
                              "http://example.com/manifest.json");
  ASSERT_EQ(link_manifest->Href(), GetManifestManager()->ManifestURL());

  // Set to some relative url.
  link_manifest->setAttribute(html_names::kHrefAttr, "static/manifest.json");
  ASSERT_EQ(link_manifest->Href(), GetManifestManager()->ManifestURL());
}

TEST_F(ManifestManagerTest, ManifestUseCredentials) {
  // Test the default result.
  EXPECT_EQ(nullptr, GetDocument().LinkManifest());

  // Check that we use the first manifest with <link rel=manifest>
  auto* link_manifest = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  link_manifest->setAttribute(blink::html_names::kRelAttr, "manifest");
  GetDocument().head()->AppendChild(link_manifest);

  // No crossorigin attribute was set so credentials shouldn't be used.
  ASSERT_FALSE(link_manifest->FastHasAttribute(html_names::kCrossoriginAttr));
  ASSERT_FALSE(GetManifestManager()->ManifestUseCredentials());

  // Crossorigin set to a random string shouldn't trigger using credentials.
  link_manifest->setAttribute(html_names::kCrossoriginAttr, "foobar");
  ASSERT_FALSE(GetManifestManager()->ManifestUseCredentials());

  // Crossorigin set to 'anonymous' shouldn't trigger using credentials.
  link_manifest->setAttribute(html_names::kCrossoriginAttr, "anonymous");
  ASSERT_FALSE(GetManifestManager()->ManifestUseCredentials());

  // Crossorigin set to 'use-credentials' should trigger using credentials.
  link_manifest->setAttribute(html_names::kCrossoriginAttr, "use-credentials");
  ASSERT_TRUE(GetManifestManager()->ManifestUseCredentials());
}

TEST_F(ManifestManagerTest, NotifyManifestChange) {
  RegisterMockedURL(base_url_, "link-manifest-change.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();

  auto* frame = web_view_helper.GetWebView()->MainFrameImpl();
  SetMockManifestChangeNotifier(frame->GetFrame());
  frame_test_helpers::LoadFrame(frame, base_url_ + "link-manifest-change.html");

  EXPECT_EQ(14, ManifestChangeCount(frame->GetFrame()));
}

}  // namespace blink
