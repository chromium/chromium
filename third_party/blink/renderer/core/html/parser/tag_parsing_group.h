// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_TAG_PARSING_GROUP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_TAG_PARSING_GROUP_H_

// This file has been split out from html_tree_builder.cc for unit testing
// purposes.

#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

static bool IsCaptionColOrColgroupTag(const AtomicString& tag_name) {
  return tag_name == html_names::kCaptionTag ||
         tag_name == html_names::kColTag ||
         tag_name == html_names::kColgroupTag;
}

static bool IsTableBodyContextTag(const AtomicString& tag_name) {
  return tag_name == html_names::kTbodyTag ||
         tag_name == html_names::kTfootTag || tag_name == html_names::kTheadTag;
}

static bool IsTableCellContextTag(const AtomicString& tag_name) {
  return tag_name == html_names::kThTag || tag_name == html_names::kTdTag;
}

static bool IsNumberedHeaderTag(const AtomicString& tag_name) {
  return tag_name == html_names::kH1Tag || tag_name == html_names::kH2Tag ||
         tag_name == html_names::kH3Tag || tag_name == html_names::kH4Tag ||
         tag_name == html_names::kH5Tag || tag_name == html_names::kH6Tag;
}

// A grouping that exists solely as a helper for ProcessStartTagForInBody()
// (although it may also be useful for related functions at some future point).
// Certain tags trigger exactly the same behavior when we see their opening;
// e.g., <base>, <link>, <meta>, <bgsound>, etc. within the document body are
// all sent on to parsing as if they occurred in the header instead.
// GetTagParsingGroup() will take a tag name in AtomicString form and map it
// into a numerical value we can switch on.
//
// Note that these tags are grouped by their role relating to start tags
// within <body>, which is the phase we optimize the most for.
enum class TagParsingGroup {
  kNoGroup = 0,

  // Groups consisting of a single tag or a group of tags.
  kATag,
  kAppletOrObjectTag,
  kBodyTag,
  kButtonTag,
  kDdOrDtTag,
  kFormTag,
  kFramesetTag,
  kHTMLTag,
  kHrTag,
  kIFrameTag,
  kImageTag,
  kInputTag,
  kLiTag,
  kListingOrPreTag,
  kMarqueeTag,
  kMathTag,
  kNobrTag,
  kNoembedTag,
  kNoscriptTag,
  kOptgroupOrOptionTag,
  kParamOrSourceOrTrackTag,
  kPlaintextTag,
  kRbOrRtcTag,
  kRtOrRpTag,
  kSelectTag,
  kSVGTag,
  kTableTag,
  kTextareaTag,
  kXmpTag,

  // Specifically named groups.
  kTagsThatCloseP,
  kReconstructFormattingTags,
  kTagsThatBelongInHead,
  kNumberedHeaderTag,              // IsNumberedHeaderTag().
  kNonAnchorNonNobrFormattingTag,  // IsNonAnchorNonNobrFormattingTag().
  kParseErrorTag,
};

// Convert a tag name in AtomicString form and map it into a numerical value we
// can switch on, based on the desired behavior (see TagParsingGroup). Since we
// have so many different tags to test for, it is more efficient to group by the
// first letter before we do testing (otherwise, a tag not in the list would
// need to go through 70–80 tests in turn).
//
// This is not the only possible design for this; we could e.g. use a hash table
// instead. Also, it would be possible to put the code directly in this switch,
// instead of returning a value and then switching on that value. However,
// that would lead to duplication, and hopefully, the compiler can figure out
// the jumps directly. In any case, the strategy seems to be successful enough
// in making ProcessStartTagForInBody() not a bottleneck during parsing.
static inline TagParsingGroup GetTagParsingGroup(const AtomicString& tag) {
  DCHECK(!tag.IsEmpty());
  switch (tag[0]) {
    case 'a':
      if (tag == html_names::kAddressTag || tag == html_names::kArticleTag ||
          tag == html_names::kAsideTag) {
        return TagParsingGroup::kTagsThatCloseP;
      }
      if (tag == html_names::kATag) {
        return TagParsingGroup::kATag;
      }
      if (tag == html_names::kAppletTag) {
        return TagParsingGroup::kAppletOrObjectTag;
      }
      if (tag == html_names::kAreaTag) {
        return TagParsingGroup::kReconstructFormattingTags;
      }
      break;

    case 'b':
      if (tag == html_names::kBlockquoteTag) {
        return TagParsingGroup::kTagsThatCloseP;
      }
      if (tag == html_names::kButtonTag) {
        return TagParsingGroup::kButtonTag;
      }
      if (tag == html_names::kBrTag) {
        return TagParsingGroup::kReconstructFormattingTags;
      }
      if (tag == html_names::kBaseTag || tag == html_names::kBasefontTag ||
          tag == html_names::kBgsoundTag) {
        return TagParsingGroup::kTagsThatBelongInHead;
      }
      if (tag == html_names::kBodyTag) {
        return TagParsingGroup::kBodyTag;
      }
      if (tag == html_names::kBTag || tag == html_names::kBigTag) {
        return TagParsingGroup::kNonAnchorNonNobrFormattingTag;
      }
      if (tag == html_names::kBrTag) {
        return TagParsingGroup::kReconstructFormattingTags;
      }
      break;

    case 'c':
      if (tag == html_names::kCommandTag) {
        return TagParsingGroup::kTagsThatBelongInHead;
      }
      if (tag == html_names::kCenterTag) {
        return TagParsingGroup::kTagsThatCloseP;
      }
      if (tag == html_names::kCodeTag) {
        return TagParsingGroup::kNonAnchorNonNobrFormattingTag;
      }
      if (IsCaptionColOrColgroupTag(tag)) {
        return TagParsingGroup::kParseErrorTag;
      }
      break;

    case 'd':
      if (tag == html_names::kDetailsTag || tag == html_names::kDialogTag ||
          tag == html_names::kDirTag || tag == html_names::kDivTag ||
          tag == html_names::kDlTag) {
        return TagParsingGroup::kTagsThatCloseP;
      }
      if (tag == html_names::kDdTag || tag == html_names::kDtTag) {
        return TagParsingGroup::kDdOrDtTag;
      }
      break;

    case 'e':
      if (tag == html_names::kEmTag) {
        return TagParsingGroup::kNonAnchorNonNobrFormattingTag;
      }
      if (tag == html_names::kEmbedTag) {
        return TagParsingGroup::kReconstructFormattingTags;
      }
      break;

    case 'f':
      if (tag == html_names::kFieldsetTag ||
          tag == html_names::kFigcaptionTag || tag == html_names::kFigureTag ||
          tag == html_names::kFooterTag) {
        return TagParsingGroup::kTagsThatCloseP;
      }
      if (tag == html_names::kFramesetTag) {
        return TagParsingGroup::kFramesetTag;
      }
      if (tag == html_names::kFormTag) {
        return TagParsingGroup::kFormTag;
      }
      if (tag == html_names::kFontTag) {
        return TagParsingGroup::kNonAnchorNonNobrFormattingTag;
      }
      if (tag == html_names::kFrameTag) {
        return TagParsingGroup::kParseErrorTag;
      }
      break;

    case 'h':
      if (tag == html_names::kHTMLTag) {
        return TagParsingGroup::kHTMLTag;
      }
      if (tag == html_names::kHeaderTag || tag == html_names::kHgroupTag) {
        return TagParsingGroup::kTagsThatCloseP;
      }
      if (IsNumberedHeaderTag(tag)) {
        return TagParsingGroup::kNumberedHeaderTag;
      }
      if (tag == html_names::kHrTag) {
        return TagParsingGroup::kHrTag;
      }
      if (tag == html_names::kHeadTag) {
        return TagParsingGroup::kParseErrorTag;
      }
      break;

    case 'i':
      if (tag == html_names::kInputTag) {
        return TagParsingGroup::kInputTag;
      }
      if (tag == html_names::kITag) {
        return TagParsingGroup::kNonAnchorNonNobrFormattingTag;
      }
      if (tag == html_names::kImgTag) {
        return TagParsingGroup::kReconstructFormattingTags;
      }
      if (tag == html_names::kImageTag) {
        return TagParsingGroup::kImageTag;
      }
      if (tag == html_names::kIFrameTag) {
        return TagParsingGroup::kIFrameTag;
      }
      break;

    case 'k':
      if (tag == html_names::kKeygenTag) {
        return TagParsingGroup::kReconstructFormattingTags;
      }
      break;

    case 'l':
      if (tag == html_names::kLiTag) {
        return TagParsingGroup::kLiTag;
      }
      if (tag == html_names::kLinkTag) {
        return TagParsingGroup::kTagsThatBelongInHead;
      }
      if (tag == html_names::kListingTag) {
        return TagParsingGroup::kListingOrPreTag;
      }
      break;

    case 'm':
      if (tag == html_names::kMetaTag) {
        return TagParsingGroup::kTagsThatBelongInHead;
      }
      if (tag == html_names::kMainTag || tag == html_names::kMenuTag) {
        return TagParsingGroup::kTagsThatCloseP;
      }
      if (tag == html_names::kMarqueeTag) {
        return TagParsingGroup::kMarqueeTag;
      }
      if (tag == mathml_names::kMathTag.LocalName()) {
        return TagParsingGroup::kMathTag;
      }
      break;

    case 'n':
      if (tag == html_names::kNoframesTag) {
        return TagParsingGroup::kTagsThatBelongInHead;
      }
      if (tag == html_names::kNavTag) {
        return TagParsingGroup::kTagsThatCloseP;
      }
      if (tag == html_names::kNobrTag) {
        return TagParsingGroup::kNobrTag;
      }
      if (tag == html_names::kNoembedTag) {
        return TagParsingGroup::kNoembedTag;
      }
      if (tag == html_names::kNoscriptTag) {
        return TagParsingGroup::kNoscriptTag;
      }
      break;

    case 'o':
      if (tag == html_names::kOlTag) {
        return TagParsingGroup::kTagsThatCloseP;
      }
      if (tag == html_names::kObjectTag) {
        return TagParsingGroup::kAppletOrObjectTag;
      }
      if (tag == html_names::kOptgroupTag || tag == html_names::kOptionTag) {
        return TagParsingGroup::kOptgroupOrOptionTag;
      }
      break;

    case 'p':
      if (tag == html_names::kParamTag) {
        return TagParsingGroup::kParamOrSourceOrTrackTag;
      }
      if (tag == html_names::kPTag) {
        return TagParsingGroup::kTagsThatCloseP;
      }
      if (tag == html_names::kPreTag) {
        return TagParsingGroup::kListingOrPreTag;
      }
      if (tag == html_names::kPlaintextTag) {
        return TagParsingGroup::kPlaintextTag;
      }
      break;

    case 'r':
      if (tag == html_names::kRbTag || tag == html_names::kRTCTag) {
        return TagParsingGroup::kRbOrRtcTag;
      }
      if (tag == html_names::kRtTag || tag == html_names::kRpTag) {
        return TagParsingGroup::kRtOrRpTag;
      }
      break;

    case 's':
      if (tag == html_names::kScriptTag || tag == html_names::kStyleTag) {
        return TagParsingGroup::kTagsThatBelongInHead;
      }
      if (tag == html_names::kSectionTag || tag == html_names::kSummaryTag) {
        return TagParsingGroup::kTagsThatCloseP;
      }
      if (tag == html_names::kSTag || tag == html_names::kSmallTag ||
          tag == html_names::kStrikeTag || tag == html_names::kStrongTag) {
        return TagParsingGroup::kNonAnchorNonNobrFormattingTag;
      }
      if (tag == html_names::kSelectTag) {
        return TagParsingGroup::kSelectTag;
      }
      if (tag == svg_names::kSVGTag.LocalName()) {
        return TagParsingGroup::kSVGTag;
      }
      if (tag == html_names::kSourceTag) {
        return TagParsingGroup::kParamOrSourceOrTrackTag;
      }
      break;

    case 't':
      if (tag == html_names::kTitleTag || tag == html_names::kTemplateTag) {
        return TagParsingGroup::kTagsThatBelongInHead;
      }
      if (tag == html_names::kTtTag) {
        return TagParsingGroup::kNonAnchorNonNobrFormattingTag;
      }
      if (tag == html_names::kTableTag) {
        return TagParsingGroup::kTableTag;
      }
      if (tag == html_names::kTextareaTag) {
        return TagParsingGroup::kTextareaTag;
      }
      if (IsTableBodyContextTag(tag) || IsTableCellContextTag(tag) ||
          tag == html_names::kTrTag) {
        return TagParsingGroup::kParseErrorTag;
      }
      if (tag == html_names::kTrackTag) {
        return TagParsingGroup::kParamOrSourceOrTrackTag;
      }
      break;

    case 'u':
      if (tag == html_names::kUlTag) {
        return TagParsingGroup::kTagsThatCloseP;
      }
      if (tag == html_names::kUTag) {
        return TagParsingGroup::kNonAnchorNonNobrFormattingTag;
      }
      break;

    case 'w':
      if (tag == html_names::kWbrTag) {
        return TagParsingGroup::kReconstructFormattingTags;
      }
      break;

    case 'x':
      if (tag == html_names::kXmpTag) {
        return TagParsingGroup::kXmpTag;
      }
      break;
  }
  return TagParsingGroup::kNoGroup;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_TAG_PARSING_GROUP_H_
