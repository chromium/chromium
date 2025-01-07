// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.build.annotations.UsedByReflection;

/** Represents the zoom information for a host. */
@UsedByReflection("host_zoom_map_impl.cc")
public final class SiteZoomInfo {
    public final String host;
    public final double zoomLevel;

    public SiteZoomInfo(String host, double zoomLevel) {
        this.host = host;
        this.zoomLevel = zoomLevel;
    }
}
