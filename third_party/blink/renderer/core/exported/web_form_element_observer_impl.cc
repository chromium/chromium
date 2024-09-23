// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/exported/web_form_element_observer_impl.h"

#include "base/functional/callback.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_observer_init.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

namespace {
constexpr const char kNullCallbackErrorMessage[] =
    " The MutationObserver should have been deactivated if callback_ was set "
    "to null. See http://crbug.com/40842164";
}

class WebFormElementObserverImpl::ObserverCallback
    : public MutationObserver::Delegate {
 public:
  ObserverCallback(HTMLElement&, base::OnceClosure callback);

  ExecutionContext* GetExecutionContext() const override;

  void Deliver(const MutationRecordVector& records, MutationObserver&) override;

  void Disconnect();

  void Trace(Visitor*) const override;

 private:
  Member<HTMLElement> element_;
  HeapHashSet<Member<Node>> parents_;
  Member<MutationObserver> mutation_observer_;
  base::OnceClosure callback_;
};

WebFormElementObserverImpl::ObserverCallback::ObserverCallback(
    HTMLElement& element,
    base::OnceClosure callback)
    : element_(element),
      mutation_observer_(MutationObserver::Create(this)),
      callback_(std::move(callback)) {
  {
    MutationObserverInit* init = MutationObserverInit::Create();
    init->setAttributes(true);
    init->setAttributeFilter({"class", "style"});
    mutation_observer_->observe(element_, init, ASSERT_NO_EXCEPTION);
  }
  for (Node* node = element_; node->parentElement();
       node = node->parentElement()) {
    MutationObserverInit* init = MutationObserverInit::Create();
    init->setChildList(true);
    init->setAttributes(true);
    init->setAttributeFilter({"class", "style"});
    mutation_observer_->observe(node->parentElement(), init,
                                ASSERT_NO_EXCEPTION);
    parents_.insert(node->parentElement());
  }
}

ExecutionContext*
WebFormElementObserverImpl::ObserverCallback::GetExecutionContext() const {
  return element_ ? element_->GetExecutionContext() : nullptr;
}

void WebFormElementObserverImpl::ObserverCallback::Deliver(
    const MutationRecordVector& records,
    MutationObserver&) {
  for (const auto& record : records) {
    if (record->type() == "childList") {
      for (unsigned i = 0; i < record->removedNodes()->length(); ++i) {
        Node* removed_node = record->removedNodes()->item(i);
        if (removed_node != element_ && !parents_.Contains(removed_node)) {
          continue;
        }
        DCHECK(callback_) << kNullCallbackErrorMessage;
        if (callback_) {
          std::move(callback_).Run();
        }
        Disconnect();
        return;
      }
    } else if (auto* element = DynamicTo<Element>(record->target())) {
      // Either "style" or "class" was modified. Check the computed style.
      auto* style = MakeGarbageCollected<CSSComputedStyleDeclaration>(element);
      if (style->GetPropertyValue(CSSPropertyID::kDisplay) == "none") {
        DCHECK(callback_) << kNullCallbackErrorMessage;
        if (callback_) {
          std::move(callback_).Run();
        }
        Disconnect();
        return;
      }
    }
  }
}

void WebFormElementObserverImpl::ObserverCallback::Disconnect() {
  mutation_observer_->disconnect();
  callback_ = base::OnceClosure();
}

void WebFormElementObserverImpl::ObserverCallback::Trace(
    blink::Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(parents_);
  visitor->Trace(mutation_observer_);
  MutationObserver::Delegate::Trace(visitor);
}

WebFormElementObserver* WebFormElementObserver::Create(
    WebFormElement& element,
    base::OnceClosure callback) {
  return MakeGarbageCollected<WebFormElementObserverImpl>(
      base::PassKey<WebFormElementObserver>(),
      *element.Unwrap<HTMLFormElement>(), std::move(callback));
}

WebFormElementObserver* WebFormElementObserver::Create(
    WebFormControlElement& element,
    base::OnceClosure callback) {
  return MakeGarbageCollected<WebFormElementObserverImpl>(
      base::PassKey<WebFormElementObserver>(), *element.Unwrap<HTMLElement>(),
      std::move(callback));
}

WebFormElementObserverImpl::WebFormElementObserverImpl(
    base::PassKey<WebFormElementObserver>,
    HTMLElement& element,
    base::OnceClosure callback) {
  mutation_callback_ =
      MakeGarbageCollected<ObserverCallback>(element, std::move(callback));
}

WebFormElementObserverImpl::~WebFormElementObserverImpl() = default;

void WebFormElementObserverImpl::Disconnect() {
  mutation_callback_->Disconnect();
  mutation_callback_ = nullptr;
  self_keep_alive_.Clear();
}

void WebFormElementObserverImpl::Trace(Visitor* visitor) const {
  visitor->Trace(mutation_callback_);
}

}  // namespace blink
