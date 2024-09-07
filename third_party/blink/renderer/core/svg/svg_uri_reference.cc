/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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
 */

#include "third_party/blink/renderer/core/svg/svg_uri_reference.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/svg/svg_animated_href.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class SVGElementReferenceObserver : public IdTargetObserver {
 public:
  SVGElementReferenceObserver(TreeScope& tree_scope,
                              const AtomicString& id,
                              base::RepeatingClosure closure)
      : IdTargetObserver(tree_scope.EnsureIdTargetObserverRegistry(), id),
        closure_(std::move(closure)) {}

 private:
  void IdTargetChanged() override { closure_.Run(); }
  base::RepeatingClosure closure_;
};
}  // namespace

SVGURIReference::SVGURIReference(SVGElement* element)
    : href_(MakeGarbageCollected<SVGAnimatedHref>(element)) {
  DCHECK(element);
}

const String& SVGURIReference::HrefString() const {
  return href_->CurrentValue()->Value();
}

SVGAnimatedString* SVGURIReference::href() const {
  return href_.Get();
}

SVGAnimatedPropertyBase* SVGURIReference::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  return href_->PropertyFromAttribute(attribute_name);
}

void SVGURIReference::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{href_.Get()};
  SVGElement::SynchronizeListOfSVGAttributes(attrs);
}

void SVGURIReference::Trace(Visitor* visitor) const {
  visitor->Trace(href_);
}

bool SVGURIReference::IsKnownAttribute(const QualifiedName& attr_name) {
  return SVGAnimatedHref::IsKnownAttribute(attr_name);
}

const AtomicString& SVGURIReference::LegacyHrefString(
    const SVGElement& element) {
  if (element.hasAttribute(svg_names::kHrefAttr))
    return element.getAttribute(svg_names::kHrefAttr);
  return element.getAttribute(xlink_names::kHrefAttr);
}

KURL SVGURIReference::LegacyHrefURL(const Document& document) const {
  return document.CompleteURL(StripLeadingAndTrailingHTMLSpaces(HrefString()));
}

SVGURLReferenceResolver::SVGURLReferenceResolver(const String& url_string,
                                                 const Document& document)
    : relative_url_(url_string),
      document_(&document),
      is_local_(url_string.StartsWith('#')) {}

KURL SVGURLReferenceResolver::AbsoluteUrl() const {
  if (absolute_url_.IsNull())
    absolute_url_ = document_->CompleteURL(relative_url_);
  return absolute_url_;
}

bool SVGURLReferenceResolver::IsLocal() const {
  return is_local_ ||
         EqualIgnoringFragmentIdentifier(AbsoluteUrl(), document_->Url());
}

AtomicString SVGURLReferenceResolver::FragmentIdentifier() const {
  // Use KURL's FragmentIdentifier to ensure that we're handling the
  // fragment in a consistent manner.
  return AtomicString(DecodeURLEscapeSequences(
      AbsoluteUrl().FragmentIdentifier(), DecodeURLMode::kUTF8OrIsomorphic));
}

AtomicString SVGURIReference::FragmentIdentifierFromIRIString(
    const String& url_string,
    const TreeScope& tree_scope) {
  SVGURLReferenceResolver resolver(url_string, tree_scope.GetDocument());
  if (!resolver.IsLocal())
    return g_empty_atom;
  return resolver.FragmentIdentifier();
}

Element* SVGURIReference::TargetElementFromIRIString(
    const String& url_string,
    const TreeScope& tree_scope,
    AtomicString* fragment_identifier) {
  AtomicString id = FragmentIdentifierFromIRIString(url_string, tree_scope);
  if (id.empty())
    return nullptr;
  if (fragment_identifier)
    *fragment_identifier = id;
  return tree_scope.getElementById(id);
}

Element* SVGURIReference::ObserveTarget(Member<IdTargetObserver>& observer,
                                        SVGElement& context_element) {
  return ObserveTarget(observer, context_element, HrefString());
}

Element* SVGURIReference::ObserveTarget(Member<IdTargetObserver>& observer,
                                        SVGElement& context_element,
                                        const String& href_string) {
  TreeScope& tree_scope = context_element.OriginatingTreeScope();
  AtomicString id = FragmentIdentifierFromIRIString(href_string, tree_scope);
  return ObserveTarget(
      observer, tree_scope, id,
      WTF::BindRepeating(&SVGElement::BuildPendingResource,
                         WrapWeakPersistent(&context_element)));
}

Element* SVGURIReference::ObserveTarget(Member<IdTargetObserver>& observer,
                                        TreeScope& tree_scope,
                                        const AtomicString& id,
                                        base::RepeatingClosure closure) {
  DCHECK(!observer);
  if (id.empty())
    return nullptr;
  observer = MakeGarbageCollected<SVGElementReferenceObserver>(
      tree_scope, id, std::move(closure));
  return tree_scope.getElementById(id);
}

void SVGURIReference::UnobserveTarget(Member<IdTargetObserver>& observer) {
  if (!observer)
    return;
  observer->Unregister();
  observer = nullptr;
}

}  // namespace blink
