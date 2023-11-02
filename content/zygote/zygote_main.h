// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ZYGOTE_ZYGOTE_MAIN_H_
#define CONTENT_ZYGOTE_ZYGOTE_MAIN_H_

#include <memory>
#include <vector>

namespace content {

class ZygoteForkDelegate;

// |delegate| must outlive this call.
bool ZygoteMain(
    std::vector<std::unique_ptr<ZygoteForkDelegate>> fork_delegates);

}  // namespace content

#endif  // CONTENT_ZYGOTE_ZYGOTE_MAIN_H_
