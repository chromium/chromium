// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build;

import com.android.tools.lint.Main;

/**
 * This wrapper is needed to fix lint hanging, see: https://crbug.com/1326906
 */
public class CustomLint {
    public static void main(String[] args) {
        System.exit(new Main().run(args));
    }
}
