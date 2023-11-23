// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.IdRes;

import java.util.Collection;
import java.util.SortedSet;
import java.util.TreeSet;

/** Data class representing a group in the text selection menu. */
public final class SelectionMenuGroup implements Comparable<SelectionMenuGroup> {
    public final @IdRes int id;
    public final int order;
    public final SortedSet<SelectionMenuItem> items;

    public SelectionMenuGroup(int id, int order) {
        this.id = id;
        this.order = order;
        items = new TreeSet<>();
    }

    public void addItem(SelectionMenuItem item) {
        items.add(item);
    }

    public void addItems(Collection<SelectionMenuItem> items) {
        this.items.addAll(items);
    }

    /** Allows usage with {@link SortedSet} sorting. */
    @Override
    public int compareTo(SelectionMenuGroup otherGroup) {
        return order - otherGroup.order;
    }
}
