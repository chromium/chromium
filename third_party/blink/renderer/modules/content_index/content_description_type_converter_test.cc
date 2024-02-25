// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_index/content_description_type_converter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_content_description.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_content_icon_definition.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

const blink::ContentDescription* CreateDescription(const WTF::String& category,
                                                   const WTF::String& url) {
  auto* description = blink::MakeGarbageCollected<blink::ContentDescription>();
  description->setId("id");
  description->setTitle("title");
  description->setDescription("description");
  description->setCategory(category);

  auto* icon_definition = MakeGarbageCollected<ContentIconDefinition>();
  icon_definition->setSrc(url);
  description->setIcons({icon_definition});

  description->setUrl(url);
  return description;
}

// Migration adapters for operator==(ContentIconDefinition).
std::optional<String> GetSizesOrNone(const ContentIconDefinition* cid) {
  if (cid->hasSizes())
    return cid->sizes();
  return std::nullopt;
}

std::optional<String> GetTypeOrNone(const ContentIconDefinition* cid) {
  if (cid->hasType())
    return cid->type();
  return std::nullopt;
}

}  // anonymous namespace

// TODO(crbug.com/1070871): Use fooOr() and drop migration adapters above.
bool operator==(const Member<ContentIconDefinition>& cid1,
                const Member<ContentIconDefinition>& cid2) {
  return cid1->src() == cid2->src() &&
         GetSizesOrNone(cid1) == GetSizesOrNone(cid2) &&
         GetTypeOrNone(cid1) == GetTypeOrNone(cid2);
}

bool operator==(const ContentDescription& cd1, const ContentDescription& cd2) {
  return cd1.id() == cd2.id() && cd1.title() == cd2.title() &&
         cd1.description() == cd2.description() &&
         cd1.category() == cd2.category() && cd1.icons() == cd2.icons() &&
         cd1.url() == cd2.url();
}

TEST(ContentDescriptionConversionTest, RoundTrip) {
  test::TaskEnvironment task_environment;
  auto* description = CreateDescription("homepage", "https://example.com/");
  auto mojo_description = mojom::blink::ContentDescription::From(description);
  ASSERT_TRUE(mojo_description);
  auto* round_trip_description =
      mojo_description.To<blink::ContentDescription*>();
  EXPECT_EQ(*description, *round_trip_description);
}

TEST(ContentDescriptionConversionTest, EnumRoundTrip) {
  test::TaskEnvironment task_environment;
  WTF::Vector<WTF::String> categories = {"homepage", "article", "video",
                                         "audio"};
  for (const auto& category : categories) {
    auto* description = CreateDescription(category, "https://example.com/");
    auto* round_trip_description =
        mojom::blink::ContentDescription::From(description)
            .To<blink::ContentDescription*>();
    EXPECT_EQ(*description, *round_trip_description);
  }
}

}  // namespace blink
