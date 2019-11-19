/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_QUERY_MATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_QUERY_MATCHER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class Document;
class MediaQueryList;
class MediaQueryListListener;
class MediaQueryEvaluator;
class MediaQuerySet;

// MediaQueryMatcher class is responsible for keeping a vector of pairs
// MediaQueryList x MediaQueryListListener. It is responsible for evaluating the
// queries whenever it is needed and to call the listeners if the corresponding
// query has changed. The listeners must be called in the very same order in
// which they have been added.

class CORE_EXPORT MediaQueryMatcher final
    : public GarbageCollected<MediaQueryMatcher> {
 public:
  static MediaQueryMatcher* Create(Document&);

  explicit MediaQueryMatcher(Document&);
  ~MediaQueryMatcher();

  void DocumentDetached();

  void AddMediaQueryList(MediaQueryList*);
  void RemoveMediaQueryList(MediaQueryList*);

  void AddViewportListener(MediaQueryListListener*);
  void RemoveViewportListener(MediaQueryListListener*);

  MediaQueryList* MatchMedia(const String&);

  void MediaFeaturesChanged();
  void ViewportChanged();
  bool Evaluate(const MediaQuerySet*);

  void Trace(blink::Visitor*);

 private:
  MediaQueryEvaluator* CreateEvaluator() const;

  Member<Document> document_;
  Member<MediaQueryEvaluator> evaluator_;

  using MediaQueryListSet = HeapLinkedHashSet<WeakMember<MediaQueryList>>;
  MediaQueryListSet media_lists_;

  using ViewportListenerSet = HeapLinkedHashSet<Member<MediaQueryListListener>>;
  ViewportListenerSet viewport_listeners_;
  DISALLOW_COPY_AND_ASSIGN(MediaQueryMatcher);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_QUERY_MATCHER_H_
