// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import androidx.collection.ArraySet;

import java.util.Set;

/**
 * Add a snippet of code here that you want to test.
 */
public class Playground {
    public static void testArraySet() {
        Set<Integer> set = new ArraySet<>();
        for (int i = 0; i < 6; i++) {
            set.add(i);
        }
        Integer[] array = set.toArray(new Integer[0]);
    }

    public static void main(String[] args) {
        testArraySet();
    }
}
