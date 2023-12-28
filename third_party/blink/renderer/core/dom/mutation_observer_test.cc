// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/mutation_observer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_observer_init.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_registration.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

class EmptyMutationCallback : public MutationObserver::Delegate {
 public:
  explicit EmptyMutationCallback(Document& document) : document_(document) {}

  ExecutionContext* GetExecutionContext() const override {
    return document_->GetExecutionContext();
  }

  void Deliver(const MutationRecordVector&, MutationObserver&) override {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(document_);
    MutationObserver::Delegate::Trace(visitor);
  }

 private:
  Member<Document> document_;
};

}  // namespace

TEST(MutationObserverTest, DisconnectCrash) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  Persistent<Document> document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  auto* root =
      To<HTMLElement>(document->CreateRawElement(html_names::kHTMLTag));
  document->AppendChild(root);
  root->setInnerHTML("<head><title>\n</title></head><body></body>");
  Node* head = root->firstChild()->firstChild();
  DCHECK(head);
  Persistent<MutationObserver> observer = MutationObserver::Create(
      MakeGarbageCollected<EmptyMutationCallback>(*document));
  MutationObserverInit* init = MutationObserverInit::Create();
  init->setCharacterDataOldValue(false);
  observer->observe(head, init, ASSERT_NO_EXCEPTION);

  head->remove();
  Persistent<MutationObserverRegistration> registration =
      observer->registrations_.begin()->Get();
  // The following GC will collect |head|, but won't collect a
  // MutationObserverRegistration for |head|.
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  observer->disconnect();
  // The test passes if disconnect() didn't crash.  crbug.com/657613.
}

}  // namespace blink
