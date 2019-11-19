// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import org.chromium.base.annotations.MainDex;

/**
 * This is needed to register multiple PrivilegedProcess services so that we can have
 * more than one unsandboxed process.
 */
@MainDex
public class PrivilegedProcessService1 extends PrivilegedProcessService {}
