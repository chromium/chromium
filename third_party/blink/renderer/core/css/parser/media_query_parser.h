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

class MediaQueryData {
  STACK_ALLOCATED();

 private:
  MediaQuery::RestrictorType restrictor_;
  String media_type_;
  ExpressionHeapVector expressions_;
  String media_feature_;
  bool media_type_set_;

  // A fake CSSParserContext for use counter only.
  // TODO(xiaochengh): Plumb the real CSSParserContext from the document.
  const CSSParserContext& fake_context_;

 public:
  MediaQueryData();
  MediaQueryData(const MediaQueryData&) = delete;
  MediaQueryData& operator=(const MediaQueryData&) = delete;
  void Clear();
  void AddExpression(CSSParserTokenRange&, const ExecutionContext*);
  bool LastExpressionValid();
  void RemoveLastExpression();
  void SetMediaType(const String&);
  std::unique_ptr<MediaQuery> TakeMediaQuery();

  inline bool CurrentMediaQueryChanged() const {
    return (restrictor_ != MediaQuery::kNone || media_type_set_ ||
            expressions_.size() > 0);
  }
  inline MediaQuery::RestrictorType Restrictor() { return restrictor_; }

  inline void SetRestrictor(MediaQuery::RestrictorType restrictor) {
    restrictor_ = restrictor;
  }

  inline void SetMediaFeature(const String& str) { media_feature_ = str; }
};

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
  bool ConsumeFeature(CSSParserTokenRange&);

  scoped_refptr<MediaQuerySet> ParseImpl(CSSParserTokenRange);

  // Like a regular Consume, except verifies that don't consume past
  // block start/ends.
  CSSParserToken ConsumeToken(CSSParserTokenRange&);

  void ProcessToken(CSSParserTokenRange&);

  void ReadRestrictor(CSSParserTokenRange&);
  void ReadMediaNot(CSSParserTokenRange&);
  void ReadMediaType(CSSParserTokenRange&);
  void ReadAnd(CSSParserTokenRange&);
  void ReadFeatureStart(CSSParserTokenRange&);
  void SkipUntilComma(CSSParserTokenRange&);
  void Done(CSSParserTokenRange&);

  using State = void (MediaQueryParser::*)(CSSParserTokenRange&);

  void SetStateAndRestrict(State, MediaQuery::RestrictorType);

  bool IsMediaFeatureAllowedInMode(const String& media_feature) const;

  State state_;
  ParserType parser_type_;
  MediaQueryData media_query_data_;
  scoped_refptr<MediaQuerySet> query_set_;
  CSSParserMode mode_;
  const ExecutionContext* execution_context_;

  const static State kReadRestrictor;
  const static State kReadMediaNot;
  const static State kReadMediaType;
  const static State kReadAnd;
  const static State kReadFeatureStart;
  const static State kSkipUntilComma;
  const static State kDone;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_MEDIA_QUERY_PARSER_H_
