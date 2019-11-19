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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_FORM_CONTROL_ELEMENT_WITH_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_FORM_CONTROL_ELEMENT_WITH_STATE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"

namespace blink {

class CORE_EXPORT HTMLFormControlElementWithState
    : public HTMLFormControlElement {
 public:
  ~HTMLFormControlElementWithState() override;

  bool CanContainRangeEndPoint() const final { return false; }

  virtual bool ShouldAutocomplete() const;
  // Implementations of 'autocomplete' IDL attribute.
  String IDLExposedAutofillValue() const;
  void setIDLExposedAutofillValue(const String& autocomplete_value);

  // ListedElement override:
  bool ClassSupportsStateRestore() const override;
  bool ShouldSaveAndRestoreFormControlState() const override;

  bool UserHasEditedTheField() const { return user_has_edited_the_field_; }
  // This is only used in tests, to fake the user's action
  void SetUserHasEditedTheFieldForTest() { user_has_edited_the_field_ = true; }

 protected:
  bool user_has_edited_the_field_ = false;
  HTMLFormControlElementWithState(const QualifiedName& tag_name, Document&);

  void FinishParsingChildren() override;
  bool IsFormControlElementWithState() const final;

 private:
  bool TypeShouldForceLegacyLayout() const final { return true; }
  int DefaultTabIndex() const override;

  // https://html.spec.whatwg.org/C/#autofill-anchor-mantle
  bool IsWearingAutofillAnchorMantle() const;
};

DEFINE_TYPE_CASTS(HTMLFormControlElementWithState,
                  ListedElement,
                  control,
                  control->IsFormControlElementWithState(),
                  control.IsFormControlElementWithState());

}  // namespace blink

#endif
