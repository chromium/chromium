// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

/**
 * An opaque interface representing parameters for
 * NavigationController::LoadUrlParams::AdditionalNavigationParams. This is used to route
 * information about the initiator frame to the navigation request, which is needed for event-level
 * reporting to function properly.
 */
public interface AdditionalNavigationParams {
    /** Releases the native resources associated with these parameters. */
    void destroy();
}
