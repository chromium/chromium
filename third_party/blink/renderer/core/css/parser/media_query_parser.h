// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_MEDIA_QUERY_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_MEDIA_QUERY_PARSER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class MediaQuerySet;
class CSSParserContext;
class ContainerQueryParser;

class CORE_EXPORT MediaQueryParser {
  STACK_ALLOCATED();

 public:
  static MediaQuerySet* ParseMediaQuerySet(const String&,
                                           const ExecutionContext*);
  static MediaQuerySet* ParseMediaQuerySet(CSSParserTokenRange,
                                           const ExecutionContext*);
  static MediaQuerySet* ParseMediaCondition(CSSParserTokenRange,
                                            const ExecutionContext*);
  static MediaQuerySet* ParseMediaQuerySetInMode(CSSParserTokenRange,
                                                 CSSParserMode,
                                                 const ExecutionContext*);

  // Passed to ConsumeFeature to determine which features are allowed.
  class FeatureSet {
    STACK_ALLOCATED();

   public:
    // Returns true if the feature name is allowed in this set.
    virtual bool IsAllowed(const String& feature) const = 0;

    // Returns true if the feature can be queried without a value.
    virtual bool IsAllowedWithoutValue(const String& feature,
                                       const ExecutionContext*) const = 0;

    // Returns true is the feature name is case sensitive.
    virtual bool IsCaseSensitive(const String& feature) const = 0;

    // Whether the features support range syntax. This is typically false for
    // style container queries.
    virtual bool SupportsRange() const = 0;
  };

 private:
  friend class ContainerQueryParser;

  enum ParserType {
    kMediaQuerySetParser,
    kMediaConditionParser,
  };

  enum class SyntaxLevel {
    // Determined by CSSMediaQueries4 flag.
    kAuto,
    // Use mediaqueries-4 syntax regardless of flags.
    kLevel4,
  };

  MediaQueryParser(ParserType,
                   CSSParserMode,
                   const ExecutionContext*,
                   SyntaxLevel = SyntaxLevel::kAuto);
  MediaQueryParser(const MediaQueryParser&) = delete;
  MediaQueryParser& operator=(const MediaQueryParser&) = delete;
  virtual ~MediaQueryParser();

  // [ not | only ]
  static MediaQuery::RestrictorType ConsumeRestrictor(CSSParserTokenRange&);

  // https://drafts.csswg.org/mediaqueries-4/#typedef-media-type
  static String ConsumeType(CSSParserTokenRange&);

  // https://drafts.csswg.org/mediaqueries-4/#typedef-mf-comparison
  static MediaQueryOperator ConsumeComparison(CSSParserTokenRange&);

  // https://drafts.csswg.org/mediaqueries-4/#typedef-mf-name
  //
  // The <mf-name> is only consumed if the name is allowed by the specified
  // FeatureSet.
  String ConsumeAllowedName(CSSParserTokenRange&, const FeatureSet&);

  // Like ConsumeAllowedName, except returns null if the name has a min-
  // or max- prefix.
  String ConsumeUnprefixedName(CSSParserTokenRange&, const FeatureSet&);

  enum class NameAffinity {
    // <mf-name> appears on the left, e.g. width < 10px.
    kLeft,
    // <mf-name> appears on the right, e.g. 10px > width.
    kRight
  };

  // Helper function for parsing features with a single MediaQueryOperator,
  // for example 'width <= 10px', or '10px = width'.
  //
  // NameAffinity::kLeft means |lhs| will be interpreted as the <mf-name>,
  // otherwise |rhs| will be interpreted as the <mf-name>.
  //
  // Note that this function accepts CSSParserTokenRanges by *value*, unlike
  // Consume* functions, and that nullptr is returned if either |lhs|
  // or |rhs| aren't fully consumed.
  const MediaQueryExpNode* ParseNameValueComparison(CSSParserTokenRange lhs,
                                                    MediaQueryOperator op,
                                                    CSSParserTokenRange rhs,
                                                    NameAffinity,
                                                    const FeatureSet&);

  // https://drafts.csswg.org/mediaqueries-4/#typedef-media-feature
  //
  // Currently, only <mf-boolean> and <mf-plain> productions are supported.
  const MediaQueryExpNode* ConsumeFeature(CSSParserTokenRange&,
                                          const FeatureSet&);

  enum class ConditionMode {
    // https://drafts.csswg.org/mediaqueries-4/#typedef-media-condition
    kNormal,
    // https://drafts.csswg.org/mediaqueries-4/#typedef-media-condition-without-or
    kWithoutOr,
  };

  // https://drafts.csswg.org/mediaqueries-4/#typedef-media-condition
  const MediaQueryExpNode* ConsumeCondition(
      CSSParserTokenRange&,
      ConditionMode = ConditionMode::kNormal);

  // https://drafts.csswg.org/mediaqueries-4/#typedef-media-in-parens
  const MediaQueryExpNode* ConsumeInParens(CSSParserTokenRange&);

  // https://drafts.csswg.org/mediaqueries-4/#typedef-general-enclosed
  const MediaQueryExpNode* ConsumeGeneralEnclosed(CSSParserTokenRange&);

  // https://drafts.csswg.org/mediaqueries-4/#typedef-media-query
  MediaQuery* ConsumeQuery(CSSParserTokenRange&);

  // Used for ParserType::kMediaConditionParser.
  //
  // Parsing a single condition is useful for the 'sizes' attribute.
  //
  // https://html.spec.whatwg.org/multipage/images.html#sizes-attribute
  MediaQuerySet* ConsumeSingleCondition(CSSParserTokenRange);

  MediaQuerySet* ParseImpl(CSSParserTokenRange);

  ParserType parser_type_;
  CSSParserMode mode_;
  const ExecutionContext* execution_context_;
  SyntaxLevel syntax_level_;
  // A fake CSSParserContext for use counter only.
  // TODO(xiaochengh): Plumb the real CSSParserContext from the document.
  const CSSParserContext& fake_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_MEDIA_QUERY_PARSER_H_
