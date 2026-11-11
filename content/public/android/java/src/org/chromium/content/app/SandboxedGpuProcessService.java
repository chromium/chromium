// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.UsedByReflection;

/**
 * Sandboxed GPU Services inherit from this class. Used to host isolated GPU processes bound via
 * bindIsolatedService.
 */
@UsedByReflection("Subclass names constructed by appending a number to this class's name")
@NullMarked
public class SandboxedGpuProcessService extends ContentChildProcessService {}
