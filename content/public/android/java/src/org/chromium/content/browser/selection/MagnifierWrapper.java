// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

/** A wrapper interface of Magnifier class. */
public interface MagnifierWrapper {
    /** Wrapper of {@link Magnifier#show()}. */
    public void show(float x, float y);

    /** Wrapper of {@link Magnifier#dismiss()}. */
    public void dismiss();

    /** To check if this MagnifierWrapper is available to show. */
    public boolean isAvailable();

    /** Only implemented on MagnifierSurfaceControl. */
    public void childLocalSurfaceIdChanged();
}
