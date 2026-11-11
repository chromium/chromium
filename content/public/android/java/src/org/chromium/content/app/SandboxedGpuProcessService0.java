// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import org.chromium.build.annotations.NullMarked;

/**
 * This is needed to register multiple SandboxedGpuProcess services so that isolated GPU processes
 * can be bound via bindIsolatedService.
 */
@NullMarked
public class SandboxedGpuProcessService0 extends SandboxedGpuProcessService {}
