// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.view.KeyEvent;

/** Interface for the classes that need to be notified of IME changes. */
public interface ImeEventObserver {
    /** Called to notify the delegate about synthetic/real key events before sending to renderer. */
    default void onImeEvent() {}

    /**
     * Called when the focused node attribute is updated.
     * @param editable {@code true} if the node becomes editable; else {@code false}.
     * @param password indicates the node is of type password if {@code true}.
     */
    default void onNodeAttributeUpdated(boolean editable, boolean password) {}

    /**
     * Called to notify the delegate that an IME called InputConnection#sendKeyEvent().
     * @param event The event passed to InputConnection#sendKeyEvent().
     */
    default void onBeforeSendKeyEvent(KeyEvent event) {}
}
