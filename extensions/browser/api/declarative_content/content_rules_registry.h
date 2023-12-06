// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_CONTENT_CONTENT_RULES_REGISTRY_H__
#define EXTENSIONS_BROWSER_API_DECLARATIVE_CONTENT_CONTENT_RULES_REGISTRY_H__

#include <string>

#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative/rules_registry.h"

namespace content {
class BrowserContext;
class NavigationHandle;
class WebContents;
}

namespace extensions {

// This class acts as an //extensions-side interface for ContentRulesRegistry
// to allow RulesRegistryService to be moved to //extensions.
// TODO(wjmaclean): Remove this once ContentRulesRegistry moves to
// //extensions.
//
// Note: when dealing with WebContents associated with OffTheRecord contexts,
// functions on this interface must be invoked for BOTH the Original and
// OffTheRecord ContentRulesRegistry instances. This is necessary because the
// Original ContentRulesRegistry instance handles spanning-mode incognito
// extensions.
class ContentRulesRegistry : public RulesRegistry {
 public:
  ContentRulesRegistry(content::BrowserContext* browser_context,
                       const std::string& event_name,
                       RulesCacheDelegate* cache_delegate,
                       int rules_registry_id)
      : RulesRegistry(browser_context,
                      event_name,
                      cache_delegate,
                      rules_registry_id) {}

  ContentRulesRegistry(const ContentRulesRegistry&) = delete;
  ContentRulesRegistry& operator=(const ContentRulesRegistry&) = delete;

  // Notifies the registry that it should evaluate rules for |contents|.
  virtual void MonitorWebContentsForRuleEvaluation(
      content::WebContents* contents) = 0;

  // Applies all content rules given that a tab was just navigated.
  // This corresponds to the notification of the same name in
  // content::WebContentsObserver.
  virtual void DidFinishNavigation(
      content::WebContents* tab,
      content::NavigationHandle* navigation_handle) = 0;

  // Applies the given CSS selector rules to |contents|.
  virtual void OnWatchedPageChanged(
      content::WebContents* contents,
      const std::vector<std::string>& css_selectors) = 0;

  // Notifies the registry that the given |contents| is being
  // destroyed.
  virtual void WebContentsDestroyed(content::WebContents* contents) = 0;

 protected:
  ~ContentRulesRegistry() override {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_CONTENT_CONTENT_RULES_REGISTRY_H__
