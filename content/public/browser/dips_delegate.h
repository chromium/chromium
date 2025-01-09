// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DIPS_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DIPS_DELEGATE_H_

#include <stdint.h>

#include "content/common/content_export.h"
#include "content/public/browser/browsing_data_remover.h"

namespace content {

// DipsDelegate is an interface that lets the //content layer
// provide embedder specific configuration for DIPS (Bounce Tracking
// Mitigations).
//
// Instances can be obtained via
// ContentBrowserClient::CreateDipsDelegate().
//
// TODO: crbug.com/387281262 - move methods to ContentBrowserClient and delete
// this class.
class CONTENT_EXPORT DipsDelegate {
 public:
  virtual ~DipsDelegate();

  // DIPS keeps separate records of storage and interactions for relevant sites.
  // It clears storage records for sites when their cookies are deleted, and
  // clears interaction records for sites when this method returns true, given
  // the `remove_mask` that a client passed to BrowsingDataRemover::Remove().
  virtual bool ShouldDeleteInteractionRecords(uint64_t remove_mask) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DIPS_DELEGATE_H_
