// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/requirements_checker.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_feature_checker.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/requirements_info.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "gpu/config/gpu_feature_type.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

RequirementsChecker::RequirementsChecker(
    scoped_refptr<const Extension> extension)
    : PreloadCheck(extension) {}

RequirementsChecker::~RequirementsChecker() {}

void RequirementsChecker::Start(ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const RequirementsInfo& requirements =
      RequirementsInfo::GetRequirements(extension());

#if !defined(USE_AURA)
  if (requirements.window_shape)
    errors_.insert(WINDOW_SHAPE_NOT_SUPPORTED);
#endif

  callback_ = std::move(callback);
  if (requirements.webgl) {
    webgl_checker_ = content::GpuFeatureChecker::Create(
        gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
        base::Bind(&RequirementsChecker::VerifyWebGLAvailability,
                   weak_ptr_factory_.GetWeakPtr()));
    webgl_checker_->CheckGpuFeatureAvailability();
  } else {
    PostRunCallback();
  }
}

base::string16 RequirementsChecker::GetErrorMessage() const {
  // Join the error messages into one string.
  std::vector<std::string> messages;
  if (errors_.count(WEBGL_NOT_SUPPORTED)) {
    messages.push_back(
        l10n_util::GetStringUTF8(IDS_EXTENSION_WEBGL_NOT_SUPPORTED));
  }
#if !defined(USE_AURA)
  if (errors_.count(WINDOW_SHAPE_NOT_SUPPORTED)) {
    messages.push_back(
        l10n_util::GetStringUTF8(IDS_EXTENSION_WINDOW_SHAPE_NOT_SUPPORTED));
  }
#endif

  return base::UTF8ToUTF16(base::JoinString(messages, " "));
}

void RequirementsChecker::VerifyWebGLAvailability(bool available) {
  if (!available)
    errors_.insert(WEBGL_NOT_SUPPORTED);
  PostRunCallback();
}

void RequirementsChecker::PostRunCallback() {
  // TODO(michaelpg): This always forces the callback to run asynchronously
  // to maintain the assumption in
  // ExtensionService::LoadExtensionsFromCommandLineFlag(). Remove these helper
  // functions after crbug.com/708354 is addressed.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&RequirementsChecker::RunCallback,
                                weak_ptr_factory_.GetWeakPtr()));
}

void RequirementsChecker::RunCallback() {
  DCHECK(callback_);
  std::move(callback_).Run(errors_);
}

}  // namespace extensions
