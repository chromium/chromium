// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_APP_HEADLESS_SHELL_H_
#define HEADLESS_APP_HEADLESS_SHELL_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/devtools/simple_devtools_protocol_client/simple_devtools_protocol_client.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_web_contents.h"
#include "url/gurl.h"

class GURL;

namespace headless {

// An application which implements a simple headless browser.
class HeadlessShell : public HeadlessWebContents::Observer {
 public:
  HeadlessShell();

  HeadlessShell(const HeadlessShell&) = delete;
  HeadlessShell& operator=(const HeadlessShell&) = delete;

  ~HeadlessShell() override;

  void OnBrowserStart(HeadlessBrowser* browser);

 private:
  // HeadlessWebContents::Observer implementation:
  void DevToolsTargetReady() override;
  void HeadlessWebContentsDestroyed() override;

  void OnTargetCrashed(const base::Value::Dict&);
  void OnLoadEventFired(const base::Value::Dict&);
  void OnVirtualTimeBudgetExpired(const base::Value::Dict&);

  void Detach();
  void ShutdownSoon();
  void Shutdown();

  void FetchTimeout();

  void OnCommandLineURL(const GURL& url);

  void PollReadyState();

  void OnEvaluateReadyStateResult(base::Value::Dict result);

  void OnPageReady();

  void FetchDom();
  void OnEvaluateFetchDomResult(base::Value::Dict result);

  void InputExpression();
  void OnEvaluateExpressionResult(base::Value::Dict result);

  void CaptureScreenshot();
  void OnCaptureScreenshotResult(base::Value::Dict result);

  void PrintToPDF();
  void OnPrintToPDFDone(base::Value::Dict result);

  void WriteFile(const std::string& file_path_switch,
                 const std::string& default_file_name,
                 std::string data);
  void OnWriteFileDone(bool success);

  GURL url_;
  raw_ptr<HeadlessBrowser> browser_ = nullptr;  // Not owned.
  simple_devtools_protocol_client::SimpleDevToolsProtocolClient
      devtools_client_;
  raw_ptr<HeadlessWebContents> web_contents_ = nullptr;
  raw_ptr<HeadlessBrowserContext> browser_context_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  bool processed_page_ready_ = false;
  bool shutdown_pending_ = false;

  base::WeakPtrFactory<HeadlessShell> weak_factory_{this};
};

}  // namespace headless

#endif  // HEADLESS_APP_HEADLESS_SHELL_H_
