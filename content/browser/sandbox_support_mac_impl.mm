// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#import "content/browser/sandbox_support_impl.h"
#import "content/browser/theme_helper_mac.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

SandboxSupportImpl::SandboxSupportImpl() = default;

SandboxSupportImpl::~SandboxSupportImpl() = default;

void SandboxSupportImpl::BindReceiver(
    mojo::PendingReceiver<mojom::SandboxSupport> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SandboxSupportImpl::GetSystemColors(GetSystemColorsCallback callback) {
  auto task_runner = GetUIThreadTaskRunner({});
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ThemeHelperMac::DuplicateReadOnlyColorMapRegion,
                     base::Unretained(ThemeHelperMac::GetInstance())),
      std::move(callback));
}

}  // namespace content
