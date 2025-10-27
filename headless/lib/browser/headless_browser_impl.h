// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_IMPL_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/public/browser/devtools_agent_host.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_export.h"
#include "ui/gfx/geometry/rect.h"

#if defined(HEADLESS_USE_POLICY)
#include "headless/lib/browser/policy/headless_browser_policy_connector.h"

namespace policy {
class PolicyService;
}
#endif

#if defined(HEADLESS_USE_PREFS)
class PrefService;
#endif

namespace os_crypt_async {
class OSCryptAsync;
}

namespace ui {
class Compositor;
}

namespace headless {

class HeadlessBrowserContextImpl;
class HeadlessRequestContextManager;
class HeadlessWebContentsImpl;
class HeadlessPlatformDelegate;

extern const base::FilePath::CharType kDefaultProfileName[];

// Exported for tests.
class HEADLESS_EXPORT HeadlessBrowserImpl : public HeadlessBrowser {
 public:
  explicit HeadlessBrowserImpl(
      base::OnceCallback<void(HeadlessBrowser*)> on_start_callback);

  HeadlessBrowserImpl(const HeadlessBrowserImpl&) = delete;
  HeadlessBrowserImpl& operator=(const HeadlessBrowserImpl&) = delete;

  ~HeadlessBrowserImpl() override;

  // HeadlessBrowser implementation:
  HeadlessBrowserContext::Builder CreateBrowserContextBuilder() override;
  scoped_refptr<base::SingleThreadTaskRunner> BrowserMainThread()
      const override;
  void Shutdown() override;
  std::vector<HeadlessBrowserContext*> GetAllBrowserContexts() override;
  HeadlessBrowserContext* GetBrowserContextForId(
      const std::string& id) override;
  void SetDefaultBrowserContext(
      HeadlessBrowserContext* browser_context) override;
  HeadlessBrowserContext* GetDefaultBrowserContext() override;

  void SetOptions(HeadlessBrowser::Options options);
  HeadlessBrowser::Options* options() { return &options_.value(); }

  HeadlessBrowserContext* CreateBrowserContext(
      HeadlessBrowserContext::Builder* builder);
  // Close given |browser_context| and delete it
  // (all web contents associated with it go away too).
  void DestroyBrowserContext(HeadlessBrowserContextImpl* browser_context);

  HeadlessWebContentsImpl* GetWebContentsForWindowId(const int window_id);

  base::WeakPtr<HeadlessBrowserImpl> GetWeakPtr();

  bool ShouldStartDevToolsServer();

  void PreMainMessageLoopRun();
  void WillRunMainMessageLoop(base::RunLoop& run_loop);
  void PostMainMessageLoopRun();

  void InitializeWebContents(HeadlessWebContentsImpl* web_contents);
  void SetWebContentsBounds(HeadlessWebContentsImpl* web_contents,
                            const gfx::Rect& bounds);
  ui::Compositor* GetCompositor(HeadlessWebContentsImpl* web_contents);

  void ShutdownWithExitCode(int exit_code);

  int exit_code() const { return exit_code_; }

  os_crypt_async::OSCryptAsync* os_crypt_async() {
    return os_crypt_async_.get();
  }

#if defined(HEADLESS_USE_PREFS)
  void CreatePrefService();
  PrefService* GetPrefs();
#endif

#if defined(HEADLESS_USE_POLICY)
  policy::PolicyService* GetPolicyService();
#endif

 private:
  void CreateOSCryptAsync();

  base::OnceCallback<void(HeadlessBrowser*)> on_start_callback_;
  std::optional<HeadlessBrowser::Options> options_;
  std::unique_ptr<HeadlessPlatformDelegate> platform_delegate_;

  int exit_code_ = 0;

  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;

  base::flat_map<std::string, std::unique_ptr<HeadlessBrowserContextImpl>>
      browser_contexts_;
  raw_ptr<HeadlessBrowserContext, AcrossTasksDanglingUntriaged>
      default_browser_context_ = nullptr;
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  std::unique_ptr<HeadlessRequestContextManager>
      system_request_context_manager_;
  base::OnceClosure quit_main_message_loop_;

#if defined(HEADLESS_USE_PREFS)
  std::unique_ptr<PrefService> local_state_;
#endif

#if defined(HEADLESS_USE_POLICY)
  std::unique_ptr<policy::HeadlessBrowserPolicyConnector> policy_connector_;
#endif

  base::WeakPtrFactory<HeadlessBrowserImpl> weak_ptr_factory_{this};
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_IMPL_H_
