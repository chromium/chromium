// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBNN_WEBNN_COMPILER_LAUNCHER_H_
#define CONTENT_BROWSER_WEBNN_WEBNN_COMPILER_LAUNCHER_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/public/mojom/webnn_compiler_service.mojom-forward.h"

namespace content {

// Launches the WebNN Compiler utility process and returns its mojo remote.
// The caller is responsible for injecting the remote into the GPU process.
// Runs in the kWebNNModelCompilation sandbox (see WebNNCompilerService mojom).
//
// EP package info is delivered to the Compiler process via the
// CreateCompilerContext mojom call, not at launch time.
// Must be called on the UI thread.
CONTENT_EXPORT mojo::Remote<webnn::mojom::WebNNCompilerService>
LaunchWebNNCompilerProcess();

}  // namespace content

#endif  // CONTENT_BROWSER_WEBNN_WEBNN_COMPILER_LAUNCHER_H_
