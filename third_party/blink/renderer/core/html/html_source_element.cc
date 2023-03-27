/*
 * Copyright (C) 2007, 2008, 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#include "third_party/blink/renderer/core/html/html_source_element.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query_list.h"
#include "third_party/blink/renderer/core/css/media_query_matcher.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/html_picture_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

#define SOURCE_LOG_LEVEL 3

namespace blink {

class HTMLSourceElement::Listener final : public MediaQueryListListener {
 public:
  explicit Listener(HTMLSourceElement* element) : element_(element) {}
  void NotifyMediaQueryChanged() override {
    if (element_)
      element_->NotifyMediaQueryChanged();
  }

  void ClearElement() { element_ = nullptr; }
  void Trace(Visitor* visitor) const override {
    visitor->Trace(element_);
    MediaQueryListListener::Trace(visitor);
  }

 private:
  Member<HTMLSourceElement> element_;
};

HTMLSourceElement::HTMLSourceElement(Document& document)
    : HTMLElement(html_names::kSourceTag, document),
      listener_(MakeGarbageCollected<Listener>(this)) {
  DVLOG(SOURCE_LOG_LEVEL) << "HTMLSourceElement - " << (void*)this;
}

HTMLSourceElement::~HTMLSourceElement() = default;

void HTMLSourceElement::CreateMediaQueryList(const AtomicString& media) {
  RemoveMediaQueryListListener();
  if (media.empty()) {
    media_query_list_ = nullptr;
    return;
  }

  ExecutionContext* execution_context = GetExecutionContext();
  MediaQuerySet* set = MediaQuerySet::Create(media, execution_context);
  media_query_list_ = MakeGarbageCollected<MediaQueryList>(
      execution_context, &GetDocument().GetMediaQueryMatcher(), set);
  AddMediaQueryListListener();
}

void HTMLSourceElement::DidMoveToNewDocument(Document& old_document) {
  CreateMediaQueryList(FastGetAttribute(html_names::kMediaAttr));
  HTMLElement::DidMoveToNewDocument(old_document);
}

Node::InsertionNotificationRequest HTMLSourceElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  Element* parent = parentElement();
  if (auto* media = DynamicTo<HTMLMediaElement>(parent))
    media->SourceWasAdded(this);

  auto* html_picture_element = parent == insertion_point
                                   ? DynamicTo<HTMLPictureElement>(parent)
                                   : nullptr;
  if (html_picture_element) {
    html_picture_element->SourceChanged(ImageSourceChangeType::kAdded);
  }
  return kInsertionDone;
}

void HTMLSourceElement::RemovedFrom(ContainerNode& removal_root) {
  Element* parent = parentElement();
  bool was_removed_from_parent = !parent;
  if (was_removed_from_parent)
    parent = DynamicTo<Element>(&removal_root);
  if (auto* media = DynamicTo<HTMLMediaElement>(parent))
    media->SourceWasRemoved(this);
  if (auto* picture = DynamicTo<HTMLPictureElement>(parent)) {
    RemoveMediaQueryListListener();
    if (was_removed_from_parent)
      picture->SourceChanged(ImageSourceChangeType::kRemoved);
  }
  HTMLElement::RemovedFrom(removal_root);
}

void HTMLSourceElement::RemoveMediaQueryListListener() {
  if (media_query_list_)
    media_query_list_->RemoveListener(listener_);
}

void HTMLSourceElement::AddMediaQueryListListener() {
  if (media_query_list_)
    media_query_list_->AddListener(listener_);
}

const AtomicString& HTMLSourceElement::type() const {
  return FastGetAttribute(html_names::kTypeAttr);
}

void HTMLSourceElement::setType(const AtomicString& type) {
  setAttribute(html_names::kTypeAttr, type);
}

void HTMLSourceElement::ScheduleErrorEvent() {
  DVLOG(SOURCE_LOG_LEVEL) << "scheduleErrorEvent - " << (void*)this;

  pending_error_event_ = PostCancellableTask(
      *GetDocument().GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      WTF::BindOnce(&HTMLSourceElement::DispatchPendingEvent,
                    WrapPersistent(this)));
}

void HTMLSourceElement::CancelPendingErrorEvent() {
  DVLOG(SOURCE_LOG_LEVEL) << "cancelPendingErrorEvent - " << (void*)this;
  pending_error_event_.Cancel();
}

void HTMLSourceElement::DispatchPendingEvent() {
  DVLOG(SOURCE_LOG_LEVEL) << "dispatchPendingEvent - " << (void*)this;
  DispatchEvent(*Event::CreateCancelable(event_type_names::kError));
}

bool HTMLSourceElement::MediaQueryMatches() const {
  if (!media_query_list_)
    return true;

  return media_query_list_->matches();
}

void HTMLSourceElement::AttributeChanged(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == html_names::kWidthAttr || name == html_names::kHeightAttr) {
    if (auto* picture = DynamicTo<HTMLPictureElement>(parentElement()))
      picture->SourceDimensionChanged();
  }

  HTMLElement::AttributeChanged(params);
}

bool HTMLSourceElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kSrcAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

void HTMLSourceElement::ParseAttribute(
    const AttributeModificationParams& params) {
  HTMLElement::ParseAttribute(params);
  const QualifiedName& name = params.name;
  if (name == html_names::kMediaAttr)
    CreateMediaQueryList(params.new_value);
  if (name == html_names::kSrcsetAttr || name == html_names::kSizesAttr ||
      name == html_names::kMediaAttr || name == html_names::kTypeAttr) {
    if (auto* picture = DynamicTo<HTMLPictureElement>(parentElement())) {
      picture->SourceChanged(ImageSourceChangeType::kAttribute);
    }
  }
}

void HTMLSourceElement::NotifyMediaQueryChanged() {
  if (auto* picture = DynamicTo<HTMLPictureElement>(parentElement())) {
    picture->SourceChanged(ImageSourceChangeType::kMedia);
  }
}

void HTMLSourceElement::Trace(Visitor* visitor) const {
  visitor->Trace(media_query_list_);
  visitor->Trace(listener_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
