// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_STARTUP_DATA_H_
#define CONTENT_PUBLIC_COMMON_STARTUP_DATA_H_

namespace content {

// Data that //content routes through its embedder which should be handed back
// to //content when the embedder launches it.
struct StartupData {
  virtual ~StartupData() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_STARTUP_DATA_H_
