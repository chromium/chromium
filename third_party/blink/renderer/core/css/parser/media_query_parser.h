// Copyright 2014 The Chromium Authors. All rights reserved.
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

class CORE_EXPORT MediaQueryParser {
  STACK_ALLOCATED();

 public:
  static scoped_refptr<MediaQuerySet> ParseMediaQuerySet(
      const String&,
      const ExecutionContext*);
  static scoped_refptr<MediaQuerySet> ParseMediaQuerySet(
      CSSParserTokenRange,
      const ExecutionContext*);
  static scoped_refptr<MediaQuerySet> ParseMediaCondition(
      CSSParserTokenRange,
      const ExecutionContext*);
  static scoped_refptr<MediaQuerySet> ParseMediaQuerySetInMode(
      CSSParserTokenRange,
      CSSParserMode,
      const ExecutionContext*);

 private:
  enum ParserType {
    kMediaQuerySetParser,
    kMediaConditionParser,
  };

  MediaQueryParser(ParserType, CSSParserMode, const ExecutionContext*);
  MediaQueryParser(const MediaQueryParser&) = delete;
  MediaQueryParser& operator=(const MediaQueryParser&) = delete;
  virtual ~MediaQueryParser();

  // [ not | only ]
  static MediaQuery::RestrictorType ConsumeRestrictor(CSSParserTokenRange&);

  // https://drafts.csswg.org/mediaqueries-4/#typedef-media-type
  static String ConsumeType(CSSParserTokenRange&);

  // https://drafts.csswg.org/mediaqueries-4/#typedef-media-feature
  //
  // Currently, only <mf-boolean> and <mf-plain> productions are supported.
  std::unique_ptr<MediaQueryExpNode> ConsumeFeature(CSSParserTokenRange&);

  // https://drafts.csswg.org/mediaqueries-4/#typedef-media-condition
  //
  // TODO(crbug.com/962417): Only a limited form of the grammar is
  // currently supported.
  std::unique_ptr<MediaQueryExpNode> ConsumeCondition(CSSParserTokenRange&);

  // https://drafts.csswg.org/mediaqueries-4/#typedef-media-query
  //
  // TODO(crbug.com/962417): Only a limited form of the grammar is
  // currently supported.
  std::unique_ptr<MediaQuery> ConsumeQuery(CSSParserTokenRange&);

  // Used for ParserType::kMediaConditionParser.
  //
  // Parsing a single condition is useful for the 'sizes' attribute.
  //
  // https://html.spec.whatwg.org/multipage/images.html#sizes-attribute
  scoped_refptr<MediaQuerySet> ConsumeSingleCondition(CSSParserTokenRange);

  scoped_refptr<MediaQuerySet> ParseImpl(CSSParserTokenRange);

  bool IsMediaFeatureAllowedInMode(const String& media_feature) const;

  ParserType parser_type_;
  scoped_refptr<MediaQuerySet> query_set_;
  CSSParserMode mode_;
  const ExecutionContext* execution_context_;
  // A fake CSSParserContext for use counter only.
  // TODO(xiaochengh): Plumb the real CSSParserContext from the document.
  const CSSParserContext& fake_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_MEDIA_QUERY_PARSER_H_
