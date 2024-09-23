// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import org.chromium.build.annotations.UsedByReflection;

/**
 * Sandboxed Services inherit from this class. We enforce the privileged/sandboxed distinction by
 * type-checking objects against this parent class.
 */
@UsedByReflection("Subclass names constructed by appending a number to this class's name")
public class SandboxedProcessService extends ContentChildProcessService {}
