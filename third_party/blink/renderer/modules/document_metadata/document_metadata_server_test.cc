// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/document_metadata/document_metadata_server.h"

#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/document_metadata/document_metadata.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {
namespace {

class DocumentMetadataServerTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    DocumentMetadataServer::BindReceiver(GetDocument().GetFrame(),
                                         remote_.BindNewPipeAndPassReceiver());
  }

  void SetHTMLInnerHTML(const String& html_content) {
    GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
        html_content);
  }

  mojom::blink::ProductClassificationResultPtr Classify(
      const Vector<String>& allowed,
      const Vector<String>& blocked) {
    mojom::blink::ProductClassificationResultPtr out_result;
    base::RunLoop run_loop;
    remote_->ClassifyProductDetails(
        allowed, blocked,
        base::BindOnce(
            [](mojom::blink::ProductClassificationResultPtr* out,
               base::OnceClosure quit_closure,
               mojom::blink::ProductClassificationResultPtr result) {
              *out = std::move(result);
              std::move(quit_closure).Run();
            },
            &out_result, run_loop.QuitClosure()));
    run_loop.Run();
    return out_result;
  }

  mojo::Remote<mojom::blink::DocumentMetadata> remote_;
};

TEST_F(DocumentMetadataServerTest, NoProduct) {
  SetHTMLInnerHTML(R"HTML(
    <body>
      <script type="application/ld+json">
        {"@type": "LocalBusiness", "name": "Diner"}
      </script>
    </body>
  )HTML");
  auto result = Classify({"diner"}, {});
  EXPECT_TRUE(result.is_null());
}

TEST_F(DocumentMetadataServerTest, ProductNoMatch) {
  SetHTMLInnerHTML(R"HTML(
    <body>
      <script type="application/ld+json">
        {"@type": "Product", "name": "Red Gizmo"}
      </script>
    </body>
  )HTML");
  auto result = Classify({"gadget"}, {"cheap"});
  ASSERT_FALSE(result.is_null());
  EXPECT_FALSE(result->allowed_keyword_found);
  EXPECT_FALSE(result->blocked_keyword_found);
}

TEST_F(DocumentMetadataServerTest, ProductAllowedMatch) {
  SetHTMLInnerHTML(R"HTML(
    <body>
      <script type="application/ld+json">
        {"@type": "Product", "name": "Red Gadget"}
      </script>
    </body>
  )HTML");
  auto result = Classify({"gadget"}, {"cheap"});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->allowed_keyword_found);
  EXPECT_FALSE(result->blocked_keyword_found);
}

TEST_F(DocumentMetadataServerTest, ProductBlockedMatch) {
  SetHTMLInnerHTML(R"HTML(
    <body>
      <script type="application/ld+json">
        {"@type": "Product", "name": "Cheap Gizmo"}
      </script>
    </body>
  )HTML");
  auto result = Classify({"gadget"}, {"cheap"});
  ASSERT_FALSE(result.is_null());
  EXPECT_FALSE(result->allowed_keyword_found);
  EXPECT_TRUE(result->blocked_keyword_found);
}

TEST_F(DocumentMetadataServerTest, ProductBothMatch) {
  SetHTMLInnerHTML(R"HTML(
    <body>
      <script type="application/ld+json">
        {"@type": "Product", "name": "Cheap Gadget"}
      </script>
    </body>
  )HTML");
  auto result = Classify({"gadget"}, {"cheap"});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->allowed_keyword_found);
  EXPECT_TRUE(result->blocked_keyword_found);
}

TEST_F(DocumentMetadataServerTest, ProductGroupSupport) {
  SetHTMLInnerHTML(R"HTML(
    <body>
      <script type="application/ld+json">
        {"@type": "ProductGroup", "name": "Cool Widget"}
      </script>
    </body>
  )HTML");
  auto result = Classify({"widget"}, {"cheap"});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->allowed_keyword_found);
  EXPECT_FALSE(result->blocked_keyword_found);
}

TEST_F(DocumentMetadataServerTest, PrioritizeProductGroup) {
  SetHTMLInnerHTML(R"HTML(
    <body>
      <script type="application/ld+json">
        {"@type": "ProductGroup", "name": "Cool Widget"}
      </script>
      <script type="application/ld+json">
        {"@type": "Product", "name": "Cheap Gadget"}
      </script>
    </body>
  )HTML");
  auto result = Classify({"widget", "gadget"}, {"cheap"});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(
      result->allowed_keyword_found);  // Matches "widget" from ProductGroup
  EXPECT_FALSE(result->blocked_keyword_found);  // Ignored "cheap" from Product
}

TEST_F(DocumentMetadataServerTest, HyphenatedName) {
  SetHTMLInnerHTML(R"HTML(
    <body>
      <script type="application/ld+json">
        {"@type": "Product", "name": "noise-cancelling headphone"}
      </script>
    </body>
  )HTML");
  // "headphone" should match.
  auto result = Classify({"headphone"}, {});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->allowed_keyword_found);

  // "noise" should match.
  result = Classify({"noise"}, {});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->allowed_keyword_found);

  // "cancelling" should match.
  result = Classify({"cancelling"}, {});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->allowed_keyword_found);

  // "noise-cancelling" will match because both keyword and target text are
  // tokenized.
  result = Classify({"noise-cancelling"}, {});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->allowed_keyword_found);
}

TEST_F(DocumentMetadataServerTest, PunctuationHandling) {
  SetHTMLInnerHTML(R"HTML(
    <body>
      <script type="application/ld+json">
        {"@type": "Product", "name": "super.widget.buy,now!"}
      </script>
    </body>
  )HTML");
  auto result = Classify({"widget"}, {});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->allowed_keyword_found);
}

TEST_F(DocumentMetadataServerTest, UnicodeCaseFolding) {
  SetHTMLInnerHTML(String::FromUtf8(R"HTML(
    <body>
      <script type="application/ld+json">
        {"@type": "Product", "name": "Ὀδυσσεύς"}
      </script>
    </body>
  )HTML"));

  // 1. Case folding of the first letter: capital 'Ὀ' (U+1F48) should fold to
  // lowercase 'ὀ' (U+1F41).
  auto result = Classify({String::FromUtf8("ὀδυσσεύς")}, {});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->allowed_keyword_found);

  // 2. Final sigma ('ς' U+03C2) vs medial sigma ('σ' U+03C3) folding.
  // Both should fold to 'σ'.
  // Page has 'ς' (ends with 'ς').
  // Query has 'σ' (ends with 'σ', non-standard spelling for test).
  result = Classify({String::FromUtf8("ὀδυσσεύσ")}, {});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->allowed_keyword_found);

  // 3. Capital Sigma ('Σ' U+03A3) vs final sigma ('ς' U+03C2) folding.
  // Page has lowercase 'ὀδυσσεύς' (ends with 'ς').
  // Query has uppercase 'ὈΔΥΣΣΕΎΣ' (ends with 'Σ').
  result = Classify({String::FromUtf8("ὈΔΥΣΣΕΎΣ")}, {});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->allowed_keyword_found);
}

TEST_F(DocumentMetadataServerTest, MultiWordKeywords) {
  SetHTMLInnerHTML(R"HTML(
    <body>
      <script type="application/ld+json">
        {"@type": "Product", "name": "Delicious hot-dog, fresh"}
      </script>
    </body>
  )HTML");

  // 1. Matches multi-word with different punctuation/spacing.
  auto result = Classify({"hot dog"}, {});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->allowed_keyword_found);

  // 2. Exact match.
  result = Classify({"delicious hot"}, {});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->allowed_keyword_found);

  // 3. No match when words are split by another word.
  result = Classify({"delicious dog"}, {});
  ASSERT_FALSE(result.is_null());
  EXPECT_FALSE(result->allowed_keyword_found);

  // 4. No match when words are combined differently.
  result = Classify({"hotdog"}, {});
  ASSERT_FALSE(result.is_null());
  EXPECT_FALSE(result->allowed_keyword_found);

  // 5. Mixed single-word and multi-word with allowed and blocked.
  SetHTMLInnerHTML(R"HTML(
    <body>
      <script type="application/ld+json">
        {"@type": "Product", "name": "hot dog bun"}
      </script>
    </body>
  )HTML");
  result = Classify({"bun"}, {"hot dog"});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->allowed_keyword_found);
  EXPECT_TRUE(result->blocked_keyword_found);
}

TEST_F(DocumentMetadataServerTest, UnicodeMultiWordKeywords) {
  SetHTMLInnerHTML(String::FromUtf8(R"HTML(
    <body>
      <script type="application/ld+json">
        {"@type": "Product", "name": "Ὀδυσσεύς ὁ μέγας"}
      </script>
    </body>
  )HTML"));

  // 1. "ὀδυσσεύσ ὁ" (lowercase, exact match with diacritics)
  auto result = Classify({String::FromUtf8("ὀδυσσεύσ ὁ")}, {});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->allowed_keyword_found);

  // 2. "ὀδυσσεύσ Ὁ" (capital Ὁ U+1F4D folds to ὁ U+1F45)
  result = Classify({String::FromUtf8("ὀδυσσεύσ Ὁ")}, {});
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->allowed_keyword_found);
}

}  // namespace
}  // namespace blink
