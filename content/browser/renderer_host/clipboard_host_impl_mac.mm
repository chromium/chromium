// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/clipboard_host_impl.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#import "ui/base/cocoa/find_pasteboard.h"

namespace content {
namespace {

// The number of utf16 code units that will be written to the find pasteboard,
// longer texts are silently ignored. This is to prevent that a compromised
// renderer can write unlimited amounts of data into the find pasteboard.
static constexpr size_t kMaxFindPboardStringLength = 4096;

}  // namespace

void ClipboardHostImpl::WriteStringToFindPboard(const std::u16string& text) {
  if (text.length() <= kMaxFindPboardStringLength) {
    NSString* nsText = base::SysUTF16ToNSString(text);
    if (nsText) {
      [[FindPasteboard sharedInstance] setFindText:nsText];
    }
  }
}

}  // namespace content
