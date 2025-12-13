// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_MEDIA_QUERY_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_MEDIA_QUERY_PARSER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/media_query.h"
#include "third_party/blink/renderer/core/css/parser/conditional_parser.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

enum class MediaQueryOperator;
class CSSParserContext;
class CSSParserTokenStream;
class ExecutionContext;
class MediaQuerySet;

class CORE_EXPORT MediaQueryParser : public ConditionalParser {
  STACK_ALLOCATED();

 public:
  MediaQueryParser(const MediaQueryParser&) = delete;
  MediaQueryParser& operator=(const MediaQueryParser&) = delete;

  static MediaQuerySet* ParseMediaQuerySet(StringView, ExecutionContext*);
  static MediaQuerySet* ParseMediaQuerySet(CSSParserTokenStream&,
                                           ExecutionContext*);
  static MediaQuerySet* ParseMediaCondition(CSSParserTokenStream&,
                                            ExecutionContext*);
  // Parses the `<media-query-list>` part of a `@custom-media` rule.
  // https://drafts.csswg.org/mediaqueries-5/#at-ruledef-custom-media
  //
  // Parsing stops at the first top-level semicolon (i.e., not inside a
  // parenthesized expression) or at the end of the stream, whichever comes
  // first.
  static MediaQuerySet* ParseCustomMediaDefinition(CSSParserTokenStream&,
                                                   ExecutionContext*);

  // Passed to ConsumeFeature to determine which features are allowed.
  class FeatureSet {
    STACK_ALLOCATED();

   public:
    // Returns true if the feature name is allowed in this set.
    virtual bool IsAllowed(const AtomicString& feature) const = 0;

    // Returns true if the feature can be queried without a value.
    virtual bool IsAllowedWithoutValue(const AtomicString& feature,
                                       const ExecutionContext*) const = 0;

    // Returns true if the feature can be queried with a value.
    virtual bool IsAllowedWithValue(const AtomicString& feature) const = 0;

    // Returns true is the feature name is case sensitive.
    virtual bool IsCaseSensitive(const AtomicString& feature) const = 0;

    // Whether the features support media query range syntax. This is typically
    // false for style container queries.
    virtual bool SupportsRange() const = 0;

    // Whether the features support style query range syntax, e.g. queries like
    // 10em < 10px < 10% or --x > --y > --z
    virtual bool SupportsStyleRange() const = 0;

    // Whether the features are evaluated in an element context
    // (true for container queries, false for media queries).
    virtual bool SupportsElementDependent() const = 0;
  };

  class MediaQueryFeatureSet : public MediaQueryParser::FeatureSet {
    STACK_ALLOCATED();

   public:
    MediaQueryFeatureSet() = default;

    bool IsAllowed(const AtomicString& feature) const override;
    bool IsAllowedWithoutValue(
        const AtomicString& feature,
        const ExecutionContext* execution_context) const override;
    bool IsAllowedWithValue(const AtomicString& feature) const override;
    bool IsCaseSensitive(const AtomicString& feature) const override {
      return false;
    }
    bool SupportsRange() const override { return true; }
    bool SupportsStyleRange() const override { return false; }
    bool SupportsElementDependent() const override { return false; }
  };

 private:
  friend class ContainerQueryParser;
  friend class CSSIfParser;

  enum ParserType {
    kMediaQuerySetParser,
    kMediaConditionParser,
  };

  MediaQueryParser(ParserType, ExecutionContext*);

  // [ not | only ]
  static MediaQuery::RestrictorType ConsumeRestrictor(CSSParserTokenStream&);

  // https://drafts.csswg.org/mediaqueries-4/#typedef-media-type
  static AtomicString ConsumeType(CSSParserTokenStream&);

  // https://drafts.csswg.org/mediaqueries-4/#typedef-mf-comparison
  static MediaQueryOperator ConsumeComparison(CSSParserTokenStream&);

  // https://drafts.csswg.org/mediaqueries-4/#typedef-mf-name
  //
  // The <mf-name> is only consumed if the name is allowed by the specified
  // FeatureSet.
  AtomicString ConsumeAllowedName(CSSParserTokenStream&, const FeatureSet&);

  // Like ConsumeAllowedName, except returns null if the name has a min-
  // or max- prefix.
  AtomicString ConsumeRangeContextFeatureName(CSSParserTokenStream&,
                                              const FeatureSet&);

  enum class NameAffinity {
    // <mf-name> appears on the left, e.g. width < 10px.
    kLeft,
    // <mf-name> appears on the right, e.g. 10px > width.
    kRight
  };

  const ConditionalExpNode* ConsumeStyleFeatureRange(
      CSSParserTokenStream& stream);

  // https://drafts.csswg.org/mediaqueries-4/#typedef-media-feature
  //
  // Currently, only <mf-boolean> and <mf-plain> productions are supported.
  const ConditionalExpNode* ConsumeFeature(CSSParserTokenStream&,
                                           const FeatureSet&);

  // https://drafts.csswg.org/mediaqueries-4/#typedef-media-query
  MediaQuery* ConsumeQuery(CSSParserTokenStream&);

  const ConditionalExpNode* ConsumeLeaf(CSSParserTokenStream&) override;
  const ConditionalExpNode* ConsumeFunction(CSSParserTokenStream&) override;

  // Used for ParserType::kMediaConditionParser.
  //
  // Parsing a single condition is useful for the 'sizes' attribute.
  //
  // https://html.spec.whatwg.org/multipage/images.html#sizes-attribute
  MediaQuerySet* ConsumeSingleCondition(CSSParserTokenStream&);

  MediaQuerySet* ParseImpl(CSSParserTokenStream&);

  void UseCountRangeSyntax();

  ParserType parser_type_;
  ExecutionContext* execution_context_;
  // A fake CSSParserContext for use counter only.
  // TODO(xiaochengh): Plumb the real CSSParserContext from the document.
  const CSSParserContext& fake_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_MEDIA_QUERY_PARSER_H_
