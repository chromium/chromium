// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/savable_resources.h"

#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_all_collection.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

// Returns |true| if |frame| contains (or should be assumed to contain)
// a html document.
bool DoesFrameContainHtmlDocument(Frame* frame, Element* element) {
  if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
    Document* document = local_frame->GetDocument();
    return document->IsHTMLDocument() || document->IsXHTMLDocument();
  }

  // Cannot inspect contents of a remote frame, so we use a heuristic:
  // Assume that <iframe> and <frame> elements contain a html document,
  // and other elements (i.e. <object>) contain plugins or other resources.
  // If the heuristic is wrong (i.e. the remote frame in <object> does
  // contain an html document), then things will still work, but with the
  // following caveats: 1) original frame content will be saved and 2) links
  // in frame's html doc will not be rewritten to point to locally saved
  // files.
  return element->HasTagName(html_names::kIFrameTag) ||
         element->HasTagName(html_names::kFrameTag);
}

// If present and valid, then push the link associated with |element|
// into either SavableResources::Result::subframes_ or
// SavableResources::Result::resources_list_.
void GetSavableResourceLinkForElement(Element* element,
                                      const Document& current_document,
                                      SavableResources::Result* result) {
  // Get absolute URL.
  String link_attribute_value =
      SavableResources::GetSubResourceLinkFromElement(element);
  KURL element_url = current_document.CompleteURL(link_attribute_value);

  // See whether to report this element as a subframe.
  if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(element)) {
    Frame* content_frame = frame_owner->ContentFrame();
    if (content_frame && DoesFrameContainHtmlDocument(content_frame, element)) {
      mojom::blink::SavableSubframePtr subframe =
          mojom::blink::SavableSubframe::New(element_url,
                                             content_frame->GetFrameToken());
      result->AppendSubframe(std::move(subframe));
      return;
    }
  }

  // Check whether the node has sub resource URL or not.
  if (link_attribute_value.IsNull())
    return;

  // Ignore invalid URL.
  if (!element_url.IsValid())
    return;

  // Ignore those URLs which are not standard protocols. Because FTP
  // protocol does no have cache mechanism, we will skip all
  // sub-resources if they use FTP protocol.
  if (!element_url.ProtocolIsInHTTPFamily() &&
      !element_url.ProtocolIs(url::kFileScheme))
    return;

  result->AppendResourceLink(element_url);
}

}  // namespace

// static
bool SavableResources::GetSavableResourceLinksForFrame(
    LocalFrame* current_frame,
    SavableResources::Result* result) {
  // Get current frame's URL.
  KURL current_frame_url = current_frame->GetDocument()->Url();

  // If url of current frame is invalid, ignore it.
  if (!current_frame_url.IsValid())
    return false;

  // If url of current frame is not a savable protocol, ignore it.
  if (!Platform::Current()->IsURLSavableForSavableResource(current_frame_url))
    return false;

  // Get current using document.
  Document* current_document = current_frame->GetDocument();
  DCHECK(current_document);

  // Go through all descent nodes.
  HTMLCollection* collection = current_document->all();

  // Go through all elements in this frame.
  for (unsigned i = 0; i < collection->length(); ++i) {
    GetSavableResourceLinkForElement(collection->item(i), *current_document,
                                     result);
  }

  return true;
}

// static
String SavableResources::GetSubResourceLinkFromElement(Element* element) {
  const QualifiedName* attribute_name = nullptr;
  if (element->HasTagName(html_names::kImgTag) ||
      element->HasTagName(html_names::kFrameTag) ||
      element->HasTagName(html_names::kIFrameTag) ||
      element->HasTagName(html_names::kScriptTag)) {
    attribute_name = &html_names::kSrcAttr;
  } else if (element->HasTagName(html_names::kInputTag)) {
    HTMLInputElement* input = To<HTMLInputElement>(element);
    if (input->FormControlType() == FormControlType::kInputImage) {
      attribute_name = &html_names::kSrcAttr;
    }
  } else if (element->HasTagName(html_names::kBodyTag) ||
             element->HasTagName(html_names::kTableTag) ||
             element->HasTagName(html_names::kTrTag) ||
             element->HasTagName(html_names::kTdTag)) {
    attribute_name = &html_names::kBackgroundAttr;
  } else if (element->HasTagName(html_names::kBlockquoteTag) ||
             element->HasTagName(html_names::kQTag) ||
             element->HasTagName(html_names::kDelTag) ||
             element->HasTagName(html_names::kInsTag)) {
    attribute_name = &html_names::kCiteAttr;
  } else if (element->HasTagName(html_names::kObjectTag)) {
    attribute_name = &html_names::kDataAttr;
  } else if (element->HasTagName(html_names::kLinkTag)) {
    // If the link element is not linked to css, ignore it.
    String type = element->getAttribute(html_names::kTypeAttr);
    String rel = element->getAttribute(html_names::kRelAttr);
    if (EqualIgnoringASCIICase(type, "text/css") ||
        EqualIgnoringASCIICase(rel, "stylesheet")) {
      // TODO(jnd): Add support for extracting links of sub-resources which
      // are inside style-sheet such as @import, url(), etc.
      // See bug: http://b/issue?id=1111667.
      attribute_name = &html_names::kHrefAttr;
    }
  }
  if (!attribute_name)
    return String();
  String value = element->getAttribute(*attribute_name);
  // If value has content and not start with "javascript:" then return it,
  // otherwise return an empty string.
  if (!value.IsNull() && !value.empty() &&
      !value.StartsWith("javascript:", kTextCaseASCIIInsensitive))
    return value;

  return String();
}

void SavableResources::Result::AppendSubframe(
    mojom::blink::SavableSubframePtr subframe) {
  subframes_->emplace_back(std::move(subframe));
}

void SavableResources::Result::AppendResourceLink(const KURL& url) {
  resources_list_->emplace_back(url);
}

}  // namespace blink
