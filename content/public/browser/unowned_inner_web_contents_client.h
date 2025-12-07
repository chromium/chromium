// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_UNOWNED_INNER_WEB_CONTENTS_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_UNOWNED_INNER_WEB_CONTENTS_CLIENT_H_

#include "base/types/pass_key.h"
#include "content/common/content_export.h"

namespace guest_contents {
class GuestContentsHandle;
}  // namespace guest_contents

namespace content {

// A static class that provides PassKeys for clients of unowned inner web
// contents to use WebContents::AttachUnownedInnerWebContents() and
// DetachUnownedInnerWebContents(). These APIs are experimental and are intended
// to be used by //components/guest_contents only. If you intend to add new
// clients, please reach out to //components/guest_contents owners and content
// owners. See an explanation of guest-contents at crbug.com/415626990.
class CONTENT_EXPORT UnownedInnerWebContentsClient {
 public:
  using PassKey = base::PassKey<UnownedInnerWebContentsClient>;

  UnownedInnerWebContentsClient() = delete;

  static PassKey GetPassKeyForTesting();

 private:
  friend class guest_contents::GuestContentsHandle;

  static PassKey GetPassKey();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_UNOWNED_INNER_WEB_CONTENTS_CLIENT_H_
