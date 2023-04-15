// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/css_toggle_inference.h"

#include <ostream>
#include <utility>

#include "base/check.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/dom/css_toggle.h"
#include "third_party/blink/renderer/core/dom/css_toggle_map.h"
#include "third_party/blink/renderer/core/dom/css_toggle_traversal.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

void CSSToggleInference::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(element_data_);
}

void CSSToggleInference::RebuildIfNeeded() {
  if (needs_rebuild_) {
    needs_rebuild_ = false;
    Rebuild();
  }
  DCHECK(!needs_rebuild_);
}

namespace {

// Check the root+trigger condition for whether something is tab-ish,
// but not the toggle-visibility on a sibling part.
bool MightBeTabIsh(Element* toggle_root) {
  DCHECK(toggle_root->GetToggleMap());
  const ComputedStyle* root_style = toggle_root->GetComputedStyle();
  if (!root_style) {
    return false;
  }
  const ToggleTriggerList* triggers = root_style->ToggleTrigger();
  if (!triggers) {
    return false;
  }

  for (const auto& [toggle_name, toggle] :
       toggle_root->GetToggleMap()->Toggles()) {
    for (const ToggleTrigger& trigger : triggers->Triggers()) {
      if (trigger.Name() == toggle_name) {
        return true;
      }
    }
  }
  return false;
}

absl::optional<AtomicString> IsTabsIsh(Element* container) {
  const ComputedStyle* container_style = container->GetComputedStyle();
  DCHECK(container_style);
  const ToggleGroupList* container_groups = container_style->ToggleGroup();
  DCHECK(container_groups);

  for (const ToggleGroup& toggle_group : container_groups->Groups()) {
    unsigned child_count = 0;
    unsigned child_tab_count = 0;
    unsigned child_panel_count = 0;
    for (Element& child : ElementTraversal::ChildrenOf(*container)) {
      const ComputedStyle* child_style = child.GetComputedStyle();
      if (!child_style) {
        continue;
      }
      ++child_count;

      bool has_trigger = false;
      const ToggleTriggerList* child_triggers = child_style->ToggleTrigger();
      const ToggleRootList* child_roots = child_style->ToggleRoot();
      if (child_triggers && child_roots) {
        for (const ToggleTrigger& child_trigger : child_triggers->Triggers()) {
          if (child_trigger.Name() == toggle_group.Name()) {
            for (const ToggleRoot& child_root : child_roots->Roots()) {
              if (child_root.Name() == toggle_group.Name() &&
                  child_root.IsGroup()) {
                has_trigger = true;
                break;
              }
            }
            if (has_trigger) {
              break;
            }
          }
        }
      }

      bool has_visibility =
          child_style->ToggleVisibility() == toggle_group.Name();

      if (has_trigger ^ has_visibility) {
        if (has_trigger) {
          ++child_tab_count;
        } else {
          DCHECK(has_visibility);
          ++child_panel_count;
        }
      }
    }
    // TODO(https://crbug.com/1250716): Should we have a condition that
    // the number of tabs and panels are similar or equal?
    if (child_tab_count > 0 && child_panel_count > 0 &&
        (child_tab_count + child_panel_count) * 2 > child_count) {
      return toggle_group.Name();
    }
  }
  return absl::nullopt;
}

struct DisclosureIshInfo {
  STACK_ALLOCATED();

 public:
  AtomicString toggle_name;
  Element* trigger_element;
};

// Check whether something is disclosure-ish.  Assumes that
// MightBeTabIsh was already called and false.  Returns nullopt to
// indicate that it's not disclosure-ish, or info if it is.
absl::optional<DisclosureIshInfo> IsDisclosureIsh(Element* toggle_root) {
  DCHECK(toggle_root->GetToggleMap());
  for (const auto& [toggle_name, toggle] :
       toggle_root->GetToggleMap()->Toggles()) {
    // We want to find out if we have exactly one descendant with
    // toggle-trigger (for this name) and exactly one descendant with
    // toggle-visibility (for this name), and those descendants are
    // different.
    Element* trigger_element = nullptr;
    bool found_visibility = false;
    bool found_duplicate_or_same_descendant = false;

    for (Element* e :
         CSSToggleScopeRange(toggle_root, toggle_name, toggle->Scope())) {
      const ComputedStyle* style = e->GetComputedStyle();
      if (!style) {
        continue;
      }

      bool e_has_trigger = false;
      if (const ToggleTriggerList* triggers = style->ToggleTrigger()) {
        for (const ToggleTrigger& trigger : triggers->Triggers()) {
          if (trigger.Name() == toggle_name) {
            e_has_trigger = true;
            break;
          }
        }
      }
      bool e_has_visibility = style->ToggleVisibility() == toggle_name;

      if (e_has_trigger && e_has_visibility) {
        found_duplicate_or_same_descendant = true;
        break;
      }

      if (e_has_trigger) {
        if (trigger_element) {
          found_duplicate_or_same_descendant = true;
          break;
        }
        trigger_element = e;
      }

      if (e_has_visibility) {
        if (found_visibility) {
          found_duplicate_or_same_descendant = true;
          break;
        }
        found_visibility = true;
      }
    }

    if (found_duplicate_or_same_descendant) {
      continue;
    }
    if (trigger_element && found_visibility) {
      return DisclosureIshInfo({toggle_name, trigger_element});
    }
  }
  return absl::nullopt;
}

bool IsAccordionIsh(
    Element* container,
    const HeapHashMap<Member<Element>, AtomicString>& disclosure_ish_elements) {
  unsigned child_element_count = 0;
  unsigned child_disclosure_ish_count = 0;
  for (Element& child : ElementTraversal::ChildrenOf(*container)) {
    ++child_element_count;
    if (disclosure_ish_elements.Contains(&child)) {
      ++child_disclosure_ish_count;
    }
  }

  // Define an element as accordion-ish if more than half of its
  // children are disclosure-ish, and there are at least 2
  // disclosure-ish children.
  return child_disclosure_ish_count >= 2 &&
         child_disclosure_ish_count * 2 > child_element_count;
}

}  // namespace

void CSSToggleInference::Rebuild() {
  DCHECK(!needs_rebuild_) << "Rebuild should be called via RebuildIfNeeded";

  element_data_.clear();

  const HeapHashSet<WeakMember<Element>> elements_with_toggles =
      document_->ElementsWithCSSToggles();

  // TODO(https://crbug.com/1250716): There are various things here that
  // count siblings and have thresholds at 50%+1.  We don't currently
  // invalidate (i.e., set needs_rebuild_) for many of the cases that
  // could change the result of these checks, since we only invalidate
  // in response to changes on things that have toggles.
  //
  // In fact, there are probably other conditions that we don't properly
  // invalidate for as well.  Before this code is every used for
  // something we ship, we should verify that all the conditions tested
  // here are properly invalidated.

  // Build the set of disclosure-ish elements (and retain the toggle
  // names associated with them, and the triggers, in two separate hash
  // maps (for now) so that we don't have to make a new garbage
  // collected struct).  Simultaneously build a too-broad set of
  // tabs-ish elements that we will reduce later.
  HeapHashMap<Member<Element>, AtomicString> disclosure_ish_elements;
  HeapHashMap<Member<Element>, Member<Element>> disclosure_ish_element_triggers;
  HeapHashSet<Member<Element>> maybe_tabs_ish_elements;
  for (Element* toggle_root : elements_with_toggles) {
    DCHECK(!disclosure_ish_elements.Contains(toggle_root));
    if (MightBeTabIsh(toggle_root)) {
      Element* parent = toggle_root->parentElement();
      if (parent) {
        const ComputedStyle* parent_style = parent->GetComputedStyle();
        if (parent_style && parent_style->ToggleGroup()) {
          maybe_tabs_ish_elements.insert(parent);
        }
      }
    } else if (absl::optional<DisclosureIshInfo> info_option =
                   IsDisclosureIsh(toggle_root)) {
      disclosure_ish_elements.insert(toggle_root, info_option->toggle_name);
      disclosure_ish_element_triggers.insert(toggle_root,
                                             info_option->trigger_element);
    }
  }

  // Build the set of accordion-ish elements by adding the parent of
  // everything disclosure-ish, and then removing those that are not
  // accordion-ish.  (This is a convenient way to test each parent once,
  // even when it has multiple disclosure-ish children.)
  HeapHashSet<Member<Element>> accordion_ish_elements;
  for (Element* disclosure_ish : disclosure_ish_elements.Keys()) {
    Element* parent = disclosure_ish->parentElement();
    if (parent) {
      accordion_ish_elements.insert(parent);
    }
  }
  {
    HeapHashSet<Member<Element>> trimmed_accordion_ish_elements;
    for (Element* e : accordion_ish_elements) {
      if (IsAccordionIsh(e, disclosure_ish_elements)) {
        trimmed_accordion_ish_elements.insert(e);
      }
    }
    std::swap(accordion_ish_elements, trimmed_accordion_ish_elements);
  }

  // Reduce the set of tabs-ish elements and record the toggle names.
  HeapHashMap<Member<Element>, AtomicString> tabs_ish_elements;
  for (Element* e : maybe_tabs_ish_elements) {
    absl::optional<AtomicString> tabs_toggle_name = IsTabsIsh(e);
    if (tabs_toggle_name) {
      tabs_ish_elements.insert(e, *tabs_toggle_name);
    }
  }

  // Now go through and assign the roles.
  for (Element* toggle_root : elements_with_toggles) {
    bool find_trigger_and_make_it_a_button = false;

    Element* parent = toggle_root->parentElement();
    const auto disclosure_ish_iter = disclosure_ish_elements.find(toggle_root);
    if (disclosure_ish_iter != disclosure_ish_elements.end()) {
      const AtomicString& toggle_name = disclosure_ish_iter->value;
      if (parent && accordion_ish_elements.Contains(parent)) {
        CSSToggleRole parent_role;
        {  // scope for lifetime of add_result
          auto add_result = element_data_.insert(
              parent, ElementData{CSSToggleRole::kNone, g_null_atom});
          // We might have already handled parent as the parent of a
          // different one of its children.
          if (add_result.is_new_entry) {
            // Determine whether we're looking at accordion or tree.
            //
            // Do this by walking the scopes of the children that are
            // disclosure-ish, and re-examining their panels (the elements
            // with toggle-visibility).
            unsigned panel_count = 0;
            unsigned accordion_ish_panel_count = 0;
            // For accordions, we want to check that the disclosure-ish
            // child actually has the same toggle name as the group
            // established by the accordion-ish element.  So we maintain
            // separate counts for each toggle name.
            HashMap<AtomicString, unsigned> panel_with_toggle_group_counts;
            for (Element& child : ElementTraversal::ChildrenOf(*parent)) {
              if (disclosure_ish_elements.Contains(&child)) {
                DCHECK(child.GetToggleMap());
                for (const auto& [child_toggle_name, toggle] :
                     child.GetToggleMap()->Toggles()) {
                  for (Element* e : CSSToggleScopeRange(
                           &child, child_toggle_name, toggle->Scope())) {
                    const ComputedStyle* e_style = e->GetComputedStyle();
                    if (!e_style) {
                      continue;
                    }
                    if (e_style->ToggleVisibility() == child_toggle_name) {
                      ++panel_count;
                      if (toggle->IsGroup()) {
                        auto count_add_result =
                            panel_with_toggle_group_counts.insert(
                                child_toggle_name, 0);
                        DCHECK(count_add_result.is_new_entry ==
                               (count_add_result.stored_value->value == 0));
                        ++count_add_result.stored_value->value;
                      } else {
                        if (accordion_ish_elements.Contains(e)) {
                          ++accordion_ish_panel_count;
                        }
                      }
                    }
                  }
                }
              }
            }
            DCHECK_GE(panel_count, 2u) << "unexpect IsAccordionIsh result";
            bool enough_accordions =
                accordion_ish_panel_count * 2 > panel_count;
            bool enough_toggle_groups = false;
            AtomicString toggle_group_name = g_null_atom;
            if (const ComputedStyle* parent_style =
                    parent->GetComputedStyle()) {
              if (const ToggleGroupList* parent_groups =
                      parent_style->ToggleGroup()) {
                for (const ToggleGroup& group : parent_groups->Groups()) {
                  auto iter = panel_with_toggle_group_counts.find(group.Name());
                  if (iter != panel_with_toggle_group_counts.end()) {
                    if (iter->value * 2 > panel_count) {
                      enough_toggle_groups = true;
                      toggle_group_name = group.Name();
                    }
                  }
                }
              }
            }
            if (enough_accordions ^ enough_toggle_groups) {
              if (enough_accordions) {
                DCHECK(!enough_toggle_groups);
                add_result.stored_value->value =
                    ElementData{CSSToggleRole::kTree, g_null_atom};
              } else {
                DCHECK(enough_toggle_groups);
                DCHECK_NE(toggle_group_name, g_null_atom);
                add_result.stored_value->value =
                    ElementData{CSSToggleRole::kAccordion, toggle_group_name};
              }
            } else {
              // TODO(https://crbug.com/1250716): For now, we don't know
              // what this is, but maybe we could do better.
              add_result.stored_value->value =
                  ElementData{CSSToggleRole::kNone, g_null_atom};
            }
          }
          parent_role = add_result.stored_value->value.role;
        }
        switch (parent_role) {
          case CSSToggleRole::kTree:
          case CSSToggleRole::kTreeGroup: {
            // TODO(https://crbug.com/1250716): This role assignment is
            // likely wrong!  Need to examine closely!
            Element* trigger_element =
                disclosure_ish_element_triggers.at(toggle_root);
            element_data_.insert(
                toggle_root,
                ElementData{CSSToggleRole::kTreeGroup, toggle_name});
            // TODO(https://crbug.com/1250716): What if the trigger is
            // just part of the tree item?
            element_data_.insert(
                trigger_element,
                ElementData{CSSToggleRole::kTreeItem, toggle_name});
            break;
          }
          case CSSToggleRole::kAccordion: {
            Element* trigger_element =
                disclosure_ish_element_triggers.at(toggle_root);
            element_data_.insert(
                toggle_root,
                ElementData{CSSToggleRole::kAccordionItem, toggle_name});
            element_data_.insert(
                trigger_element,
                ElementData{CSSToggleRole::kAccordionItemButton, toggle_name});
            break;
          }
          case CSSToggleRole::kNone:
            find_trigger_and_make_it_a_button = true;
            break;
          default:
            // This could happen if some other part of the assignment
            // logic touches it.
            // TODO(https://crbug.com/1250716): Do we want to do this?
            find_trigger_and_make_it_a_button = true;
            break;
        }
      } else {
        DCHECK(toggle_root->GetToggleMap());
        CSSToggle* toggle =
            toggle_root->GetToggleMap()->Toggles().at(toggle_name);
        DCHECK(toggle);
        if (toggle->IsGroup()) {
          // This is not a detectable pattern.
          find_trigger_and_make_it_a_button = true;
        } else {
          // Find the panel (which we previously found when marking the
          // element as disclosure-ish) to determine whether this is a
          // popup or disclosure.
          for (Element* e :
               CSSToggleScopeRange(toggle_root, toggle_name, toggle->Scope())) {
            const ComputedStyle* e_style = e->GetComputedStyle();
            if (e_style && e_style->ToggleVisibility() == toggle_name) {
              bool positioned_or_popover = e_style->HasOutOfFlowPosition();
              if (!positioned_or_popover) {
                HTMLElement* e_html = DynamicTo<HTMLElement>(e);
                positioned_or_popover =
                    e_html && e_html->PopoverType() != PopoverValueType::kNone;
              }
              Element* trigger_element =
                  disclosure_ish_element_triggers.at(toggle_root);
              if (positioned_or_popover) {
                element_data_.insert(
                    trigger_element,
                    ElementData{CSSToggleRole::kButtonWithPopup, toggle_name});
              } else {
                element_data_.insert(
                    toggle_root,
                    ElementData{CSSToggleRole::kDisclosure, toggle_name});
                element_data_.insert(
                    trigger_element,
                    ElementData{CSSToggleRole::kDisclosureButton, toggle_name});
              }
              break;
            }
          }
        }
      }
    } else if (CSSToggle* tabs_ish_toggle = [&]() -> CSSToggle* {
                 if (!parent) {
                   return nullptr;
                 }
                 auto elements_iter = tabs_ish_elements.find(parent);
                 if (elements_iter == tabs_ish_elements.end()) {
                   return nullptr;
                 }
                 AtomicString toggle_name = elements_iter->value;
                 DCHECK(toggle_root->GetToggleMap());
                 ToggleMap& map = toggle_root->GetToggleMap()->Toggles();
                 auto toggles_iter = map.find(toggle_name);
                 if (toggles_iter == map.end()) {
                   return nullptr;
                 }
                 CSSToggle* toggle = toggles_iter->value;
                 DCHECK(toggle);
                 DCHECK_EQ(toggle->Name(), toggle_name);
                 return toggle;
               }()) {
      element_data_.insert(parent, ElementData{CSSToggleRole::kTabContainer,
                                               tabs_ish_toggle->Name()});
      element_data_.insert(toggle_root, ElementData{CSSToggleRole::kTab,
                                                    tabs_ish_toggle->Name()});

      for (Element* e :
           CSSToggleScopeRange(toggle_root, tabs_ish_toggle->Name(),
                               tabs_ish_toggle->Scope())) {
        const ComputedStyle* e_style = e->GetComputedStyle();
        if (e_style && e_style->ToggleVisibility() == tabs_ish_toggle->Name()) {
          element_data_.insert(e, ElementData{CSSToggleRole::kTabPanel,
                                              tabs_ish_toggle->Name()});
        }
      }
    } else {
      CSSToggleMap* toggle_map = toggle_root->GetToggleMap();
      DCHECK(toggle_map);
      const CSSToggle* toggle_with_trigger = nullptr;
      if (const ComputedStyle* style = toggle_root->GetComputedStyle()) {
        for (const auto& [toggle_name, toggle] : toggle_map->Toggles()) {
          if (const ToggleTriggerList* triggers = style->ToggleTrigger()) {
            for (const ToggleTrigger& trigger : triggers->Triggers()) {
              if (toggle_name == trigger.Name()) {
                DCHECK_EQ(toggle_name, toggle->Name());
                toggle_with_trigger = toggle;
                break;
              }
            }
          }
          if (toggle_with_trigger) {
            break;
          }
        }
      }

      if (toggle_with_trigger) {
        bool found_visibility = false;
        const AtomicString& toggle_name = toggle_with_trigger->Name();
        for (Element* e : CSSToggleScopeRange(toggle_root, toggle_name,
                                              toggle_with_trigger->Scope())) {
          const ComputedStyle* e_style = e->GetComputedStyle();
          if (e_style && e_style->ToggleVisibility() == toggle_name) {
            found_visibility = true;
            break;
          }
        }

        CSSToggleRole parent_role = CSSToggleRole::kNone;
        if (found_visibility) {
          // TODO(https://crbug.com/1250716): The current spec draft
          // says that this should be "no pattern detected", but it
          // seems bad to fail to report something.  I'm going to report
          // button instead.
          element_data_.insert(
              toggle_root, ElementData{CSSToggleRole::kButton, toggle_name});
        } else if (toggle_with_trigger->IsGroup()) {
          // TODO(https://crbug.com/1250716): Detecting this pattern as
          // radio item and radio group is not quite right since the
          // underlying toggle behavior here doesn't match the normal
          // interaction of radios.  In particular, this toggle pattern
          // allows users to unselect a selected radio item by clicking
          // on it.  The ability to do this leads to a keyboard
          // interaction behavior that also doesn't really match the
          // normal interaction for radios (where focus and selection
          // are the same whenever focused, with the sole exception of
          // focusing into a radio group with no current selection).
          // It's not entirely clear if we should change the detection
          // behavior here or change the keyboard interactions to match
          // normal radio behavior.  (Changing the keyboard interactions
          // has the disadvantage that it would then leave keyboard
          // users without the ability to do something that remains
          // possible with the mouse.)
          element_data_.insert(
              toggle_root, ElementData{CSSToggleRole::kRadioItem, toggle_name});
          if (const ComputedStyle* parent_style = parent->GetComputedStyle()) {
            if (const ToggleGroupList* parent_groups =
                    parent_style->ToggleGroup()) {
              for (const ToggleGroup& group : parent_groups->Groups()) {
                if (toggle_name == group.Name()) {
                  parent_role = CSSToggleRole::kRadioGroup;
                  break;
                }
              }
            }
          }
        } else {
          // TODO(https://crbug.com/1250716): Maybe this should be
          // Switch, depending on device.
          element_data_.insert(
              toggle_root, ElementData{CSSToggleRole::kCheckbox, toggle_name});
          // TODO(https://crbug.com/1250716): We should only set
          // parent_role here when there are multiple checkbox siblings
          // with few non-checkbox siblings!  (Once we do this we can
          // probably remove the tabs_ish_elements.Contains(parent) test
          // below.)
          parent_role = CSSToggleRole::kCheckboxGroup;
        }

        // TODO(https://crbug.com/1250716): Figure out if we need to
        // exclude additional cases where the parent would be assigned a
        // different role (in order to ensure that hash map processing
        // order doesn't affect the result).
        if (parent_role != CSSToggleRole::kNone && parent &&
            !accordion_ish_elements.Contains(parent) &&
            !tabs_ish_elements.Contains(parent)) {
          ElementData data{parent_role, toggle_name};
          auto parent_add_result = element_data_.insert(parent, data);
          // prefer checkbox group to radio group if some children
          // lead to either
          if (parent_add_result.stored_value->value.role != parent_role &&
              parent_add_result.stored_value->value.role ==
                  CSSToggleRole::kRadioGroup) {
            parent_add_result.stored_value->value = data;
          }
        }
      } else {
        find_trigger_and_make_it_a_button = true;
      }
    }

    if (find_trigger_and_make_it_a_button) {
      CSSToggleMap* toggle_map = toggle_root->GetToggleMap();
      DCHECK(toggle_map);
      for (const auto& [toggle_name, toggle] : toggle_map->Toggles()) {
        for (Element* e :
             CSSToggleScopeRange(toggle_root, toggle_name, toggle->Scope())) {
          if (const ComputedStyle* e_style = e->GetComputedStyle()) {
            if (const ToggleTriggerList* triggers = e_style->ToggleTrigger()) {
              for (const ToggleTrigger& trigger : triggers->Triggers()) {
                if (trigger.Name() == toggle_name) {
                  element_data_.insert(
                      e, ElementData{CSSToggleRole::kButton, toggle_name});
                  break;
                }
              }
            }
          }
        }
      }
    }
  }
}

CSSToggleRole CSSToggleInference::RoleForElement(
    const blink::Element* element) {
  RebuildIfNeeded();

  auto iter = element_data_.find(element);
  if (iter == element_data_.end()) {
    return CSSToggleRole::kNone;
  }

  return iter->value.role;
}

AtomicString CSSToggleInference::ToggleNameForElement(
    const blink::Element* element) {
  RebuildIfNeeded();

  auto iter = element_data_.find(element);
  if (iter == element_data_.end()) {
    return g_null_atom;
  }

  return iter->value.toggle_name;
}

}  // namespace blink
