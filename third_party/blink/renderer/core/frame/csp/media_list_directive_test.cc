// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/media_list_directive.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class MediaListDirectiveTest : public testing::Test {
 public:
  MediaListDirectiveTest()
      : csp(MakeGarbageCollected<ContentSecurityPolicy>()) {}

 protected:
  Persistent<ContentSecurityPolicy> csp;
};

TEST_F(MediaListDirectiveTest, GetIntersect) {
  MediaListDirective a(
      "plugin-types",
      "application/x-shockwave-flash application/pdf text/plain", csp.Get());
  MediaListDirective empty_a("plugin-types", "", csp.Get());

  struct TestCase {
    const char* policy_b;
    const Vector<const char*> expected;
  } cases[] = {
      {"", Vector<const char*>()},
      {"text/", Vector<const char*>()},
      {"text/*", Vector<const char*>()},
      {"*/plain", Vector<const char*>()},
      {"text/plain */plain", {"text/plain"}},
      {"text/plain application/*", {"text/plain"}},
      {"text/plain", {"text/plain"}},
      {"application/pdf", {"application/pdf"}},
      {"application/x-shockwave-flash", {"application/x-shockwave-flash"}},
      {"application/x-shockwave-flash text/plain",
       {"application/x-shockwave-flash", "text/plain"}},
      {"application/pdf text/plain", {"text/plain", "application/pdf"}},
      {"application/x-shockwave-flash application/pdf text/plain",
       {"application/x-shockwave-flash", "application/pdf", "text/plain"}},
  };

  for (const auto& test : cases) {
    MediaListDirective b("plugin-types", test.policy_b, csp.Get());

    HashSet<String> result = a.GetIntersect(b.plugin_types_);
    EXPECT_EQ(result.size(), test.expected.size());

    for (auto* const type : test.expected)
      EXPECT_TRUE(result.Contains(type));

    // If we change the order of `A` and `B`, intersection should not change.
    result = b.GetIntersect(a.plugin_types_);
    EXPECT_EQ(result.size(), test.expected.size());

    for (auto* const type : test.expected)
      EXPECT_TRUE(result.Contains(type));

    // When `A` is empty, there should not be any intersection.
    result = empty_a.GetIntersect(b.plugin_types_);
    EXPECT_FALSE(result.size());
  }
}

TEST_F(MediaListDirectiveTest, Subsumes) {
  MediaListDirective a(
      "plugin-types",
      "application/x-shockwave-flash application/pdf text/plain text/*",
      csp.Get());

  struct TestCase {
    const Vector<const char*> policies_b;
    bool subsumed;
    bool subsumed_by_empty_a;
  } cases[] = {
      // `A` subsumes `policiesB`.
      {{""}, true, true},
      {{"text/"}, true, true},
      {{"text/*"}, true, false},
      {{"application/*"}, false, false},
      {{"application/"}, true, true},
      {{"*/plain"}, false, false},
      {{"application"}, true, true},
      {{"text/plain"}, true, false},
      {{"application/pdf"}, true, false},
      {{"application/x-shockwave-flash"}, true, false},
      {{"application/x-shockwave-flash text/plain"}, true, false},
      {{"application/pdf text/plain"}, true, false},
      {{"application/x-shockwave-flash text/plain application/pdf"},
       true,
       false},
      {{"application/x-shockwave-flash text "}, true, false},
      {{"text/* application/x-shockwave-flash"}, true, false},
      {{"application/ application/x-shockwave-flash"}, true, false},
      {{"*/plain application/x-shockwave-flash"}, false, false},
      {{"text/ application/x-shockwave-flash"}, true, false},
      {{"application application/x-shockwave-flash"}, true, false},
      {{"application/x-shockwave-flash text/plain "
        "application/x-blink-test-plugin",
        "application/x-shockwave-flash text/plain"},
       true,
       false},
      {{"application/x-shockwave-flash text/plain "
        "application/x-blink-test-plugin",
        "text/plain"},
       true,
       false},
      {{"application/x-blink-test-plugin", "text/plain"}, true, true},
      {{"application/x-shockwave-flash",
        "text/plain application/x-shockwave-flash"},
       true,
       false},
      {{"application/x-shockwave-flash text/plain",
        "application/x-blink-test-plugin", "text/plain"},
       true,
       true},
      // `A` does not subsumes `policiesB`.
      {Vector<const char*>(), false, false},
      {{"application/x-blink-test-plugin"}, false, false},
      {{"application/x-shockwave-flash text/plain "
        "application/x-blink-test-plugin"},
       false,
       false},
      {{"application/x-shockwave-flash text application/x-blink-test-plugin"},
       false,
       false},
      {{"application/x-invalid-type text application/"}, false, false},
      {{"application/x-blink-test-plugin text application/",
        "application/x-blink-test-plugin"},
       false,
       false},
  };

  MediaListDirective empty_a("plugin-types", "", csp.Get());

  for (const auto& test : cases) {
    HeapVector<Member<MediaListDirective>> policies_b;
    for (auto* const policy : test.policies_b) {
      policies_b.push_back(MakeGarbageCollected<MediaListDirective>(
          "plugin-types", policy, csp.Get()));
    }

    EXPECT_EQ(a.Subsumes(policies_b), test.subsumed);
    EXPECT_EQ(empty_a.Subsumes(policies_b), test.subsumed_by_empty_a);
  }
}

}  // namespace blink
