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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_LISTED_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_LISTED_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ContainerNode;
class Document;
class FormAttributeTargetObserver;
class FormData;
class HTMLElement;
class HTMLFormElement;
class Node;
class ValidityState;

// https://html.spec.whatwg.org/multipage/forms.html#category-listed
class CORE_EXPORT ListedElement : public GarbageCollectedMixin {
 public:
  virtual ~ListedElement();

  static HTMLFormElement* FindAssociatedForm(const HTMLElement*,
                                             const AtomicString& form_id,
                                             HTMLFormElement* form_ancestor);
  HTMLFormElement* Form() const { return form_.Get(); }
  ValidityState* validity();

  virtual bool IsFormControlElement() const = 0;
  virtual bool IsFormControlElementWithState() const;
  virtual bool IsEnumeratable() const = 0;

  // Returns the 'name' attribute value. If this element has no name
  // attribute, it returns an empty string instead of null string.
  // Note that the 'name' IDL attribute doesn't use this function.
  virtual const AtomicString& GetName() const;

  // Override in derived classes to get the encoded name=value pair for
  // submitting.
  virtual void AppendToFormData(FormData&) {}

  void ResetFormOwner();

  void FormRemovedFromTree(const Node& form_root);

  // ValidityState attribute implementations
  bool CustomError() const;

  // Override functions for patterMismatch, rangeOverflow, rangerUnderflow,
  // stepMismatch, tooLong, tooShort and valueMissing must call willValidate
  // method.
  virtual bool HasBadInput() const;
  virtual bool PatternMismatch() const;
  virtual bool RangeOverflow() const;
  virtual bool RangeUnderflow() const;
  virtual bool StepMismatch() const;
  virtual bool TooLong() const;
  virtual bool TooShort() const;
  virtual bool TypeMismatch() const;
  virtual bool ValueMissing() const;
  virtual String validationMessage() const;
  virtual String ValidationSubMessage() const;
  bool Valid() const;
  virtual void setCustomValidity(const String&);

  void FormAttributeTargetChanged();

  typedef HeapVector<Member<ListedElement>> List;

  void Trace(blink::Visitor*) override;

 protected:
  ListedElement();

  void InsertedInto(ContainerNode&);
  void RemovedFrom(ContainerNode&);
  void DidMoveToNewDocument(Document& old_document);

  // FIXME: Remove usage of setForm. resetFormOwner should be enough, and
  // setForm is confusing.
  void SetForm(HTMLFormElement*);
  void AssociateByParser(HTMLFormElement*);
  void FormAttributeChanged();

  // If you add an override of willChangeForm() or didChangeForm() to a class
  // derived from this one, you will need to add a call to setForm(0) to the
  // destructor of that class.
  virtual void WillChangeForm();
  virtual void DidChangeForm();

  String CustomValidationMessage() const;

 private:
  void SetFormAttributeTargetObserver(FormAttributeTargetObserver*);
  void ResetFormAttributeTargetObserver();

  Member<FormAttributeTargetObserver> form_attribute_target_observer_;
  Member<HTMLFormElement> form_;
  Member<ValidityState> validity_state_;
  String custom_validation_message_;
  // If form_was_set_by_parser_ is true, form_ is always non-null.
  bool form_was_set_by_parser_;
};

CORE_EXPORT HTMLElement* ToHTMLElement(ListedElement*);
CORE_EXPORT HTMLElement& ToHTMLElement(ListedElement&);
CORE_EXPORT const HTMLElement* ToHTMLElement(const ListedElement*);
CORE_EXPORT const HTMLElement& ToHTMLElement(const ListedElement&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_LISTED_ELEMENT_H_
