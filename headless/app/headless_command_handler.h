// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_APP_HEADLESS_COMMAND_HANDLER_H_
#define HEADLESS_APP_HEADLESS_COMMAND_HANDLER_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/devtools/simple_devtools_protocol_client/simple_devtools_protocol_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace headless {

class HeadlessCommandHandler : public content::WebContentsObserver {
 public:
  typedef base::OnceCallback<void()> DoneCallback;

  HeadlessCommandHandler(const HeadlessCommandHandler&) = delete;
  HeadlessCommandHandler& operator=(const HeadlessCommandHandler&) = delete;

  static GURL GetHandlerUrl();

  static void ProcessCommands(content::WebContents* web_contents,
                              GURL target_url,
                              DoneCallback done_callback);

 private:
  using SimpleDevToolsProtocolClient =
      simple_devtools_protocol_client::SimpleDevToolsProtocolClient;

  HeadlessCommandHandler(content::WebContents* web_contents,
                         GURL target_url,
                         DoneCallback done_callback);
  ~HeadlessCommandHandler() override;

  void ExecuteCommands();

  // content::WebContentsObserver implementation:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void WebContentsDestroyed() override;

  void OnTargetCrashed(const base::Value::Dict&);

  void OnCommandsResult(base::Value::Dict result);

  void Done();

  SimpleDevToolsProtocolClient devtools_client_;
  SimpleDevToolsProtocolClient browser_devtools_client_;
  raw_ptr<content::WebContents> web_contents_;
  GURL target_url_;
  DoneCallback done_callback_;

  base::FilePath pdf_file_path_;
  base::FilePath screenshot_file_path_;
};

}  // namespace headless

#endif  // HEADLESS_APP_HEADLESS_COMMAND_HANDLER_H_
