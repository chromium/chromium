// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

/** Singleton used for tracking accessibility data during content shell unit tests. */
public class AccessibilityContentShellTestData {
    private static AccessibilityContentShellTestData sInstance;

    public static AccessibilityContentShellTestData getInstance() {
        if (sInstance == null) {
            sInstance = new AccessibilityContentShellTestData();
        }
        return sInstance;
    }

    public static void resetData() {
        sInstance = null;
    }

    public int traverseFromIndex;
    public int traverseToIndex;
    public int selectionFromIndex;
    public int selectionToIndex;
    public String announcementText;
    public int typeWindowContentChangedCount;
    public boolean receivedEvent;
    public boolean receivedAccessibilityFocusEvent;
    public boolean receivedTraversalEvent;
    public boolean receivedSelectionEvent;

    private AccessibilityContentShellTestData() {
        traverseFromIndex = -1;
        traverseToIndex = -1;
        selectionFromIndex = -1;
        selectionToIndex = -1;
        announcementText = "";
        typeWindowContentChangedCount = 0;
        receivedEvent = false;
        receivedAccessibilityFocusEvent = false;
        receivedTraversalEvent = false;
        receivedSelectionEvent = false;
    }

    public int getTraverseFromIndex() {
        return traverseFromIndex;
    }

    public void setTraverseFromIndex(int traverseFromIndex) {
        this.traverseFromIndex = traverseFromIndex;
    }

    public int getTraverseToIndex() {
        return traverseToIndex;
    }

    public void setTraverseToIndex(int traverseToIndex) {
        this.traverseToIndex = traverseToIndex;
    }

    public int getSelectionFromIndex() {
        return selectionFromIndex;
    }

    public void setSelectionFromIndex(int selectionFromIndex) {
        this.selectionFromIndex = selectionFromIndex;
    }

    public int getSelectionToIndex() {
        return selectionToIndex;
    }

    public void setSelectionToIndex(int selectionToIndex) {
        this.selectionToIndex = selectionToIndex;
    }

    public String getAnnouncementText() {
        return announcementText;
    }

    public void setAnnouncementText(String announcementText) {
        this.announcementText = announcementText;
    }

    public int getTypeWindowContentChangedCount() {
        return typeWindowContentChangedCount;
    }

    public void setTypeWindowContentChangedCount(int typeWindowContentChangedCount) {
        this.typeWindowContentChangedCount = typeWindowContentChangedCount;
    }

    public void incrementWindowContentChangedCount() {
        this.typeWindowContentChangedCount++;
    }

    public boolean hasReceivedEvent() {
        return receivedEvent;
    }

    public void setReceivedEvent(boolean receivedEvent) {
        this.receivedEvent = receivedEvent;
    }

    public boolean hasReceivedAccessibilityFocusEvent() {
        return receivedAccessibilityFocusEvent;
    }

    public void setReceivedAccessibilityFocusEvent(boolean receivedAccessibilityFocusEvent) {
        this.receivedAccessibilityFocusEvent = receivedAccessibilityFocusEvent;
    }

    public boolean hasReceivedTraversalEvent() {
        return receivedTraversalEvent;
    }

    public void setReceivedTraversalEvent(boolean receivedTraversalEvent) {
        this.receivedTraversalEvent = receivedTraversalEvent;
    }

    public boolean hasReceivedSelectionEvent() {
        return receivedSelectionEvent;
    }

    public void setReceivedSelectionEvent(boolean receivedSelectionEvent) {
        this.receivedSelectionEvent = receivedSelectionEvent;
    }
}
