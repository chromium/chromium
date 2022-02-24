// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_HAS_INVALIDATION_FLAGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_HAS_INVALIDATION_FLAGS_H_

namespace blink {

// Flags for :has() invalidation.
//
// - affected_by_non_subject_has :
//     Indicates that this element may match a non-subject :has() selector,
//     which means we need to schedule descendant and sibling invalidation
//     sets on this element when the :has() state changes.
//
// - ancestors_or_ancestor_siblings_affected_by_has
//     Indicates that this element possibly matches any of the :has()
//     subselectors, and we need to traverse ancestors or siblings of ancestors
//     to find the elements affected by subject or non-subject :has() state
//     change.
//
// - siblings_affected_by_has
//     Indicates that this element possibly matches any of the :has()
//     subselectors, and we need to traverse siblings to find the elements
//     affected by subject or non-subject :has() state change.
//
// Example 1) Subject :has() (has only descendant relationship)
//  <style> .a:has(.b) {...} </style>
//  <div>
//    <div class=a>  <!-- AffectedBySubjectHas (computed style extra flag) -->
//      <div>           <!-- AncestorsOrAncestorSiblingsAffectedByHas -->
//        <div></div>   <!-- AncestorsOrAncestorSiblingsAffectedByHas -->
//        <div></div>   <!-- AncestorsOrAncestorSiblingsAffectedByHas -->
//      </div>
//    </div>
//  </div>
//
//
// Example 2) Non-subject :has()
//  <style> .a:has(.b) .c {...} </style>
//  <div>
//    <div class=a>          <!-- AffectedByNonSubjectHas -->
//      <div>                <!-- AncestorsOrAncestorSiblingsAffectedByHas -->
//        <div></div>        <!-- AncestorsOrAncestorSiblingsAffectedByHas -->
//        <div class=c></div><!-- AncestorsOrAncestorSiblingsAffectedByHas -->
//      </div>
//    </div>
//  </div>
//
//
// Example 3) Subject :has() (has only sibling relationship)
//  <style> .a:has(~ .b) {...} </style>
//  <div>
//    <div></div>
//    <div class=a>  <!-- AffectedBySubjectHas (computed style extra flag) -->
//      <div></div>
//    </div>
//    <div></div>    <!-- SiblingsAffectedByHas -->
//    <div></div>    <!-- SiblingsAffectedByHas -->
//  </div>
//
//
// Example 4) Subject :has() (has both sibling and descendant relationship)
//  <style> .a:has(~ .b .c) {...} </style>
//  <div>
//    <div></div>
//    <div class=a>  <!-- AffectedBySubjectHas (computed style extra flag) -->
//    </div>
//    <div>          <!-- SiblingsAffectedByHas -->
//      <div></div>  <!-- AncestorsOrAncestorSiblingsAffectedByHas -->
//      <div></div>  <!-- AncestorsOrAncestorSiblingsAffectedByHas -->
//    </div>
//  </div>
struct HasInvalidationFlags {
  unsigned affected_by_non_subject_has : 1;
  unsigned ancestors_or_ancestor_siblings_affected_by_has : 1;
  unsigned siblings_affected_by_has : 1;

  // Dynamic restyle flags for :has()
  unsigned affected_by_pseudos_in_has : 1;
  unsigned ancestors_or_siblings_affected_by_hover_in_has : 1;
  unsigned ancestors_or_siblings_affected_by_active_in_has : 1;
  unsigned ancestors_or_siblings_affected_by_focus_in_has : 1;
  unsigned ancestors_or_siblings_affected_by_focus_visible_in_has : 1;

  HasInvalidationFlags()
      : affected_by_non_subject_has(false),
        ancestors_or_ancestor_siblings_affected_by_has(false),
        siblings_affected_by_has(false),
        affected_by_pseudos_in_has(false),
        ancestors_or_siblings_affected_by_hover_in_has(false),
        ancestors_or_siblings_affected_by_active_in_has(false),
        ancestors_or_siblings_affected_by_focus_in_has(false),
        ancestors_or_siblings_affected_by_focus_visible_in_has(false) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_HAS_INVALIDATION_FLAGS_H_
