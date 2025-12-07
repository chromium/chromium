// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_devtools_frontend.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_devtools_bindings.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "base/command_line.h"
#include "content/shell/common/shell_switches.h"
#endif

namespace content {

namespace {
static GURL GetFrontendURL() {
  int port = ShellDevToolsManagerDelegate::GetHttpHandlerPort();
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const char* query_string = "";
#else
  const char* query_string = "?targetType=tab";
#endif

  return GURL(base::StringPrintf(
      "http://127.0.0.1:%d/devtools/devtools_app.html%s", port, query_string));
}
}  // namespace

// static
ShellDevToolsFrontend* ShellDevToolsFrontend::Show(
    WebContents* inspected_contents) {
  Shell* shell = Shell::CreateNewWindow(inspected_contents->GetBrowserContext(),
                                        GURL(), nullptr, gfx::Size());
  ShellDevToolsFrontend* devtools_frontend =
      new ShellDevToolsFrontend(shell, inspected_contents);
  shell->LoadURL(GetFrontendURL());
  return devtools_frontend;
}

void ShellDevToolsFrontend::Activate() {
  frontend_shell_->ActivateContents(frontend_shell_->web_contents());
}

void ShellDevToolsFrontend::InspectElementAt(int x, int y) {
  devtools_bindings_->InspectElementAt(x, y);
}

void ShellDevToolsFrontend::Close() {
  frontend_shell_->Close();
}

void ShellDevToolsFrontend::PrimaryMainDocumentElementAvailable() {
  devtools_bindings_->Attach();
}

void ShellDevToolsFrontend::WebContentsDestroyed() {
  delete this;
}

ShellDevToolsFrontend::ShellDevToolsFrontend(Shell* frontend_shell,
                                             WebContents* inspected_contents)
    : WebContentsObserver(frontend_shell->web_contents()),
      frontend_shell_(frontend_shell),
      devtools_bindings_(
          new ShellDevToolsBindings(frontend_shell->web_contents(),
                                    inspected_contents,
                                    this)) {}

ShellDevToolsFrontend::~ShellDevToolsFrontend() {}

base::WeakPtr<ShellDevToolsFrontend> ShellDevToolsFrontend::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace content
