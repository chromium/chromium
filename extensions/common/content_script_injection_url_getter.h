// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_CONTENT_SCRIPT_INJECTION_URL_GETTER_H_
#define EXTENSIONS_COMMON_CONTENT_SCRIPT_INJECTION_URL_GETTER_H_

#include <memory>

#include "extensions/common/script_constants.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

// A helper for deciding which URL to use for deciding whether to inject a
// content script - it finds the effective document URL by (depending on content
// script options) possibly looking at the parent-or-opener document instead,
// looking at the precursor origin of data: documents, etc.
//
// TODO(https://crbug.com/1186321): Content script injection assumes that
// about:blank inherits origin from the parent.  This can return the incorrect
// result, e.g.  if a parent frame navigates a grandchild frame to about:blank.
class ContentScriptInjectionUrlGetter {
 public:
  // Only static methods.
  ContentScriptInjectionUrlGetter() = delete;

  // An interface that contains data about the current context. Additionally,
  // it abstracts away differences in data between the browser and renderer,
  // for example between a RenderFrameHost and RenderFrame.
  // TODO(b/267673751): Adjust ContextData to hold more data.
  class ContextData {
   public:
    virtual ~ContextData();
    virtual std::unique_ptr<ContextData> Clone() const = 0;
    virtual std::unique_ptr<ContextData> GetLocalParentOrOpener() const = 0;
    virtual GURL GetUrl() const = 0;
    virtual url::Origin GetOrigin() const = 0;
    virtual bool CanAccess(const url::Origin& target) const = 0;
    virtual bool CanAccess(const ContextData& target) const = 0;
    virtual uintptr_t GetId() const = 0;
  };

  static GURL Get(const ContextData& frame,
                  const GURL& document_url,
                  MatchOriginAsFallbackBehavior match_origin_as_fallback,
                  bool allow_inaccessible_parents);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_CONTENT_SCRIPT_INJECTION_URL_GETTER_H_
