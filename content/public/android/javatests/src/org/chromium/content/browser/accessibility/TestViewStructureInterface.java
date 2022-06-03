// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.os.Bundle;

/**
 * Interface for an implementation of ViewStructure that allows us
 * to print it out as a string and assert that the data in the
 * structure is correct.
 *
 * This is needed because if AssistViewStructureTest references
 * TestViewStructure in any way, the entire test APK fails to run
 * if the SDK version is less than M.
 */
interface TestViewStructureInterface {
    /**
     * Because ViewStructure can be populated asynchronously,
     * returns true if it's complete - i.e. if every call to
     * asyncNewChild has been balanced by a call to asyncCommit.
     */
    public boolean isDone();

    /**
     * Get the extras bundle for this ViewStructure node.
     */
    public Bundle getExtras();

    /**
     * Get the number of children.
     */
    public int getChildCount();

    /**
     * Get a child by index.
     */
    public TestViewStructureInterface getChild(int childIndex);

    /**
     * Dump HTML tag names in toString.
     */
    public void dumpHtmlTags();

    /**
     * Get the Android View-style class name.
     */
    public String getClassName();

    /** Get the text size. */
    public float getTextSize();

    /** Get the foreground color. */
    public int getFgColor();

    /** Get the background color. */
    public int getBgColor();

    /** Get the style, @see android.view.ViewStructure#setTestStyle */
    public int getStyle();

    /** @see android.view.ViewStructure#getText */
    public CharSequence getText();

    /** @see android.view.ViewStructure#getTextSelectionStart */
    public int getTextSelectionStart();

    /** @see android.view.ViewStructure#getTextSelectionEnd */
    public int getTextSelectionEnd();
}
