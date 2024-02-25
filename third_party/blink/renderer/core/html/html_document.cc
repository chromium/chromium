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
 * Other contributors: David Baron <dbaron@dbaron.org>
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

#include "third_party/blink/renderer/bindings/core/v8/local_window_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

HTMLDocument::HTMLDocument(const DocumentInit& initializer,
                           DocumentClassFlags extended_document_classes)
    : Document(initializer,
               base::Union(DocumentClassFlags({DocumentClass::kHTML}),
                           extended_document_classes)) {
  ClearXMLVersion();
  if (IsSrcdocDocument()) {
    DCHECK(InNoQuirksMode());
    LockCompatibilityMode();
  }
}

HTMLDocument::~HTMLDocument() = default;

HTMLDocument* HTMLDocument::CreateForTest(ExecutionContext& execution_context) {
  return MakeGarbageCollected<HTMLDocument>(
      DocumentInit::Create().ForTest(execution_context));
}

Document* HTMLDocument::CloneDocumentWithoutChildren() const {
  return MakeGarbageCollected<HTMLDocument>(
      DocumentInit::Create()
          .WithExecutionContext(GetExecutionContext())
          .WithAgent(GetAgent())
          .WithURL(Url()));
}

// --------------------------------------------------------------------------
// not part of the DOM
// --------------------------------------------------------------------------

void HTMLDocument::AddNamedItem(const AtomicString& name) {
  if (name.empty())
    return;
  named_item_counts_.insert(name);
  if (LocalDOMWindow* window = domWindow()) {
    window->GetScriptController()
        .WindowProxy(DOMWrapperWorld::MainWorld(window->GetIsolate()))
        ->NamedItemAdded(this, name);
  }
}

void HTMLDocument::RemoveNamedItem(const AtomicString& name) {
  if (name.empty())
    return;
  named_item_counts_.erase(name);
  if (LocalDOMWindow* window = domWindow()) {
    window->GetScriptController()
        .WindowProxy(DOMWrapperWorld::MainWorld(window->GetIsolate()))
        ->NamedItemRemoved(this, name);
  }
}

bool HTMLDocument::IsCaseSensitiveAttribute(
    const QualifiedName& attribute_name) {
  if (attribute_name.HasPrefix() ||
      attribute_name.NamespaceURI() != g_null_atom) {
    // Not an HTML attribute.
    return true;
  }
  AtomicString local_name = attribute_name.LocalName();
  if (local_name.length() < 3) {
    return true;
  }

  // This is the list of attributes in HTML 4.01 with values marked as "[CI]"
  // or case-insensitive. Mozilla treats all other values as case-sensitive,
  // thus so do we.
  switch (local_name[0]) {
    case 'a':
      return local_name != html_names::kAcceptCharsetAttr.LocalName() &&
             local_name != html_names::kAcceptAttr.LocalName() &&
             local_name != html_names::kAlignAttr.LocalName() &&
             local_name != html_names::kAlinkAttr.LocalName() &&
             local_name != html_names::kAxisAttr.LocalName();
    case 'b':
      return local_name != html_names::kBgcolorAttr;
    case 'c':
      return local_name != html_names::kCharsetAttr &&
             local_name != html_names::kCheckedAttr &&
             local_name != html_names::kClearAttr &&
             local_name != html_names::kCodetypeAttr &&
             local_name != html_names::kColorAttr &&
             local_name != html_names::kCompactAttr;
    case 'd':
      return local_name != html_names::kDeclareAttr &&
             local_name != html_names::kDeferAttr &&
             local_name != html_names::kDirAttr &&
             local_name != html_names::kDirectionAttr &&
             local_name != html_names::kDisabledAttr;
    case 'e':
      return local_name != html_names::kEnctypeAttr;
    case 'f':
      return local_name != html_names::kFaceAttr &&
             local_name != html_names::kFrameAttr;
    case 'h':
      return local_name != html_names::kHreflangAttr &&
             local_name != html_names::kHttpEquivAttr;
    case 'l':
      return local_name != html_names::kLangAttr &&
             local_name != html_names::kLanguageAttr &&
             local_name != html_names::kLinkAttr;
    case 'm':
      return local_name != html_names::kMediaAttr &&
             local_name != html_names::kMethodAttr &&
             local_name != html_names::kMultipleAttr;
    case 'n':
      return local_name != html_names::kNohrefAttr &&
             local_name != html_names::kNoresizeAttr &&
             local_name != html_names::kNoshadeAttr &&
             local_name != html_names::kNowrapAttr;
    case 'r':
      return local_name != html_names::kReadonlyAttr &&
             local_name != html_names::kRelAttr &&
             local_name != html_names::kRevAttr &&
             local_name != html_names::kRulesAttr;
    case 's':
      return local_name != html_names::kScopeAttr.LocalName() &&
             local_name != html_names::kScrollingAttr &&
             local_name != html_names::kSelectedAttr &&
             local_name != html_names::kShapeAttr;
    case 't':
      return local_name != html_names::kTargetAttr &&
             local_name != html_names::kTextAttr &&
             local_name != html_names::kTypeAttr;
    case 'v':
      return local_name != html_names::kValignAttr &&
             local_name != html_names::kValuetypeAttr &&
             local_name != html_names::kVlinkAttr;
    default:
      return true;
  }
}

}  // namespace blink
