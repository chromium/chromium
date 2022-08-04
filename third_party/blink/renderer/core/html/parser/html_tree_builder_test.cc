// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/tag_parsing_group.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

TEST(HTMLTreeBuilderTest, GetTagParsingGroup) {
  HashSet<AtomicString> checked_tags;
  auto get = [&checked_tags](const char* tag) -> TagParsingGroup {
    checked_tags.insert(tag);
    return GetTagParsingGroup(tag);
  };

  // These are all the tags that exist in html_tag_names.h.
  EXPECT_EQ(TagParsingGroup::kATag, get("a"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("abbr"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("acronym"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("address"));
  EXPECT_EQ(TagParsingGroup::kReconstructFormattingTags, get("area"));
  EXPECT_EQ(TagParsingGroup::kAppletOrObjectTag, get("applet"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("article"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("aside"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("audio"));
  EXPECT_EQ(TagParsingGroup::kNonAnchorNonNobrFormattingTag, get("b"));
  EXPECT_EQ(TagParsingGroup::kTagsThatBelongInHead, get("base"));
  EXPECT_EQ(TagParsingGroup::kTagsThatBelongInHead, get("basefont"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("bdi"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("bdo"));
  EXPECT_EQ(TagParsingGroup::kTagsThatBelongInHead, get("bgsound"));
  EXPECT_EQ(TagParsingGroup::kNonAnchorNonNobrFormattingTag, get("big"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("blockquote"));
  EXPECT_EQ(TagParsingGroup::kBodyTag, get("body"));
  EXPECT_EQ(TagParsingGroup::kReconstructFormattingTags, get("br"));
  EXPECT_EQ(TagParsingGroup::kButtonTag, get("button"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("canvas"));
  EXPECT_EQ(TagParsingGroup::kParseErrorTag, get("caption"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("center"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("cite"));
  EXPECT_EQ(TagParsingGroup::kNonAnchorNonNobrFormattingTag, get("code"));
  EXPECT_EQ(TagParsingGroup::kParseErrorTag, get("col"));
  EXPECT_EQ(TagParsingGroup::kParseErrorTag, get("colgroup"));
  EXPECT_EQ(TagParsingGroup::kTagsThatBelongInHead, get("command"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("data"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("datalist"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("del"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("details"));
  EXPECT_EQ(TagParsingGroup::kDdOrDtTag, get("dd"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("dfn"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("dialog"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("dir"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("div"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("dl"));
  EXPECT_EQ(TagParsingGroup::kDdOrDtTag, get("dt"));
  EXPECT_EQ(TagParsingGroup::kNonAnchorNonNobrFormattingTag, get("em"));
  EXPECT_EQ(TagParsingGroup::kReconstructFormattingTags, get("embed"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("fencedframe"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("fieldset"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("figcaption"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("figure"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("footer"));
  EXPECT_EQ(TagParsingGroup::kNonAnchorNonNobrFormattingTag, get("font"));
  EXPECT_EQ(TagParsingGroup::kFormTag, get("form"));
  EXPECT_EQ(TagParsingGroup::kParseErrorTag, get("frame"));
  EXPECT_EQ(TagParsingGroup::kFramesetTag, get("frameset"));
  EXPECT_EQ(TagParsingGroup::kNumberedHeaderTag, get("h1"));
  EXPECT_EQ(TagParsingGroup::kNumberedHeaderTag, get("h2"));
  EXPECT_EQ(TagParsingGroup::kNumberedHeaderTag, get("h3"));
  EXPECT_EQ(TagParsingGroup::kNumberedHeaderTag, get("h4"));
  EXPECT_EQ(TagParsingGroup::kNumberedHeaderTag, get("h5"));
  EXPECT_EQ(TagParsingGroup::kNumberedHeaderTag, get("h6"));
  EXPECT_EQ(TagParsingGroup::kParseErrorTag, get("head"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("header"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("hgroup"));
  EXPECT_EQ(TagParsingGroup::kHrTag, get("hr"));
  EXPECT_EQ(TagParsingGroup::kHTMLTag, get("html"));
  EXPECT_EQ(TagParsingGroup::kNonAnchorNonNobrFormattingTag, get("i"));
  EXPECT_EQ(TagParsingGroup::kIFrameTag, get("iframe"));
  EXPECT_EQ(TagParsingGroup::kImageTag, get("image"));
  EXPECT_EQ(TagParsingGroup::kReconstructFormattingTags, get("img"));
  EXPECT_EQ(TagParsingGroup::kInputTag, get("input"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("ins"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("kbd"));
  EXPECT_EQ(TagParsingGroup::kReconstructFormattingTags, get("keygen"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("label"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("layer"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("legend"));
  EXPECT_EQ(TagParsingGroup::kLiTag, get("li"));
  EXPECT_EQ(TagParsingGroup::kTagsThatBelongInHead, get("link"));
  EXPECT_EQ(TagParsingGroup::kListingOrPreTag, get("listing"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("main"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("map"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("mark"));
  EXPECT_EQ(TagParsingGroup::kMarqueeTag, get("marquee"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("menu"));
  EXPECT_EQ(TagParsingGroup::kTagsThatBelongInHead, get("meta"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("meter"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("nav"));
  EXPECT_EQ(TagParsingGroup::kNobrTag, get("nobr"));
  EXPECT_EQ(TagParsingGroup::kNoembedTag, get("noembed"));
  EXPECT_EQ(TagParsingGroup::kTagsThatBelongInHead, get("noframes"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("nolayer"));
  EXPECT_EQ(TagParsingGroup::kNoscriptTag, get("noscript"));
  EXPECT_EQ(TagParsingGroup::kAppletOrObjectTag, get("object"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("ol"));
  EXPECT_EQ(TagParsingGroup::kOptgroupOrOptionTag, get("optgroup"));
  EXPECT_EQ(TagParsingGroup::kOptgroupOrOptionTag, get("option"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("output"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("p"));
  EXPECT_EQ(TagParsingGroup::kParamOrSourceOrTrackTag, get("param"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("picture"));
  EXPECT_EQ(TagParsingGroup::kPlaintextTag, get("plaintext"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("portal"));
  EXPECT_EQ(TagParsingGroup::kListingOrPreTag, get("pre"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("progress"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("q"));
  EXPECT_EQ(TagParsingGroup::kRbOrRtcTag, get("rb"));
  EXPECT_EQ(TagParsingGroup::kRtOrRpTag, get("rp"));
  EXPECT_EQ(TagParsingGroup::kRtOrRpTag, get("rt"));
  EXPECT_EQ(TagParsingGroup::kRbOrRtcTag, get("rtc"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("ruby"));
  EXPECT_EQ(TagParsingGroup::kNonAnchorNonNobrFormattingTag, get("s"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("samp"));
  EXPECT_EQ(TagParsingGroup::kTagsThatBelongInHead, get("script"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("section"));
  EXPECT_EQ(TagParsingGroup::kSelectTag, get("select"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("selectmenu"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("slot"));
  EXPECT_EQ(TagParsingGroup::kNonAnchorNonNobrFormattingTag, get("small"));
  EXPECT_EQ(TagParsingGroup::kParamOrSourceOrTrackTag, get("source"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("span"));
  EXPECT_EQ(TagParsingGroup::kNonAnchorNonNobrFormattingTag, get("strike"));
  EXPECT_EQ(TagParsingGroup::kNonAnchorNonNobrFormattingTag, get("strong"));
  EXPECT_EQ(TagParsingGroup::kTagsThatBelongInHead, get("style"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("sub"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("summary"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("sup"));
  EXPECT_EQ(TagParsingGroup::kTableTag, get("table"));
  EXPECT_EQ(TagParsingGroup::kParseErrorTag, get("tbody"));
  EXPECT_EQ(TagParsingGroup::kParseErrorTag, get("td"));
  EXPECT_EQ(TagParsingGroup::kTagsThatBelongInHead, get("template"));
  EXPECT_EQ(TagParsingGroup::kTextareaTag, get("textarea"));
  EXPECT_EQ(TagParsingGroup::kParseErrorTag, get("tfoot"));
  EXPECT_EQ(TagParsingGroup::kParseErrorTag, get("th"));
  EXPECT_EQ(TagParsingGroup::kParseErrorTag, get("thead"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("time"));
  EXPECT_EQ(TagParsingGroup::kTagsThatBelongInHead, get("title"));
  EXPECT_EQ(TagParsingGroup::kParseErrorTag, get("tr"));
  EXPECT_EQ(TagParsingGroup::kParamOrSourceOrTrackTag, get("track"));
  EXPECT_EQ(TagParsingGroup::kNonAnchorNonNobrFormattingTag, get("tt"));
  EXPECT_EQ(TagParsingGroup::kNonAnchorNonNobrFormattingTag, get("u"));
  EXPECT_EQ(TagParsingGroup::kTagsThatCloseP, get("ul"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("var"));
  EXPECT_EQ(TagParsingGroup::kNoGroup, get("video"));
  EXPECT_EQ(TagParsingGroup::kReconstructFormattingTags, get("wbr"));
  EXPECT_EQ(TagParsingGroup::kXmpTag, get("xmp"));

  EXPECT_EQ(TagParsingGroup::kNoGroup,
            GetTagParsingGroup("thistagdoesnotexist"));

  // Verify that we've checked all the tags.
  std::unique_ptr<const HTMLQualifiedName*[]> qualified_names =
      html_names::GetTags();
  EXPECT_EQ(checked_tags.size(), html_names::kTagsCount);
  for (wtf_size_t i = 0; i < html_names::kTagsCount; ++i) {
    SCOPED_TRACE(qualified_names[i]->LocalName());
    EXPECT_TRUE(checked_tags.Contains(qualified_names[i]->LocalName()));
  }
}

}  // namespace blink
