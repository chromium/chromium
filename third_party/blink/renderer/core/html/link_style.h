// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LINK_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LINK_STYLE_H_

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/link_resource.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class HTMLLinkElement;
struct LinkLoadParameters;

// LinkStyle handles dynamically change-able link resources, which is
// typically @rel="stylesheet".
//
// It could be @rel="shortcut icon" or something else though. Each of
// types might better be handled by a separate class, but dynamically
// changing @rel makes it harder to move such a design so we are
// sticking current way so far.
class LinkStyle final : public LinkResource, ResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(LinkStyle);

 public:
  explicit LinkStyle(HTMLLinkElement* owner);
  ~LinkStyle() override;

  LinkResourceType GetType() const override { return kStyle; }
  void Process() override;
  void OwnerRemoved() override;
  bool HasLoaded() const override { return loaded_sheet_; }
  void Trace(Visitor*) override;

  void StartLoadingDynamicSheet();
  void NotifyLoadedSheetAndAllCriticalSubresources(
      Node::LoadedSheetErrorStatus);
  bool SheetLoaded();

  void SetDisabledState(bool);
  void SetSheetTitle(const String&);

  bool StyleSheetIsLoading() const;
  bool HasSheet() const { return sheet_; }
  bool IsDisabled() const { return disabled_state_ == kDisabled; }
  bool IsEnabledViaScript() const {
    return disabled_state_ == kEnabledViaScript;
  }
  bool IsUnset() const { return disabled_state_ == kUnset; }

  CSSStyleSheet* Sheet() const { return sheet_.Get(); }

 private:
  // From ResourceClient
  void NotifyFinished(Resource*) override;
  String DebugName() const override { return "LinkStyle"; }
  enum LoadReturnValue { kLoaded, kNotNeeded, kBail };
  LoadReturnValue LoadStylesheetIfNeeded(const LinkLoadParameters&,
                                         const WTF::TextEncoding&);

  enum DisabledState { kUnset, kEnabledViaScript, kDisabled };

  enum PendingSheetType { kNone, kNonBlocking, kBlocking };

  void ClearSheet();
  void AddPendingSheet(PendingSheetType);
  void RemovePendingSheet();

  Member<CSSStyleSheet> sheet_;
  DisabledState disabled_state_;
  PendingSheetType pending_sheet_type_;
  StyleEngineContext style_engine_context_;
  bool loading_;
  bool fired_load_;
  bool loaded_sheet_;
};

}  // namespace blink

#endif
