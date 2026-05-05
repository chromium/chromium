// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.provider.Browser;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.selection.SelectionUtils;

/** Unit tests for {@link SelectionUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SelectionUtilsTest {

    @Test
    public void testSanitizeQuery() {
        assertNull(SelectionUtils.sanitizeQuery(null, 10));
        assertEquals("", SelectionUtils.sanitizeQuery("", 10));
        assertEquals("short", SelectionUtils.sanitizeQuery("short", 10));
        assertEquals("long stri…", SelectionUtils.sanitizeQuery("long string", 9));
    }

    @Test
    public void testShare() {
        Context context = RuntimeEnvironment.application;
        SelectionUtils.share(context, "test share");
        Intent intent =
                Shadows.shadowOf((android.app.Application) context).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Intent.ACTION_CHOOSER, intent.getAction());
    }

    @Test
    public void testWebSearch() {
        Context context = RuntimeEnvironment.application;
        SelectionUtils.webSearch(context, "test search");
        Intent intent = Shadows.shadowOf((Application) context).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Intent.ACTION_WEB_SEARCH, intent.getAction());
        assertTrue(intent.getBooleanExtra(Browser.EXTRA_CREATE_NEW_TAB, false));
        assertTrue((intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0);
    }

    @Test
    public void testTranslate() {
        Context context = RuntimeEnvironment.application;
        SelectionUtils.translate(context, "test translate");
        Intent intent = Shadows.shadowOf((Application) context).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Intent.ACTION_TRANSLATE, intent.getAction());
    }
}
