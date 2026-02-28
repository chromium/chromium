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

#include "base/functional/callback.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/radio_button_group_scope.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/loader/form_submission.h"
#include "third_party/blink/renderer/core/script_tools/model_context.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

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
  bool HasNamedElements(const AtomicString&);

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

  const Node* GetListedElementsScope() const;
  // Returns the scope that includes the highest reference target host.
  const Node* GetReferenceTargetScope() const;

  // Returns the listed elements (form controls) associated with `this`.
  const ListedElement::List& ListedElements() const {
    return CollectAndCacheListedElements(/*collect_for_autofill*/ false);
  }

  // Returns the contained form control elements associated with `this`, also
  // including descendants of `this` that are form control elements and inside
  // Shadow DOM. The result will contain the form control elements of <form>s
  // nested inside `this`. In principle, form nesting is prohibited by the HTML
  // standard, but in practice it can still occur - e.g., by dynamically
  // appending <form> children to (a descendant of) `this`.
  const ListedElement::List& AllContainedFormElementsForAutofill() const {
    return CollectAndCacheListedElements(/*collect_for_autofill*/ true);
  }

  const HeapVector<Member<HTMLImageElement>>& ImageElements();

  V8UnionElementOrRadioNodeList* AnonymousNamedGetter(const AtomicString& name);
  bool NamedPropertyQuery(const AtomicString& name, ExceptionState&);
  bool HasAnyNamedProperties() const;

  void InvalidateDefaultButtonStyle() const;

  // 'construct the entry list'
  // https://html.spec.whatwg.org/C/#constructing-the-form-data-set
  // Returns nullptr if this form is already running this function.
  FormData* ConstructEntryList(HTMLFormControlElement* submit_button,
                               const TextEncoding& encoding);

  void InvalidateListedElementsForAutofill();
  void UseCountPropertyAccess(v8::Local<v8::Name>&,
                              const v8::PropertyCallbackInfo<v8::Value>&);

  bool IsActiveToolSubmitButton(const HTMLFormControlElement* element) const;
  bool MatchesToolFormActivePseudoClass() const;

 private:
  friend class HTMLFormMcpToolTest;

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;
  void FinishParsingChildren() override;

  void HandleLocalEvents(Event&) override;

  void AttributeChanged(const AttributeModificationParams&) override;
  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsURLAttribute(const Attribute&) const override;
  bool HasLegalLinkAttribute(const QualifiedName&) const override;

  NamedItemType GetNamedItemType() const override {
    return NamedItemType::kName;
  }

  void SubmitDialog(FormSubmission*);
  void ScheduleFormSubmission(const Event*,
                              HTMLFormControlElement* submit_button);

  void CollectListedElementsForReferenceTarget(
      const Node& root,
      ListedElement::List& elements,
      ListedElement::List* elements_for_autofill = nullptr) const;
  void CollectListedElements(
      const Node* root,
      ListedElement::List& elements,
      ListedElement::List* elements_for_autofill = nullptr,
      bool in_shadow_tree = false) const;
  void CollectImageElements(Node& root, HeapVector<Member<HTMLImageElement>>&);

  // Utility function used by ListedElements and
  // AllContainedFormElementsForAutofill. Takes care of caching two lists of
  // listed elements, one including shadow- contained elements, and one "normal"
  // list without those. If `collect_for_autofill` is `true`, then the list will
  // also contain descendants of `this` that are form control elements and
  // inside Shadow DOM. Note that if `collect_for_autofill` is true, then,
  // additionally, the result will contain the form control elements of <form>s
  // nested inside `this`. In principle, form nesting is prohibited by the HTML
  // standard, but in practice it can still occur - e.g., by dynamically
  // appending <form> children to (a descendant of) `this`.
  const ListedElement::List& CollectAndCacheListedElements(
      bool collect_for_autofill) const;

  // Returns true if the submission should proceed.
  bool ValidateInteractively();

  // Validates each of the controls, and stores controls of which 'invalid'
  // event was not canceled to the specified vector. Returns true if there
  // are any invalid controls in this form.
  bool CheckInvalidControlsAndCollectUnhandled(ListedElement::List*);

  Element* ElementFromPastNamesMap(const AtomicString&);
  void AddToPastNamesMap(Element*, const AtomicString& past_name);
  void RemoveFromPastNamesMap(HTMLElement&);
  bool PastNamesEmpty() const;

  bool IsValidWebMCPForm() const;
  void UpdateMcpDefinitionsIfNeeded();

  using PastNamesMap = GCedHeapHashMap<AtomicString, Member<Element>>;

  FormSubmission::Attributes attributes_;
  Member<PastNamesMap> past_names_map_;

  RadioButtonGroupScope radio_button_group_scope_;

  // Do not access listed_elements_ directly. Use ListedElements() instead.
  ListedElement::List listed_elements_;
  // Do not access listed_elements_for_autofill_ directly. Use
  // AllContainedFormElementsForAutofill() instead.
  ListedElement::List listed_elements_for_autofill_;
  // Do not access image_elements_ directly. Use ImageElements() instead.
  HeapVector<Member<HTMLImageElement>> image_elements_;

  base::OnceClosure cancel_last_submission_;

  using McpToolCallbackResult =
      base::expected<blink::String, blink::WebDocument::ScriptToolError>;
  class CORE_EXPORT HTMLFormMcpTool final
      : public GarbageCollected<HTMLFormMcpTool>,
        public DeclarativeWebMCPTool {
   public:
    HTMLFormMcpTool() = delete;
    HTMLFormMcpTool(const HTMLFormMcpTool&) = delete;
    HTMLFormMcpTool& operator=(const HTMLFormMcpTool&) = delete;
    HTMLFormMcpTool(HTMLFormElement& form,
                    String tool_name,
                    String tool_description)
        : tool_name_(tool_name),
          tool_description_(tool_description),
          form_(form) {
      CHECK(!tool_name.IsNull() && !tool_description.IsNull());
    }
    String ComputeInputSchema() override;
    Element* FormElement() const override { return form_; }
    void ExecuteTool(
        String input_arguments,
        base::OnceCallback<void(McpToolCallbackResult)> done_callback) override;
    // Fill form controls with data as provided by `input_arguments`.
    //
    // If no error is returned, then all specified tool parameters (form
    // controls) were filled successfully. Otherwise, the state of all form
    // controls are left unchanged.
    std::optional<WebDocument::ScriptToolError> FillFormControls(
        const String& input_arguments,
        bool require_submit_button,
        HTMLFormControlElement** submit_button);
    String ToolName() const { return tool_name_; }
    String ToolDescription() const { return tool_description_; }
    bool IsValidTool() const { return !tool_name_.IsNull(); }
    bool CurrentlyRunning() const {
      return IsValidTool() && is_currently_running_;
    }
    HTMLFormControlElement* ActiveToolSubmitButton() const {
      CHECK(is_currently_running_);
      return active_submit_button_;
    }
    void CallDoneCallback(McpToolCallbackResult result);
    void Trace(Visitor* visitor) const override;

   private:
    bool is_currently_running_ = false;
    String tool_name_;
    String tool_description_;
    Member<HTMLFormElement> form_;
    Member<HTMLFormControlElement> active_submit_button_;
    base::OnceCallback<void(McpToolCallbackResult)> done_callback_;
  };

  void HandleWebMcpToolResponse(HTMLFormMcpTool* tool,
                                bool resolved,
                                ScriptState* script_state,
                                ScriptValue value);

  // Used only for (experimental) declarative WebMCP.
  Member<HTMLFormMcpTool> active_webmcp_tool_;

  bool is_submitting_ = false;
  bool in_user_js_submit_event_ = false;
  bool is_constructing_entry_list_ = false;

  bool listed_elements_are_dirty_ : 1;
  bool listed_elements_for_autofill_are_dirty_ : 1;
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
