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

#include "third_party/blink/renderer/core/css/media_query_list.h"

#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/media_query_list_listener.h"
#include "third_party/blink/renderer/core/css/media_query_matcher.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_object.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

MediaQueryList::MediaQueryList(ExecutionContext* context,
                               MediaQueryMatcher* matcher,
                               MediaQuerySet* media)
    : ActiveScriptWrappable<MediaQueryList>({}),
      ExecutionContextLifecycleObserver(context),
      matcher_(matcher),
      media_(media),
      matches_dirty_(true),
      matches_(false) {
  matcher_->AddMediaQueryList(this);
  UpdateMatches();
}

MediaQueryList::~MediaQueryList() = default;

String MediaQueryList::media() const {
  return media_->MediaText();
}

void MediaQueryList::addDeprecatedListener(V8EventListener* listener) {
  addEventListener(event_type_names::kChange, listener);
}

void MediaQueryList::removeDeprecatedListener(V8EventListener* listener) {
  removeEventListener(event_type_names::kChange, listener);
}

void MediaQueryList::AddListener(MediaQueryListListener* listener) {
  if (!listener) {
    return;
  }

  listeners_.insert(listener);
}

void MediaQueryList::RemoveListener(MediaQueryListListener* listener) {
  if (!listener) {
    return;
  }

  listeners_.erase(listener);
}

bool MediaQueryList::HasPendingActivity() const {
  return GetExecutionContext() &&
         (listeners_.size() || HasEventListeners(event_type_names::kChange));
}

void MediaQueryList::ContextDestroyed() {
  listeners_.clear();
  RemoveAllEventListeners();
}

bool MediaQueryList::MediaFeaturesChanged(
    HeapVector<Member<MediaQueryListListener>>* listeners_to_notify) {
  matches_dirty_ = true;
  if (!UpdateMatches()) {
    return false;
  }
  for (const auto& listener : listeners_) {
    listeners_to_notify->push_back(listener);
  }
  return HasEventListeners(event_type_names::kChange);
}

bool MediaQueryList::UpdateMatches() {
  matches_dirty_ = false;
  if (matches_ != matcher_->Evaluate(media_.Get())) {
    matches_ = !matches_;
    return true;
  }
  return false;
}

bool MediaQueryList::matches() {
  // If this is an iframe, viewport size depends on the layout of the embedding
  // document.
  if (matcher_->GetDocument() && matcher_->GetDocument()->GetFrame()) {
    if (auto* owner =
            matcher_->GetDocument()->GetFrame()->OwnerLayoutObject()) {
      owner->GetDocument().UpdateStyleAndLayout(
          DocumentUpdateReason::kJavaScript);
    }
  }
  UpdateMatches();
  return matches_;
}

void MediaQueryList::Trace(Visitor* visitor) const {
  visitor->Trace(matcher_);
  visitor->Trace(media_);
  visitor->Trace(listeners_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

const AtomicString& MediaQueryList::InterfaceName() const {
  return event_target_names::kMediaQueryList;
}

ExecutionContext* MediaQueryList::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

}  // namespace blink
