/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_FORM_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_FORM_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/radio_button_group_scope.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/loader/form_submission.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

namespace blink {

class DOMTokenList;
class Event;
class HTMLFormControlElement;
class HTMLFormControlsCollection;
class HTMLImageElement;
class ListedElement;
class RelList;
class V8UnionElementOrRadioNodeList;

class CORE_EXPORT HTMLFormElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum RelAttribute {
    kNone = 0,
    kNoReferrer = 1 << 0,
    kNoOpener = 1 << 1,
    kOpener = 1 << 2,
  };

  explicit HTMLFormElement(Document&);
  ~HTMLFormElement() override;
  void Trace(Visitor*) const override;

  HTMLFormControlsCollection* elements();
  void GetNamedElements(const AtomicString&, HeapVector<Member<Element>>&);

  unsigned length() const;
  HTMLElement* item(unsigned index);

  String action() const;
  void setAction(const AtomicString&);

  String enctype() const { return attributes_.EncodingType(); }
  void setEnctype(const AtomicString&);

  String encoding() const { return attributes_.EncodingType(); }
  void setEncoding(const AtomicString& value) { setEnctype(value); }

  DOMTokenList& relList() const;

  bool HasRel(RelAttribute relation) const;

  bool ShouldAutocomplete() const;

  void Associate(ListedElement&);
  void Disassociate(ListedElement&);
  void Associate(HTMLImageElement&);
  void Disassociate(HTMLImageElement&);
  void DidAssociateByParser();

  void PrepareForSubmission(const Event*,
                            HTMLFormControlElement* submit_button);
  void submitFromJavaScript();
  void requestSubmit(ExceptionState& exception_state);
  void requestSubmit(HTMLElement* submitter, ExceptionState& exception_state);
  void reset();

  void AttachLayoutTree(AttachContext& context) override;
  void DetachLayoutTree(bool performing_reattach) override;

  void SubmitImplicitly(const Event&, bool from_implicit_submission_trigger);

  String GetName() const;

  bool NoValidate() const;

  const AtomicString& Action() const;

  String method() const;
  void setMethod(const AtomicString&);
  FormSubmission::SubmitMethod Method() const { return attributes_.Method(); }

  // Find the 'default button.'
  // https://html.spec.whatwg.org/C/#default-button
  HTMLFormControlElement* FindDefaultButton() const;

  bool checkValidity();
  bool reportValidity();
  bool MatchesValidityPseudoClasses() const final;
  bool IsValidElement() final;

  RadioButtonGroupScope& GetRadioButtonGroupScope() {
    return radio_button_group_scope_;
  }

  const ListedElement::List& ListedElements(
      bool include_shadow_trees = false) const;
  const HeapVector<Member<HTMLImageElement>>& ImageElements();

  V8UnionElementOrRadioNodeList* AnonymousNamedGetter(const AtomicString& name);
  void InvalidateDefaultButtonStyle() const;

  // 'construct the entry list'
  // https://html.spec.whatwg.org/C/#constructing-the-form-data-set
  // Returns nullptr if this form is already running this function.
  FormData* ConstructEntryList(HTMLFormControlElement* submit_button,
                               const WTF::TextEncoding& encoding);

  uint64_t UniqueRendererFormId() const { return unique_renderer_form_id_; }

  void InvalidateListedElementsIncludingShadowTrees();

 private:
  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;
  void FinishParsingChildren() override;

  void HandleLocalEvents(Event&) override;

  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsURLAttribute(const Attribute&) const override;
  bool HasLegalLinkAttribute(const QualifiedName&) const override;

  NamedItemType GetNamedItemType() const override {
    return NamedItemType::kName;
  }

  void SubmitDialog(FormSubmission*);
  void ScheduleFormSubmission(const Event*,
                              HTMLFormControlElement* submit_button);

  void CollectListedElements(
      const Node& root,
      ListedElement::List& elements,
      ListedElement::List* elements_including_shadow_trees = nullptr,
      bool in_shadow_tree = false) const;
  void CollectImageElements(Node& root, HeapVector<Member<HTMLImageElement>>&);

  // Returns true if the submission should proceed.
  bool ValidateInteractively();

  // Validates each of the controls, and stores controls of which 'invalid'
  // event was not canceled to the specified vector. Returns true if there
  // are any invalid controls in this form.
  bool CheckInvalidControlsAndCollectUnhandled(ListedElement::List*);

  Element* ElementFromPastNamesMap(const AtomicString&);
  void AddToPastNamesMap(Element*, const AtomicString& past_name);
  void RemoveFromPastNamesMap(HTMLElement&);

  typedef HeapHashMap<AtomicString, Member<Element>> PastNamesMap;

  FormSubmission::Attributes attributes_;
  Member<PastNamesMap> past_names_map_;

  RadioButtonGroupScope radio_button_group_scope_;

  // Do not access listed_elements_ directly. Use ListedElements() instead.
  ListedElement::List listed_elements_;
  // Do not access listed_elements_including_shadow_trees_ directly. Use
  // ListedElements(true) instead.
  ListedElement::List listed_elements_including_shadow_trees_;
  // Do not access image_elements_ directly. Use ImageElements() instead.
  HeapVector<Member<HTMLImageElement>> image_elements_;

  uint64_t unique_renderer_form_id_;

  base::OnceClosure cancel_last_submission_;

  bool is_submitting_ = false;
  bool in_user_js_submit_event_ = false;
  bool is_constructing_entry_list_ = false;

  bool listed_elements_are_dirty_ : 1;
  bool listed_elements_including_shadow_trees_are_dirty_ : 1;
  bool image_elements_are_dirty_ : 1;
  bool has_elements_associated_by_parser_ : 1;
  bool has_elements_associated_by_form_attribute_ : 1;
  bool did_finish_parsing_children_ : 1;
  bool is_in_reset_function_ : 1;

  Member<RelList> rel_list_;
  unsigned rel_attribute_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_FORM_ELEMENT_H_
