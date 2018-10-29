// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test_utils.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "net/base/filename_util.h"

namespace content {

base::FilePath GetTestFilePath(const char* dir, const char* file) {
  base::FilePath path;
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::PathService::Get(DIR_TEST_DATA, &path);
  if (dir)
    path = path.AppendASCII(dir);
  return path.AppendASCII(file);
}

GURL GetTestUrl(const char* dir, const char* file) {
  return net::FilePathToFileURL(GetTestFilePath(dir, file));
}

void NavigateToURLBlockUntilNavigationsComplete(Shell* window,
                                                const GURL& url,
                                                int number_of_navigations) {
  NavigateToURLBlockUntilNavigationsComplete(window->web_contents(), url,
                                             number_of_navigations);
}

void ReloadBlockUntilNavigationsComplete(Shell* window,
                                         int number_of_navigations) {
  WaitForLoadStop(window->web_contents());
  TestNavigationObserver same_tab_observer(window->web_contents(),
                                           number_of_navigations);

  window->Reload();
  same_tab_observer.Wait();
}

void ReloadBypassingCacheBlockUntilNavigationsComplete(
    Shell* window,
    int number_of_navigations) {
  WaitForLoadStop(window->web_contents());
  TestNavigationObserver same_tab_observer(window->web_contents(),
                                           number_of_navigations);

  window->ReloadBypassingCache();
  same_tab_observer.Wait();
}

void LoadDataWithBaseURL(Shell* window,
                         const GURL& url,
                         const std::string& data,
                         const GURL& base_url) {
  WaitForLoadStop(window->web_contents());
  TestNavigationObserver same_tab_observer(window->web_contents(), 1);

  window->LoadDataWithBaseURL(url, data, base_url);
  same_tab_observer.Wait();
}

bool NavigateToURL(Shell* window, const GURL& url) {
  return NavigateToURL(window->web_contents(), url);
}

bool NavigateToURLFromRenderer(const ToRenderFrameHost& adapter,
                               const GURL& url) {
  RenderFrameHost* rfh = adapter.render_frame_host();
  TestFrameNavigationObserver nav_observer(rfh);
  if (!ExecuteScript(rfh, "location = '" + url.spec() + "';"))
    return false;
  nav_observer.Wait();
  return nav_observer.last_committed_url() == url;
}

bool NavigateToURLAndExpectNoCommit(Shell* window, const GURL& url) {
  NavigationEntry* old_entry =
      window->web_contents()->GetController().GetLastCommittedEntry();
  NavigateToURLBlockUntilNavigationsComplete(window, url, 1);
  NavigationEntry* new_entry =
      window->web_contents()->GetController().GetLastCommittedEntry();
  return old_entry == new_entry;
}

void WaitForAppModalDialog(Shell* window) {
  ShellJavaScriptDialogManager* dialog_manager =
      static_cast<ShellJavaScriptDialogManager*>(
          window->GetJavaScriptDialogManager(window->web_contents()));

  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner();
  dialog_manager->set_dialog_request_callback(runner->QuitClosure());
  runner->Run();
}

RenderFrameHost* ConvertToRenderFrameHost(Shell* shell) {
  return shell->web_contents()->GetMainFrame();
}

void LookupAndLogNameAndIdOfFirstCamera() {
  DCHECK(BrowserMainLoop::GetInstance());
  MediaStreamManager* media_stream_manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();
  base::RunLoop run_loop;
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          [](MediaStreamManager* media_stream_manager,
             base::Closure quit_closure) {
            media_stream_manager->video_capture_manager()->EnumerateDevices(
                base::BindOnce(
                    [](base::Closure quit_closure,
                       const media::VideoCaptureDeviceDescriptors&
                           descriptors) {
                      if (descriptors.empty()) {
                        LOG(WARNING) << "No camera found";
                        return;
                      }
                      LOG(INFO) << "Using camera "
                                << descriptors.front().display_name() << " ("
                                << descriptors.front().model_id << ")";
                      std::move(quit_closure).Run();
                    },
                    std::move(quit_closure)));
          },
          media_stream_manager, run_loop.QuitClosure()));
  run_loop.Run();
}

ShellAddedObserver::ShellAddedObserver() : shell_(nullptr) {
  Shell::SetShellCreatedCallback(
      base::Bind(&ShellAddedObserver::ShellCreated, base::Unretained(this)));
}

ShellAddedObserver::~ShellAddedObserver() {}

Shell* ShellAddedObserver::GetShell() {
  if (shell_)
    return shell_;

  runner_ = new MessageLoopRunner();
  runner_->Run();
  return shell_;
}

void ShellAddedObserver::ShellCreated(Shell* shell) {
  DCHECK(!shell_);
  shell_ = shell;
  if (runner_.get())
    runner_->QuitClosure().Run();
}

}  // namespace content
