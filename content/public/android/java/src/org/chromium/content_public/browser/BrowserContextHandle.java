// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

/** An interface that provides access to a native BrowserContext. */
public interface BrowserContextHandle {
    /** @return A pointer to the native BrowserContext that this object wraps. */
    long getNativeBrowserContextPointer();
}
