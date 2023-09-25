// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package androidx.window.java.layout;

import android.app.Activity;
import android.content.Context;
import androidx.annotation.UiContext;
import androidx.core.util.Consumer;
import androidx.window.layout.WindowInfoTracker;
import androidx.window.layout.WindowLayoutInfo;
import java.util.concurrent.Executor;

/**
 * Placeholder so that things that use it compile.
 */
public class WindowInfoTrackerCallbackAdapter {

  public WindowInfoTrackerCallbackAdapter(WindowInfoTracker tracker) {
  }
  public void addWindowLayoutInfoListener(Activity activity, Executor executor, Consumer<WindowLayoutInfo> consumer) {
  }
  public void addWindowLayoutInfoListener( @UiContext Context context, Executor executor, Consumer<WindowLayoutInfo> consumer) {
  }
  public void removeWindowLayoutInfoListener(Consumer<WindowLayoutInfo> consumer) {
  }
}
