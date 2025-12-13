// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/blocklist_check.h"

#include "base/functional/bind.h"
#include "extensions/browser/blocklist.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

BlocklistCheck::BlocklistCheck(Blocklist* blocklist,
                               scoped_refptr<const Extension> extension)
    : PreloadCheck(extension), blocklist_(blocklist) {}

BlocklistCheck::~BlocklistCheck() = default;

void BlocklistCheck::Start(ResultCallback callback) {
  callback_ = std::move(callback);

  blocklist_->IsBlocklisted(
      extension()->id(),
      base::BindOnce(&BlocklistCheck::OnBlocklistedStateRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BlocklistCheck::OnBlocklistedStateRetrieved(
    BlocklistState blocklist_state) {
  Errors errors;
  if (blocklist_state == BlocklistState::BLOCKLISTED_MALWARE) {
    errors.insert(PreloadCheck::Error::kBlocklistedId);
  } else if (blocklist_state == BlocklistState::BLOCKLISTED_UNKNOWN) {
    errors.insert(PreloadCheck::Error::kBlocklistedUnknown);
  }
  std::move(callback_).Run(errors);
}

}  // namespace extensions
