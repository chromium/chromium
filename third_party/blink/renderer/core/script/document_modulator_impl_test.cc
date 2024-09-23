// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

class DocumentModulatorImplTest : public PageTestBase {

 public:
  DocumentModulatorImplTest() = default;
  DocumentModulatorImplTest(const DocumentModulatorImplTest&) = delete;
  DocumentModulatorImplTest& operator=(const DocumentModulatorImplTest&) =
      delete;
  void SetUp() override;

 protected:
  Persistent<Modulator> modulator_;
};

void DocumentModulatorImplTest::SetUp() {
  PageTestBase::SetUp(gfx::Size(500, 500));
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  modulator_ = Modulator::From(script_state);
}

TEST_F(DocumentModulatorImplTest, ResolveModuleSpecifier) {
  // Taken from examples listed in
  // https://html.spec.whatwg.org/C/#resolve-a-module-specifier

  // "The following are valid module specifiers according to the above
  // algorithm:"
  EXPECT_TRUE(modulator_
                  ->ResolveModuleSpecifier("https://example.com/apples.js",
                                           NullURL(),
                                           /*failure_reason=*/nullptr)
                  .IsValid());

  KURL resolved = modulator_->ResolveModuleSpecifier(
      "http:example.com\\pears.mjs", NullURL(), /*failure_reason=*/nullptr);
  EXPECT_TRUE(resolved.IsValid());
  EXPECT_EQ("http://example.com/pears.mjs", resolved.GetString());

  KURL base_url(NullURL(), "https://example.com");
  EXPECT_TRUE(modulator_
                  ->ResolveModuleSpecifier("//example.com/", base_url,
                                           /*failure_reason=*/nullptr)
                  .IsValid());
  EXPECT_TRUE(modulator_
                  ->ResolveModuleSpecifier("./strawberries.js.cgi", base_url,
                                           /*failure_reason=*/nullptr)
                  .IsValid());
  EXPECT_TRUE(modulator_
                  ->ResolveModuleSpecifier("../lychees", base_url,
                                           /*failure_reason=*/nullptr)
                  .IsValid());
  EXPECT_TRUE(modulator_
                  ->ResolveModuleSpecifier("/limes.jsx", base_url,
                                           /*failure_reason=*/nullptr)
                  .IsValid());
  EXPECT_TRUE(modulator_
                  ->ResolveModuleSpecifier(
                      "data:text/javascript,export default 'grapes';",
                      NullURL(), /*failure_reason=*/nullptr)
                  .IsValid());
  EXPECT_TRUE(
      modulator_
          ->ResolveModuleSpecifier(
              "blob:https://whatwg.org/d0360e2f-caee-469f-9a2f-87d5b0456f6f",
              KURL(), /*failure_reason=*/nullptr)
          .IsValid());

  // "The following are valid module specifiers according to the above
  // algorithm, but will invariably cause failures when they are fetched:"
  EXPECT_TRUE(
      modulator_
          ->ResolveModuleSpecifier("javascript:export default 'artichokes';",
                                   NullURL(), /*failure_reason=*/nullptr)
          .IsValid());
  EXPECT_TRUE(
      modulator_
          ->ResolveModuleSpecifier("data:text/plain,export default 'kale';",
                                   NullURL(), /*failure_reason=*/nullptr)
          .IsValid());
  EXPECT_TRUE(modulator_
                  ->ResolveModuleSpecifier("about:legumes", NullURL(),
                                           /*failure_reason=*/nullptr)
                  .IsValid());
  EXPECT_TRUE(modulator_
                  ->ResolveModuleSpecifier("wss://example.com/celery",
                                           NullURL(),
                                           /*failure_reason=*/nullptr)
                  .IsValid());

  // "The following are not valid module specifiers according to the above
  // algorithm:"
  EXPECT_FALSE(modulator_
                   ->ResolveModuleSpecifier("https://f:b/c", NullURL(),
                                            /*failure_reason=*/nullptr)
                   .IsValid());
  EXPECT_FALSE(modulator_
                   ->ResolveModuleSpecifier("pumpkins.js", NullURL(),
                                            /*failure_reason=*/nullptr)
                   .IsValid());

  // Unprefixed module specifiers should still fail w/ a valid baseURL.
  EXPECT_FALSE(modulator_
                   ->ResolveModuleSpecifier("avocados.js", base_url,
                                            /*failure_reason=*/nullptr)
                   .IsValid());
}

}  // namespace blink
