/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_LINK_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_LINK_ELEMENT_H_

#include <memory>

#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/create_element_flags.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/increment_load_event_delay_count.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/link_rel_attribute.h"
#include "third_party/blink/renderer/core/html/link_resource.h"
#include "third_party/blink/renderer/core/html/link_style.h"
#include "third_party/blink/renderer/core/html/rel_list.h"
#include "third_party/blink/renderer/core/loader/link_loader_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"

namespace blink {

class KURL;
class LinkImport;
class LinkLoader;
struct LinkLoadParameters;

class CORE_EXPORT HTMLLinkElement final : public HTMLElement,
                                          public LinkLoaderClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(HTMLLinkElement);

 public:
  HTMLLinkElement(Document&, const CreateElementFlags);
  ~HTMLLinkElement() override;

  KURL Href() const;
  const AtomicString& Rel() const;
  String Media() const { return media_; }
  String TypeValue() const { return type_; }
  String AsValue() const { return as_; }
  String IntegrityValue() const { return integrity_; }
  String ImportanceValue() const { return importance_; }
  network::mojom::ReferrerPolicy GetReferrerPolicy() const {
    return referrer_policy_;
  }
  const LinkRelAttribute& RelAttribute() const { return rel_attribute_; }
  DOMTokenList& relList() const {
    return static_cast<DOMTokenList&>(*rel_list_);
  }
  String Scope() const { return scope_; }

  const AtomicString& GetType() const;

  IconType GetIconType() const;

  // the icon sizes as parsed from the HTML attribute
  const Vector<IntSize>& IconSizes() const;

  bool Async() const;

  CSSStyleSheet* sheet() const {
    return GetLinkStyle() ? GetLinkStyle()->Sheet() : nullptr;
  }
  Document* import() const;

  bool StyleSheetIsLoading() const;

  bool IsImport() const { return GetLinkImport(); }
  bool IsDisabled() const {
    return GetLinkStyle() && GetLinkStyle()->IsDisabled();
  }
  bool IsEnabledViaScript() const {
    return GetLinkStyle() && GetLinkStyle()->IsEnabledViaScript();
  }

  DOMTokenList* sizes() const;

  void ScheduleEvent();

  // From LinkLoaderClient
  bool ShouldLoadLink() override;
  bool IsLinkCreatedByParser() override;

  // For LinkStyle
  bool LoadLink(const LinkLoadParameters&);
  void LoadStylesheet(const LinkLoadParameters&,
                      const WTF::TextEncoding&,
                      FetchParameters::DeferOption,
                      ResourceClient*);
  bool IsAlternate() const {
    return GetLinkStyle()->IsUnset() && rel_attribute_.IsAlternate();
  }
  bool ShouldProcessStyle() {
    return LinkResourceToProcess() && GetLinkStyle();
  }
  bool IsCreatedByParser() const { return created_by_parser_; }

  void Trace(Visitor*) override;

 private:
  LinkStyle* GetLinkStyle() const;
  LinkImport* GetLinkImport() const;
  LinkResource* LinkResourceToProcess();

  void Process();
  static void ProcessCallback(Node*);

  // Always call this asynchronously because this can cause synchronous
  // Document load event and JavaScript execution.
  void DispatchPendingEvent(std::unique_ptr<IncrementLoadEventDelayCount>);

  // From Node and subclassses
  void ParseAttribute(const AttributeModificationParams&) override;
  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;
  bool IsURLAttribute(const Attribute&) const override;
  bool HasLegalLinkAttribute(const QualifiedName&) const override;
  const QualifiedName& SubResourceAttributeName() const override;
  bool SheetLoaded() override;
  void NotifyLoadedSheetAndAllCriticalSubresources(
      LoadedSheetErrorStatus) override;
  void StartLoadingDynamicSheet() override;
  void FinishParsingChildren() override;
  bool HasActivationBehavior() const override;

  // From LinkLoaderClient
  void LinkLoaded() override;
  void LinkLoadingErrored() override;
  void DidStartLinkPrerender() override;
  void DidStopLinkPrerender() override;
  void DidSendLoadForLinkPrerender() override;
  void DidSendDOMContentLoadedForLinkPrerender() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetLoadingTaskRunner() override;

  Member<LinkResource> link_;
  Member<LinkLoader> link_loader_;

  String type_;
  String as_;
  String media_;
  String integrity_;
  String importance_;
  network::mojom::ReferrerPolicy referrer_policy_;
  Member<DOMTokenList> sizes_;
  Vector<IntSize> icon_sizes_;
  Member<RelList> rel_list_;
  LinkRelAttribute rel_attribute_;
  String scope_;

  bool created_by_parser_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_LINK_ELEMENT_H_
