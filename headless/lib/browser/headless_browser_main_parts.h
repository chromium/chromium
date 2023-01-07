// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_MAIN_PARTS_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_parts.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_export.h"

#if defined(HEADLESS_USE_PREFS)
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#endif

#if defined(HEADLESS_USE_POLICY)
#include "headless/lib/browser/policy/headless_browser_policy_connector.h"
#endif

namespace device {
class GeolocationManager;
}  // namespace device

namespace headless {

class HeadlessBrowserImpl;

class HEADLESS_EXPORT HeadlessBrowserMainParts
    : public content::BrowserMainParts {
 public:
  explicit HeadlessBrowserMainParts(HeadlessBrowserImpl* browser);

  HeadlessBrowserMainParts(const HeadlessBrowserMainParts&) = delete;
  HeadlessBrowserMainParts& operator=(const HeadlessBrowserMainParts&) = delete;

  ~HeadlessBrowserMainParts() override;

  // content::BrowserMainParts implementation:
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostMainMessageLoopRun() override;
#if BUILDFLAG(IS_MAC)
  void PreCreateMainMessageLoop() override;
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  void PostCreateMainMessageLoop() override;
#endif
  void QuitMainMessageLoop();

#if BUILDFLAG(IS_MAC)
  device::GeolocationManager* GetGeolocationManager();
  void SetGeolocationManagerForTesting(
      std::unique_ptr<device::GeolocationManager> fake_geolocation_manager);
#endif

#if defined(HEADLESS_USE_PREFS)
  PrefService* GetPrefs() { return local_state_.get(); }
#endif

#if defined(HEADLESS_USE_POLICY)
  policy::PolicyService* GetPolicyService();
#endif

 private:
  void MaybeStartLocalDevToolsHttpHandler();
#if defined(HEADLESS_USE_PREFS)
  void CreatePrefService();
#endif

  raw_ptr<HeadlessBrowserImpl> browser_;    // Not owned.

#if defined(HEADLESS_USE_POLICY)
  std::unique_ptr<policy::HeadlessBrowserPolicyConnector> policy_connector_;
#endif

#if defined(HEADLESS_USE_PREFS)
  std::unique_ptr<PrefService> local_state_;
#endif

  bool devtools_http_handler_started_ = false;
  base::OnceClosure quit_main_message_loop_;
#if BUILDFLAG(IS_MAC)
  std::unique_ptr<device::GeolocationManager> geolocation_manager_;
#endif
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_MAIN_PARTS_H_
