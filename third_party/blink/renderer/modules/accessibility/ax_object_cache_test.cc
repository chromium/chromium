// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"

#include <algorithm>
#include <vector>

#include "base/auto_reset.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_transition_callback.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/core/view_transition/dom_view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_test_utils.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_relation_cache.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_WIN)
#include "third_party/blink/public/web/win/web_font_rendering.h"
#endif

namespace blink {

// TODO(nektar): Break test up into multiple tests.
TEST_F(AccessibilityTest, IsARIAWidget) {
  String test_content =
      "<body>"
      "<span id=\"plain\">plain</span><br>"
      "<span id=\"button\" role=\"button\">button</span><br>"
      "<span id=\"button-parent\" "
      "role=\"button\"><span>button-parent</span></span><br>"
      "<span id=\"button-caps\" role=\"BUTTON\">button-caps</span><br>"
      "<span id=\"button-second\" role=\"another-role "
      "button\">button-second</span><br>"
      "<span id=\"aria-bogus\" aria-bogus=\"bogus\">aria-bogus</span><br>"
      "<span id=\"aria-selected\" aria-selected>aria-selected</span><br>"
      "<span id=\"haspopup\" "
      "aria-haspopup=\"true\">aria-haspopup-true</span><br>"
      "<div id=\"focusable\" tabindex=\"1\">focusable</div><br>"
      "<div tabindex=\"2\"><div "
      "id=\"focusable-parent\">focusable-parent</div></div><br>"
      "</body>";

  SetBodyInnerHTML(test_content);
  Element* root(GetDocument().documentElement());
  EXPECT_FALSE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("plain"))));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("button"))));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("button-parent"))));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("button-caps"))));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("button-second"))));
  EXPECT_FALSE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("aria-bogus"))));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("aria-selected"))));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("haspopup"))));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("focusable"))));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("focusable-parent"))));
}

TEST_F(AccessibilityTest, HistogramTest) {
  SetBodyInnerHTML("<body><button>Press Me</button></body>");

  auto& cache = GetAXObjectCache();
  cache.SetAXMode(ui::kAXModeBasic);

  // No logs initially.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.AXObjectCacheImpl.Snapshot", 0);
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental", 0);
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental.Float", 0);
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental.Int", 0);
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental.HTML", 0);
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental.String", 0);

  {
    ui::AXTreeUpdate response;
    // Create a secondary AXObjectCache for a snapshot.
    Member<blink::AXObjectCache> snapshot_cache =
        blink::AXObjectCache::CreateSnapshotter(GetDocument(),
                                                ui::kAXModeBasic);
    std::set<ui::AXSerializationErrorFlag> out_error;
    snapshot_cache->SerializeEntireTreeAndDispose(/* max_node_count */ 1000,
                                                  base::TimeDelta::FiniteMax(),
                                                  &response, &out_error);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Snapshot", 1);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental", 0);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental.Float", 0);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental.Int", 0);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental.HTML", 0);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental.String", 0);
  }

  {
    std::vector<ui::AXTreeUpdate> updates;
    std::vector<ui::AXEvent> events;
    bool had_end_of_test_event = true;
    bool had_load_complete_messages = true;
    ScopedFreezeAXCache freeze(cache);
    cache.GetUpdatesAndEventsForSerialization(
        updates, events, had_end_of_test_event, had_load_complete_messages);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Snapshot", 1);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental", 1);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental.Float", 1);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental.Int", 1);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental.HTML", 1);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental.String", 1);
  }
}

TEST_F(AccessibilityTest, SnapshotWithDanglingAriaOwnsInOwnedSubtree) {
  SetBodyInnerHTML(R"HTML(
    <button aria-owns="target">Owner</button>
    <div id="target">
      <button aria-owns="does-not-exist">Dangling owner</button>
    </div>
  )HTML");

  Member<AXObjectCache> snapshot_cache = AXObjectCache::CreateSnapshotter(
      GetDocument(), ui::AXMode(ui::AXMode::kPDFPrinting));
  ui::AXTreeUpdate response;
  std::set<ui::AXSerializationErrorFlag> out_error;
  snapshot_cache->SerializeEntireTreeAndDispose(
      /*max_nodes=*/1000, base::TimeDelta::FiniteMax(), &response, &out_error);

  EXPECT_FALSE(response.nodes.empty());
  EXPECT_TRUE(out_error.empty());
}

TEST_F(AccessibilityTest, SnapshotWithValidAriaOwnsInOwnedSubtree) {
  SetBodyInnerHTML(R"HTML(
    <button aria-owns="target">Owner</button>
    <div id="target">
      <button id="inner-owner" aria-owns="inner-target">Inner owner</button>
    </div>
    <div id="inner-target">Inner target</div>
  )HTML");

  AXID inner_owner_id = GetElementById("inner-owner")->GetDomNodeId();
  AXID inner_target_id = GetElementById("inner-target")->GetDomNodeId();
  Member<AXObjectCache> snapshot_cache = AXObjectCache::CreateSnapshotter(
      GetDocument(), ui::AXMode(ui::AXMode::kPDFPrinting));
  ui::AXTreeUpdate response;
  std::set<ui::AXSerializationErrorFlag> out_error;
  snapshot_cache->SerializeEntireTreeAndDispose(
      /*max_nodes=*/1000, base::TimeDelta::FiniteMax(), &response, &out_error);

  EXPECT_FALSE(response.nodes.empty());
  EXPECT_TRUE(out_error.empty());
  bool inner_target_is_owned = false;
  for (const ui::AXNodeData& node : response.nodes) {
    if (node.id == inner_owner_id) {
      inner_target_is_owned =
          std::ranges::find(node.child_ids, inner_target_id) !=
          node.child_ids.end();
      break;
    }
  }
  EXPECT_TRUE(inner_target_is_owned);
}

TEST_F(AccessibilityTest, RemoveReferencesToAXID) {
  auto& cache = GetAXObjectCache();
  SetBodyInnerHTML(R"HTML(
      <div id="f" style="position:fixed">aaa</div>
      <h2 id="h">Heading</h2>)HTML");
  AXObject* fixed = GetAXObjectByElementId("f");
  // GetBoundsInFrameCoordinates() updates fixed_or_sticky_node_ids_.
  fixed->GetBoundsInFrameCoordinates();
  EXPECT_EQ(1u, cache.fixed_or_sticky_node_ids_.size());

  // RemoveReferencesToAXID() on node that is not fixed or sticky should not
  // affect fixed_or_sticky_node_ids_.
  cache.RemoveReferencesToAXID(GetAXObjectByElementId("h")->AXObjectID());
  EXPECT_EQ(1u, cache.fixed_or_sticky_node_ids_.size());

  // RemoveReferencesToAXID() on node that fixed should affect
  // fixed_or_sticky_node_ids_.
  cache.RemoveReferencesToAXID(GetAXObjectByElementId("f")->AXObjectID());
  EXPECT_EQ(0u, cache.fixed_or_sticky_node_ids_.size());
}

TEST_F(AccessibilityTest, RegisteredIdAttributesAreRemovedOnDisconnection) {
  SetBodyInnerHTML(R"HTML(<div id="persistent"></div>)HTML");

  AXObjectCacheImpl& cache = GetAXObjectCache();
  AXRelationCache* relation_cache = cache.RelationCache();
  ASSERT_NE(nullptr, relation_cache);
  const wtf_size_t initial_count =
      relation_cache->registered_id_attributes_.size();

  // HTML ids are registered before AXObject creation, so removing an element
  // must clear its registration even when no AXObject exists.
  Element* element_without_ax_object =
      GetDocument().CreateRawElement(html_names::kDivTag);
  element_without_ax_object->setAttribute(html_names::kIdAttr,
                                          AtomicString("without-ax-object"));
  GetDocument().body()->AppendChild(element_without_ax_object);

  ASSERT_EQ(nullptr, cache.Get(element_without_ax_object));
  ASSERT_EQ(initial_count + 1,
            relation_cache->registered_id_attributes_.size());

  element_without_ax_object->remove();

  EXPECT_EQ(initial_count, relation_cache->registered_id_attributes_.size());

  // Cleanup must also work after lifecycle processing creates an AXObject.
  Element* element_with_ax_object =
      GetDocument().CreateRawElement(html_names::kDivTag);
  element_with_ax_object->setAttribute(html_names::kIdAttr,
                                       AtomicString("with-ax-object"));
  GetDocument().body()->AppendChild(element_with_ax_object);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  ASSERT_NE(nullptr, cache.Get(element_with_ax_object));
  ASSERT_EQ(initial_count + 1,
            relation_cache->registered_id_attributes_.size());

  element_with_ax_object->remove();

  EXPECT_EQ(initial_count, relation_cache->registered_id_attributes_.size());
}

TEST_F(AccessibilityTest, DetachedElementsDoNotRegisterIdAttributes) {
  SetBodyInnerHTML(R"HTML(<div id="label">Accessible label</div>)HTML");

  AXObjectCacheImpl& cache = GetAXObjectCache();
  AXRelationCache* relation_cache = cache.RelationCache();
  ASSERT_NE(nullptr, relation_cache);
  const wtf_size_t initial_count =
      relation_cache->registered_id_attributes_.size();

  Element* label = GetElementById("label");
  ASSERT_NE(nullptr, label);
  ASSERT_FALSE(cache.IsLabelOrDescription(*label));

  // Detached elements must not register HTML ids, but their reverse relations
  // to connected elements must still be cached.
  Element* detached_parent =
      GetDocument().CreateRawElement(html_names::kDivTag);
  Element* detached_button =
      GetDocument().CreateRawElement(html_names::kButtonTag);
  detached_button->setAttribute(html_names::kIdAttr,
                                AtomicString("detached-button"));
  auto* labels = MakeGarbageCollected<GCedHeapVector<Member<Element>>>();
  labels->push_back(label);
  detached_button->setAriaLabelledByElements(labels);
  detached_parent->AppendChild(detached_button);

  EXPECT_FALSE(detached_button->isConnected());
  EXPECT_TRUE(cache.IsLabelOrDescription(*label));
  EXPECT_EQ(initial_count, relation_cache->registered_id_attributes_.size());

  GetDocument().body()->AppendChild(detached_parent);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  AXObject* button = cache.Get(detached_button);
  ASSERT_NE(nullptr, button);
  EXPECT_EQ("Accessible label", button->ComputedName());
}

TEST_F(AccessibilityTest,
       RegisteredIdAttributesSurviveConnectedAXObjectRecreation) {
  SetBodyInnerHTML(R"HTML(
      <div id="owner" aria-owns="target"></div>
      <div id="target"></div>
  )HTML");

  AXObjectCacheImpl& cache = GetAXObjectCache();
  AXRelationCache* relation_cache = cache.RelationCache();
  ASSERT_NE(nullptr, relation_cache);

  Element* owner = GetElementById("owner");
  ASSERT_NE(nullptr, owner);
  ASSERT_NE(nullptr, cache.Get(owner));
  AXObject* target = GetAXObjectByElementId("target");
  ASSERT_NE(nullptr, target);
  ASSERT_TRUE(cache.IsAriaOwned(target));
  ASSERT_TRUE(relation_cache->registered_id_attributes_.Contains(
      owner->GetDomNodeId()));

  cache.Remove(owner, /*notify_parent=*/false);

  EXPECT_TRUE(owner->isConnected());
  EXPECT_EQ(nullptr, cache.Get(owner));
  EXPECT_TRUE(relation_cache->registered_id_attributes_.Contains(
      owner->GetDomNodeId()));

  cache.ChildrenChanged(owner->parentNode());
  cache.UpdateAXForAllDocuments();

  AXObject* recreated_owner = cache.Get(owner);
  ASSERT_NE(nullptr, recreated_owner);
  AXObject* recreated_target = GetAXObjectByElementId("target");
  ASSERT_NE(nullptr, recreated_target);
  EXPECT_TRUE(cache.IsAriaOwned(recreated_target));
  EXPECT_EQ(recreated_owner, recreated_target->ParentObject());
}

class MockAXObject : public AXObject {
 public:
  explicit MockAXObject(AXObjectCacheImpl& ax_object_cache)
      : AXObject(ax_object_cache) {}
  static unsigned num_children_changed_calls_;

  void ChildrenChangedWithCleanLayout() final { num_children_changed_calls_++; }
  Document* GetDocument() const final { return &AXObjectCache().GetDocument(); }
  void AddChildren() final {}
  ax::mojom::blink::Role NativeRoleIgnoringAria() const override {
    return ax::mojom::blink::Role::kUnknown;
  }

  String ToString(bool verbose) const override { return "mock"; }
};

unsigned MockAXObject::num_children_changed_calls_ = 0;

TEST_F(AccessibilityTest, PauseUpdatesAfterMaxNumberQueued) {
  auto& document = GetDocument();
  auto* ax_object_cache =
      To<AXObjectCacheImpl>(document.ExistingAXObjectCache());
  DCHECK(ax_object_cache);

  wtf_size_t max_updates = 10;
  ax_object_cache->SetMaxPendingUpdatesForTesting(max_updates);

  MockAXObject* ax_obj = MakeGarbageCollected<MockAXObject>(*ax_object_cache);
  ax_object_cache->AssociateAXID(ax_obj);
  for (unsigned i = 0; i < max_updates + 1; i++) {
    ax_object_cache->DeferTreeUpdate(TreeUpdateReason::kChildrenChanged,
                                     ax_obj);
  }
  ax_object_cache->ProcessCleanLayoutCallbacks(document);

  ASSERT_EQ(0u, MockAXObject::num_children_changed_calls_);
}

TEST_F(AccessibilityTest, UpdateAXForAllDocumentsAfterPausedUpdates) {
  auto& document = GetDocument();
  auto* ax_object_cache =
      To<AXObjectCacheImpl>(document.ExistingAXObjectCache());
  DCHECK(ax_object_cache);

  wtf_size_t max_updates = 1;
  ax_object_cache->SetMaxPendingUpdatesForTesting(max_updates);

  UpdateAllLifecyclePhasesForTest();
  AXObject* root = ax_object_cache->Root();
  // Queue one update too many.
  ax_object_cache->DeferTreeUpdate(TreeUpdateReason::kChildrenChanged, root);
  ax_object_cache->DeferTreeUpdate(TreeUpdateReason::kChildrenChanged, root);

  ax_object_cache->UpdateAXForAllDocuments();
  ScopedFreezeAXCache freeze(*ax_object_cache);
  CHECK(!root->NeedsToUpdateCachedValues());
}

// A node-less AXObject, like AXValidationMessage, with hooks for controlling
// notifications and removals during queued dispatch.
class QueuedDispatchTestAXObject final : public AXObject {
 public:
  QueuedDispatchTestAXObject(AXObjectCacheImpl& ax_object_cache, String name)
      : AXObject(ax_object_cache), name_(std::move(name)) {}

  static Vector<String>* log_;

  void SetNotifyTargetOnComputeIsIgnored(AXObject* obj) {
    notify_target_on_compute_is_ignored_ = obj;
  }
  void SetNotifyTargetOnDispatch(AXObject* obj) {
    notify_target_on_dispatch_ = obj;
  }
  void SetRemoveTargetOnDispatch(AXObject* obj) {
    remove_target_on_dispatch_ = obj;
  }

  // AXObject:
  void ChildrenChangedWithCleanLayout() final {
    if (log_) {
      log_->push_back(name_ + "-begin");
    }
    if (AXObject* target = notify_target_on_dispatch_) {
      notify_target_on_dispatch_ = nullptr;
      AXObjectCache().ChildrenChangedOnAncestorOf(target);
    }
    if (AXObject* target = remove_target_on_dispatch_) {
      remove_target_on_dispatch_ = nullptr;
      AXObjectCache().Remove(target, /*notify_parent=*/false);
    }
    if (log_) {
      log_->push_back(name_ + "-end");
    }
  }
  bool ComputeIsIgnored(IgnoredReasons*) const final {
    if (AXObject* target = notify_target_on_compute_is_ignored_) {
      notify_target_on_compute_is_ignored_ = nullptr;
      AXObjectCache().ChildrenChangedOnAncestorOf(target);
    }
    return false;
  }
  Document* GetDocument() const final { return &AXObjectCache().GetDocument(); }
  void AddChildren() final {}
  ax::mojom::blink::Role NativeRoleIgnoringAria() const final {
    return ax::mojom::blink::Role::kUnknown;
  }
  String ToString(bool verbose) const final { return name_; }

  void Trace(Visitor* visitor) const final {
    visitor->Trace(notify_target_on_compute_is_ignored_);
    visitor->Trace(notify_target_on_dispatch_);
    visitor->Trace(remove_target_on_dispatch_);
    AXObject::Trace(visitor);
  }

 private:
  String name_;
  mutable Member<AXObject> notify_target_on_compute_is_ignored_;
  Member<AXObject> notify_target_on_dispatch_;
  Member<AXObject> remove_target_on_dispatch_;
};

Vector<String>* QueuedDispatchTestAXObject::log_ = nullptr;

// A and B are included ancestors. During kProcessDeferredUpdates, a
// children-changed notification raised inside a
// ScopedCachedAttributeValuesUpdate queues A instead of dispatching it. When
// the scope exits and A is dispatched, its handler raises another
// notification, which must append B to the queue and run it after A's
// handler returns, not nested inside it.
TEST_F(AccessibilityTest, QueuedChildrenChangedFlattensReentrantDispatch) {
  SetBodyInnerHTML(R"HTML(<p>text</p>)HTML");
  auto& cache = GetAXObjectCache();
  UpdateAllLifecyclePhasesForTest();
  AXObject* root = cache.Root();
  ASSERT_NE(nullptr, root);

  Vector<String> log;
  base::AutoReset<Vector<String>*> scoped_log(&QueuedDispatchTestAXObject::log_,
                                              &log);
  auto* parent_a = MakeGarbageCollected<QueuedDispatchTestAXObject>(cache, "A");
  auto* child_a =
      MakeGarbageCollected<QueuedDispatchTestAXObject>(cache, "child-a");
  auto* parent_b = MakeGarbageCollected<QueuedDispatchTestAXObject>(cache, "B");
  auto* child_b =
      MakeGarbageCollected<QueuedDispatchTestAXObject>(cache, "child-b");
  cache.AssociateAXID(parent_a);
  cache.AssociateAXID(child_a);
  cache.AssociateAXID(parent_b);
  cache.AssociateAXID(child_b);
  parent_a->SetParent(root);
  parent_b->SetParent(root);
  child_a->SetParent(parent_a);
  child_b->SetParent(parent_b);
  // A and B are clean, included ancestors.
  for (QueuedDispatchTestAXObject* included : {parent_a, parent_b}) {
    included->cached_is_ignored_ = false;
    included->cached_is_ignored_but_included_in_tree_ = false;
    included->cached_values_need_update_ = false;
  }

  // While A is being dispatched, raise a notification that queues B.
  parent_a->SetNotifyTargetOnDispatch(child_b);

  ASSERT_EQ(AXObjectCacheLifecycle::kDeferTreeUpdates,
            cache.lifecycle().GetState());
  cache.lifecycle_.AdvanceTo(AXObjectCacheLifecycle::kProcessDeferredUpdates);
  {
    AXObjectCacheImpl::ScopedCachedAttributeValuesUpdate guard(cache);
    cache.ChildrenChangedOnAncestorOf(child_a);
    // Not dispatched while the scope is alive.
    EXPECT_TRUE(log.empty());
  }
  cache.lifecycle_.EnsureStateAtMost(AXObjectCacheLifecycle::kDeferTreeUpdates);

  EXPECT_EQ((Vector<String>{"A-begin", "A-end", "B-begin", "B-end"}), log);
  EXPECT_TRUE(cache.queued_children_changed_ancestors_.empty());
  EXPECT_FALSE(cache.in_cached_attribute_values_update_);
}

// The child has dirty children and stale cached values. Recomputing them
// from UpdateChildrenIfNecessary() raises a children-changed notification
// that queues the parent; when the outermost scope dispatches it, the parent
// removes the child, like a RemoveSubtree() cascade. On return,
// UpdateChildrenIfNecessary() must notice the removal instead of reaching
// the CHECK in ClearChildren().
TEST_F(AccessibilityTest,
       UpdateChildrenIfNecessaryToleratesDetachDuringCachedValueUpdate) {
  SetBodyInnerHTML(R"HTML(<p>text</p>)HTML");
  auto& cache = GetAXObjectCache();
  UpdateAllLifecyclePhasesForTest();
  AXObject* root = cache.Root();
  ASSERT_NE(nullptr, root);

  auto* parent =
      MakeGarbageCollected<QueuedDispatchTestAXObject>(cache, "parent");
  auto* child =
      MakeGarbageCollected<QueuedDispatchTestAXObject>(cache, "child");
  cache.AssociateAXID(parent);
  cache.AssociateAXID(child);
  parent->SetParent(root);
  child->SetParent(parent);
  parent->cached_is_ignored_ = false;
  parent->cached_is_ignored_but_included_in_tree_ = false;
  parent->cached_values_need_update_ = false;
  // Like an object invalidated while updates are being processed: the child
  // needs both a children update and a cached-value update.
  child->cached_is_ignored_ = false;
  child->cached_is_ignored_but_included_in_tree_ = false;
  child->children_dirty_ = true;

  // The child's update raises a notification on its parent; dispatching it
  // removes the child, like a RemoveSubtree() cascade.
  child->SetNotifyTargetOnComputeIsIgnored(child);
  parent->SetRemoveTargetOnDispatch(child);

  ASSERT_EQ(AXObjectCacheLifecycle::kDeferTreeUpdates,
            cache.lifecycle().GetState());
  cache.lifecycle_.AdvanceTo(AXObjectCacheLifecycle::kProcessDeferredUpdates);
  child->UpdateChildrenIfNecessary();
  cache.lifecycle_.EnsureStateAtMost(AXObjectCacheLifecycle::kDeferTreeUpdates);

  EXPECT_TRUE(child->IsDetached());
  EXPECT_TRUE(parent->NeedsToUpdateChildren());
  EXPECT_TRUE(cache.queued_children_changed_ancestors_.empty());
  EXPECT_FALSE(cache.in_cached_attribute_values_update_);
}

TEST_F(AccessibilityTest,
       FinalizeTreeClearsDirtyDescendantsBelowCleanIgnoredObject) {
  SetBodyInnerHTML(R"HTML(
      <p id="before">before</p>
      <div>
        <p id="paragraph">paragraph</p>
      </div>
      <p id="after">after</p>)HTML");

  auto& cache = GetAXObjectCache();
  AXObject* body = GetAXBodyObject();
  ASSERT_NE(nullptr, body);
  ASSERT_EQ(3, body->ChildCountIncludingIgnored());
  AXObject* ignored_div = body->ChildAtIncludingIgnored(1);
  ASSERT_NE(nullptr, ignored_div);
  ASSERT_TRUE(ignored_div->IsIgnored());
  AXObject* paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, paragraph);
  ASSERT_EQ(ignored_div, paragraph->ParentObjectIncludedInTree());
  AXObject* text = paragraph->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, text->RoleValue());

  text->SetNeedsToUpdateChildren();
  ASSERT_TRUE(paragraph->HasDirtyDescendants());
  ASSERT_TRUE(ignored_div->HasDirtyDescendants());

  // Simulate a state that can occur mid-finalization: the ignored ancestor was
  // already processed and had its dirty-descendant bit cleared, but a
  // descendant below it was dirtied later in the same tree update.
  ignored_div->SetHasDirtyDescendants(false);
  cache.UpdateAXForAllDocuments();
  EXPECT_FALSE(paragraph->HasDirtyDescendants());
}

TEST_F(AccessibilityTest, AccessibilityFocus) {
  String test_content =
      "<body>"
      "<button id=button></button>"
      "<ul id=ul></ul>"
      "</body>";

  SetBodyInnerHTML(test_content);
  Element* root(GetDocument().documentElement());
  Element* button = root->getElementById(AtomicString("button"));
  ASSERT_NE(nullptr, button);
  Element* ul = root->getElementById(AtomicString("ul"));
  ASSERT_NE(nullptr, ul);

  auto& cache = GetAXObjectCache();
  cache.SetAXMode(ui::kAXModeBasic);
  EXPECT_EQ(nullptr, cache.GetAccessibilityFocus());
  auto* ax_button = cache.FirstObjectWithRole(ax::mojom::Role::kButton);
  ASSERT_NE(nullptr, ax_button);
  ui::AXActionData action;
  action.action = ax::mojom::Action::kSetAccessibilityFocus;
  ax_button->PerformAction(action);
  EXPECT_EQ(button, cache.GetAccessibilityFocus());

  auto* ax_ul = cache.FirstObjectWithRole(ax::mojom::Role::kList);
  ASSERT_NE(nullptr, ax_ul);
  ax_ul->PerformAction(action);
  EXPECT_EQ(ul, cache.GetAccessibilityFocus());
}

TEST_F(AccessibilityTest, LocationSerializationDelayForAccessibilityFocus) {
  SetBodyInnerHTML(R"HTML(
      <button id="button">Click</button>
  )HTML");

  Element* button = GetElementById("button");
  ASSERT_NE(nullptr, button);

  auto& cache = GetAXObjectCache();
  cache.SetAXMode(ui::kAXModeBasic);

  auto* ax_button = cache.FirstObjectWithRole(ax::mojom::Role::kButton);
  ASSERT_NE(nullptr, ax_button);

  // Without any changes, delay defaults to non-focused delay (500ms).
  EXPECT_EQ(500, cache.GetLocationSerializationDelay());

  // Invalidate bounds on button when it does not have focus.
  cache.InvalidateBoundingBox(ax_button->AXObjectID());
  EXPECT_EQ(500, cache.GetLocationSerializationDelay());

  // Set accessibility focus on the button.
  ui::AXActionData action;
  action.action = ax::mojom::Action::kSetAccessibilityFocus;
  ax_button->PerformAction(action);
  EXPECT_EQ(button, cache.GetAccessibilityFocus());

  // With accessibility focus set, location updates are scheduled with
  // the focused delay (75ms) instead of the non-focused delay (500ms).
  EXPECT_EQ(75, cache.GetLocationSerializationDelay());

  // Clear accessibility focus and verify it reverts to 500ms.
  action.action = ax::mojom::Action::kClearAccessibilityFocus;
  ax_button->PerformAction(action);
  EXPECT_EQ(nullptr, cache.GetAccessibilityFocus());
  EXPECT_EQ(500, cache.GetLocationSerializationDelay());
}

TEST_F(AccessibilityTest, SetMenuListOptionsBoundsBasePickerClearsState) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #sel, #sel::picker(select) {
          appearance: base-select;
        }
      </style>
      <select id="sel" autofocus>
        <option>one</option>
        <option>two</option>
      </select>)HTML");

  auto* select = To<HTMLSelectElement>(GetElementById("sel"));
  ASSERT_NE(select, nullptr);
  select->ShowPopup();
  EXPECT_TRUE(select->PopupIsVisible());

  auto& cache = GetAXObjectCache();
  cache.current_menu_list_axid_ = 123;
  Vector<gfx::Rect> stale_bounds;
  stale_bounds.push_back(gfx::Rect(1, 2, 3, 4));
  cache.options_bounds_ = stale_bounds;

  Vector<gfx::Rect> new_bounds;
  new_bounds.push_back(gfx::Rect(5, 6, 7, 8));
  cache.SetMenuListOptionsBounds(select, new_bounds);

  EXPECT_EQ(cache.current_menu_list_axid_, 0);
  EXPECT_TRUE(cache.options_bounds_.empty());

  AXObject* ax_select = GetAXObjectByElementId("sel");
  ASSERT_NE(ax_select, nullptr);
  EXPECT_EQ(cache.GetOptionsBounds(*ax_select), nullptr);
}

#if AX_FAIL_FAST_BUILD()
TEST_F(AccessibilityTest, NodesRequiringCacheUpdate) {
  String test_content =
      "<body>"
      "<div id=foo></div>"
      "<div id=bar></div>"
      "<div id=baz></div>"
      "</body>";
  SetBodyInnerHTML(test_content);

  auto& cache = GetAXObjectCache();
  auto& nodes_requiring_cache_update = cache.GetNodesRequiringCacheUpdate();
  ASSERT_TRUE(nodes_requiring_cache_update.empty());

  AXObject* foo = GetAXObjectByElementId("foo");
  AXID foo_id = foo->AXObjectID();
  CHECK(foo);

  AXObject* bar = GetAXObjectByElementId("bar");
  AXID bar_id = bar->AXObjectID();
  CHECK(bar);

  AXObject* baz = GetAXObjectByElementId("baz");
  AXID baz_id = baz->AXObjectID();
  CHECK(baz);

  // DeferTreeUpdate() should require the node's cached attribute values be
  // updated. Make sure we are tracking these nodes in
  // GetNodesRequiringCacheUpdate() mapped to the correct update reason.
  cache.DeferTreeUpdate(TreeUpdateReason::kChildrenChanged, foo);
  ASSERT_TRUE(nodes_requiring_cache_update.Contains(foo_id));
  ASSERT_EQ(nodes_requiring_cache_update.size(), 1U);

  auto entry = nodes_requiring_cache_update.find(foo_id);
  ASSERT_EQ(entry->value, TreeUpdateReason::kChildrenChanged);

  // Calling DeferTreeUpdate() on a second node should result in two entries in
  // GetNodesRequiringCacheUpdate().
  cache.DeferTreeUpdate(TreeUpdateReason::kUpdateAriaOwns, bar);
  ASSERT_TRUE(nodes_requiring_cache_update.Contains(bar_id));
  ASSERT_TRUE(nodes_requiring_cache_update.Contains(foo_id));
  ASSERT_EQ(nodes_requiring_cache_update.size(), 2U);

  entry = nodes_requiring_cache_update.find(bar_id);
  ASSERT_EQ(entry->value, TreeUpdateReason::kUpdateAriaOwns);

  // Calling DeferTreeUpdate() on a node already in
  // GetNodesRequiringCacheUpdate() should replace the existing entry.
  cache.DeferTreeUpdate(TreeUpdateReason::kMarkDocumentDirty, bar);
  ASSERT_TRUE(nodes_requiring_cache_update.Contains(bar_id));
  ASSERT_TRUE(nodes_requiring_cache_update.Contains(foo_id));
  ASSERT_EQ(nodes_requiring_cache_update.size(), 2U);

  entry = nodes_requiring_cache_update.find(bar_id);
  ASSERT_EQ(entry->value, TreeUpdateReason::kMarkDocumentDirty);

  // Calling SetCachedValuesNeedUpdate() on a third node should result in three
  // entries in GetNodesRequiringCacheUpdate().
  baz->SetCachedValuesNeedUpdate(true, TreeUpdateReason::kFocusableChanged);
  ASSERT_TRUE(nodes_requiring_cache_update.Contains(baz_id));
  ASSERT_TRUE(nodes_requiring_cache_update.Contains(bar_id));
  ASSERT_TRUE(nodes_requiring_cache_update.Contains(foo_id));
  ASSERT_EQ(nodes_requiring_cache_update.size(), 3U);

  entry = nodes_requiring_cache_update.find(baz_id);
  ASSERT_EQ(entry->value, TreeUpdateReason::kFocusableChanged);

  // Detaching an object should remove it from GetNodesRequiringCacheUpdate().
  foo->Detach();
  ASSERT_EQ(nodes_requiring_cache_update.size(), 2U);
  ASSERT_TRUE(nodes_requiring_cache_update.Contains(bar_id));
  ASSERT_TRUE(nodes_requiring_cache_update.Contains(baz_id));

  // Reset foo's AXObjectCacheImpl to avoid failure on completion of test.
  foo->SetAXObjectCacheForTest(cache);

  // Removing an object from the AXCache should remove if from
  // GetNodesRequiringCacheUpdate(), as well.
  cache.Remove(bar_id, false);
  ASSERT_EQ(nodes_requiring_cache_update.size(), 1U);
  ASSERT_TRUE(nodes_requiring_cache_update.Contains(baz_id));

  // Calling SetCachedValuesNeedUpdate() to false will remove the last object
  // from GetNodesRequiringCacheUpdate().
  baz->SetCachedValuesNeedUpdate(false);
  ASSERT_TRUE(nodes_requiring_cache_update.empty());
}
#endif

class AXViewTransitionTest : public testing::Test {
 public:
  AXViewTransitionTest() {}

  void SetUp() override {
    web_view_helper_ = std::make_unique<frame_test_helpers::WebViewHelper>();
    web_view_helper_->Initialize();
    web_view_helper_->Resize(gfx::Size(200, 200));
  }

  void TearDown() override { web_view_helper_.reset(); }

  Document& GetDocument() {
    return *web_view_helper_->GetWebView()
                ->MainFrameImpl()
                ->GetFrame()
                ->GetDocument();
  }

  void UpdateAllLifecyclePhasesAndFinishDirectives() {
    UpdateAllLifecyclePhasesForTest();
    ViewTransitionTestUtils::ProcessPendingDirectives(GetDocument(),
                                                      LayerTreeHost());
  }

  cc::LayerTreeHost* LayerTreeHost() {
    return web_view_helper_->LocalMainFrame()
        ->FrameWidgetImpl()
        ->LayerTreeHostForTesting();
  }

  void SetHtmlInnerHTML(const String& content) {
    GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(content);
    UpdateAllLifecyclePhasesForTest();
  }

  void UpdateAllLifecyclePhasesForTest() {
    web_view_helper_->GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  using State = ViewTransition::State;

  State GetState(DOMViewTransition* transition) const {
    return transition->GetViewTransitionForTest()->state_;
  }

 protected:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;
};

TEST_F(AXViewTransitionTest, TransitionPseudoNotRelevant) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .shared {
        width: 100px;
        height: 100px;
        view-transition-name: shared;
        contain: layout;
        background: green;
      }
    </style>
    <div id=target class=shared></div>
  )HTML");

  auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
  ScriptState::Scope scope(script_state);

  MockFunctionScope funcs(script_state);
  auto* view_transition_callback = V8ViewTransitionCallback::Create(
      funcs.ExpectCall()->ToV8Function(script_state));

  auto* transition = ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(), view_transition_callback,
      ASSERT_NO_EXCEPTION);

  ScriptPromiseTester finish_tester(script_state,
                                    transition->finished(script_state));

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(GetState(transition), State::kCapturing);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  EXPECT_EQ(GetState(transition), State::kDOMCallbackRunning);

  // We should have a start request from the async callback passed to start()
  // resolving.
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesAndFinishDirectives();

  // We should have a transition pseudo
  auto* transition_pseudo = GetDocument().documentElement()->GetPseudoElement(
      kPseudoIdViewTransition);
  ASSERT_TRUE(transition_pseudo);
  auto* container_pseudo = transition_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionGroup, AtomicString("shared"));
  ASSERT_TRUE(container_pseudo);
  auto* image_wrapper_pseudo = container_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionImagePair, AtomicString("shared"));
  ASSERT_TRUE(image_wrapper_pseudo);
  auto* nested_groups_pseudo = container_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionGroupChildren, AtomicString("shared"));
  ASSERT_FALSE(nested_groups_pseudo);
  auto* incoming_image_pseudo = image_wrapper_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionNew, AtomicString("shared"));
  ASSERT_TRUE(incoming_image_pseudo);
  auto* outgoing_image_pseudo = image_wrapper_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionOld, AtomicString("shared"));
  ASSERT_TRUE(outgoing_image_pseudo);

  ASSERT_TRUE(transition_pseudo->GetLayoutObject());
  ASSERT_TRUE(container_pseudo->GetLayoutObject());
  ASSERT_TRUE(image_wrapper_pseudo->GetLayoutObject());
  ASSERT_TRUE(incoming_image_pseudo->GetLayoutObject());
  ASSERT_TRUE(outgoing_image_pseudo->GetLayoutObject());

  EXPECT_FALSE(AXObjectCacheImpl::IsRelevantPseudoElement(*transition_pseudo));
  EXPECT_FALSE(AXObjectCacheImpl::IsRelevantPseudoElement(*container_pseudo));
  EXPECT_FALSE(
      AXObjectCacheImpl::IsRelevantPseudoElement(*image_wrapper_pseudo));
  EXPECT_FALSE(
      AXObjectCacheImpl::IsRelevantPseudoElement(*incoming_image_pseudo));
  EXPECT_FALSE(
      AXObjectCacheImpl::IsRelevantPseudoElement(*outgoing_image_pseudo));
}

class AccessibilityEnabledLaterTest : public AccessibilityTest {
  USING_FAST_MALLOC(AccessibilityEnabledLaterTest);

 public:
  AccessibilityEnabledLaterTest(LocalFrameClient* local_frame_client = nullptr)
      : AccessibilityTest(local_frame_client) {}

  void SetUp() override { RenderingTest::SetUp(); }

  void EnableAccessibility() {
    ax_context_ =
        std::make_unique<AXContext>(GetDocument(), ui::kAXModeComplete);
  }
};

TEST_F(AccessibilityTest, CanvasWithContentVisibilityAutoShouldNotCrash) {
  // Test that canvas fallback content with content-visibility: auto
  // doesn't cause display lock crashes when accessibility is enabled.
  SetBodyInnerHTML(R"HTML(
    <canvas style="content-visibility: auto;">
      <div>Canvas fallback content</div>
    </canvas>
  )HTML");
}

TEST_F(AccessibilityTest, ValidationMessageIncludedInRootChildren) {
#if BUILDFLAG(IS_WIN)
  blink::WebFontRendering::SetMenuFontMetrics(
      blink::WebString::FromAscii("Arial"), 12);
#endif
  SetBodyInnerHTML(R"HTML(<input id="input">)HTML");

  AXObject* root = GetAXRootObject();
  ASSERT_TRUE(root);

  auto* input = To<HTMLInputElement>(GetElementById("input"));
  ASSERT_TRUE(input);
  input->setCustomValidity("Error");
  input->reportValidity();
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  AXObject* message = GetAXObjectCache().ValidationMessageObjectIfInvalid();
  ASSERT_TRUE(message);
  EXPECT_TRUE(root->CachedChildrenIncludingIgnored().Contains(message));
}

TEST_F(AccessibilityTest, RestoreAriaOwnsAfterAriaHiddenRemoved) {
  SetBodyInnerHTML(R"HTML(
      <div id="container">
        <ul id="list" aria-owns="item1 item2"></ul>
      </div>
      <li id="item1">Item 1</li>
      <li id="item2">Item 2</li>
  )HTML");

  AXObject* list = GetAXObjectByElementId("list");
  ASSERT_NE(nullptr, list);

  AXObject* item1 = GetAXObjectByElementId("item1");
  ASSERT_NE(nullptr, item1);

  AXObject* item2 = GetAXObjectByElementId("item2");
  ASSERT_NE(nullptr, item2);

  // Initial state: list owns item1 and item2.
  EXPECT_EQ(2u, list->ChildrenIncludingIgnored().size());
  EXPECT_EQ(list, item1->ParentObject());
  EXPECT_EQ(list, item2->ParentObject());

  // Set aria-hidden on container.
  Element* container = GetElementById("container");
  ASSERT_NE(nullptr, container);
  container->setAttribute(html_names::kAriaHiddenAttr, AtomicString("true"));
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // list should now be hidden/ignored, and the items should not be owned by it.
  list = GetAXObjectByElementId("list");
  if (list) {
    EXPECT_TRUE(list->IsIgnored());
    EXPECT_EQ(0u, list->ChildrenIncludingIgnored().size());
  }

  // item1 and item2 should have their parent restored.
  item1 = GetAXObjectByElementId("item1");
  ASSERT_NE(nullptr, item1);
  EXPECT_NE(list, item1->ParentObject());

  item2 = GetAXObjectByElementId("item2");
  ASSERT_NE(nullptr, item2);
  EXPECT_NE(list, item2->ParentObject());

  // Remove aria-hidden from container.
  container->removeAttribute(html_names::kAriaHiddenAttr);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // list should be restored and own the items again.
  list = GetAXObjectByElementId("list");
  ASSERT_NE(nullptr, list);
  EXPECT_FALSE(list->IsIgnored());

  item1 = GetAXObjectByElementId("item1");
  ASSERT_NE(nullptr, item1);

  item2 = GetAXObjectByElementId("item2");
  ASSERT_NE(nullptr, item2);

  EXPECT_EQ(2u, list->ChildrenIncludingIgnored().size());
  EXPECT_EQ(list, item1->ParentObject());
  EXPECT_EQ(list, item2->ParentObject());
}

// Regression test for crbug.com/503872382.
TEST_F(AccessibilityTest, ReleaseAriaOwnedChildWhenOwnerBecomesEditable) {
  SetBodyInnerHTML(R"HTML(
      <ul id="list" aria-owns="item"></ul>
      <li id="item">Item</li>
  )HTML");

  AXObject* item = GetAXObjectByElementId("item");
  ASSERT_NE(nullptr, item);
  EXPECT_EQ(GetAXObjectByElementId("list"), item->ParentObject());

  // The element with aria-owns stops being a valid owner.
  Element* list_element = GetElementById("list");
  list_element->setAttribute(html_names::kContenteditableAttr, g_empty_atom);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  AXObject* list = GetAXObjectByElementId("list");
  item = GetAXObjectByElementId("item");
  ASSERT_NE(nullptr, list);
  ASSERT_NE(nullptr, item);
  EXPECT_EQ(0u, list->ChildrenIncludingIgnored().size());
  EXPECT_EQ(GetAXBodyObject(), item->ParentObject());

  // The element with aria-owns becomes a valid owner again.
  list_element->removeAttribute(html_names::kContenteditableAttr);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  list = GetAXObjectByElementId("list");
  item = GetAXObjectByElementId("item");
  ASSERT_NE(nullptr, list);
  ASSERT_NE(nullptr, item);
  EXPECT_EQ(list, item->ParentObject());
  EXPECT_TRUE(list->ChildrenIncludingIgnored().Contains(item));
}

// Regression test for crbug.com/503872382.
TEST_F(AccessibilityTest, LazyRemovalOfInvalidAriaOwnsRestoresParent) {
  SetBodyInnerHTML(R"HTML(
      <ul id="list" aria-owns="item"></ul>
      <li id="item">Item</li>
  )HTML");

  // Hold a cache reference, as each GetAXObjectCache() runs a lifecycle update.
  AXObjectCacheImpl& cache = GetAXObjectCache();
  AXObject* item = GetAXObjectByElementId("item");
  ASSERT_NE(nullptr, item);
  EXPECT_EQ(GetAXObjectByElementId("list"), item->ParentObject());

  // The element with aria-owns stops being a valid owner.
  GetElementById("list")->setAttribute(html_names::kContenteditableAttr,
                                       g_empty_atom);

  // Lazy revalidation (which requires clean layout) detaches the child from
  // its invalid owner; its natural parent will be restored by the next update.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  ASSERT_TRUE(cache.IsAriaOwned(item));
  EXPECT_EQ(nullptr, cache.ValidatedAriaOwner(item));
  EXPECT_EQ(nullptr, item->ParentObjectIfPresent());

  cache.UpdateAXForAllDocuments();

  item = GetAXObjectByElementId("item");
  ASSERT_NE(nullptr, item);
  EXPECT_EQ(GetAXBodyObject(), item->ParentObject());
}

#if AX_FAIL_FAST_BUILD()
// Regression test for crbug.com/511730260: the cache's included node count and
// the serializer's client tree count may diverge when an included node is left
// unreachable from the root during loading; this must not crash the browser.
//
// The last update of a serialization pass carries AXTreeChecks::node_count.
// The browser applies those updates, and AXTree::CheckTreeConsistency() checks
// that node_count matches the size of the resulting tree (id_map_.size()) and
// crashes (NOTREACHED) if it does not. That size also equals the serializer's
// client tree count (ClientTreeNodeCount()), which is the number of nodes in
// its current client tree.
//
// The cache also keeps a separate included node count (GetIncludedNodeCount())
// that can diverge from the client tree count (crbug.com/456786676). Sending
// that value as node_count could cause a crash because of "tree inconsistency".
//
// This test creates that divergence: |owner| aria-owns |target|, then leaves
// |target| included but unreachable from the root during loading, so the
// serializer never reaches it. The test then verifies node_count tracks the
// client tree count, not the cache's included count.
TEST_F(AccessibilityTest,
       TreeChecksNodeCountMatchesSerializerClientTreeWhenIncludedCountDrifts) {
  // |owner| aria-owns |target|, so |target| is an aria-owned child of |owner|
  // in the accessibility tree without being its DOM child.
  StringBuilder body;
  body.Append(R"HTML(
      <div id="owner" aria-owns="target"></div>
      <div id="target" role="button" aria-label="Target"></div>
  )HTML");
  // Having more than 100 included nodes skips the expensive DCHECK that the
  // renderer-side CheckTreeConsistency() runs only for small trees, comparing
  // the cache's included count against a recursive count from the root.
  for (int i = 0; i < 150; ++i) {
    body.Append("<p>x</p>");
  }
  SetBodyInnerHTML(body.ToString());

  auto& cache = GetAXObjectCache();
  AXObject* owner = GetAXObjectByElementId("owner");
  AXObject* target = GetAXObjectByElementId("target");
  ASSERT_TRUE(owner);
  ASSERT_TRUE(target);
  ASSERT_EQ(owner, target->ParentObject());
  ASSERT_TRUE(owner->CachedChildrenIncludingIgnored().Contains(target));
  ASSERT_GT(cache.GetIncludedNodeCount(), 100u);

  // Reset the serializer so the next pass re-serializes the whole tree.
  cache.ResetSerializer();
  ASSERT_FALSE(cache.HasObjectsPendingSerialization());

  const AXID target_id = target->AXObjectID();

  // Make |target| unreachable from the root. ClearChildren() drops it from
  // |owner|'s child list, and SetParent() then restores its parent pointer but
  // not its place in the child list. Therefore |target| stays included and
  // parented yet no ancestor lists it, so the serializer will never reach it.
  owner->ClearChildren();
  ASSERT_TRUE(target->IsMissingParent());
  target->SetParent(owner);
  ASSERT_EQ(owner, target->ParentObjectIfPresent());
  ASSERT_FALSE(target->IsMissingParent());
  ASSERT_TRUE(target->IsIncludedInTree());
  ASSERT_FALSE(owner->CachedChildrenIncludingIgnored().Contains(target));

  // Set the state of the document to "loading" so the consistency checks in
  // CheckTreeIsFinalized() (every included node must be listed by its parent)
  // are skipped, leaving |target| in place instead of crashing the renderer.
  // SerializeAXUpdatesIfNeeded() does not serialize anything here but
  // resets the cache lifecycle state so the next pass can advance it again.
  GetDocument().SetReadyState(Document::kLoading);
  ASSERT_TRUE(cache.CommitAXUpdates(GetDocument(), /*force=*/true));
  cache.SerializeAXUpdatesIfNeeded(GetDocument());
  ASSERT_FALSE(cache.IsDirty());
  ASSERT_FALSE(cache.HasObjectsPendingSerialization());

  // Confirm that |target| is still included and parented to |owner|, while
  // absent from its child list. It is the one unreachable included node, so the
  // included count exceeds the client tree count by one.
  owner = GetAXObjectByElementId("owner");
  target = GetAXObjectByElementId("target");
  ASSERT_TRUE(owner);
  ASSERT_TRUE(target);
  ASSERT_EQ(target_id, target->AXObjectID());
  ASSERT_TRUE(target->IsIncludedInTree());
  ASSERT_FALSE(target->IsMissingParent());
  ASSERT_EQ(owner, target->ParentObjectIfPresent());
  ASSERT_FALSE(owner->CachedChildrenIncludingIgnored().Contains(target));

  // Serialize, calling GetUpdatesAndEventsForSerialization directly to capture
  // the list of updates.
  cache.AddDirtyObjectToSerializationQueue(cache.Root());
  std::vector<ui::AXTreeUpdate> updates;
  std::vector<ui::AXEvent> events;
  bool had_end_of_test_event = false;
  bool had_load_complete_messages = false;
  {
    ScopedFreezeAXCache freeze(cache);
    ASSERT_TRUE(cache.HasObjectsPendingSerialization());
    cache.GetUpdatesAndEventsForSerialization(
        updates, events, had_end_of_test_event, had_load_complete_messages);
  }

  // |target| is counted as included but never serialized, so node_count is
  // exactly one below the cache's included count.
  ASSERT_FALSE(updates.empty());
  ASSERT_TRUE(updates.back().tree_checks.has_value());
  ASSERT_EQ(updates.back().tree_checks->node_count + 1,
            cache.GetIncludedNodeCount());

  // This serialization pass serialized the entire reachable tree, so its
  // distinct node ids are exactly what the browser maps into id_map_. Verify
  // that node_count equals the number of serialized nodes, and that |target|
  // is not among them.
  HashSet<AXID> serialized_node_ids;
  for (const auto& update : updates) {
    for (const auto& node : update.nodes) {
      serialized_node_ids.insert(node.id);
    }
  }
  EXPECT_EQ(updates.back().tree_checks->node_count,
            static_cast<size_t>(serialized_node_ids.size()));
  EXPECT_FALSE(serialized_node_ids.Contains(target_id));
}
#endif  // AX_FAIL_FAST_BUILD()

// Test that `PreviousLayoutObjectTextOnLine` handles `LayoutText` objects where
// `IsInLayoutNGInlineFormattingContext()` is false without crashing on DCHECK.
TEST_F(AccessibilityTest, ComputeNodesOnLineWithNonIfcText) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <span id="target">Visible text</span>
    </div>
  )HTML");

  AXObjectCacheImpl& cache = GetAXObjectCache();
  cache.SetAXMode(ui::kAXModeComplete);

  Element* container = GetElementById("container");
  Text* new_text = GetDocument().createTextNode("Unlaid out text ");
  container->insertBefore(new_text, GetElementById("target"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  new_text->GetLayoutObject()->SetIsInLayoutNGInlineFormattingContext(false);

  AXObject* target = GetAXObjectByElementId("target");
  ASSERT_TRUE(target);
  ASSERT_TRUE(target->GetLayoutObject());

  cache.ComputeNodesOnLine(target->GetLayoutObject());
}

TEST_F(AccessibilityTest, IsRelevantSlotElement) {
  SetBodyInnerHTML(R"HTML(
    <select id="sel"><option>one</option></select>
    <select id="empty_sel"></select>
  )HTML");

  auto* select = To<HTMLSelectElement>(GetElementById("sel"));
  ASSERT_NE(select, nullptr);
  ShadowRoot* shadow_root = select->UserAgentShadowRoot();
  ASSERT_NE(shadow_root, nullptr);
  auto* slot = To<HTMLSlotElement>(
      shadow_root->getElementById(shadow_element_names::kSelectOptions));
  ASSERT_NE(slot, nullptr);
  EXPECT_TRUE(AXObjectCacheImpl::IsRelevantSlotElement(*slot));

  auto* empty_select = To<HTMLSelectElement>(GetElementById("empty_sel"));
  ASSERT_NE(empty_select, nullptr);
  ShadowRoot* empty_shadow_root = empty_select->UserAgentShadowRoot();
  ASSERT_NE(empty_shadow_root, nullptr);
  auto* empty_slot = To<HTMLSlotElement>(
      empty_shadow_root->getElementById(shadow_element_names::kSelectOptions));
  ASSERT_NE(empty_slot, nullptr);
  EXPECT_TRUE(AXObjectCacheImpl::IsRelevantSlotElement(*empty_slot));
}

TEST_F(AccessibilityTest, AriaModalRetainsActiveStateWhenFocusMovesToBody) {
  GetDocument().GetSettings()->SetAriaModalPrunesAXTree(true);

  SetBodyInnerHTML(R"HTML(
    <button id="bg_button">Background Button</button>
    <div id="dialog" role="dialog" aria-modal="true">
      <button id="modal_button">Modal Button</button>
    </div>
  )HTML");

  AXObjectCacheImpl* cache =
      To<AXObjectCacheImpl>(GetDocument().ExistingAXObjectCache());
  ASSERT_NE(cache, nullptr);

  Element* modal_button = GetElementById("modal_button");
  ASSERT_NE(modal_button, nullptr);

  // Focus node inside the modal dialog.
  cache->HandleFocusedUIElementChanged(nullptr, modal_button);
  EXPECT_EQ(cache->GetActiveAriaModalDialog(), GetElementById("dialog"));

  // Move focus to document body (ancestor of dialog).
  cache->HandleFocusedUIElementChanged(modal_button, GetDocument().body());

  // Verify active_aria_modal_dialog_ remains set when focus moves to an
  // ancestor.
  EXPECT_EQ(cache->GetActiveAriaModalDialog(), GetElementById("dialog"));
}

TEST_F(AccessibilityTest, AriaModalClearsWhenFocusMovesToBackgroundButton) {
  GetDocument().GetSettings()->SetAriaModalPrunesAXTree(true);

  SetBodyInnerHTML(R"HTML(
    <button id="bg_button">Background Button</button>
    <div id="dialog" role="dialog" aria-modal="true">
      <button id="modal_button">Modal Button</button>
    </div>
  )HTML");

  AXObjectCacheImpl* cache =
      To<AXObjectCacheImpl>(GetDocument().ExistingAXObjectCache());
  ASSERT_NE(cache, nullptr);

  Element* bg_button = GetElementById("bg_button");
  Element* modal_button = GetElementById("modal_button");
  ASSERT_NE(bg_button, nullptr);
  ASSERT_NE(modal_button, nullptr);

  // Focus node inside the modal dialog.
  cache->HandleFocusedUIElementChanged(nullptr, modal_button);
  EXPECT_EQ(cache->GetActiveAriaModalDialog(), GetElementById("dialog"));

  // Move focus outside the modal dialog to the background button.
  cache->HandleFocusedUIElementChanged(modal_button, bg_button);

  // Verify active_aria_modal_dialog_ is cleared when focus moves to background.
  EXPECT_EQ(cache->GetActiveAriaModalDialog(), nullptr);
}

TEST_F(AccessibilityTest, AriaModalClearsWhenModalBecomesDisplayNone) {
  GetDocument().GetSettings()->SetAriaModalPrunesAXTree(true);

  SetBodyInnerHTML(R"HTML(
    <div id="dialog" role="dialog" aria-modal="true">
      <button id="modal_button">Modal Button</button>
    </div>
  )HTML");

  AXObjectCacheImpl* cache =
      To<AXObjectCacheImpl>(GetDocument().ExistingAXObjectCache());
  ASSERT_NE(cache, nullptr);

  Element* dialog = GetElementById("dialog");
  Element* modal_button = GetElementById("modal_button");
  ASSERT_NE(dialog, nullptr);
  ASSERT_NE(modal_button, nullptr);

  cache->HandleFocusedUIElementChanged(nullptr, modal_button);
  EXPECT_EQ(cache->GetActiveAriaModalDialog(), dialog);

  // Hide the dialog via display: none.
  dialog->setAttribute(html_names::kStyleAttr, AtomicString("display: none;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // Trigger update.
  cache->HandleFocusedUIElementChanged(modal_button, GetDocument().body());

  // Verify active_aria_modal_dialog_ is cleared when dialog is display: none.
  EXPECT_EQ(cache->GetActiveAriaModalDialog(), nullptr);
}

TEST_F(AccessibilityTest, AriaModalClearsWhenAttributeChangesToFalse) {
  GetDocument().GetSettings()->SetAriaModalPrunesAXTree(true);

  SetBodyInnerHTML(R"HTML(
    <div id="dialog" role="dialog" aria-modal="true">
      <button id="modal_button">Modal Button</button>
    </div>
  )HTML");

  AXObjectCacheImpl* cache =
      To<AXObjectCacheImpl>(GetDocument().ExistingAXObjectCache());
  ASSERT_NE(cache, nullptr);

  Element* dialog = GetElementById("dialog");
  Element* modal_button = GetElementById("modal_button");
  ASSERT_NE(dialog, nullptr);
  ASSERT_NE(modal_button, nullptr);

  cache->HandleFocusedUIElementChanged(nullptr, modal_button);
  EXPECT_EQ(cache->GetActiveAriaModalDialog(), dialog);

  // Change aria-modal attribute to false.
  dialog->setAttribute(html_names::kAriaModalAttr, AtomicString("false"));

  // Trigger update.
  cache->HandleFocusedUIElementChanged(modal_button, GetDocument().body());

  // Verify active_aria_modal_dialog_ is cleared when aria-modal="false".
  EXPECT_EQ(cache->GetActiveAriaModalDialog(), nullptr);
}

TEST_F(AccessibilityTest, AriaModalClearsWhenAriaHiddenSetToTrue) {
  GetDocument().GetSettings()->SetAriaModalPrunesAXTree(true);

  SetBodyInnerHTML(R"HTML(
    <div id="dialog" role="dialog" aria-modal="true">
      <button id="modal_button">Modal Button</button>
    </div>
  )HTML");

  AXObjectCacheImpl* cache =
      To<AXObjectCacheImpl>(GetDocument().ExistingAXObjectCache());
  ASSERT_NE(cache, nullptr);

  Element* dialog = GetElementById("dialog");
  Element* modal_button = GetElementById("modal_button");
  ASSERT_NE(dialog, nullptr);
  ASSERT_NE(modal_button, nullptr);

  cache->HandleFocusedUIElementChanged(nullptr, modal_button);
  EXPECT_EQ(cache->GetActiveAriaModalDialog(), dialog);

  // Set aria-hidden="true" on the dialog.
  dialog->setAttribute(html_names::kAriaHiddenAttr, AtomicString("true"));

  // Trigger update.
  cache->HandleFocusedUIElementChanged(modal_button, GetDocument().body());

  // Verify active_aria_modal_dialog_ is cleared when dialog is
  // aria-hidden="true".
  EXPECT_EQ(cache->GetActiveAriaModalDialog(), nullptr);
}

TEST_F(AccessibilityTest, AriaModalRetainsActiveStateWhenFocusBecomesNull) {
  GetDocument().GetSettings()->SetAriaModalPrunesAXTree(true);

  SetBodyInnerHTML(R"HTML(
    <button id="bg_button">Background Button</button>
    <div id="dialog" role="dialog" aria-modal="true">
      <button id="modal_button">Modal Button</button>
    </div>
  )HTML");

  AXObjectCacheImpl* cache =
      To<AXObjectCacheImpl>(GetDocument().ExistingAXObjectCache());
  ASSERT_NE(cache, nullptr);

  Element* modal_button = GetElementById("modal_button");
  ASSERT_NE(modal_button, nullptr);

  // Focus node inside the modal dialog.
  cache->HandleFocusedUIElementChanged(nullptr, modal_button);
  EXPECT_EQ(cache->GetActiveAriaModalDialog(), GetElementById("dialog"));

  // Move focus to nullptr (e.g. focus cleared / lost / window blurred).
  cache->HandleFocusedUIElementChanged(modal_button, nullptr);

  // Verify active_aria_modal_dialog_ remains set when focus is null.
  EXPECT_EQ(cache->GetActiveAriaModalDialog(), GetElementById("dialog"));
}

TEST_F(AccessibilityTest,
       AriaModalRetainsActiveStateWhenFocusMovesToDialogContainer) {
  GetDocument().GetSettings()->SetAriaModalPrunesAXTree(true);

  SetBodyInnerHTML(R"HTML(
    <button id="bg_button">Background Button</button>
    <div id="dialog" role="dialog" aria-modal="true" tabindex="-1">
      <button id="modal_button">Modal Button</button>
    </div>
  )HTML");

  AXObjectCacheImpl* cache =
      To<AXObjectCacheImpl>(GetDocument().ExistingAXObjectCache());
  ASSERT_NE(cache, nullptr);

  Element* dialog = GetElementById("dialog");
  Element* modal_button = GetElementById("modal_button");
  ASSERT_NE(dialog, nullptr);
  ASSERT_NE(modal_button, nullptr);

  // 1. Initial focus directly on dialog container before any descendant focus
  // -> no pruning.
  cache->HandleFocusedUIElementChanged(nullptr, dialog);
  EXPECT_EQ(cache->GetActiveAriaModalDialog(), nullptr);

  // 2. Focus descendant inside dialog -> pruning activated.
  cache->HandleFocusedUIElementChanged(dialog, modal_button);
  EXPECT_EQ(cache->GetActiveAriaModalDialog(), dialog);

  // 3. Move focus back to dialog container itself -> pruning remains active.
  cache->HandleFocusedUIElementChanged(modal_button, dialog);
  EXPECT_EQ(cache->GetActiveAriaModalDialog(), dialog);
}

TEST_F(AccessibilityTest, AriaOwnsWithUnmappedOrDetachedChildDoesNotCrash) {
  // Test case based on ClusterFuzz issue 521081377 reproducer.
  SetBodyInnerHTML(R"HTML(
    <select></textarea><div aria-owns="z1 z2">
      <select><<drow id="z1"><applet>>
      <div id="z2"><
  )HTML");
  // Simply accessing AXObjectCache to process clean layout/aria-owns should not
  // crash.
  GetAXObjectCache().UpdateAXForAllDocuments();
}

}  // namespace blink
