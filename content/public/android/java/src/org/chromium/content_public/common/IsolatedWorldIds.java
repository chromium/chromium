// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.common;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The Java copy of //content/public/common/isolated_world_ids.h
 * See there for definitions of each ID.
 *
 * Both files must be kept in sync.
 */
// LINT.IfChange
@IntDef({
    IsolatedWorldIds.ISOLATED_WORLD_ID_GLOBAL,
    IsolatedWorldIds.ISOLATED_WORLD_ID_CONTENT_END,
    IsolatedWorldIds.ISOLATED_WORLD_ID_MAX
})
@Retention(RetentionPolicy.SOURCE)
public @interface IsolatedWorldIds {
    int ISOLATED_WORLD_ID_GLOBAL = 0;
    int ISOLATED_WORLD_ID_CONTENT_END = 1;
    int ISOLATED_WORLD_ID_MAX = ISOLATED_WORLD_ID_CONTENT_END + 10;
}
// LINT.ThenChange(//content/public/common/isolated_world_ids.h)
