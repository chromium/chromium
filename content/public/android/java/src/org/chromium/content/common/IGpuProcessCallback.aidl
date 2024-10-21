// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.common;

import android.view.Surface;
import android.window.InputTransferToken;

import org.chromium.content.common.InputTransferTokenWrapper;
import org.chromium.content.common.SurfaceWrapper;

interface IGpuProcessCallback {

  oneway void forwardSurfaceForSurfaceRequest(
      in UnguessableToken requestToken, in Surface surface);

  SurfaceWrapper getViewSurface(int surfaceId);

  // Send the input transfer token from Viz to Browser so that the Browser can use it later
  // to transfer touch sequence.
  oneway void forwardInputTransferToken(int surfaceId, in InputTransferTokenWrapper vizInputToken);
}
