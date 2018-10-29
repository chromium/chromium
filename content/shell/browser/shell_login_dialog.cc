// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_login_dialog.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/auth.h"
#include "ui/gfx/text_elider.h"

namespace content {

// static
scoped_refptr<ShellLoginDialog> ShellLoginDialog::Create(
    net::AuthChallengeInfo* auth_info,
    LoginAuthRequiredCallback auth_required_callback) {
  auto ret = base::WrapRefCounted(
      new ShellLoginDialog(std::move(auth_required_callback)));
  ret->Init(auth_info);
  return ret;
}

ShellLoginDialog::ShellLoginDialog(
    LoginAuthRequiredCallback auth_required_callback)
    : auth_required_callback_(std::move(auth_required_callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void ShellLoginDialog::Init(net::AuthChallengeInfo* auth_info) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ShellLoginDialog::PrepDialog, this,
          url_formatter::FormatOriginForSecurityDisplay(auth_info->challenger),
          base::UTF8ToUTF16(auth_info->realm)));
}

void ShellLoginDialog::OnRequestCancelled() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&ShellLoginDialog::PlatformRequestCancelled, this));
}

void ShellLoginDialog::UserAcceptedAuth(const base::string16& username,
                                        const base::string16& password) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ShellLoginDialog::SendAuthToRequester, this, true,
                     username, password));
}

void ShellLoginDialog::UserCancelledAuth() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ShellLoginDialog::SendAuthToRequester, this, false,
                     base::string16(), base::string16()));
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&ShellLoginDialog::PlatformCleanUp, this));
}

ShellLoginDialog::~ShellLoginDialog() {
  // Cannot post any tasks here; this object is going away and cannot be
  // referenced/dereferenced.
}

#if !defined(OS_MACOSX)
// Bogus implementations for linking. They are never called because
// ResourceDispatcherHostDelegate::CreateLoginDelegate returns NULL.
// TODO: implement ShellLoginDialog for other platforms, drop this #if
void ShellLoginDialog::PlatformCreateDialog(const base::string16& message) {}
void ShellLoginDialog::PlatformCleanUp() {}
void ShellLoginDialog::PlatformRequestCancelled() {}
#endif

void ShellLoginDialog::PrepDialog(const base::string16& host,
                                  const base::string16& realm) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // The realm is controlled by the remote server, so there is no reason to
  // believe it is of a reasonable length.
  base::string16 elided_realm;
  gfx::ElideString(realm, 120, &elided_realm);

  base::string16 explanation =
      base::ASCIIToUTF16("The server ") + host +
      base::ASCIIToUTF16(" requires a username and password.");

  if (!elided_realm.empty()) {
    explanation += base::ASCIIToUTF16(" The server says: ");
    explanation += elided_realm;
    explanation += base::ASCIIToUTF16(".");
  }

  PlatformCreateDialog(explanation);
}

void ShellLoginDialog::SendAuthToRequester(bool success,
                                           const base::string16& username,
                                           const base::string16& password) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!auth_required_callback_.is_null()) {
    if (success) {
      std::move(auth_required_callback_)
          .Run(net::AuthCredentials(username, password));
    } else {
      std::move(auth_required_callback_).Run(base::nullopt);
    }
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&ShellLoginDialog::PlatformCleanUp, this));
}

}  // namespace content
