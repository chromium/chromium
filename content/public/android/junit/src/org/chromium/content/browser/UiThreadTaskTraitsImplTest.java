// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.BrowserTaskType;

/**
 * Tests for {@link UiThreadTaskTraitsImpl}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UiThreadTaskTraitsImplTest {
    @Test
    @SmallTest
    public void testContainsExtension() {
        TaskTraits traits = UiThreadTaskTraitsImpl.BOOTSTRAP;
        UiThreadTaskTraitsImpl impl = traits.getExtension(UiThreadTaskTraitsImpl.DESCRIPTOR);

        assertNotNull(impl);
    }

    @Test
    @SmallTest
    public void testCanDeserializeProperties() {
        TaskTraits traits = UiThreadTaskTraitsImpl.BOOTSTRAP;
        UiThreadTaskTraitsImpl impl = traits.getExtension(UiThreadTaskTraitsImpl.DESCRIPTOR);

        assertEquals(BrowserTaskType.BOOTSTRAP, impl.getTaskType());
    }
}
