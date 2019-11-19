/*
 * CSS Media Query Evaluator
 *
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_QUERY_EVALUATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_QUERY_EVALUATOR_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class LocalFrame;
class MediaQuery;
class MediaQueryExp;
class MediaQueryResult;
class MediaQuerySet;
class MediaValues;
class MediaValuesInitialViewport;

using MediaQueryResultList = Vector<MediaQueryResult>;

// Class that evaluates css media queries as defined in
// CSS3 Module "Media Queries" (http://www.w3.org/TR/css3-mediaqueries/)
// Special constructors are needed, if simple media queries are to be
// evaluated without knowledge of the medium features. This can happen
// for example when parsing UA stylesheets, if evaluation is done
// right after parsing.
//
// the boolean parameter is used to approximate results of evaluation, if
// the device characteristics are not known. This can be used to prune the
// loading of stylesheets to only those which are probable to match.

class CORE_EXPORT MediaQueryEvaluator final
    : public GarbageCollected<MediaQueryEvaluator> {
 public:
  static void Init();

  // Creates evaluator which evaluates to true for all media queries.
  MediaQueryEvaluator() = default;

  // Creates evaluator which evaluates only simple media queries
  // Evaluator returns true for acceptedMediaType and returns true for any media
  // features.
  MediaQueryEvaluator(const char* accepted_media_type);

  // Creates evaluator which evaluates full media queries.
  explicit MediaQueryEvaluator(LocalFrame*);

  // Creates evaluator which evaluates in a thread-safe manner a subset of media
  // values.
  explicit MediaQueryEvaluator(const MediaValues&);

  explicit MediaQueryEvaluator(MediaValuesInitialViewport*);

  ~MediaQueryEvaluator();

  bool MediaTypeMatch(const String& media_type_to_match) const;

  // Evaluates a list of media queries.
  bool Eval(const MediaQuerySet&,
            MediaQueryResultList* viewport_dependent = nullptr,
            MediaQueryResultList* device_dependent = nullptr) const;

  // Evaluates media query.
  bool Eval(const MediaQuery&,
            MediaQueryResultList* viewport_dependent = nullptr,
            MediaQueryResultList* device_dependent = nullptr) const;

  // Evaluates media query subexpression, ie "and (media-feature: value)" part.
  bool Eval(const MediaQueryExp&) const;

  void Trace(blink::Visitor*);

 private:
  const String MediaType() const;

  String media_type_;
  Member<MediaValues> media_values_;
  DISALLOW_COPY_AND_ASSIGN(MediaQueryEvaluator);
};

}  // namespace blink
#endif
