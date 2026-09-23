// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/mutation_observer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_observer_init.h"
#include "third_party/blink/renderer/core/dom/child_list_mutation_scope.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_registration.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/dom/text.h"
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
  root->SetInnerHTMLWithoutTrustedTypes(
      "<head><title>\n</title></head><body></body>");
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

// When nobody observes a node, the outermost ChildListMutationScope for it
// notes that on the Document so that nested scopes for the same node neither
// look up observers again nor start recording for an observer registered in
// between, just as they would have joined the outer scope's accumulator.
// Scopes for other nodes nested inside still find their observers (or, if
// they have none either, look that up each time without touching the note),
// and the note is gone when the outer scope ends.
TEST(MutationObserverTest, NestedUnobservedChildListMutationScopes) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  Persistent<Document> document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  auto* root =
      To<HTMLElement>(document->CreateRawElement(html_names::kHTMLTag));
  document->AppendChild(root);
  root->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id=observed></div><div id=a></div><div id=b></div>"
      "<div id=c></div></body>");
  Element* observed = document->getElementById(AtomicString("observed"));
  Element* a = document->getElementById(AtomicString("a"));
  Element* b = document->getElementById(AtomicString("b"));
  Element* c = document->getElementById(AtomicString("c"));
  MutationObserverInit* init = MutationObserverInit::Create();
  init->setChildList(true);

  // An unrelated observer, so that the document tracks child list mutations,
  // and one for |b|. Nothing observes |a| or |c|.
  Persistent<MutationObserver> unrelated = MutationObserver::Create(
      MakeGarbageCollected<EmptyMutationCallback>(*document));
  unrelated->observe(observed, init, ASSERT_NO_EXCEPTION);
  Persistent<MutationObserver> b_observer = MutationObserver::Create(
      MakeGarbageCollected<EmptyMutationCallback>(*document));
  b_observer->observe(b, init, ASSERT_NO_EXCEPTION);

  Persistent<MutationObserver> late = MutationObserver::Create(
      MakeGarbageCollected<EmptyMutationCallback>(*document));
  EXPECT_FALSE(document->UnobservedChildListMutationTarget());
  {
    ChildListMutationScope outer_a(*a);
    EXPECT_EQ(document->UnobservedChildListMutationTarget(), a);
    a->AppendChild(Text::Create(*document, "1"));
    // Registered while the unobserved scope for |a| is active.
    late->observe(a, init, ASSERT_NO_EXCEPTION);
    {
      ChildListMutationScope inner_a(*a);
      a->AppendChild(Text::Create(*document, "2"));
      // A compound operation on another, observed node nested inside.
      ChildListMutationScope outer_b(*b);
      b->AppendChild(Text::Create(*document, "3"));
      b->AppendChild(Text::Create(*document, "4"));
      EXPECT_EQ(document->UnobservedChildListMutationTarget(), a);
      {
        // Another unobserved node nested inside: there is only one slot, so
        // this scope neither takes it over nor clears it.
        ChildListMutationScope inner_c(*c);
        c->AppendChild(Text::Create(*document, "c1"));
        EXPECT_EQ(document->UnobservedChildListMutationTarget(), a);
      }
      EXPECT_EQ(document->UnobservedChildListMutationTarget(), a);
    }
    a->AppendChild(Text::Create(*document, "5"));
    EXPECT_TRUE(late->takeRecords().empty());
  }
  EXPECT_FALSE(document->UnobservedChildListMutationTarget());

  // |b|'s two insertions were one compound operation for its observer.
  MutationRecordVector b_records = b_observer->takeRecords();
  ASSERT_EQ(b_records.size(), 1u);
  EXPECT_EQ(b_records[0]->target(), b);
  EXPECT_EQ(b_records[0]->addedNodes()->length(), 2u);

  // With no scope for |a| active any more, |late| now gets records for it.
  a->AppendChild(Text::Create(*document, "6"));
  MutationRecordVector late_records = late->takeRecords();
  ASSERT_EQ(late_records.size(), 1u);
  EXPECT_EQ(late_records[0]->target(), a);
  EXPECT_TRUE(unrelated->takeRecords().empty());
}

}  // namespace blink
