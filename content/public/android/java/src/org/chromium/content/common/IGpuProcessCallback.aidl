// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.common;

import org.chromium.content.common.SurfaceWrapper;
import android.view.Surface;

interface IGpuProcessCallback {

  oneway void forwardSurfaceForSurfaceRequest(
      in UnguessableToken requestToken, in Surface surface);

  SurfaceWrapper getViewSurface(int surfaceId);
}
