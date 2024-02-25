// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package androidx.window.layout;

import android.content.Context;

/**
 * Placeholder so that things that use it compile.
 */
public interface WindowInfoTracker {
  public static WindowInfoTracker getOrCreate(Context context) {
    return new WindowInfoTracker() {};
  }
}
