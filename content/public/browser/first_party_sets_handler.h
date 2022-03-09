// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FIRST_PARTY_SETS_HANDLER_H_
#define CONTENT_PUBLIC_BROWSER_FIRST_PARTY_SETS_HANDLER_H_

#include "base/files/file.h"
#include "content/common/content_export.h"

namespace content {

// The FirstPartySetsHandler class allows an embedder to provide
// First-Party Sets inputs from custom sources.
class CONTENT_EXPORT FirstPartySetsHandler {
 public:
  virtual ~FirstPartySetsHandler() = default;

  // Returns the singleton instance.
  static FirstPartySetsHandler* GetInstance();

  // Sets the First-Party Sets data from `sets_file` to initialize the
  // FirstPartySets instance. `sets_file` is expected to contain a sequence of
  // newline-delimited JSON records. Each record is a set declaration in the
  // format specified here: https://github.com/privacycg/first-party-sets.
  //
  // Embedder should call this method as early as possible during browser
  // startup if First-Party Sets are enabled, since no First-Party Sets queries
  // are answered until initialization is complete.
  virtual void SetPublicFirstPartySets(base::File sets_file) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FIRST_PARTY_SETS_HANDLER_H_
