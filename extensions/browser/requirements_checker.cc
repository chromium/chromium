// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/requirements_checker.h"

#include <string>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
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

RequirementsChecker::~RequirementsChecker() = default;

void RequirementsChecker::Start(ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const RequirementsInfo& requirements =
      RequirementsInfo::GetRequirements(extension());

  callback_ = std::move(callback);
  if (requirements.webgl) {
    scoped_refptr<content::GpuFeatureChecker> webgl_checker =
        content::GpuFeatureChecker::Create(
            gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
            base::BindOnce(&RequirementsChecker::VerifyWebGLAvailability,
                           weak_ptr_factory_.GetWeakPtr()));
    webgl_checker->CheckGpuFeatureAvailability();
  } else {
    RunCallback();
  }
}

std::u16string RequirementsChecker::GetErrorMessage() const {
  if (errors_.empty()) {
    return std::u16string();
  }

  CHECK_EQ(errors_.size(), 1u);
  CHECK(errors_.contains(Error::kWebglNotSupported));
  return l10n_util::GetStringUTF16(IDS_EXTENSION_WEBGL_NOT_SUPPORTED);
}

void RequirementsChecker::VerifyWebGLAvailability(bool available) {
  if (!available) {
    errors_.insert(Error::kWebglNotSupported);
  }

  RunCallback();
}

void RequirementsChecker::RunCallback() {
  DCHECK(callback_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback_).Run(errors_);
}

}  // namespace extensions
