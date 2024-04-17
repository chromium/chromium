// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SECURITY_COOP_COOP_SWAP_RESULT_H_
#define CONTENT_BROWSER_SECURITY_COOP_COOP_SWAP_RESULT_H_

namespace content {

// This enum indicates what the comparison of Cross-Origin-Opener-Policy headers
// has yielded.
enum class CoopSwapResult {
  // Indicates that no BrowsingContext group swap is required, based on COOP
  // values.
  kNoSwap,
  // Indicates that a BrowsingContext group swap is required, but that we should
  // use a BrowsingContext group that is "related", preserving restricted
  // openers.
  kSwapRelated,
  // Indicates that a BrowsingContext group swap is required, and that should
  // sever all links between the two BrowsingContext groups, opener, names, etc.
  kSwap
};

}  // namespace content

#endif  // CONTENT_BROWSER_SECURITY_COOP_COOP_SWAP_RESULT_H_
