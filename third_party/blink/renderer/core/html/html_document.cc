/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights
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
 * Portions are Copyright (C) 2002 Netscape Communications Corporation.
 * Other contributors: David Baron <dbaron@fas.harvard.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Alternatively, the document type parsing portions of this file may be used
 * under the terms of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "third_party/blink/renderer/core/html/html_document.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

HTMLDocument::HTMLDocument(const DocumentInit& initializer,
                           DocumentClassFlags extended_document_classes)
    : Document(initializer, kHTMLDocumentClass | extended_document_classes) {
  ClearXMLVersion();
  if (IsSrcdocDocument() || initializer.ImportsController()) {
    DCHECK(InNoQuirksMode());
    LockCompatibilityMode();
  }
}

HTMLDocument::~HTMLDocument() = default;

HTMLDocument* HTMLDocument::CreateForTest() {
  return MakeGarbageCollected<HTMLDocument>(DocumentInit::Create().ForTest());
}

Document* HTMLDocument::CloneDocumentWithoutChildren() const {
  return MakeGarbageCollected<HTMLDocument>(
      DocumentInit::Create()
          .WithExecutionContext(GetExecutionContext())
          .WithURL(Url())
          .WithRegistrationContext(RegistrationContext()));
}

// --------------------------------------------------------------------------
// not part of the DOM
// --------------------------------------------------------------------------

void HTMLDocument::AddNamedItem(const AtomicString& name) {
  if (name.IsEmpty())
    return;
  named_item_counts_.insert(name);
  if (LocalDOMWindow* window = domWindow()) {
    window->GetScriptController()
        .WindowProxy(DOMWrapperWorld::MainWorld())
        ->NamedItemAdded(this, name);
  }
}

void HTMLDocument::RemoveNamedItem(const AtomicString& name) {
  if (name.IsEmpty())
    return;
  named_item_counts_.erase(name);
  if (LocalDOMWindow* window = domWindow()) {
    window->GetScriptController()
        .WindowProxy(DOMWrapperWorld::MainWorld())
        ->NamedItemRemoved(this, name);
  }
}

static HashSet<StringImpl*>* CreateHtmlCaseInsensitiveAttributesSet() {
  // This is the list of attributes in HTML 4.01 with values marked as "[CI]" or
  // case-insensitive.  Mozilla treats all other values as case-sensitive, thus
  // so do we.
  HashSet<StringImpl*>* attr_set = new HashSet<StringImpl*>;

  const QualifiedName* case_insensitive_attributes[] = {
      &html_names::kAcceptCharsetAttr, &html_names::kAcceptAttr,
      &html_names::kAlignAttr,         &html_names::kAlinkAttr,
      &html_names::kAxisAttr,          &html_names::kBgcolorAttr,
      &html_names::kCharsetAttr,       &html_names::kCheckedAttr,
      &html_names::kClearAttr,         &html_names::kCodetypeAttr,
      &html_names::kColorAttr,         &html_names::kCompactAttr,
      &html_names::kDeclareAttr,       &html_names::kDeferAttr,
      &html_names::kDirAttr,           &html_names::kDirectionAttr,
      &html_names::kDisabledAttr,      &html_names::kEnctypeAttr,
      &html_names::kFaceAttr,          &html_names::kFrameAttr,
      &html_names::kHreflangAttr,      &html_names::kHttpEquivAttr,
      &html_names::kLangAttr,          &html_names::kLanguageAttr,
      &html_names::kLinkAttr,          &html_names::kMediaAttr,
      &html_names::kMethodAttr,        &html_names::kMultipleAttr,
      &html_names::kNohrefAttr,        &html_names::kNoresizeAttr,
      &html_names::kNoshadeAttr,       &html_names::kNowrapAttr,
      &html_names::kReadonlyAttr,      &html_names::kRelAttr,
      &html_names::kRevAttr,           &html_names::kRulesAttr,
      &html_names::kScopeAttr,         &html_names::kScrollingAttr,
      &html_names::kSelectedAttr,      &html_names::kShapeAttr,
      &html_names::kTargetAttr,        &html_names::kTextAttr,
      &html_names::kTypeAttr,          &html_names::kValignAttr,
      &html_names::kValuetypeAttr,     &html_names::kVlinkAttr};

  attr_set->ReserveCapacityForSize(base::size(case_insensitive_attributes));
  for (const QualifiedName* attr : case_insensitive_attributes)
    attr_set->insert(attr->LocalName().Impl());

  return attr_set;
}

bool HTMLDocument::IsCaseSensitiveAttribute(
    const QualifiedName& attribute_name) {
  static HashSet<StringImpl*>* html_case_insensitive_attributes_set =
      CreateHtmlCaseInsensitiveAttributesSet();
  bool is_possible_html_attr = !attribute_name.HasPrefix() &&
                               (attribute_name.NamespaceURI() == g_null_atom);
  return !is_possible_html_attr ||
         !html_case_insensitive_attributes_set->Contains(
             attribute_name.LocalName().Impl());
}

}  // namespace blink
