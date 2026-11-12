// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LINK_MODULE_IMPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LINK_MODULE_IMPORT_H_

#include "third_party/blink/renderer/core/html/link_resource.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class CSSStyleSheet;
class ModuleScript;
class TreeScope;

// LinkResource for `<link rel="stylesheet" import="...">`.
class LinkModuleImport final : public LinkResource {
 public:
  explicit LinkModuleImport(HTMLLinkElement*);
  ~LinkModuleImport() override;

  LinkResourceType GetType() const override { return kStyleModule; }
  void Process(LinkLoadParameters::Reason reason) override;
  void OwnerRemoved() override;
  bool HasLoaded() const override { return loaded_; }
  void Trace(Visitor*) const override;

  CSSStyleSheet* Sheet() const override { return sheet_.Get(); }
  bool StyleSheetIsLoading() const override { return pending_ != nullptr; }
  const KURL& ResolvedURL() const { return resolved_url_; }

 private:
  class ModuleClient;

  void CancelPendingRequest();
  void ScheduleFailure();
  void SetSheet(CSSStyleSheet*);
  void NotifyModuleLoadFinished(ModuleClient*, ModuleScript*);

  Member<CSSStyleSheet> sheet_;
  Member<TreeScope> sheet_tree_scope_;
  Member<ModuleClient> pending_;
  KURL resolved_url_;
  bool loaded_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LINK_MODULE_IMPORT_H_
