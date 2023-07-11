/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/dom/processing_instruction.h"

#include <memory>

#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/increment_load_event_delay_count.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/core/loader/resource/xsl_style_sheet_resource.h"
#include "third_party/blink/renderer/core/xml/document_xslt.h"
#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"  // for parseAttributes()
#include "third_party/blink/renderer/core/xml/xsl_style_sheet.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"

namespace blink {

ProcessingInstruction::ProcessingInstruction(Document& document,
                                             const String& target,
                                             const String& data)
    : CharacterData(document, data, kCreateProcessingInstruction),
      target_(target),
      loading_(false),
      alternate_(false),
      is_css_(false),
      is_xsl_(false),
      listener_for_xslt_(nullptr) {}

ProcessingInstruction::~ProcessingInstruction() = default;

EventListener* ProcessingInstruction::EventListenerForXSLT() {
  if (!listener_for_xslt_)
    return nullptr;

  return listener_for_xslt_->ToEventListener();
}

void ProcessingInstruction::ClearEventListenerForXSLT() {
  if (listener_for_xslt_) {
    listener_for_xslt_->Detach();
    listener_for_xslt_.Clear();
  }
}

String ProcessingInstruction::nodeName() const {
  return target_;
}

CharacterData* ProcessingInstruction::CloneWithData(Document& factory,
                                                    const String& data) const {
  // FIXME: Is it a problem that this does not copy local_href_?
  // What about other data members?
  return MakeGarbageCollected<ProcessingInstruction>(factory, target_, data);
}

void ProcessingInstruction::DidAttributeChanged() {
  if (sheet_) {
    if (sheet_->IsLoading())
      RemovePendingSheet();
    ClearSheet();
  }

  String href;
  String charset;
  if (!CheckStyleSheet(href, charset))
    return;
  Process(href, charset);
}

bool ProcessingInstruction::CheckStyleSheet(String& href, String& charset) {
  if (target_ != "xml-stylesheet" || !GetDocument().GetFrame() ||
      parentNode() != GetDocument())
    return false;

  // see http://www.w3.org/TR/xml-stylesheet/
  // ### support stylesheet included in a fragment of this (or another) document
  // ### make sure this gets called when adding from javascript
  bool attrs_ok;
  const HashMap<String, String> attrs = ParseAttributes(data_, attrs_ok);
  if (!attrs_ok)
    return false;
  HashMap<String, String>::const_iterator i = attrs.find("type");
  String type;
  if (i != attrs.end())
    type = i->value;

  is_css_ = type.empty() || type == "text/css";
  is_xsl_ = (type == "text/xml" || type == "text/xsl" ||
             type == "application/xml" || type == "application/xhtml+xml" ||
             type == "application/rss+xml" || type == "application/atom+xml");
  if (!is_css_ && !is_xsl_)
    return false;

  auto it_href = attrs.find("href");
  href = it_href != attrs.end() ? it_href->value : "";
  auto it_charset = attrs.find("charset");
  charset = it_charset != attrs.end() ? it_charset->value : "";
  auto it_alternate = attrs.find("alternate");
  String alternate = it_alternate != attrs.end() ? it_alternate->value : "";
  alternate_ = alternate == "yes";
  auto it_title = attrs.find("title");
  title_ = it_title != attrs.end() ? it_title->value : "";
  auto it_media = attrs.find("media");
  media_ = it_media != attrs.end() ? it_media->value : "";

  return !alternate_ || !title_.empty();
}

void ProcessingInstruction::Process(const String& href, const String& charset) {
  if (href.length() > 1 && href[0] == '#') {
    local_href_ = href.Substring(1);
    // We need to make a synthetic XSLStyleSheet that is embedded.
    // It needs to be able to kick off import/include loads that
    // can hang off some parent sheet.
    if (is_xsl_) {
      KURL final_url(local_href_);
      sheet_ = MakeGarbageCollected<XSLStyleSheet>(this, final_url.GetString(),
                                                   final_url, true);
      loading_ = false;
    }
    return;
  }

  ClearResource();

  ResourceLoaderOptions options(GetExecutionContext()->GetCurrentWorld());
  options.initiator_info.name =
      fetch_initiator_type_names::kProcessinginstruction;
  FetchParameters params(ResourceRequest(GetDocument().CompleteURL(href)),
                         options);
  loading_ = true;
  if (is_xsl_) {
    params.MutableResourceRequest().SetMode(
        network::mojom::RequestMode::kSameOrigin);
    XSLStyleSheetResource::Fetch(params, GetDocument().Fetcher(), this);
  } else {
    params.SetCharset(charset.empty() ? GetDocument().Encoding()
                                      : WTF::TextEncoding(charset));
    GetDocument().GetStyleEngine().AddPendingBlockingSheet(
        *this, PendingSheetType::kBlocking);
    CSSStyleSheetResource::Fetch(params, GetDocument().Fetcher(), this);
  }
}

bool ProcessingInstruction::IsLoading() const {
  if (loading_)
    return true;
  if (!sheet_)
    return false;
  return sheet_->IsLoading();
}

bool ProcessingInstruction::SheetLoaded() {
  if (!IsLoading()) {
    if (!DocumentXSLT::SheetLoaded(GetDocument(), this))
      RemovePendingSheet();
    return true;
  }
  return false;
}

void ProcessingInstruction::NotifyFinished(Resource* resource) {
  if (!isConnected()) {
    DCHECK(!sheet_);
    return;
  }

  std::unique_ptr<IncrementLoadEventDelayCount> delay =
      is_xsl_ ? std::make_unique<IncrementLoadEventDelayCount>(GetDocument())
              : nullptr;
  if (is_xsl_) {
    sheet_ = MakeGarbageCollected<XSLStyleSheet>(
        this, resource->Url(), resource->GetResponse().ResponseUrl(), false);
    To<XSLStyleSheet>(sheet_.Get())
        ->ParseString(To<XSLStyleSheetResource>(resource)->Sheet());
  } else {
    DCHECK(is_css_);
    auto* style_resource = To<CSSStyleSheetResource>(resource);
    auto* parser_context = MakeGarbageCollected<CSSParserContext>(
        GetDocument(), style_resource->GetResponse().ResponseUrl(),
        style_resource->GetResponse().IsCorsSameOrigin(),
        Referrer(style_resource->GetResponse().ResponseUrl(),
                 style_resource->GetReferrerPolicy()),
        style_resource->Encoding());
    if (style_resource->GetResourceRequest().IsAdResource())
      parser_context->SetIsAdRelated();

    auto* new_sheet = MakeGarbageCollected<StyleSheetContents>(
        parser_context, style_resource->Url());

    auto* css_sheet = MakeGarbageCollected<CSSStyleSheet>(new_sheet, *this);
    css_sheet->setDisabled(alternate_);
    css_sheet->SetTitle(title_);
    if (!alternate_ && !title_.empty()) {
      GetDocument().GetStyleEngine().SetPreferredStylesheetSetNameIfNotSet(
          title_);
    }
    css_sheet->SetMediaQueries(
        MediaQuerySet::Create(media_, GetExecutionContext()));
    sheet_ = css_sheet;
    // We don't need the cross-origin security check here because we are
    // getting the sheet text in "strict" mode. This enforces a valid CSS MIME
    // type.
    css_sheet->Contents()->ParseString(
        style_resource->SheetText(parser_context));
  }

  ClearResource();
  loading_ = false;

  if (is_css_)
    To<CSSStyleSheet>(sheet_.Get())->Contents()->CheckLoaded();
  else if (is_xsl_)
    To<XSLStyleSheet>(sheet_.Get())->CheckLoaded();
}

Node::InsertionNotificationRequest ProcessingInstruction::InsertedInto(
    ContainerNode& insertion_point) {
  CharacterData::InsertedInto(insertion_point);
  if (!insertion_point.isConnected())
    return kInsertionDone;

  String href;
  String charset;
  bool is_valid = CheckStyleSheet(href, charset);
  if (!DocumentXSLT::ProcessingInstructionInsertedIntoDocument(GetDocument(),
                                                               this))
    GetDocument().GetStyleEngine().AddStyleSheetCandidateNode(*this);
  if (is_valid)
    Process(href, charset);
  return kInsertionDone;
}

void ProcessingInstruction::RemovedFrom(ContainerNode& insertion_point) {
  CharacterData::RemovedFrom(insertion_point);
  if (!insertion_point.isConnected())
    return;

  // No need to remove XSLStyleSheet from StyleEngine.
  if (!DocumentXSLT::ProcessingInstructionRemovedFromDocument(GetDocument(),
                                                              this)) {
    GetDocument().GetStyleEngine().RemoveStyleSheetCandidateNode(
        *this, insertion_point);
  }

  if (IsLoading())
    RemovePendingSheet();

  if (sheet_) {
    DCHECK_EQ(sheet_->ownerNode(), this);
    ClearSheet();
  }

  // No need to remove pending sheets.
  ClearResource();
}

void ProcessingInstruction::ClearSheet() {
  DCHECK(sheet_);
  sheet_.Release()->ClearOwnerNode();
}

void ProcessingInstruction::RemovePendingSheet() {
  if (is_xsl_)
    return;
  GetDocument().GetStyleEngine().RemovePendingBlockingSheet(
      *this, PendingSheetType::kBlocking);
}

void ProcessingInstruction::Trace(Visitor* visitor) const {
  visitor->Trace(sheet_);
  visitor->Trace(listener_for_xslt_);
  CharacterData::Trace(visitor);
  ResourceClient::Trace(visitor);
}

}  // namespace blink
