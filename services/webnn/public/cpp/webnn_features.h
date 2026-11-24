// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_WEBNN_FEATURES_H_
#define SERVICES_WEBNN_PUBLIC_CPP_WEBNN_FEATURES_H_

namespace webnn {

// Returns true if WebNN is configured in an unsafe mode, i.e. without the
// compiler utility process (on Windows and Apple platforms) or in-renderer
// LiteRT GPU execution enabled.
bool IsWebNNUnsafe();

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_WEBNN_FEATURES_H_
