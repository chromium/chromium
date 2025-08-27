// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import org.chromium.build.annotations.NullMarked;

/** A wrapper interface of Magnifier class. */
@NullMarked
public interface MagnifierWrapper {
    /** Wrapper of {@link Magnifier#show()}. */
    void show(float x, float y);

    /** Wrapper of {@link Magnifier#dismiss()}. */
    void dismiss();

    /** To check if this MagnifierWrapper is available to show. */
    boolean isAvailable();

    /** Only implemented on MagnifierSurfaceControl. */
    void childLocalSurfaceIdChanged();
}
