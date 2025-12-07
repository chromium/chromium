/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package androidx.test.espresso.base;

import static androidx.test.espresso.util.Throwables.throwIfUnchecked;

import android.os.Looper;
import android.util.Log;
import android.view.View;
import androidx.annotation.Nullable;
import androidx.test.espresso.RootViewException;
import androidx.test.espresso.base.DefaultFailureHandler.TypedFailureHandler;
import androidx.test.espresso.util.HumanReadables;
import androidx.test.platform.io.PlatformTestStorage;
import androidx.test.platform.io.PlatformTestStorageRegistry;
import androidx.test.services.storage.TestStorageException;
import java.io.IOException;
import java.io.OutputStream;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Supplier;
import org.hamcrest.Matcher;

/**
 * An Espresso failure handler that handles a {@link RootViewException} to dump the full view
 * hierarchy to an artifact file.
 */
class ViewHierarchyExceptionHandler<T extends Throwable & RootViewException>
    extends TypedFailureHandler<T> {
  private static final String TAG = ViewHierarchyExceptionHandler.class.getSimpleName();
  // Max stack length that can be printed without being truncated in android log. Used to
  // display a truncated version of the view hierarchy and still have some extra room for the
  // stack trace.
  private static final int MAX_MSG_SIZE = (64 - 2) * 1024;
  private static final String VIEW_HIERARCHY_CHAR_LIMIT = "view_hierarchy_char_limit";

  private final AtomicInteger failureCount;
  private final Truncater<T> truncater;

  interface Truncater<T> {
    Throwable truncateExceptionMessage(T exception, int msgLen, String viewHierarchyFile);
  }

  public ViewHierarchyExceptionHandler(
      AtomicInteger failureCount,
      Class<T> expectedType,
      Truncater<T> truncater) {
    super(expectedType);
    this.failureCount = failureCount;
    this.truncater = truncater;
  }

  @Override
  public void handleSafely(T exception, Matcher<View> viewMatcher) {
    String viewHierarchyFile = dumpFullViewHierarchyToFile(exception);

    exception.setStackTrace(Thread.currentThread().getStackTrace());

    int msgLen = getMsgLen();

    // Truncate exception message to fit within a printable stack frame dump.
    Throwable error = runOnUiThread(exception, () -> truncater.truncateExceptionMessage(exception, msgLen, viewHierarchyFile));

    throwIfUnchecked(error);
    throw new RuntimeException(error);
  }

  private int getMsgLen() {
    PlatformTestStorage testStorage = PlatformTestStorageRegistry.getInstance();
    try {
      if (testStorage.getInputArgs().containsKey(VIEW_HIERARCHY_CHAR_LIMIT)) {
        String limit = testStorage.getInputArg(VIEW_HIERARCHY_CHAR_LIMIT);
        if (limit != null) {
          return Integer.parseInt(limit);
        }
      }
    } catch (NumberFormatException | TestStorageException e) {
      Log.e(TAG, "Failed to parse input argument " + VIEW_HIERARCHY_CHAR_LIMIT, e);
    }
    return MAX_MSG_SIZE;
  }

  private static <T> T runOnUiThread(RootViewException e, Supplier<T> func) {
    if (Looper.myLooper() == Looper.getMainLooper()) {
      return func.get();
    }

    AtomicReference<T> ret = new AtomicReference<>();
    e.getRootView().post(() -> {
      ret.set(func.get());
      synchronized (ret) {
        ret.notifyAll();
      }
    });
    synchronized (ret) {
      while (ret.get() == null) {
        try {
          ret.wait();
        } catch (InterruptedException unused) {
          // Ignore.
        }
      }
    }
    return ret.get();
  }

  @Nullable
  private String dumpFullViewHierarchyToFile(T error) {
    String viewHierarchyMsg = runOnUiThread(error, () ->
        HumanReadables.getViewHierarchyErrorMessage(
            error.getRootView(),
            /* problemViews= */ null,
            /* errorHeader= */ "",
            /* problemViewSuffix= */ null));

    String viewHierarchyFile = "view-hierarchy-" + failureCount + ".txt";
    try {
      addOutputFile(viewHierarchyFile, viewHierarchyMsg);
      Log.w(
          TAG,
          "The complete view hierarchy is available in artifact file '" + viewHierarchyFile + "'.");
      return viewHierarchyFile;
    } catch (IOException e) {
      // Log and ignore.
      Log.w(TAG, "Failed to save the view hierarchy to file " + viewHierarchyFile, e);
      return null;
    }
  }

  private void addOutputFile(String filename, String content) throws IOException {
    try (OutputStream out = PlatformTestStorageRegistry.getInstance().openOutputFile(filename)) {
      out.write(content.getBytes());
    }
  }
}
